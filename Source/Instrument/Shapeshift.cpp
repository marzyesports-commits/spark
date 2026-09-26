#include "Shapeshift.h"
#include "Common/Envelope.h"
#include "Common/Wavetable.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <numeric>
#include <complex>

namespace spark::shapeshift
{
namespace
{
    std::vector<float> monoMix (const juce::AudioBuffer<float>& a)
    {
        std::vector<float> m ((size_t) a.getNumSamples(), 0.0f);
        for (int c = 0; c < a.getNumChannels(); ++c)
            juce::FloatVectorOperations::addWithMultiply (m.data(), a.getReadPointer (c), 1.0f / (float) a.getNumChannels(), a.getNumSamples());
        return m;
    }

    // YIN pitch estimate on x[start .. start+window+maxLag]. Returns frequency or 0, and the confidence (lower is better).
    double yin (const std::vector<float>& x, int start, int window, double sr, double minHz, double maxHz, float& aperiodicity)
    {
        const int minLag = juce::jmax (2, (int) (sr / maxHz));
        const int maxLag = (int) (sr / minHz);
        aperiodicity = 1.0f;
        if (start < 0 || start + window + maxLag + 2 >= (int) x.size())
            return 0.0;

        std::vector<double> d ((size_t) maxLag + 2, 0.0);
        for (int tau = 1; tau <= maxLag + 1; ++tau)
        {
            double sum = 0;
            const float* a = x.data() + start;
            const float* b = a + tau;
            for (int i = 0; i < window; ++i)
            {
                const double diff = (double) a[i] - b[i];
                sum += diff * diff;
            }
            d[(size_t) tau] = sum;
        }
        // cumulative mean normalised difference
        std::vector<double> cm (d.size(), 1.0);
        double running = 0;
        for (int tau = 1; tau <= maxLag + 1; ++tau)
        {
            running += d[(size_t) tau];
            cm[(size_t) tau] = running > 0 ? d[(size_t) tau] * tau / running : 1.0;
        }
        int best = -1;
        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            if (cm[(size_t) tau] < 0.15)
            {
                while (tau + 1 <= maxLag && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag; tau <= maxLag; ++tau)
                if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
        }
        aperiodicity = (float) cm[(size_t) best];
        // parabolic interpolation around the dip
        double shift = 0;
        if (best > 1 && best < maxLag)
        {
            const double a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
            const double den = a - 2 * b + c;
            if (std::abs (den) > 1e-12) shift = 0.5 * (a - c) / den;
        }
        return sr / (best + juce::jlimit (-0.5, 0.5, shift));
    }

    // Level (RMS over 20 ms, both channels, so detuned stereo voices don't fool it) every 5 ms
    std::vector<float> envelopeOf (const juce::AudioBuffer<float>& a, double sr, int& hop)
    {
        hop = juce::jmax (1, (int) (sr * 0.005));
        const int win = hop * 4;
        std::vector<float> e;
        for (int i = 0; i + win <= a.getNumSamples(); i += hop)
        {
            double s = 0;
            for (int c = 0; c < a.getNumChannels(); ++c)
            {
                const float* d = a.getReadPointer (c, i);
                for (int k = 0; k < win; ++k) s += (double) d[k] * d[k];
            }
            e.push_back ((float) std::sqrt (s / (win * a.getNumChannels())));
        }
        return e;
    }

    // Moving average, to see the envelope through unison beating
    std::vector<float> smooth (const std::vector<float>& e, int radius)
    {
        std::vector<float> out (e.size());
        double acc = 0;
        std::vector<double> prefix (e.size() + 1, 0.0);
        for (size_t i = 0; i < e.size(); ++i) prefix[i + 1] = prefix[i] + e[i];
        for (int i = 0; i < (int) e.size(); ++i)
        {
            const int a = juce::jmax (0, i - radius), b = juce::jmin ((int) e.size(), i + radius + 1);
            out[(size_t) i] = (float) ((prefix[(size_t) b] - prefix[(size_t) a]) / (b - a));
        }
        juce::ignoreUnused (acc);
        return out;
    }

    // Find the curve c for which Envelope::shape (0.5, c) == target.
    float solveCurve (float target)
    {
        target = juce::jlimit (0.03f, 0.97f, target);
        float lo = -1.0f, hi = 1.0f;
        for (int i = 0; i < 40; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            if (Envelope::shape (0.5f, mid) < target) lo = mid; else hi = mid;
        }
        return 0.5f * (lo + hi);
    }

}

namespace
{
    // Complex amplitude of each harmonic of f0 around 'centre', measured over ~4 periods with a Hann window.
    // Averaging over several periods is what lets detuned unison voices through without cancelling the highs.
    std::vector<std::complex<double>> harmonicSpectrum (const std::vector<float>& x, double centre, double f0, double sr, int maxHarmonics)
    {
        const int W = juce::jmax (256, (int) (4.0 * sr / f0));
        const int start = juce::jlimit (0, juce::jmax (0, (int) x.size() - W), (int) centre - W / 2);
        const int n = juce::jmin (W, (int) x.size() - start);
        std::vector<double> w ((size_t) n);
        double wsum = 0;
        for (int i = 0; i < n; ++i) { w[(size_t) i] = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (n - 1)); wsum += w[(size_t) i]; }

        std::vector<std::complex<double>> out ((size_t) maxHarmonics + 1);
        for (int h = 1; h <= maxHarmonics; ++h)
        {
            const double omega = juce::MathConstants<double>::twoPi * h * f0 / sr;
            // recurrence for e^{-i omega n}
            std::complex<double> rot (std::cos (omega), -std::sin (omega)), z (1.0, 0.0), acc (0.0, 0.0);
            for (int i = 0; i < n; ++i)
            {
                acc += (double) x[(size_t) (start + i)] * w[(size_t) i] * z;
                z *= rot;
            }
            // reference phase to the window centre so frames at different times line up
            const double centrePhase = omega * (centre - start);
            out[(size_t) h] = acc * (2.0 / wsum) * std::complex<double> (std::cos (centrePhase), std::sin (centrePhase));
        }
        return out;
    }

    // Strength of each harmonic as the total energy in its band ((h - 0.5) f0 .. (h + 0.5) f0).
    // Unison detune spreads upper harmonics across the band; summing the band keeps them.
    std::vector<double> harmonicBandMagnitudes (const std::vector<float>& x, double centre, double f0, double sr, int maxHarmonics)
    {
        const int W = juce::jmax (256, (int) (4.0 * sr / f0));
        int order = 1;
        while ((1 << order) < 2 * W) ++order;
        const int N = 1 << order;
        const int start = juce::jlimit (0, juce::jmax (0, (int) x.size() - W), (int) centre - W / 2);
        const int n = juce::jmin (W, (int) x.size() - start);

        std::vector<float> buf ((size_t) N * 2, 0.0f);
        double w2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (n - 1));
            buf[(size_t) i] = (float) (x[(size_t) (start + i)] * w);
            w2 += w * w;
        }
        juce::dsp::FFT fft (order);
        fft.performFrequencyOnlyForwardTransform (buf.data());

        std::vector<double> mags ((size_t) maxHarmonics + 1, 0.0);
        const double binHz = sr / N;
        for (int h = 1; h <= maxHarmonics; ++h)
        {
            const int k0 = juce::jmax (1, (int) std::ceil ((h - 0.5) * f0 / binHz));
            const int k1 = juce::jmin (N / 2 - 1, (int) std::floor ((h + 0.5) * f0 / binHz));
            double power = 0;
            for (int k = k0; k <= k1; ++k) power += (double) buf[(size_t) k] * buf[(size_t) k];
            mags[(size_t) h] = std::sqrt (4.0 * power / (N * w2));
        }
        return mags;
    }

    std::vector<float> synthesiseCycle (const std::vector<double>& mags, const std::vector<double>& phases)
    {
        constexpr int N = Wavetable::frameSize;
        std::vector<float> y ((size_t) N, 0.0f);
        for (size_t h = 1; h < mags.size(); ++h)
        {
            if (mags[h] < 1e-7) continue;
            for (int i = 0; i < N; ++i)
                y[(size_t) i] += (float) (mags[h] * std::cos (juce::MathConstants<double>::twoPi * (double) h * i / N + phases[h]));
        }
        float peak = 0;
        for (auto v : y) peak = juce::jmax (peak, std::abs (v));
        if (peak > 1e-6f)
            for (auto& v : y) v *= 0.95f / peak;
        return y;
    }
}

juce::String noteName (float midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = juce::roundToInt (midi);
    const int cents = juce::roundToInt ((midi - (float) n) * 100.0f);
    juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 2); // MIDI 60 = C3 (Ableton / FL convention)
    if (std::abs (cents) >= 5)
        s << (cents > 0 ? " +" : " ") << cents << "c";
    return s;
}

float timeToParam (float seconds)
{
    return juce::jlimit (0.0f, 1.0f, std::sqrt (juce::jmax (0.0f, (seconds * 1000.0f - 1.0f) / 4999.0f)));
}

float detectMidiNote (const juce::AudioBuffer<float>& audio, double sr)
{
    const auto x = monoMix (audio);
    const int len = (int) x.size();
    if (len < (int) (sr * 0.08))
        return -1.0f;

    // try a few spots after the attack and take the median of the confident ones
    std::vector<double> notes;
    const int window = juce::jmin ((int) (sr * 0.04), len / 4);
    for (double frac : { 0.15, 0.3, 0.45 })
    {
        float ap = 1;
        const double f = yin (x, (int) (len * frac), window, sr, 30.0, 2000.0, ap);
        if (f > 0 && ap < 0.2f)
            notes.push_back (69.0 + 12.0 * std::log2 (f / 440.0));
    }
    if (notes.size() < 2)
        return -1.0f;
    std::sort (notes.begin(), notes.end());
    const double med = notes[notes.size() / 2];
    for (auto n : notes)
        if (std::abs (n - med) > 0.5) return -1.0f; // unstable: not a single clear pitch
    return (float) med;
}

ShapeshiftResult analyse (const juce::AudioBuffer<float>& audio, double sr)
{
    ShapeshiftResult r;
    const auto x = monoMix (audio);
    const int len = (int) x.size();
    if (len < (int) (sr * 0.1))
    {
        r.error = "That sound is too short. Bounce a single note of at least half a second.";
        return r;
    }

    // ---- level envelope, onset and end
    int hop = 1;
    auto env = envelopeOf (audio, sr, hop);
    const float globalPeak = *std::max_element (env.begin(), env.end());
    if (globalPeak < 1e-4f)
    {
        r.error = "That sound is silent.";
        return r;
    }
    const int frames = (int) env.size();
    int onset = 0;
    while (onset < frames - 1 && env[(size_t) onset] < globalPeak * 0.03f) ++onset;
    int last = frames - 1;
    while (last > onset && env[(size_t) last] < globalPeak * 0.001f) --last;
    const auto t = [&] (int k) { return (float) k * (float) hop / (float) sr; };

    // ---- attack: the first peak, looked for in the opening of the note only
    const int searchEnd = juce::jmin (last, onset + juce::jmax (8, juce::jmin ((int) (0.6 * sr / hop), (last - onset) * 40 / 100)));
    int peakIdx = onset;
    for (int k = onset; k <= searchEnd; ++k)
        if (env[(size_t) k] > env[(size_t) peakIdx]) peakIdx = k;
    const float attackPeak = env[(size_t) peakIdx];
    for (auto& e : env) e /= attackPeak;   // levels are relative to the attack peak from here on
    const auto envS = smooth (env, 10);     // ~50 ms each side: looks through unison beating

    int a90 = onset;
    while (a90 < peakIdx && env[(size_t) a90] < 0.9f) ++a90;
    r.attack = juce::jmax (0.001f, t (a90 - onset));
    if (a90 - onset >= 4)
    {
        const int mid = onset + (a90 - onset) / 2;
        const float lo = env[(size_t) onset], hi = env[(size_t) a90];
        r.attackCurve = solveCurve ((env[(size_t) mid] - lo) / juce::jmax (0.05f, hi - lo));
    }

    // ---- sustain: the typical level in the middle of the note
    std::vector<float> middle;
    const int m0 = peakIdx + (last - peakIdx) * 30 / 100, m1 = peakIdx + (last - peakIdx) * 60 / 100;
    for (int k = m0; k < m1; ++k) middle.push_back (juce::jmin (1.0f, envS[(size_t) k]));
    float sustain = 0.0f;
    if (! middle.empty())
    {
        std::nth_element (middle.begin(), middle.begin() + (long) middle.size() / 2, middle.end());
        sustain = middle[middle.size() / 2];
    }
    const bool plucky = sustain < 0.06f;

    // ---- release start: the last time the level is still near the sustain level
    int relStart = last;
    if (! plucky)
    {
        relStart = last;
        while (relStart > peakIdx && envS[(size_t) relStart] < sustain * 0.85f) --relStart;
    }

    // ---- hold and decay
    int holdEnd = peakIdx;
    while (holdEnd < relStart && envS[(size_t) holdEnd] >= 0.95f) ++holdEnd;
    r.hold = t (holdEnd - peakIdx) > 0.02f ? t (holdEnd - peakIdx) : 0.0f;

    const float target = plucky ? 0.001f : sustain + 0.03f * (1.0f - sustain);
    int decEnd = holdEnd;
    while (decEnd < relStart && envS[(size_t) decEnd] > target) ++decEnd;
    r.decay = juce::jmax (0.005f, t (decEnd - holdEnd));
    r.sustain = plucky ? 0.0f : juce::jlimit (0.0f, 1.0f, sustain);
    if (decEnd - holdEnd >= 4 && r.sustain < 0.97f)
    {
        const float lvl = juce::jmin (1.0f, envS[(size_t) (holdEnd + (decEnd - holdEnd) / 2)]);
        const float progress = (1.0f - lvl) / juce::jmax (0.02f, 1.0f - r.sustain);
        r.decayCurve = solveCurve (progress);
    }

    // ---- release
    if (plucky)
    {
        r.release = 0.08f;
    }
    else
    {
        int relEnd = relStart;
        const float startLevel = envS[(size_t) relStart];
        while (relEnd < last && envS[(size_t) relEnd] > startLevel * 0.01f) ++relEnd;
        r.release = juce::jlimit (0.01f, 5.0f, t (relEnd - relStart));
        if (relEnd - relStart >= 4)
        {
            const float lvl = envS[(size_t) (relStart + (relEnd - relStart) / 2)] / juce::jmax (1e-4f, startLevel);
            r.releaseCurve = solveCurve (1.0f - lvl);
        }
    }

    // ---- stereo width (side vs mid energy through the sounding part)
    if (audio.getNumChannels() > 1)
    {
        double mid = 0, side = 0;
        const float* L = audio.getReadPointer (0);
        const float* R = audio.getReadPointer (1);
        for (int i = onset * hop; i < juce::jmin (len, relStart * hop); ++i)
        {
            const double m = 0.5 * (L[i] + R[i]), sd = 0.5 * (L[i] - R[i]);
            mid += m * m;
            side += sd * sd;
        }
        r.width = mid > 0 ? juce::jlimit (0.0f, 1.0f, (float) std::sqrt (side / mid) * 1.5f) : 0.0f;
    }

    // ---- pitch and the evolving wavetable
    const int soundStart = onset * hop;
    const int soundEnd = juce::jmax (soundStart + (int) (sr * 0.05), (plucky ? decEnd : relStart) * hop);
    const float midi = detectMidiNote (audio, sr);
    r.pitched = midi > 0.0f;
    r.midiNote = r.pitched ? midi : 60.0f;
    r.scanSeconds = juce::jmax (0.05f, (float) (soundEnd - soundStart) / (float) sr);

    constexpr int numFrames = 64;
    if (r.pitched)
    {
        const double f0 = 440.0 * std::pow (2.0, (r.midiNote - 69.0) / 12.0);
        const int maxH = juce::jlimit (1, Wavetable::frameSize / 2 - 1, (int) (sr * 0.45 / f0));
        std::vector<std::vector<std::complex<double>>> spectra;
        std::vector<std::vector<double>> bandMags;
        for (int k = 0; k < numFrames; ++k)
        {
            const double centre = soundStart + (soundEnd - soundStart) * (k + 0.5) / numFrames;
            // follow small glides or drift in the note
            float ap = 1;
            const int w = (int) (sr / f0 * 2.5);
            const double local = yin (x, (int) centre - w, w, sr, f0 * 0.9, f0 * 1.1, ap);
            const double fk = local > 0 && ap < 0.3f ? local : f0;
            spectra.push_back (harmonicSpectrum (x, centre, fk, sr, maxH));
            bandMags.push_back (harmonicBandMagnitudes (x, centre, fk, sr, maxH));
        }

        // One shared set of phases (from the brightest early frame, relative to the fundamental) for every frame,
        // so neighbouring frames blend without cancelling each other while Morph scans through them.
        int ref = 0;
        double best = -1;
        for (int k = 0; k < juce::jmin (16, numFrames); ++k)
        {
            double e = 0;
            for (int h = 2; h <= maxH; ++h) e += std::norm (spectra[(size_t) k][(size_t) h]) * h;
            if (e > best) { best = e; ref = k; }
        }
        std::vector<double> phases ((size_t) maxH + 1, 0.0);
        const double phi1 = std::arg (spectra[(size_t) ref][1]);
        for (int h = 1; h <= maxH; ++h)
            phases[(size_t) h] = std::arg (spectra[(size_t) ref][(size_t) h]) - h * phi1;

        for (int k = 0; k < numFrames; ++k)
        {
            std::vector<double> mags ((size_t) maxH + 1, 0.0);
            for (int h = 1; h <= maxH; ++h) mags[(size_t) h] = bandMags[(size_t) k][(size_t) h];
            r.frames.push_back (synthesiseCycle (mags, phases));
        }
    }
    else
    {
        juce::AudioBuffer<float> part (1, soundEnd - soundStart);
        part.copyFrom (0, 0, x.data() + soundStart, part.getNumSamples());
        auto table = Wavetable::fromSample (part, numFrames);
        for (int k = 0; k < table->getNumFrames(); ++k)
            r.frames.push_back (table->rawFrame (k));
    }

    // ---- summary
    auto ms = [] (float s) { return s >= 1.0f ? juce::String (s, 2) + " s" : juce::String (juce::roundToInt (s * 1000.0f)) + " ms"; };
    r.summary << (r.pitched ? "Note " + noteName (r.midiNote) : juce::String ("No single clear pitch, treated as a texture")) << "\n"
              << "Timbre: " << numFrames << " wavetable frames, played through over " << ms (r.scanSeconds) << "\n"
              << "Envelope: attack " << ms (r.attack) << (r.hold > 0 ? ", hold " + ms (r.hold) : juce::String())
              << ", decay " << ms (r.decay) << ", sustain " << juce::roundToInt (r.sustain * 100.0f) << "%, release " << ms (r.release) << "\n"
              << "Stereo width: " << juce::roundToInt (r.width * 100.0f) << "%";
    r.ok = true;
    return r;
}
} // namespace spark::shapeshift

#include "SparkVoice.h"

namespace spark
{
namespace
{
    constexpr int chunkSize = 512;

    struct HannTable
    {
        static constexpr int size = 1024;
        float data[size + 1];
        HannTable()
        {
            for (int i = 0; i <= size; ++i)
                data[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        }
        float at (float x) const noexcept // x in 0..1
        {
            const float p = juce::jlimit (0.0f, 1.0f, x) * size;
            const int i = juce::jmin ((int) p, size - 1);
            return data[i] + (data[i + 1] - data[i]) * (p - (float) i);
        }
    };

    const HannTable& hann()
    {
        static HannTable t;
        return t;
    }

    inline float readInterp (const float* d, int len, double pos) noexcept
    {
        const int i0 = (int) pos;
        const float frac = (float) (pos - i0);
        const float a = d[i0 % len];
        const float b = d[(i0 + 1) % len];
        return a + (b - a) * frac;
    }
}

SparkVoice::SparkVoice (InstrumentProcessor& p) : processor (p)
{
    scratch.setSize (2, chunkSize);
}

void SparkVoice::setCurrentPlaybackSampleRate (double newRate)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    if (newRate > 0)
    {
        ampEnv.setSampleRate (newRate);
        toneEnv.setSampleRate (newRate);
        filter.prepare ({ newRate, (juce::uint32) chunkSize, 2 });
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setResonance (0.85f);
    }
}

void SparkVoice::startNote (int midiNote, float vel, juce::SynthesiserSound*, int pitchWheel)
{
    source = processor.getSource();
    note = midiNote;
    velocity = vel;
    pitchWheelMoved (pitchWheel);

    for (auto& g : grains) g.active = false;
    samplesToNextGrain = 0.0;
    samplePos = 0.0;
    noteSamples = 0.0;
    // Unison oscillators start together so every note has the same level and punch;
    // the detune then drifts them apart naturally.
    const double startPhase = random.nextDouble();
    for (auto& ph : phases) ph = startPhase;
    lfoPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;

    filter.reset();
    driveAmount = processor.params.facet[InstrumentProcessor::drive]->load();
    baseCutoff = fmt::cutoffHz (processor.params.facet[InstrumentProcessor::tone]->load());
    ampEnv.reset();
    toneEnv.reset();
    ampEnv.noteOn();
    toneEnv.noteOn();
}

void SparkVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        toneEnv.noteOff();
    }
    else
    {
        ampEnv.reset();
        toneEnv.reset();
        clearCurrentNote();
    }
}

// Per note: drive -> low-pass (swept by the tone envelope) -> amp envelope.
void SparkVoice::processChain (float* l, float* r, int n)
{
    auto& p = processor.params;
    const auto ampSettings = p.amp.settings();
    const auto toneSettings = p.toneEnv.settings();

    const float velAmp = p.ampVelocity->load();
    const float ampGain = 1.0f - velAmp * (1.0f - velocity);
    const float velTone = p.toneVelocity->load();
    const float octaves = p.toneAmount->load() * 5.0f * (1.0f - velTone * (1.0f - velocity));

    const float targetDrive = p.facet[InstrumentProcessor::drive]->load();
    const float targetCutoff = fmt::cutoffHz (p.facet[InstrumentProcessor::tone]->load());
    const float nyquistSafe = (float) getSampleRate() * 0.45f;

    for (int i = 0; i < n; ++i)
    {
        // gentle per-sample smoothing of the facet values
        driveAmount += (targetDrive - driveAmount) * 0.002f;
        baseCutoff += (targetCutoff - baseCutoff) * 0.002f;

        const float te = toneEnv.next (toneSettings);
        if ((i & 15) == 0)
        {
            const float hz = baseCutoff * std::exp2 (octaves * te);
            filter.setCutoffFrequency (juce::jlimit (20.0f, nyquistSafe, hz));
        }

        const float g = 1.0f + driveAmount * 12.0f;
        const float makeup = 1.0f / std::sqrt (g);
        const float env = ampEnv.next (ampSettings) * ampGain;
        l[i] = filter.processSample (0, std::tanh (g * l[i]) * makeup) * env;
        r[i] = filter.processSample (1, std::tanh (g * r[i]) * makeup) * env;
    }
}

void SparkVoice::pitchWheelMoved (int value)
{
    bendSemitones = (float) (value - 8192) / 8192.0f * 2.0f;
}

void SparkVoice::spawnGrain (const SourceData& src, double ratio, float position, float grainSec, float motion, float scan)
{
    auto it = std::find_if (grains.begin(), grains.end(), [] (const Grain& g) { return ! g.active; });
    if (it == grains.end())
        return;

    const int len = src.audio.getNumSamples();
    const float spray = (random.nextFloat() - 0.5f) * motion * 0.3f;
    float start = std::fmod (position + scan + spray + 2.0f, 1.0f);

    auto& g = *it;
    g.active = true;
    g.pos = (double) start * (double) (len - 1);
    g.rate = ratio;
    g.age = 0;
    g.length = juce::jmax (64, (int) (grainSec * getSampleRate()));

    const float pan = (random.nextFloat() * 2.0f - 1.0f) * (0.2f + motion * 0.6f);
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g.gainL = std::cos (angle);
    g.gainR = std::sin (angle);
}

void SparkVoice::renderGrains (float* left, float* right, int n, const SourceData& src, double ratio,
                               float position, float grainSec, float motion, float scan)
{
    const int len = src.audio.getNumSamples();
    if (len < 2)
        return;

    const float* chL = src.audio.getReadPointer (0);
    const float* chR = src.audio.getReadPointer (src.audio.getNumChannels() > 1 ? 1 : 0);
    const double grainLength = juce::jmax (64.0, (double) grainSec * getSampleRate());
    const auto& window = hann();

    for (int i = 0; i < n; ++i)
    {
        if (samplesToNextGrain <= 0.0)
        {
            spawnGrain (src, ratio, position, grainSec, motion, scan);
            // four overlapping grains, with timing jitter from Motion
            samplesToNextGrain += grainLength * 0.25 * (1.0 + (random.nextDouble() - 0.5) * motion * 0.6);
        }
        samplesToNextGrain -= 1.0;

        float l = 0.0f, r = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active)
                continue;
            const float w = window.at ((float) g.age / (float) g.length);
            l += readInterp (chL, len, g.pos) * w * g.gainL;
            r += readInterp (chR, len, g.pos) * w * g.gainR;
            g.pos += g.rate;
            while (g.pos >= (double) len) g.pos -= (double) len;
            if (++g.age >= g.length)
                g.active = false;
        }
        left[i] = l * 0.6f;
        right[i] = r * 0.6f;
    }
}

void SparkVoice::renderSample (float* left, float* right, int n, const SourceData& src, double ratio)
{
    // Straight playback of the recording from the start, pitched from its root note.
    const int len = src.audio.getNumSamples();
    const float* chL = src.audio.getReadPointer (0);
    const float* chR = src.audio.getReadPointer (src.audio.getNumChannels() > 1 ? 1 : 0);
    for (int i = 0; i < n; ++i)
    {
        if (samplePos >= len - 1)
        {
            left[i] = right[i] = 0.0f;
            continue;
        }
        const int i0 = (int) samplePos;
        const float fr = (float) (samplePos - i0);
        left[i] = (chL[i0] + (chL[i0 + 1] - chL[i0]) * fr) * 0.8f;
        right[i] = (chR[i0] + (chR[i0 + 1] - chR[i0]) * fr) * 0.8f;
        samplePos += ratio;
    }
}

void SparkVoice::renderTable (float* left, float* right, int n, const Wavetable& table, double baseHz, float morph, float motion, float scanSeconds)
{
    const double sr = getSampleRate();
    // Unison as stereo width: the centre oscillator carries the sound (full and bright in mono),
    // the detuned pair only adds side signal, so nothing cancels when summed to mono.
    const float detuneCents = motion * 12.0f;
    const float sideAmount = juce::jmin (1.0f, motion * 2.5f) * 0.5f;
    const double ratios[3] = { 1.0, std::pow (2.0, detuneCents / 1200.0), std::pow (2.0, -detuneCents / 1200.0) };
    const float lastFrame = (float) (table.getNumFrames() - 1);
    const float lfoInc = (0.1f + motion * 0.9f) * juce::MathConstants<float>::twoPi / (float) sr;

    int mips[3];
    for (int k = 0; k < 3; ++k)
        mips[k] = Wavetable::mipForIncrement (baseHz * ratios[k] / sr);

    for (int i = 0; i < n; ++i)
    {
        // Morph scan sweeps from Morph to the last frame over scanSeconds (Shapeshift uses this to replay a sound's evolution)
        const float scanned = scanSeconds > 0.0f ? morph + (1.0f - morph) * (float) juce::jmin (1.0, noteSamples / (scanSeconds * sr)) : morph;
        noteSamples += 1.0;
        const float m = juce::jlimit (0.0f, 1.0f, scanned + std::sin (lfoPhase) * motion * (scanSeconds > 0.0f ? 0.05f : 0.35f));
        const float framePos = m * lastFrame;
        lfoPhase += lfoInc;
        if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

        float s[3];
        for (int k = 0; k < 3; ++k)
        {
            s[k] = table.sample (framePos, phases[k], mips[k]);
            phases[k] += baseHz * ratios[k] / sr;
            phases[k] -= std::floor (phases[k]);
        }
        const float side = (s[1] - s[2]) * sideAmount;
        left[i] = (s[0] + side) * 0.6f;
        right[i] = (s[0] - side) * 0.6f;
    }
}

void SparkVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive() || source == nullptr)
        return;

    auto& p = processor.params;

    const float pitchSt = fmt::semitoneValue (p.facet[InstrumentProcessor::pitch]->load());
    const float position = p.facet[InstrumentProcessor::position]->load();
    const float grainSec = fmt::grainSeconds (p.facet[InstrumentProcessor::grain]->load());
    const float morph = p.facet[InstrumentProcessor::morph]->load();
    const float motion = p.facet[InstrumentProcessor::motion]->load();
    const int mode = juce::roundToInt (p.mode->load());
    const bool tableMode = mode == InstrumentProcessor::tableMode;
    const bool sampleMode = mode == InstrumentProcessor::sampleMode;
    const float scanSeconds = p.scanTime->load() > 0.001f ? fmt::envSeconds (p.scanTime->load()) * 2.0f : 0.0f;

    const float semis = (float) note - source->rootNote + pitchSt + bendSemitones;
    const double ratio = std::pow (2.0, semis / 12.0) * (source->sampleRate / getSampleRate());
    const double baseHz = 440.0 * std::pow (2.0, ((double) note - 69.0 + pitchSt + bendSemitones) / 12.0);

    while (numSamples > 0)
    {
        const int n = juce::jmin (numSamples, chunkSize);
        float* l = scratch.getWritePointer (0);
        float* r = scratch.getWritePointer (1);

        if (sampleMode)
        {
            renderSample (l, r, n, *source, ratio);
        }
        else if (tableMode && source->table != nullptr)
        {
            renderTable (l, r, n, *source->table, baseHz, morph, motion, scanSeconds);
        }
        else
        {
            // Motion also scans the grain position slowly through the sound
            const float scan = std::sin (lfoPhase) * motion * 0.15f;
            lfoPhase += (0.15f + motion * 1.5f) * juce::MathConstants<float>::twoPi * (float) n / (float) getSampleRate();
            if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;
            renderGrains (l, r, n, *source, ratio, position, grainSec, motion, scan);
        }

        processChain (l, r, n);

        out.addFrom (0, startSample, l, n);
        if (out.getNumChannels() > 1)
            out.addFrom (1, startSample, r, n);

        startSample += n;
        numSamples -= n;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
} // namespace spark

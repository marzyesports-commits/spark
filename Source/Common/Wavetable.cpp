#include "Wavetable.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>

namespace spark
{
namespace
{
    std::vector<float> resampleCycle (const float* src, int length, int outLength)
    {
        std::vector<float> out ((size_t) outLength);
        for (int i = 0; i < outLength; ++i)
        {
            const double pos = (double) i * (double) length / (double) outLength;
            const int i0 = (int) pos;
            const float frac = (float) (pos - i0);
            const float a = src[i0 % length];
            const float b = src[(i0 + 1) % length];
            out[(size_t) i] = a + (b - a) * frac;
        }
        return out;
    }

    void removeDcAndNormalise (std::vector<float>& f, float target = 0.95f)
    {
        double mean = 0;
        for (auto x : f) mean += x;
        mean /= (double) f.size();
        float peak = 0;
        for (auto& x : f) { x -= (float) mean; peak = juce::jmax (peak, std::abs (x)); }
        if (peak > 1.0e-6f)
            for (auto& x : f) x *= target / peak;
    }

    // Rotate so the cycle starts at the rising zero crossing closest to its start;
    // keeps neighbouring frames phase-aligned so morphing doesn't comb-filter.
    void alignToZeroCrossing (std::vector<float>& f)
    {
        const int n = (int) f.size();
        int best = 0;
        for (int i = 0; i < n; ++i)
        {
            if (f[(size_t) i] <= 0.0f && f[(size_t) ((i + 1) % n)] > 0.0f)
            {
                best = (i + 1) % n;
                break;
            }
        }
        std::rotate (f.begin(), f.begin() + best, f.end());
    }

    // Normalised autocorrelation period estimate. Returns 0 when nothing is clearly periodic.
    int detectPeriod (const float* x, int length)
    {
        const int minLag = 32, maxLag = juce::jmin (1024, length / 2);
        if (maxLag <= minLag)
            return 0;

        const int window = length - maxLag;
        double bestScore = 0.0;
        int bestLag = 0;
        double e0 = 0;
        for (int i = 0; i < window; ++i) e0 += (double) x[i] * x[i];
        if (e0 < 1.0e-8)
            return 0;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double c = 0, e1 = 0;
            for (int i = 0; i < window; ++i)
            {
                c += (double) x[i] * x[i + lag];
                e1 += (double) x[i + lag] * x[i + lag];
            }
            const double score = c / std::sqrt (e0 * e1 + 1.0e-12);
            // prefer the first strong peak (the fundamental), not later multiples
            if (score > bestScore + 0.02)
            {
                bestScore = score;
                bestLag = lag;
            }
            if (bestScore > 0.9 && score < bestScore - 0.2)
                break;
        }
        return bestScore > 0.6 ? bestLag : 0;
    }

    void fft (std::vector<std::complex<float>>& data, bool inverse)
    {
        static juce::dsp::FFT engine (11); // 2048
        std::vector<std::complex<float>> out (data.size());
        engine.perform (data.data(), out.data(), inverse);
        data.swap (out);
    }
}

Wavetable::Ptr Wavetable::fromFrames (std::vector<std::vector<float>> frames)
{
    Ptr t (new Wavetable());
    if (frames.empty())
        frames.push_back ({});

    for (auto& f : frames)
    {
        if (f.empty())
        {
            f.resize (frameSize);
            for (int i = 0; i < frameSize; ++i)
                f[(size_t) i] = std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) frameSize);
        }
        if ((int) f.size() != frameSize)
            f = resampleCycle (f.data(), (int) f.size(), frameSize);
        if ((int) t->raw.size() < maxFrames)
            t->raw.push_back (std::move (f));
    }
    t->numFrames = (int) t->raw.size();
    t->build();
    return t;
}

Wavetable::Ptr Wavetable::fromSample (const juce::AudioBuffer<float>& audio, int wantedFrames)
{
    const int len = audio.getNumSamples();
    if (len < 64)
        return fromFrames ({ {} });

    // mono mix
    std::vector<float> mono ((size_t) len);
    for (int c = 0; c < audio.getNumChannels(); ++c)
        juce::FloatVectorOperations::addWithMultiply (mono.data(), audio.getReadPointer (c), 1.0f / (float) audio.getNumChannels(), len);

    const int analysis = juce::jmin (3072, len);
    std::vector<std::vector<float>> frames;
    for (int f = 0; f < wantedFrames; ++f)
    {
        const double centre = ((double) f + 0.5) / (double) wantedFrames * (double) len;
        const int start = juce::jlimit (0, juce::jmax (0, len - analysis), (int) centre - analysis / 2);
        const float* seg = mono.data() + start;

        std::vector<float> cycle;
        if (const int period = detectPeriod (seg, analysis); period > 0)
        {
            cycle = resampleCycle (seg, period, frameSize);
        }
        else
        {
            // Unpitched: take a chunk and crossfade its tail into its head so it loops cleanly.
            const int chunk = juce::jlimit (256, analysis, len / wantedFrames);
            std::vector<float> c (seg, seg + chunk);
            const int fade = chunk / 8;
            for (int i = 0; i < fade; ++i)
            {
                const float w = (float) i / (float) fade;
                c[(size_t) i] = c[(size_t) i] * w + c[(size_t) (chunk - fade + i)] * (1.0f - w);
            }
            c.resize ((size_t) (chunk - fade));
            cycle = resampleCycle (c.data(), (int) c.size(), frameSize);
        }

        removeDcAndNormalise (cycle);
        alignToZeroCrossing (cycle);
        frames.push_back (std::move (cycle));
    }
    return fromFrames (std::move (frames));
}

Wavetable::Ptr Wavetable::fromTableAudio (const juce::AudioBuffer<float>& audio, int frameLength)
{
    const int len = audio.getNumSamples();
    frameLength = juce::jmax (16, frameLength);
    const int count = juce::jlimit (1, maxFrames, len / frameLength);
    std::vector<std::vector<float>> frames;
    for (int f = 0; f < count; ++f)
    {
        std::vector<float> fr ((size_t) frameLength, 0.0f);
        for (int c = 0; c < audio.getNumChannels(); ++c)
            juce::FloatVectorOperations::addWithMultiply (fr.data(), audio.getReadPointer (c) + f * frameLength,
                                                          1.0f / (float) audio.getNumChannels(), frameLength);
        frames.push_back (std::move (fr));
    }
    return fromFrames (std::move (frames));
}

void Wavetable::build()
{
    mipData.assign ((size_t) numFrames * numMips * (frameSize + 1), 0.0f);
    std::vector<std::complex<float>> spectrum ((size_t) frameSize), work ((size_t) frameSize);

    for (int f = 0; f < numFrames; ++f)
    {
        for (int i = 0; i < frameSize; ++i)
            spectrum[(size_t) i] = { raw[(size_t) f][(size_t) i], 0.0f };
        fft (spectrum, false);

        double rawRms = 0;
        {
            double mean = 0;
            for (auto x : raw[(size_t) f]) mean += x;
            mean /= frameSize;
            for (auto x : raw[(size_t) f]) rawRms += (x - mean) * (x - mean);
        }

        float scale = 1.0f;
        for (int m = 0; m < numMips; ++m)
        {
            const int maxHarmonic = juce::jmax (1, (frameSize / 2 - 1) >> m);
            work.assign ((size_t) frameSize, { 0.0f, 0.0f });
            for (int h = 1; h <= maxHarmonic; ++h)
            {
                work[(size_t) h] = spectrum[(size_t) h];
                work[(size_t) (frameSize - h)] = spectrum[(size_t) (frameSize - h)];
            }
            fft (work, true);

            auto* dst = const_cast<float*> (mipPtr (f, m));
            for (int i = 0; i < frameSize; ++i)
                dst[i] = work[(size_t) i].real();

            if (m == 0)
            {
                // work out the inverse FFT's scaling once, so every mip matches the source level
                double rms = 0;
                for (int i = 0; i < frameSize; ++i) rms += (double) dst[i] * dst[i];
                scale = rms > 1.0e-12 ? (float) std::sqrt (rawRms / rms) : 1.0f;
            }
            for (int i = 0; i < frameSize; ++i)
                dst[i] *= scale;
            dst[frameSize] = dst[0];
        }
    }
}

int Wavetable::mipForIncrement (double inc) noexcept
{
    if (inc <= 0.0)
        return 0;
    // highest harmonic that stays under ~0.45 of the sample rate
    const double maxHarmonic = 0.45 / inc;
    const double level = std::log2 ((double) (frameSize / 2 - 1) / juce::jmax (1.0, maxHarmonic));
    return juce::jlimit (0, numMips - 1, (int) std::ceil (level));
}

float Wavetable::sample (float framePos, double phase, int mip) const noexcept
{
    framePos = juce::jlimit (0.0f, (float) (numFrames - 1), framePos);
    const int f0 = (int) framePos;
    const int f1 = juce::jmin (f0 + 1, numFrames - 1);
    const float ff = framePos - (float) f0;

    const double pos = phase * frameSize;
    const int i0 = juce::jlimit (0, frameSize - 1, (int) pos);
    const float frac = (float) (pos - i0);

    const float* a = mipPtr (f0, mip);
    const float* b = mipPtr (f1, mip);
    const float sa = a[i0] + (a[i0 + 1] - a[i0]) * frac;
    const float sb = b[i0] + (b[i0 + 1] - b[i0]) * frac;
    return sa + (sb - sa) * ff;
}

std::vector<std::vector<float>> Wavetable::bake (float drive01, float tone01) const
{
    std::vector<std::vector<float>> out;
    const float g = 1.0f + drive01 * 8.0f;
    const float cutoffHarmonic = 1.0f + tone01 * tone01 * 511.0f;
    std::vector<std::complex<float>> spec ((size_t) frameSize);

    for (int f = 0; f < numFrames; ++f)
    {
        std::vector<float> fr = raw[(size_t) f];
        const float norm = std::tanh (g);
        for (auto& x : fr) x = std::tanh (g * x) / norm;

        for (int i = 0; i < frameSize; ++i) spec[(size_t) i] = { fr[(size_t) i], 0.0f };
        fft (spec, false);
        for (int h = 1; h < frameSize / 2; ++h)
        {
            const float gain = 1.0f / (1.0f + std::pow ((float) h / cutoffHarmonic, 4.0f));
            spec[(size_t) h] *= gain;
            spec[(size_t) (frameSize - h)] *= gain;
        }
        spec[0] = 0.0f;
        spec[(size_t) frameSize / 2] = 0.0f;
        fft (spec, true);
        for (int i = 0; i < frameSize; ++i) fr[(size_t) i] = spec[(size_t) i].real();

        removeDcAndNormalise (fr);
        out.push_back (std::move (fr));
    }
    return out;
}

void Wavetable::getFrameShape (float framePos, std::vector<float>& out, int n) const
{
    out.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        out[(size_t) i] = sample (framePos, (double) i / (double) n, 3);
}

bool Wavetable::writeWav (const juce::File& file, const std::vector<std::vector<float>>& frames)
{
    juce::MemoryOutputStream data;
    for (const auto& f : frames)
    {
        auto fr = (int) f.size() == frameSize ? f : resampleCycle (f.data(), (int) f.size(), frameSize);
        for (auto x : fr)
            data.writeFloat (juce::jlimit (-1.0f, 1.0f, x));
    }

    juce::String clmText = "<!>2048 00000000 wavetable Spark";
    juce::MemoryBlock clm (clmText.toRawUTF8(), clmText.getNumBytesAsUTF8());
    if (clm.getSize() % 2 != 0) clm.append ("\0", 1);

    const int sampleRate = 44100;
    juce::MemoryOutputStream wav;
    const auto dataSize = (juce::uint32) data.getDataSize();
    const auto riffSize = (juce::uint32) (4 + (8 + 16) + (8 + clm.getSize()) + (8 + dataSize));

    wav.write ("RIFF", 4);  wav.writeInt ((int) riffSize);  wav.write ("WAVE", 4);
    wav.write ("fmt ", 4);  wav.writeInt (16);
    wav.writeShort (3);            // IEEE float
    wav.writeShort (1);            // mono
    wav.writeInt (sampleRate);
    wav.writeInt (sampleRate * 4); // byte rate
    wav.writeShort (4);            // block align
    wav.writeShort (32);           // bits
    wav.write ("clm ", 4);  wav.writeInt ((int) clm.getSize());  wav.write (clm.getData(), clm.getSize());
    wav.write ("data", 4);  wav.writeInt ((int) dataSize);  wav.write (data.getData(), dataSize);

    file.getParentDirectory().createDirectory();
    return file.replaceWithData (wav.getData(), wav.getDataSize());
}

int Wavetable::readClmFrameSize (const juce::File& file)
{
    juce::FileInputStream in (file);
    if (! in.openedOk())
        return 0;

    char tag[5] = {};
    if (in.read (tag, 4) != 4 || juce::String (tag) != "RIFF")
        return 0;
    in.readInt();
    if (in.read (tag, 4) != 4 || juce::String (tag) != "WAVE")
        return 0;

    while (! in.isExhausted())
    {
        if (in.read (tag, 4) != 4)
            break;
        const auto size = (juce::uint32) in.readInt();
        const auto next = in.getPosition() + size + (size & 1);
        if (juce::String (tag) == "clm " && size < 1024)
        {
            juce::MemoryBlock mb;
            in.readIntoMemoryBlock (mb, (ssize_t) size);
            const auto text = mb.toString();
            if (text.startsWith ("<!>"))
            {
                const int n = text.substring (3).getIntValue();
                return n >= 16 && n <= 65536 ? n : 0;
            }
            return 0;
        }
        if (! in.setPosition (next))
            break;
    }
    return 0;
}
} // namespace spark

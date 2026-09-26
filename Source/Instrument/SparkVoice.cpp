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
        adsr.setSampleRate (newRate);
}

void SparkVoice::updateEnvelope()
{
    auto& p = processor.params;
    juce::ADSR::Parameters e;
    e.attack = fmt::envSeconds (p.attack->load());
    e.decay = fmt::envSeconds (p.decay->load());
    e.sustain = p.sustain->load();
    e.release = fmt::envSeconds (p.release->load());
    adsr.setParameters (e);
}

void SparkVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel)
{
    source = processor.getSource();
    note = midiNote;
    velocityGain = 0.25f + 0.75f * velocity;
    pitchWheelMoved (pitchWheel);

    for (auto& g : grains) g.active = false;
    samplesToNextGrain = 0.0;
    for (auto& ph : phases) ph = random.nextDouble();
    lfoPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;

    updateEnvelope();
    adsr.reset();
    adsr.noteOn();
}

void SparkVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
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

void SparkVoice::renderTable (float* left, float* right, int n, const Wavetable& table, double baseHz, float morph, float motion)
{
    const double sr = getSampleRate();
    const float detuneCents = (0.15f + motion) * 9.0f;
    const double ratios[3] = { 1.0, std::pow (2.0, detuneCents / 1200.0), std::pow (2.0, -detuneCents / 1200.0) };
    const float lastFrame = (float) (table.getNumFrames() - 1);
    const float lfoInc = (0.1f + motion * 0.9f) * juce::MathConstants<float>::twoPi / (float) sr;

    int mips[3];
    for (int k = 0; k < 3; ++k)
        mips[k] = Wavetable::mipForIncrement (baseHz * ratios[k] / sr);

    for (int i = 0; i < n; ++i)
    {
        const float m = juce::jlimit (0.0f, 1.0f, morph + std::sin (lfoPhase) * motion * 0.35f);
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
        left[i] = (s[0] * 0.55f + s[1] * 0.45f) * 0.6f;
        right[i] = (s[0] * 0.55f + s[2] * 0.45f) * 0.6f;
    }
}

void SparkVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive() || source == nullptr)
        return;

    auto& p = processor.params;
    updateEnvelope();

    const float pitchSt = fmt::semitoneValue (p.facet[InstrumentProcessor::pitch]->load());
    const float position = p.facet[InstrumentProcessor::position]->load();
    const float grainSec = fmt::grainSeconds (p.facet[InstrumentProcessor::grain]->load());
    const float morph = p.facet[InstrumentProcessor::morph]->load();
    const float motion = p.facet[InstrumentProcessor::motion]->load();
    const bool tableMode = juce::roundToInt (p.mode->load()) == InstrumentProcessor::tableMode;

    const float semis = (float) (note - rootNote) + pitchSt + bendSemitones;
    const double ratio = std::pow (2.0, semis / 12.0) * (source->sampleRate / getSampleRate());
    const double baseHz = 440.0 * std::pow (2.0, ((double) note - 69.0 + pitchSt + bendSemitones) / 12.0);

    while (numSamples > 0)
    {
        const int n = juce::jmin (numSamples, chunkSize);
        float* l = scratch.getWritePointer (0);
        float* r = scratch.getWritePointer (1);

        if (tableMode && source->table != nullptr)
        {
            renderTable (l, r, n, *source->table, baseHz, morph, motion);
        }
        else
        {
            // Motion also scans the grain position slowly through the sound
            const float scan = std::sin (lfoPhase) * motion * 0.15f;
            lfoPhase += (0.15f + motion * 1.5f) * juce::MathConstants<float>::twoPi * (float) n / (float) getSampleRate();
            if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;
            renderGrains (l, r, n, *source, ratio, position, grainSec, motion, scan);
        }

        for (int i = 0; i < n; ++i)
        {
            const float env = adsr.getNextSample() * velocityGain;
            l[i] *= env;
            r[i] *= env;
        }

        out.addFrom (0, startSample, l, n);
        if (out.getNumChannels() > 1)
            out.addFrom (1, startSample, r, n);

        startSample += n;
        numSamples -= n;

        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
} // namespace spark

#include "FxProcessor.h"
#include "FxEditor.h"
#include "Common/Presets.h"

namespace spark
{
namespace
{
    std::vector<FacetSpec> fxFacets()
    {
        return {
            { "size",    "SIZE",    0.4f,  fmt::grainMs,   "Length of each grain" },
            { "density", "DENSITY", 0.55f, fmt::percent,   "How many grains play at once" },
            { "spray",   "SPRAY",   0.3f,  fmt::percent,   "How far back in time grains are picked, plus stereo scatter" },
            { "pitch",   "PITCH",   0.5f,  fmt::semitones, "Grain pitch, in semitones" },
            { "stutter", "STUTTER", 0.25f, fmt::percent,   "Chance of tempo-synced repeats" },
            { "tone",    "TONE",    0.72f, fmt::cutoff,    "Low-pass filter on the Spark signal" },
            { "space",   "SPACE",   0.48f, fmt::percent,   "Reverb size and amount" },
            { "mix",     "MIX",     0.58f, fmt::percent,   "Dry / wet balance" },
        };
    }

    void addFxParameters (SparkProcessorBase::Layout& layout)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "level", 1 }, "Level",
                                                                 juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                                 juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    constexpr double historySeconds = 4.0;
}

FxProcessor::FxProcessor()
    : SparkProcessorBase (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          fxFacets(), addFxParameters, makeFxPresets(), "fx")
{
    for (int i = 0; i < numFacets; ++i)
        facet[i] = apvts.getRawParameterValue (getFacets()[(size_t) i].id);
    freezeParam = apvts.getRawParameterValue ("freeze");
    bypassParam = apvts.getRawParameterValue ("bypass");
    levelParam = apvts.getRawParameterValue ("level");
}

bool FxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void FxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    historyLength = (int) (sampleRate * historySeconds) + 1;
    history.setSize (2, historyLength);
    history.clear();
    writePos = 0;

    slice.setSize (2, (int) (sampleRate * 2.0));
    slice.clear();
    haveSlice = repeating = false;
    sampleInStep = 0.0;

    wet.setSize (2, samplesPerBlock);
    for (auto& g : grains) g.active = false;
    samplesToNextGrain = 0.0;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance (0.8f);
    reverb.prepare (spec);

    mixSmooth.reset (sampleRate, 0.05);
    levelSmooth.reset (sampleRate, 0.05);
    cutoffSmooth.reset (sampleRate, 0.03);
    mixSmooth.setCurrentAndTargetValue (facet[mix]->load());
    levelSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (levelParam->load()));
    cutoffSmooth.setCurrentAndTargetValue (fmt::cutoffHz (facet[tone]->load()));
}

void FxProcessor::spawnGrain (float grainSec, float sprayAmt, float semis)
{
    auto it = std::find_if (grains.begin(), grains.end(), [] (const Grain& g) { return ! g.active; });
    if (it == grains.end())
        return;

    auto& g = *it;
    const float jitterSemis = (random.nextFloat() - 0.5f) * sprayAmt * 0.6f;
    g.rate = std::pow (2.0, (semis + jitterSemis) / 12.0);
    g.length = juce::jmax (64, (int) (grainSec * sr));
    g.age = 0;

    // Read far enough behind the write head that a fast (pitched-up) grain never overtakes it.
    double delay = 0.012 * sr + sprayAmt * random.nextDouble() * 1.2 * sr
                 + (double) g.length * juce::jmax (0.0, g.rate - 1.0) + 64.0;
    delay = juce::jmin (delay, (double) historyLength - (double) g.length * g.rate - 16.0);
    delay = juce::jmax (delay, 16.0);

    g.pos = (double) writePos - delay;
    while (g.pos < 0) g.pos += historyLength;

    const float pan = (random.nextFloat() * 2.0f - 1.0f) * (0.15f + sprayAmt * 0.75f);
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g.gainL = std::cos (angle);
    g.gainR = std::sin (angle);
    g.active = true;
}

void FxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0)
        return;
    if (wet.getNumSamples() < n)
        wet.setSize (2, n, false, false, true);

    // ---- tempo
    double stepLength = 0.0;
    bool hostPlaying = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm.store (juce::jlimit (30.0, 300.0, *b));
            if (auto p = pos->getPpqPosition()) { ppq = *p; hostPlaying = pos->getIsPlaying(); }
        }
    }
    stepLength = 60.0 / bpm.load() / 4.0 * sr; // a sixteenth note
    stepLength = juce::jmin (stepLength, (double) slice.getNumSamples());
    if (hostPlaying)
        sampleInStep = (ppq * 4.0 - std::floor (ppq * 4.0)) * stepLength;

    // ---- parameters
    const float grainSec = fmt::grainSeconds (facet[size]->load());
    const float dens = facet[density]->load();
    const float sprayAmt = facet[spray]->load();
    const float semis = fmt::semitoneValue (facet[pitch]->load());
    const float stutterAmt = facet[stutter]->load();
    const bool frozen = freezeParam->load() > 0.5f;
    const float grainsPerSecond = 2.0f + dens * dens * 60.0f;
    const float overlap = juce::jmax (1.0f, grainsPerSecond * grainSec);
    const float wetGain = 1.1f / std::sqrt (overlap);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numCh > 1 ? 1 : 0);
    float* hL = history.getWritePointer (0);
    float* hR = history.getWritePointer (1);
    float* wL = wet.getWritePointer (0);
    float* wR = wet.getWritePointer (1);
    float* sL = slice.getWritePointer (0);
    float* sR = slice.getWritePointer (1);

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float l = inL[i], r = inR[i];
        peakIn = juce::jmax (peakIn, std::abs (l), std::abs (r));

        if (! frozen)
        {
            hL[writePos] = l;
            hR[writePos] = r;
            if (++writePos >= historyLength) writePos = 0;
        }

        if ((samplesToNextGrain -= 1.0) <= 0.0)
        {
            spawnGrain (grainSec, sprayAmt, semis);
            samplesToNextGrain += sr / grainsPerSecond * (1.0 + (random.nextDouble() - 0.5) * sprayAmt);
        }

        float gl = 0.0f, gr = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float x = (float) g.age / (float) g.length;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * x);
            const int i0 = (int) g.pos;
            const int i1 = i0 + 1 >= historyLength ? 0 : i0 + 1;
            const float frac = (float) (g.pos - i0);
            gl += (hL[i0] + (hL[i1] - hL[i0]) * frac) * w * g.gainL;
            gr += (hR[i0] + (hR[i1] - hR[i0]) * frac) * w * g.gainR;
            g.pos += g.rate;
            if (g.pos >= historyLength) g.pos -= historyLength;
            if (++g.age >= g.length) g.active = false;
        }
        gl *= wetGain;
        gr *= wetGain;

        // ---- stutter: at every sixteenth, maybe repeat the last one (or half of it)
        const int step = juce::jmax (32, (int) stepLength);
        if (sampleInStep >= step || sampleInStep < 0.0)
        {
            sampleInStep = 0.0;
            repeating = haveSlice && random.nextFloat() < stutterAmt * 0.85f;
            repeatLength = (repeating && random.nextFloat() < stutterAmt * 0.5f) ? step / 2 : step;
            repeatIndex = 0;
            if (! repeating) sliceLength = step;
        }
        const int k = (int) sampleInStep;
        if (repeating)
        {
            const int idx = repeatIndex % juce::jmax (1, repeatLength);
            const float fade = juce::jmin (1.0f, (float) idx / 32.0f, (float) (repeatLength - idx) / 32.0f);
            gl = sL[idx] * fade;
            gr = sR[idx] * fade;
            ++repeatIndex;
        }
        else if (k < slice.getNumSamples())
        {
            sL[k] = gl;
            sR[k] = gr;
            if (k == sliceLength - 1) haveSlice = true;
        }
        sampleInStep += 1.0;

        wL[i] = gl;
        wR[i] = gr;
    }

    // ---- tone and space on the Spark signal only
    cutoffSmooth.setTargetValue (fmt::cutoffHz (facet[tone]->load()));
    juce::dsp::AudioBlock<float> wetBlock (wet.getArrayOfWritePointers(), 2, (size_t) n);
    for (int i = 0; i < n; i += 32)
    {
        const int len = juce::jmin (32, n - i);
        filter.setCutoffFrequency (juce::jmin (cutoffSmooth.getNextValue(), (float) sr * 0.45f));
        cutoffSmooth.skip (len - 1);
        auto sub = wetBlock.getSubBlock ((size_t) i, (size_t) len);
        filter.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }
    const float spaceAmt = facet[space]->load();
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.68f * spaceAmt;
    rp.damping = 0.4f;
    rp.wetLevel = spaceAmt * 0.6f;
    rp.dryLevel = 1.0f - spaceAmt * 0.4f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.process (juce::dsp::ProcessContextReplacing<float> (wetBlock));

    // ---- mix
    const bool bypassed = bypassParam->load() > 0.5f;
    mixSmooth.setTargetValue (bypassed ? 0.0f : facet[mix]->load());
    levelSmooth.setTargetValue (bypassed ? 1.0f : juce::Decibels::decibelsToGain (levelParam->load()));

    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSmooth.getNextValue();
        const float lvl = levelSmooth.getNextValue();
        const float dryG = std::cos (m * juce::MathConstants<float>::halfPi);
        const float wetG = std::sin (m * juce::MathConstants<float>::halfPi);

        const float dl = buffer.getSample (0, i);
        const float dr = numCh > 1 ? buffer.getSample (1, i) : dl;
        float ol = (dl * dryG + wL[i] * wetG) * lvl;
        float orr = (dr * dryG + wR[i] * wetG) * lvl;
        if (numCh == 1) ol = 0.5f * (ol + orr);

        buffer.setSample (0, i, ol);
        if (numCh > 1) buffer.setSample (1, i, orr);
        peakOut = juce::jmax (peakOut, std::abs (ol), std::abs (orr));

        if (++scopeCounter >= 8)
        {
            scopeCounter = 0;
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scopeIn[(size_t) w].store (0.5f * (dl + dr), std::memory_order_relaxed);
            scopeOut[(size_t) w].store (0.5f * (ol + (numCh > 1 ? orr : ol)), std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % scopeSize, std::memory_order_release);
        }
    }

    inPeak.store (juce::jmax (peakIn, inPeak.load() * 0.8f));
    outPeak.store (juce::jmax (peakOut, outPeak.load() * 0.8f));
}

void FxProcessor::getScope (std::vector<float>& in, std::vector<float>& out) const
{
    in.resize (scopeSize);
    out.resize (scopeSize);
    const int w = scopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < scopeSize; ++i)
    {
        const int idx = (w + i) % scopeSize;
        in[(size_t) i] = scopeIn[(size_t) idx].load (std::memory_order_relaxed);
        out[(size_t) i] = scopeOut[(size_t) idx].load (std::memory_order_relaxed);
    }
}

void FxProcessor::getCoreShape (std::vector<float>& out, int n)
{
    std::vector<float> in, o;
    getScope (in, o);
    out.assign ((size_t) n, 0.0f);
    float peak = 1.0e-4f;
    for (int i = 0; i < n; ++i)
    {
        out[(size_t) i] = o[(size_t) (scopeSize - n + i)];
        peak = juce::jmax (peak, std::abs (out[(size_t) i]));
    }
    const float norm = peak > 0.01f ? 1.0f / peak : 0.0f; // silence draws a calm circle
    for (auto& x : out) x *= norm;
}

juce::File FxProcessor::getCaptureFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Spark").getChildFile ("Captures");
}

juce::File FxProcessor::captureToFile (juce::String& error)
{
    if (history.getNumSamples() < 2)
    {
        error = "Play some audio through Spark FX first.";
        return {};
    }

    // Unroll the circular history so the newest audio is at the end.
    const int len = historyLength;
    const int start = writePos;
    juce::AudioBuffer<float> audio (2, len);
    for (int ch = 0; ch < 2; ++ch)
    {
        const int firstPart = len - start;
        audio.copyFrom (ch, 0, history, ch, start, firstPart);
        if (start > 0)
            audio.copyFrom (ch, firstPart, history, ch, 0, start);
    }

    // Trim leading silence
    int first = 0;
    while (first < len && std::abs (audio.getSample (0, first)) < 1.0e-4f && std::abs (audio.getSample (1, first)) < 1.0e-4f)
        ++first;
    if (len - first < (int) (sr * 0.05))
    {
        error = "Nothing to capture yet. Play some audio through Spark FX first.";
        return {};
    }

    auto folder = getCaptureFolder();
    folder.createDirectory();
    auto file = folder.getNonexistentChildFile ("Spark Capture " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), ".wav", false);

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
    {
        error = "Couldn't write to " + folder.getFullPathName();
        return {};
    }
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr)
    {
        error = "Couldn't create the WAV file.";
        return {};
    }
    writer->writeFromAudioSampleBuffer (audio, first, len - first);
    return file;
}

juce::AudioProcessorEditor* FxProcessor::createEditor()
{
    return new FxEditor (*this);
}
} // namespace spark

#if ! SPARK_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new spark::FxProcessor();
}
#endif

#include "FxRack.h"
#include "Common/SparkProcessorBase.h"

namespace spark
{
namespace
{
    const juce::StringArray distTypes { "Soft", "Hard", "Fold", "Crush" };
    const juce::StringArray stutterRates { "1/32", "1/16", "1/8", "1/4" };
    const double stutterBeats[] = { 0.125, 0.25, 0.5, 1.0 };
    const juce::StringArray delayTimes { "1/16", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
    const double delayBeats[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    constexpr double historySeconds = 4.0;
    constexpr double maxDelaySeconds = 3.0;
}

const std::vector<FxRack::ModuleInfo>& FxRack::modules()
{
    static const std::vector<ModuleInfo> m {
        { "dist",    "DISTORTION",  "fxDistOn",    { "fxDistType", "fxDistDrive", "fxDistMix" },                                  { "TYPE", "DRIVE", "MIX" },              "Soft, hard, fold or crush" },
        { "eq",      "EQ",          "fxEqOn",      { "fxEqLow", "fxEqMid", "fxEqHigh" },                                          { "LOW", "MID", "HIGH" },                "Shape the tone" },
        { "chorus",  "CHORUS",      "fxChorusOn",  { "fxChorusRate", "fxChorusDepth", "fxChorusMix" },                            { "RATE", "DEPTH", "MIX" },              "Width and shimmer" },
        { "grain",   "GRAINS",      "fxGrainOn",   { "fxGrainSize", "fxGrainDensity", "fxGrainSpray", "fxGrainPitch", "fxGrainMix" }, { "SIZE", "DENSITY", "SPRAY", "PITCH", "MIX" }, "A cloud of pitched grains" },
        { "stutter", "STUTTER",     "fxStutterOn", { "fxStutterRate", "fxStutterAmount" },                                        { "RATE", "AMOUNT" },                    "Tempo-synced repeats" },
        { "delay",   "DELAY",       "fxDelayOn",   { "fxDelayTime", "fxDelayFeedback", "fxDelayMix" },                            { "TIME", "FEEDBACK", "MIX" },           "Ping-pong, synced to tempo" },
        { "reverb",  "REVERB",      "fxReverbOn",  { "fxReverbSize", "fxReverbDamp", "fxReverbWidth", "space" },                 { "SIZE", "DAMP", "WIDTH", "SPACE" },    "Amount is the Space facet" },
    };
    return m;
}

void FxRack::addParameters (Layout& layout)
{
    using namespace juce;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::percent (v); });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    NormalisableRange<float> unit (0.0f, 1.0f), gain (-12.0f, 12.0f);
    // Two-step choices rather than AudioParameterBool: a bool keeps whatever fraction a host sends,
    // which then doesn't restore cleanly from saved state. A choice always snaps to Off or On.
    auto onSwitch = [&] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, StringArray { "Off", "On" }, def ? 1 : 0)); };
    auto knob = [&] (const char* id, const char* name, float def, AudioParameterFloatAttributes attr) { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, unit, def, attr)); };

    onSwitch ("fxDistOn", "Distortion On", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxDistType", 1 }, "Distortion Type", distTypes, 0));
    knob ("fxDistDrive", "Distortion Drive", 0.4f, pct);
    knob ("fxDistMix", "Distortion Mix", 1.0f, pct);

    onSwitch ("fxEqOn", "EQ On", false);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fxEqLow", 1 }, "EQ Low", gain, 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fxEqMid", 1 }, "EQ Mid", gain, 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fxEqHigh", 1 }, "EQ High", gain, 0.0f, db));

    onSwitch ("fxChorusOn", "Chorus On", false);
    knob ("fxChorusRate", "Chorus Rate", 0.25f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (0.1f + v * 3.9f, 2) + " Hz"; }));
    knob ("fxChorusDepth", "Chorus Depth", 0.45f, pct);
    knob ("fxChorusMix", "Chorus Mix", 0.45f, pct);

    onSwitch ("fxGrainOn", "Grain Cloud On", false);
    knob ("fxGrainSize", "Grain Size", 0.4f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::grainMs (v); }));
    knob ("fxGrainDensity", "Grain Density", 0.55f, pct);
    knob ("fxGrainSpray", "Grain Spray", 0.3f, pct);
    knob ("fxGrainPitch", "Grain Pitch", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::semitones (v); }));
    knob ("fxGrainMix", "Grain Mix", 0.5f, pct);

    onSwitch ("fxStutterOn", "Stutter On", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxStutterRate", 1 }, "Stutter Rate", stutterRates, 1));
    knob ("fxStutterAmount", "Stutter Amount", 0.35f, pct);

    onSwitch ("fxDelayOn", "Delay On", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxDelayTime", 1 }, "Delay Time", delayTimes, 2));
    knob ("fxDelayFeedback", "Delay Feedback", 0.4f, pct);
    knob ("fxDelayMix", "Delay Mix", 0.25f, pct);

    onSwitch ("fxReverbOn", "Reverb On", true);
    knob ("fxReverbSize", "Reverb Size", 0.65f, pct);
    knob ("fxReverbDamp", "Reverb Damping", 0.45f, pct);
    knob ("fxReverbWidth", "Reverb Width", 1.0f, pct);
}

void FxRack::attach (juce::AudioProcessorValueTreeState& s)
{
    for (const auto& m : modules())
    {
        raw[m.onParam] = s.getRawParameterValue (m.onParam);
        for (const auto& id : m.params)
            raw[id] = s.getRawParameterValue (id);
    }
    for (auto& [id, ptr] : raw)
        jassert (ptr != nullptr);
}

float FxRack::p (const char* id) const
{
    auto it = raw.find (id);
    return it != raw.end() && it->second != nullptr ? it->second->load() : 0.0f;
}

void FxRack::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    scratch.setSize (2, juce::jmax (1, maxBlockSize));

    for (auto* g : { &gDist, &gEq, &gChorus, &gGrain, &gStutter, &gDelay })
        g->reset (sampleRate, 0.03);

    juce::dsp::ProcessSpec mono { sampleRate, (juce::uint32) maxBlockSize, 1 };
    for (int c = 0; c < 2; ++c)
        for (auto* f : { &eqLow[c], &eqMid[c], &eqHigh[c] })
            f->prepare (mono);
    lastLow = lastMid = lastHigh = 99;

    juce::dsp::ProcessSpec stereo { sampleRate, (juce::uint32) maxBlockSize, 2 };
    chorus.prepare (stereo);
    reverb.prepare (stereo);

    historyLength = (int) (sampleRate * historySeconds) + 1;
    history.setSize (2, historyLength);
    slice.setSize (2, (int) (sampleRate * 2.0));
    delayLine.setSize (2, (int) (sampleRate * maxDelaySeconds) + 4);
    delaySamples.reset (sampleRate, 0.12);
    reset();
}

void FxRack::reset()
{
    history.clear();
    slice.clear();
    delayLine.clear();
    writePos = delayWrite = 0;
    for (auto& g : grains) g.active = false;
    samplesToNextGrain = 0;
    repeating = haveSlice = false;
    sampleInStep = 0;
    dampL = dampR = 0;
    crushHoldL = crushHoldR = 0;
    crushCounter = 0;
    chorus.reset();
    reverb.reset();
    for (int c = 0; c < 2; ++c)
        for (auto* f : { &eqLow[c], &eqMid[c], &eqHigh[c] })
            f->reset();
    for (auto* g : { &gDist, &gEq, &gChorus, &gGrain, &gStutter, &gDelay })
        g->setCurrentAndTargetValue (0.0f);
    delaySamples.setCurrentAndTargetValue ((float) (0.375 * sr));
}

template <typename Fn>
void FxRack::blend (juce::AudioBuffer<float>& io, int n, juce::SmoothedValue<float>& gain, bool enabled, Fn&& module)
{
    gain.setTargetValue (enabled ? 1.0f : 0.0f);
    if (! enabled && ! gain.isSmoothing() && gain.getCurrentValue() <= 0.0f)
        return;

    for (int c = 0; c < 2; ++c)
        scratch.copyFrom (c, 0, io, c, 0, n);
    module (scratch.getWritePointer (0), scratch.getWritePointer (1), n);

    float* l = io.getWritePointer (0);
    float* r = io.getWritePointer (1);
    const float* wl = scratch.getReadPointer (0);
    const float* wr = scratch.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float g = gain.getNextValue();
        l[i] += (wl[i] - l[i]) * g;
        r[i] += (wr[i] - r[i]) * g;
    }
}

void FxRack::process (juce::AudioBuffer<float>& buffer, double bpm, double ppq, bool hostPlaying, float spaceFacet)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;
    if (scratch.getNumSamples() < n)
        scratch.setSize (2, n, false, false, true);

    blend (buffer, n, gDist, on ("fxDistOn"), [&] (float* l, float* r, int k) { distortion (l, r, k); });
    blend (buffer, n, gEq, on ("fxEqOn"), [&] (float* l, float* r, int k)
    {
        juce::AudioBuffer<float> view (std::array<float*, 2> { l, r }.data(), 2, k);
        equaliser (view, k);
    });
    blend (buffer, n, gChorus, on ("fxChorusOn"), [&] (float* l, float* r, int k)
    {
        juce::AudioBuffer<float> view (std::array<float*, 2> { l, r }.data(), 2, k);
        chorusFx (view, k);
    });
    blend (buffer, n, gGrain, on ("fxGrainOn"), [&] (float* l, float* r, int k) { grainCloud (l, r, k); });
    blend (buffer, n, gStutter, on ("fxStutterOn"), [&] (float* l, float* r, int k) { stutter (l, r, k, bpm, ppq, hostPlaying); });
    blend (buffer, n, gDelay, on ("fxDelayOn"), [&] (float* l, float* r, int k) { delayFx (l, r, k, bpm); });
    reverbFx (buffer, n, spaceFacet);
}

// ---------------------------------------------------------------- distortion
void FxRack::distortion (float* l, float* r, int n)
{
    const int type = juce::roundToInt (p ("fxDistType"));
    const float drive = p ("fxDistDrive");
    const float mix = p ("fxDistMix");
    const float g = 1.0f + drive * 20.0f;
    const float makeup = 1.0f / std::sqrt (g);

    auto shape = [&] (float x)
    {
        switch (type)
        {
            case 0:  return std::tanh (g * x) * makeup;
            case 1:  return juce::jlimit (-1.0f, 1.0f, g * x) * makeup;
            case 2:  return std::sin (x * (1.0f + drive * 9.0f) * juce::MathConstants<float>::halfPi) * 0.75f;
            default: return x;
        }
    };

    if (type == 3)
    {
        // Crush: fewer bits and a lower sample rate as Drive rises
        const float levels = std::pow (2.0f, 14.0f - drive * 11.0f);
        const int hold = 1 + (int) (drive * drive * 24.0f);
        for (int i = 0; i < n; ++i)
        {
            if (crushCounter++ % hold == 0)
            {
                crushHoldL = std::round (l[i] * levels) / levels;
                crushHoldR = std::round (r[i] * levels) / levels;
            }
            l[i] += (crushHoldL - l[i]) * mix;
            r[i] += (crushHoldR - r[i]) * mix;
        }
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        l[i] += (shape (l[i]) - l[i]) * mix;
        r[i] += (shape (r[i]) - r[i]) * mix;
    }
}

// ---------------------------------------------------------------- EQ
void FxRack::equaliser (juce::AudioBuffer<float>& buf, int n)
{
    const float low = p ("fxEqLow"), mid = p ("fxEqMid"), high = p ("fxEqHigh");
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    if (low != lastLow)
    {
        auto c = Coeffs::makeLowShelf (sr, 150.0, 0.7, juce::Decibels::decibelsToGain (low));
        eqLow[0].coefficients = c; eqLow[1].coefficients = c; lastLow = low;
    }
    if (mid != lastMid)
    {
        auto c = Coeffs::makePeakFilter (sr, 1000.0, 0.8, juce::Decibels::decibelsToGain (mid));
        eqMid[0].coefficients = c; eqMid[1].coefficients = c; lastMid = mid;
    }
    if (high != lastHigh)
    {
        auto c = Coeffs::makeHighShelf (sr, juce::jmin (6000.0, sr * 0.4), 0.7, juce::Decibels::decibelsToGain (high));
        eqHigh[0].coefficients = c; eqHigh[1].coefficients = c; lastHigh = high;
    }
    for (int c = 0; c < 2; ++c)
    {
        float* d = buf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            d[i] = eqHigh[c].processSample (eqMid[c].processSample (eqLow[c].processSample (d[i])));
    }
}

// ---------------------------------------------------------------- chorus
void FxRack::chorusFx (juce::AudioBuffer<float>& buf, int n)
{
    chorus.setRate (0.1f + p ("fxChorusRate") * 3.9f);
    chorus.setDepth (juce::jlimit (0.0f, 1.0f, p ("fxChorusDepth")));
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.15f);
    chorus.setMix (juce::jlimit (0.0f, 1.0f, p ("fxChorusMix")));
    juce::dsp::AudioBlock<float> block (buf.getArrayOfWritePointers(), 2, (size_t) n);
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
}

// ---------------------------------------------------------------- grain cloud
void FxRack::grainCloud (float* l, float* r, int n)
{
    const float grainSec = fmt::grainSeconds (p ("fxGrainSize"));
    const float dens = p ("fxGrainDensity");
    const float spray = p ("fxGrainSpray");
    const float semis = fmt::semitoneValue (p ("fxGrainPitch"));
    const float mix = p ("fxGrainMix");
    const float gps = 2.0f + dens * dens * 60.0f;
    const float wetGain = 1.1f / std::sqrt (juce::jmax (1.0f, gps * grainSec));
    const float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
    const float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);

    float* hL = history.getWritePointer (0);
    float* hR = history.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        hL[writePos] = l[i];
        hR[writePos] = r[i];
        if (++writePos >= historyLength) writePos = 0;

        if ((samplesToNextGrain -= 1.0) <= 0.0)
        {
            auto it = std::find_if (grains.begin(), grains.end(), [] (const Grain& g) { return ! g.active; });
            if (it != grains.end())
            {
                auto& g = *it;
                g.rate = std::pow (2.0, (semis + (random.nextFloat() - 0.5f) * spray * 0.6f) / 12.0);
                g.length = juce::jmax (64, (int) (grainSec * sr));
                g.age = 0;
                double delay = 0.012 * sr + spray * random.nextDouble() * 1.2 * sr + g.length * juce::jmax (0.0, g.rate - 1.0) + 64.0;
                delay = juce::jlimit (16.0, (double) historyLength - g.length * g.rate - 16.0, delay);
                g.pos = writePos - delay;
                while (g.pos < 0) g.pos += historyLength;
                const float pan = (random.nextFloat() * 2.0f - 1.0f) * (0.15f + spray * 0.75f);
                const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                g.gl = std::cos (angle);
                g.gr = std::sin (angle);
                g.active = true;
            }
            samplesToNextGrain += sr / gps * (1.0 + (random.nextDouble() - 0.5) * spray);
        }

        float gl = 0, gr = 0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) g.age / (float) g.length);
            const int i0 = (int) g.pos;
            const int i1 = i0 + 1 >= historyLength ? 0 : i0 + 1;
            const float fr = (float) (g.pos - i0);
            gl += (hL[i0] + (hL[i1] - hL[i0]) * fr) * w * g.gl;
            gr += (hR[i0] + (hR[i1] - hR[i0]) * fr) * w * g.gr;
            g.pos += g.rate;
            if (g.pos >= historyLength) g.pos -= historyLength;
            if (++g.age >= g.length) g.active = false;
        }
        l[i] = l[i] * dryG + gl * wetGain * wetG;
        r[i] = r[i] * dryG + gr * wetGain * wetG;
    }
}

// ---------------------------------------------------------------- stutter
void FxRack::stutter (float* l, float* r, int n, double bpm, double ppq, bool hostPlaying)
{
    const double beats = stutterBeats[juce::jlimit (0, 3, juce::roundToInt (p ("fxStutterRate")))];
    const double stepLength = juce::jmin (60.0 / bpm * beats * sr, (double) slice.getNumSamples());
    const float amount = p ("fxStutterAmount");
    const int step = juce::jmax (32, (int) stepLength);
    if (hostPlaying)
        sampleInStep = (ppq / beats - std::floor (ppq / beats)) * stepLength;

    float* sL = slice.getWritePointer (0);
    float* sR = slice.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        if (sampleInStep >= step || sampleInStep < 0.0)
        {
            sampleInStep = 0.0;
            repeating = haveSlice && random.nextFloat() < amount * 0.85f;
            repeatLength = (repeating && random.nextFloat() < amount * 0.5f) ? step / 2 : step;
            repeatIndex = 0;
            if (! repeating) sliceLength = step;
        }
        const int k = (int) sampleInStep;
        if (repeating)
        {
            const int idx = repeatIndex % juce::jmax (1, repeatLength);
            const float fade = juce::jmin (1.0f, (float) idx / 32.0f, (float) (repeatLength - idx) / 32.0f);
            l[i] = sL[idx] * fade;
            r[i] = sR[idx] * fade;
            ++repeatIndex;
        }
        else if (k < slice.getNumSamples())
        {
            sL[k] = l[i];
            sR[k] = r[i];
            if (k == sliceLength - 1) haveSlice = true;
        }
        sampleInStep += 1.0;
    }
}

// ---------------------------------------------------------------- delay (ping-pong, tempo-synced)
void FxRack::delayFx (float* l, float* r, int n, double bpm)
{
    const double beats = delayBeats[juce::jlimit (0, 5, juce::roundToInt (p ("fxDelayTime")))];
    const int len = delayLine.getNumSamples();
    delaySamples.setTargetValue ((float) juce::jlimit (16.0, len - 4.0, 60.0 / bpm * beats * sr));
    const float fb = juce::jlimit (0.0f, 0.95f, p ("fxDelayFeedback") * 0.92f);
    const float mix = p ("fxDelayMix");
    const float damp = 0.35f; // one-pole low-pass in the feedback path
    float* dL = delayLine.getWritePointer (0);
    float* dR = delayLine.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float d = delaySamples.getNextValue();
        double rp = delayWrite - (double) d;
        while (rp < 0) rp += len;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % len;
        const float fr = (float) (rp - i0);
        const float outL = dL[i0] + (dL[i1] - dL[i0]) * fr;
        const float outR = dR[i0] + (dR[i1] - dR[i0]) * fr;
        dampL += (outL - dampL) * damp;
        dampR += (outR - dampR) * damp;

        // ping-pong: the input enters on the left, each repeat crosses to the other side
        dL[delayWrite] = (l[i] + r[i]) * 0.5f + dampR * fb;
        dR[delayWrite] = dampL * fb;
        if (++delayWrite >= len) delayWrite = 0;

        l[i] += outL * mix * 1.4f;
        r[i] += outR * mix * 1.4f;
    }
}

// ---------------------------------------------------------------- reverb
void FxRack::reverbFx (juce::AudioBuffer<float>& buf, int n, float spaceFacet)
{
    const bool enabled = on ("fxReverbOn");
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.68f * p ("fxReverbSize");
    rp.damping = p ("fxReverbDamp");
    rp.width = p ("fxReverbWidth");
    rp.wetLevel = enabled ? spaceFacet * 0.55f : 0.0f;
    rp.dryLevel = enabled ? 1.0f - spaceFacet * 0.35f : 1.0f;
    reverb.setParameters (rp); // juce::dsp::Reverb smooths these itself
    juce::dsp::AudioBlock<float> block (buf.getArrayOfWritePointers(), 2, (size_t) n);
    reverb.process (juce::dsp::ProcessContextReplacing<float> (block));
}

// ---------------------------------------------------------------- chains
const std::vector<FxRack::Chain>& FxRack::chains()
{
    static const std::vector<Chain> c {
        { "Clean",             "Just the Space reverb",                       { } },
        { "Wide Chorus",       "Lush stereo width",                           { { "fxChorusOn", 1 }, { "fxChorusRate", .2f }, { "fxChorusDepth", .6f }, { "fxChorusMix", .5f } } },
        { "Airy Polish",       "Bright, wide and clean",                      { { "fxEqOn", 1 }, { "fxEqLow", -2 }, { "fxEqHigh", 4 }, { "fxChorusOn", 1 }, { "fxChorusDepth", .3f }, { "fxChorusMix", .3f } } },
        { "Warm Saturation",   "Gentle drive with a fuller low end",          { { "fxDistOn", 1 }, { "fxDistType", 0 }, { "fxDistDrive", .35f }, { "fxEqOn", 1 }, { "fxEqLow", 2 }, { "fxEqHigh", -2 } } },
        { "Lo-fi Crush",       "Crunchy, dusty and narrow",                   { { "fxDistOn", 1 }, { "fxDistType", 3 }, { "fxDistDrive", .5f }, { "fxDistMix", .6f }, { "fxEqOn", 1 }, { "fxEqLow", 2 }, { "fxEqHigh", -6 } } },
        { "Fold Madness",      "Wavefolded and echoing",                      { { "fxDistOn", 1 }, { "fxDistType", 2 }, { "fxDistDrive", .7f }, { "fxDistMix", .8f }, { "fxDelayOn", 1 }, { "fxDelayTime", 2 }, { "fxDelayMix", .2f } } },
        { "Radio",             "Thin, mid-heavy and gritty",                  { { "fxEqOn", 1 }, { "fxEqLow", -12 }, { "fxEqMid", 6 }, { "fxEqHigh", -8 }, { "fxDistOn", 1 }, { "fxDistType", 1 }, { "fxDistDrive", .3f } } },
        { "Dub Delay",         "Dark dotted-quarter echoes",                  { { "fxDelayOn", 1 }, { "fxDelayTime", 4 }, { "fxDelayFeedback", .6f }, { "fxDelayMix", .35f }, { "fxEqOn", 1 }, { "fxEqLow", -3 }, { "fxEqHigh", -4 } } },
        { "Ping Pong Eighths", "Bouncing stereo eighths",                     { { "fxDelayOn", 1 }, { "fxDelayTime", 1 }, { "fxDelayFeedback", .45f }, { "fxDelayMix", .3f } } },
        { "Tape Echo Wash",    "Wobbly, warm repeats",                        { { "fxChorusOn", 1 }, { "fxChorusDepth", .3f }, { "fxChorusMix", .35f }, { "fxDelayOn", 1 }, { "fxDelayTime", 2 }, { "fxDelayFeedback", .55f }, { "fxDelayMix", .3f }, { "fxEqOn", 1 }, { "fxEqHigh", -5 } } },
        { "Shimmer Cloud",     "Octave-up grains into a big hall",            { { "fxGrainOn", 1 }, { "fxGrainSize", .7f }, { "fxGrainDensity", .7f }, { "fxGrainSpray", .5f }, { "fxGrainPitch", .75f }, { "fxGrainMix", .45f }, { "fxReverbSize", .9f } } },
        { "Octave Down Cloud", "A grainy shadow an octave below",             { { "fxGrainOn", 1 }, { "fxGrainSize", .5f }, { "fxGrainDensity", .65f }, { "fxGrainSpray", .25f }, { "fxGrainPitch", .25f }, { "fxGrainMix", .4f } } },
        { "Grain Freeze Wash", "Smears everything into a pad",                { { "fxGrainOn", 1 }, { "fxGrainSize", 1.0f }, { "fxGrainDensity", .9f }, { "fxGrainSpray", .7f }, { "fxGrainMix", .6f }, { "fxReverbSize", .9f } } },
        { "Stutter Glitch",    "Chopped, scattered and glitchy",              { { "fxStutterOn", 1 }, { "fxStutterRate", 1 }, { "fxStutterAmount", .6f }, { "fxGrainOn", 1 }, { "fxGrainSize", .1f }, { "fxGrainDensity", .8f }, { "fxGrainSpray", .6f }, { "fxGrainMix", .3f } } },
        { "Roll Machine",      "Fast 32nd-note rolls",                        { { "fxStutterOn", 1 }, { "fxStutterRate", 0 }, { "fxStutterAmount", .7f } } },
        { "Big Room",          "Huge space with a quarter-note echo",         { { "fxReverbSize", .95f }, { "fxReverbDamp", .3f }, { "fxDelayOn", 1 }, { "fxDelayTime", 3 }, { "fxDelayFeedback", .3f }, { "fxDelayMix", .2f } } },
    };
    return c;
}
} // namespace spark

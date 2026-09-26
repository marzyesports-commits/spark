#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace spark
{
// Spark's modulation: 2 LFOs, 4 macros, mod wheel, aftertouch and velocity, routed through
// 8 slots (source -> destination -> amount). Everything is a normal parameter, so it saves,
// automates and loads with presets. Amounts are in units of the destination's full range.
namespace mod
{
    enum Source { none = 0, lfo1, lfo2, macro1, macro2, macro3, macro4, modWheel, aftertouch, velocity, numSources };
    enum Dest { pitch = 0, position, grain, morph, tone, drive, motion, space, resonance, volume, numDests };
    enum Shape { sine = 0, triangle, sawUp, sawDown, square, sampleHold, drift, numShapes };

    constexpr int numSlots = 8;
    constexpr int numLfos = 2;
    constexpr int numMacros = 4;

    const juce::StringArray& sourceNames();     // "None", "LFO 1", ...
    const juce::StringArray& destNames();       // "Pitch", ..., "Resonance", "Volume"
    const juce::StringArray& shapeNames();
    const juce::StringArray& divisionNames();   // tempo-sync divisions
    double divisionBeats (int index);           // length of one LFO cycle in beats
    float rateHz (float normalised);            // 0.02 .. 20 Hz
    bool isBipolar (int source);                // LFOs swing both ways; the rest go 0..1

    // one LFO cycle, phase 0..1 -> -1..1. 'held' and 'next' are the random values for S&H and Drift.
    float shapeValue (int shape, float phase, float held, float next) noexcept;

    juce::String slotParam (int slot, const char* what);   // "mod3Src", "mod3Dst", "mod3Amt"
    juce::String lfoParam (int lfo, const char* what);     // "lfo1Shape", "lfo1Rate", ...
    juce::String macroParam (int macro);                   // "macro1"

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);

    // Raw parameter pointers, read once per block on the audio thread.
    struct Params
    {
        std::atomic<float> *src[numSlots] {}, *dst[numSlots] {}, *amt[numSlots] {};
        std::atomic<float> *shape[numLfos] {}, *rate[numLfos] {}, *sync[numLfos] {}, *division[numLfos] {}, *retrig[numLfos] {};
        std::atomic<float> *macro[numMacros] {};
        void attach (juce::AudioProcessorValueTreeState&);
    };

    struct Slot { int src = none, dst = pitch; float amount = 0.0f; };

    // The routing and LFO settings for one audio block: resolved once, read by every voice.
    struct BlockState
    {
        Slot slots[numSlots];
        int numActive = 0;
        bool usesLfo[numLfos] {};
        bool retrigger[numLfos] {};
        int shape[numLfos] {};
        double lfoPhase[numLfos] {};     // free-running phase at the start of the block
        double lfoInc[numLfos] {};       // phase per sample
        float held[numLfos] {}, next[numLfos] {};   // random values for S&H / Drift (free-running)
        float macro[numMacros] {};
        float modWheel = 0.0f, aftertouch = 0.0f;

        bool active() const noexcept { return numActive > 0; }
        // sum of every slot into each destination, given the source values
        void route (const float (&sourceValues)[numSources], float (&out)[numDests]) const noexcept
        {
            for (auto& o : out) o = 0.0f;
            for (int i = 0; i < numActive; ++i)
                out[slots[i].dst] += slots[i].amount * sourceValues[slots[i].src];
        }
    };
} // namespace mod
} // namespace spark

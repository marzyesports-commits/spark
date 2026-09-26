#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <map>

namespace spark
{
// Spark's built-in effects: Distortion -> EQ -> Chorus -> Grain Cloud -> Stutter -> Delay -> Reverb.
// Each module has an on switch that crossfades, so toggling never clicks.
class FxRack
{
public:
    struct ModuleInfo
    {
        juce::String id;            // "dist", "eq", ...
        juce::String name;          // shown on the card
        juce::String onParam;       // bool parameter id
        juce::StringArray params;   // controls shown on the card, in order
        juce::StringArray labels;   // short knob labels
        juce::String blurb;         // one line under the title
    };
    static const std::vector<ModuleInfo>& modules();

    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    static void addParameters (Layout&);
    void attach (juce::AudioProcessorValueTreeState&);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    // spaceFacet: the Space facet sets how much reverb you hear.
    void process (juce::AudioBuffer<float>&, double bpm, double ppq, bool hostPlaying, float spaceFacet);

    struct Chain
    {
        juce::String name;
        juce::String hint;
        std::map<juce::String, float> values; // real (not normalised) values; unlisted params go to defaults
    };
    static const std::vector<Chain>& chains();

private:
    float p (const char* id) const;
    bool on (const char* id) const { return p (id) > 0.5f; }

    void distortion (float* l, float* r, int n);
    void equaliser (juce::AudioBuffer<float>&, int n);
    void chorusFx (juce::AudioBuffer<float>&, int n);
    void grainCloud (float* l, float* r, int n);
    void stutter (float* l, float* r, int n, double bpm, double ppq, bool hostPlaying);
    void delayFx (float* l, float* r, int n, double bpm);
    void reverbFx (juce::AudioBuffer<float>&, int n, float spaceFacet);

    // Runs 'module' on a copy and crossfades it in by the module's on/off gain.
    template <typename Fn> void blend (juce::AudioBuffer<float>& io, int n, juce::SmoothedValue<float>& gain, bool enabled, Fn&& module);

    std::map<juce::String, std::atomic<float>*> raw;
    double sr = 44100.0;
    juce::AudioBuffer<float> scratch;
    juce::Random random;

    // on/off crossfades
    juce::SmoothedValue<float> gDist, gEq, gChorus, gGrain, gStutter, gDelay;

    // distortion
    float crushHoldL = 0, crushHoldR = 0;
    int crushCounter = 0;

    // EQ
    juce::dsp::IIR::Filter<float> eqLow[2], eqMid[2], eqHigh[2];
    float lastLow = 99, lastMid = 99, lastHigh = 99;

    // chorus
    juce::dsp::Chorus<float> chorus;

    // grain cloud (the Spark FX engine)
    struct Grain { bool active = false; double pos = 0, rate = 1; int age = 0, length = 1; float gl = 0.7f, gr = 0.7f; };
    juce::AudioBuffer<float> history;
    int historyLength = 1, writePos = 0;
    std::array<Grain, 64> grains;
    double samplesToNextGrain = 0;

    // stutter
    juce::AudioBuffer<float> slice;
    double sampleInStep = 0;
    bool repeating = false, haveSlice = false;
    int repeatLength = 1, repeatIndex = 0, sliceLength = 1;

    // delay
    juce::AudioBuffer<float> delayLine;
    int delayWrite = 0;
    juce::SmoothedValue<float> delaySamples;
    float dampL = 0, dampR = 0;

    // reverb
    juce::dsp::Reverb reverb;
};
} // namespace spark

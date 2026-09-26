#pragma once

#include "InstrumentProcessor.h"
#include <array>

namespace spark
{
struct SparkSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// One playing note. Grain mode sprays windowed grains from the source audio;
// Table mode plays the band-limited wavetable with three detuned oscillators.
class SparkVoice : public juce::SynthesiserVoice
{
public:
    explicit SparkVoice (InstrumentProcessor&);

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int value) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;
    void setCurrentPlaybackSampleRate (double) override;

    // Sources play at their own pitch on their root note (MIDI 60 unless the pitch was detected)

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0, rate = 1.0;
        int age = 0, length = 1;
        float gainL = 0.7f, gainR = 0.7f;
    };

    void processChain (float* l, float* r, int n);
    void spawnGrain (const SourceData&, double ratio, float position, float grainSec, float motion, float scan);
    void renderGrains (float* left, float* right, int n, const SourceData&, double ratio,
                       float position, float grainSec, float motion, float scan);
    void renderTable (float* left, float* right, int n, const Wavetable&, double baseHz, float morph, float motion, float scanSeconds);
    void renderSample (float* left, float* right, int n, const SourceData&, double ratio);

    InstrumentProcessor& processor;
    SourceData::Ptr source;
    Envelope ampEnv, toneEnv;
    juce::dsp::StateVariableTPTFilter<float> filter;
    float driveAmount = 0.0f, baseCutoff = 1000.0f;
    juce::Random random;

    std::array<Grain, 40> grains;
    double samplesToNextGrain = 0.0;
    double phases[3] {};
    float lfoPhase = 0.0f;
    float velocity = 1.0f;
    float bendSemitones = 0.0f;
    int note = 60;
    double samplePos = 0.0;      // Sample mode play head
    double noteSamples = 0.0;    // time since note-on, for Morph scan

    juce::AudioBuffer<float> scratch;
};
} // namespace spark

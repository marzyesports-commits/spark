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
    SparkVoice (InstrumentProcessor&, int index);

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int value) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;
    void setCurrentPlaybackSampleRate (double) override;

    // Mono/Legato: move this voice to another key. Mono restarts the envelopes; Legato doesn't.
    // Either way the pitch glides there if Glide is on.
    void changeNote (int midiNote, float velocity, bool retrigger);
    int getTargetNote() const noexcept { return targetNote; }

    // Sources play at their own pitch on their root note (MIDI 60 unless the pitch was detected)

private:
    // State-variable filter (Simper/Cytomic topology): low-pass, high-pass, band-pass and notch from one core.
    struct Svf
    {
        float ic1[2] {}, ic2[2] {};
        float a1 = 0, a2 = 0, a3 = 0, k = 1;
        void reset() noexcept { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }
        void set (float hz, float q, double sr) noexcept
        {
            const float g = std::tan (juce::MathConstants<float>::pi * hz / (float) sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        float process (int ch, float v0, int type) noexcept
        {
            const float v3 = v0 - ic2[ch];
            const float v1 = a1 * ic1[ch] + a2 * v3;
            const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
            ic1[ch] = 2.0f * v1 - ic1[ch];
            ic2[ch] = 2.0f * v2 - ic2[ch];
            switch (type)
            {
                case 1:  return v0 - k * v1 - v2;   // high-pass
                case 2:  return k * v1;             // band-pass, unity gain at the centre
                case 3:  return v0 - k * v1;        // notch
                default: return v2;                 // low-pass
            }
        }
    };

    struct Grain
    {
        bool active = false;
        double pos = 0.0, rate = 1.0;
        int age = 0, length = 1;
        float gainL = 0.7f, gainR = 0.7f;
    };

    // tone/drive/resonance are normalised 0..1 (modulated); volume ramps from volStart to volEnd over the chunk
    void processChain (float* l, float* r, int n, float toneNorm, float driveNorm, float resNorm, float volStart, float volEnd);
    void computeModulation (int blockOffset, int n, float (&offsets)[mod::numDests]);
    void startGlide();
    void spawnGrain (const SourceData&, double ratio, float position, float grainSec, float motion, float scan);
    void renderGrains (float* left, float* right, int n, const SourceData&, double ratio,
                       float position, float grainSec, float motion, float scan);
    void renderTable (float* left, float* right, int n, const Wavetable&, double baseHz, float morph, float motion, float scanSeconds);
    void renderSample (float* left, float* right, int n, const SourceData&, double ratio);

    InstrumentProcessor& processor;
    int index = 0;
    void publishEnvelopes (bool active);
    SourceData::Ptr source;
    Envelope ampEnv, toneEnv;
    Svf filter;
    float driveAmount = 0.0f, baseCutoff = 1000.0f;
    juce::Random random;

    std::array<Grain, 40> grains;
    double samplesToNextGrain = 0.0;
    double phases[3] {};
    float lfoPhase = 0.0f;
    float velocity = 1.0f;
    int wheelValue = 8192;
    int note = 60;               // the key JUCE started this voice with
    int targetNote = 60;         // the key it is playing now (differs in Mono/Legato)
    float pitchNote = 60.0f;     // the sounding pitch, gliding towards targetNote
    float glideStep = 0.0f;      // semitones per sample while gliding
    int glideLeft = 0;           // samples of glide remaining
    double samplePos = 0.0;      // Sample mode play head
    double noteSamples = 0.0;    // time since note-on, for Morph scan

    // per-voice LFOs (used when an LFO is set to Retrigger)
    double lfoVoicePhase[mod::numLfos] {};
    float lfoHeld[mod::numLfos] {}, lfoNext[mod::numLfos] {};
    float volumeNow = 1.0f;
    // layers
    double subPhase = 0.0;
    float noiseLp[2] {};
    void addLayers (float* l, float* r, int n, double baseHz);

    juce::AudioBuffer<float> scratch;
};
} // namespace spark

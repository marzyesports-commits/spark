#pragma once

#include "LeadProcessor.h"
#include "Common/Svf.h"

namespace spark
{
struct LeadSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// One lead note: up to 7 unison copies of oscillator A spread across the stereo field,
// oscillator B, a sine sub and breath noise, then drive, filter and amp.
class LeadVoice : public juce::SynthesiserVoice
{
public:
    explicit LeadVoice (LeadProcessor&);

    bool canPlaySound (juce::SynthesiserSound*) override { return true; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int value) override { wheelValue = value; }
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;
    void setCurrentPlaybackSampleRate (double) override;

    // Mono/Legato: move to another key. Mono restarts the envelopes (and scoops); Legato glides without a new attack.
    void changeNote (int midiNote, float velocity, bool retrigger);
    int getTargetNote() const noexcept { return targetNote; }

    static constexpr int maxUnison = 7;

private:
    void startGlide();
    void render (float* l, float* r, int n, int blockOffset);
    void computeModulation (int blockOffset, int n, float (&offsets)[mod::numDests]);
    // oscillator A from a sound: grains or straight playback (unison copies are detuned playheads)
    void renderGrains (float* l, float* r, int n, const SourceData&, double ratio, const double* detuneRatios,
                       const float* gl, const float* gr, int voices, float position, float grainSec, float spray);
    void renderSample (float* l, float* r, int n, const SourceData&, double ratio, const double* detuneRatios,
                       const float* gl, const float* gr, int voices, float position);

    LeadProcessor& processor;
    Envelope ampEnv, fltEnv;
    Svf filter;
    juce::Random random;

    double phaseA[maxUnison] {}, phaseB = 0.0, subPhase = 0.0;
    float velocity = 1.0f;
    int wheelValue = 8192;
    int targetNote = 60;
    float pitchNote = 60.0f, glideStep = 0.0f;
    int glideLeft = 0;
    double vibPhase = 0.0;
    double sinceAttack = 0.0;     // seconds since the note (or legato move) began: vibrato waits, then fades in
    double sinceRelease = -1.0;   // seconds since key up, for the fall-off
    float scoopNow = 0.0f;        // semitones below the note, easing to 0
    float driveNow = 0.0f, cutoffNow = 1000.0f;
    float noiseLp = 0.0f;

    SourceData::Ptr source;       // grabbed at note start so it can't change under us
    struct Grain { bool active = false; double pos = 0.0, rate = 1.0; int age = 0, length = 1, slot = 0; float gl = 0.7f, gr = 0.7f; };
    std::array<Grain, 48> grains;
    double samplesToNextGrain = 0.0;
    double playPos[maxUnison] {};   // sample playheads
    double noteSeconds = 0.0;       // for the table scan
    // per-voice LFOs (when an LFO is set to Retrigger), and the volume being modulated
    double lfoVoicePhase[mod::numLfos] {};
    float lfoHeld[mod::numLfos] {}, lfoNext[mod::numLfos] {};
    float volumeNow = -1.0f;
    juce::AudioBuffer<float> scratch;
};
} // namespace spark

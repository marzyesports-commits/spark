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
    void render (float* l, float* r, int n);

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
    juce::AudioBuffer<float> scratch;
};
} // namespace spark

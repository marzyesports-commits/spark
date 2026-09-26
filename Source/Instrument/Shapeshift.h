#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace spark
{
// Shapeshift: listens to one bounced note from any synth (Serum, Serum 2, Vital, ...) and describes
// it in Spark's terms: pitch, an evolving wavetable, the amp envelope and stereo width.
struct ShapeshiftResult
{
    bool ok = false;
    juce::String error;

    bool pitched = false;
    float midiNote = 60.0f;                  // fractional: 45.1 = A2 +10 cents
    std::vector<std::vector<float>> frames;  // single cycles through the note, first to last
    float scanSeconds = 0.0f;                // how long the frames take to play through

    float attack = 0.01f, hold = 0.0f, decay = 0.3f, sustain = 1.0f, release = 0.2f; // seconds / level
    float attackCurve = 0.0f, decayCurve = 0.0f, releaseCurve = 0.0f;
    float width = 0.0f;                      // 0 = mono, 1 = very wide
    juce::String summary;
};

namespace shapeshift
{
    ShapeshiftResult analyse (const juce::AudioBuffer<float>& audio, double sampleRate);

    // Pitch of a recording as a fractional MIDI note, or < 0 when there is no clear pitch.
    float detectMidiNote (const juce::AudioBuffer<float>& audio, double sampleRate);

    juce::String noteName (float midiNote);
    float timeToParam (float seconds);   // inverse of fmt::envSeconds
}
} // namespace spark

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace spark
{
// Spark's built-in sound library (Resources/Sounds, synthesised by tools/factory_sounds/make_sounds.py).
namespace factory
{
    struct Sound
    {
        juce::String id, name, category;
        float root = 60.0f;     // MIDI note it plays at
        int tableFrame = 0;     // > 0: a wavetable with frames of this length
    };

    const std::vector<Sound>& sounds();
    juce::StringArray categories();                  // in library order
    const Sound* find (const juce::String& id);
    bool decode (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRate);
}
} // namespace spark

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Common/Wavetable.h"

namespace spark
{
// The sound Spark plays: the dropped audio plus the wavetable made from it.
struct SourceData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SourceData>;

    juce::AudioBuffer<float> audio;   // up to 2 channels at the file's own rate
    double sampleRate = 48000.0;
    juce::String name;
    juce::File file;                  // empty for the built-in sound
    Wavetable::Ptr table;
    bool loadedAsWavetable = false;
    std::vector<float> peaks;         // 0..1 envelope for drawing, 256 bins
    int tableFrameLength = 0;         // >0 when the file was a wavetable (its frame size)
    float rootNote = 60.0f;           // MIDI note the recording plays at (fractional = detuned)
    bool shapeshifted = false;        // table was rebuilt by Shapeshift
    juce::String factoryId;           // set for sounds from Spark's library (saved by name, not audio)

    float tableGain = 1.0f;           // OBSDN: evens out the loudness of tables made from different sounds
    float audioGain = 1.0f;           // OBSDN: the same for grains and sample playback

    void computePeaks();
    void computeGains();              // fills tableGain and audioGain

    // A source from audio: tableFrameLength > 0 slices it as a wavetable file, otherwise the table is cut from the sample
    static Ptr make (juce::AudioBuffer<float> audio, double sampleRate, const juce::String& name,
                     const juce::File& file, int tableFrameLength);

    // Lossless copy for saving inside the project, encoded once and reused.
    juce::String getEmbeddedAudio() const;

private:
    mutable juce::CriticalSection embedLock;
    mutable juce::String embedded;
};

} // namespace spark

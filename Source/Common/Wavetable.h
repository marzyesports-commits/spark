#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace spark
{
// A band-limited wavetable: up to 256 single-cycle frames of 2048 samples,
// each stored at 11 harmonic limits (mips) so high notes don't alias.
class Wavetable : public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<Wavetable>;
    static constexpr int frameSize = 2048;
    static constexpr int numMips = 11;
    static constexpr int maxFrames = 256;

    // Frames may be any length; they are resampled to 2048.
    static Ptr fromFrames (std::vector<std::vector<float>> frames);
    // Slices a sample into 'numFrames' single cycles (pitch-detected where possible).
    static Ptr fromSample (const juce::AudioBuffer<float>& audio, int numFrames = 64);
    // Splits a wavetable file's audio into frames of 'frameLength' samples.
    static Ptr fromTableAudio (const juce::AudioBuffer<float>& audio, int frameLength);

    int getNumFrames() const noexcept { return numFrames; }
    const std::vector<float>& rawFrame (int f) const { return raw[(size_t) juce::jlimit (0, numFrames - 1, f)]; }

    // phaseIncrement = cycles per output sample
    static int mipForIncrement (double phaseIncrement) noexcept;
    float sample (float framePosition, double phase, int mip) const noexcept;

    // New frames shaped by drive (waveshaping) and tone (spectral roll-off), normalised.
    std::vector<std::vector<float>> bake (float drive01, float tone01) const;
    void getFrameShape (float framePosition, std::vector<float>& out, int n) const;

    // Writes a mono 32-bit float WAV with a Serum-style 'clm ' chunk so Serum and Vital
    // recognise it as a 2048-sample wavetable.
    static bool writeWav (const juce::File&, const std::vector<std::vector<float>>& frames);
    // Frame size declared in a WAV's 'clm ' chunk, or 0 if it has none.
    static int readClmFrameSize (const juce::File&);

private:
    Wavetable() = default;
    void build();
    const float* mipPtr (int frame, int mip) const noexcept
    {
        return mipData.data() + ((size_t) frame * numMips + (size_t) mip) * (size_t) (frameSize + 1);
    }

    int numFrames = 0;
    std::vector<std::vector<float>> raw;   // [frame][2048]
    std::vector<float> mipData;            // [frame][mip][2049] (last sample wraps)
};
} // namespace spark

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Common/SparkProcessorBase.h"
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

    void computePeaks();
};

class InstrumentProcessor : public SparkProcessorBase,
                            private juce::Timer
{
public:
    enum Facet { pitch, position, grain, morph, tone, drive, motion, space };
    enum Mode { grainMode = 0, tableMode = 1 };

    InstrumentProcessor();
    ~InstrumentProcessor() override;

    // ---- source management (message thread)
    SourceData::Ptr getSource() const;
    bool loadFile (const juce::File&, juce::String& error);
    void makeTableFromSample();
    bool exportTable (const juce::File&, juce::String& error) const;
    juce::AudioFormatManager& getFormatManager() noexcept { return formats; }
    juce::String getSupportedExtensions() const;

    Mode getMode() const noexcept { return (Mode) juce::roundToInt (modeParam->load()); }
    void getCoreShape (std::vector<float>& out, int n) override;

    // raw parameter access for voices (audio thread)
    struct Params
    {
        std::atomic<float>* facet[numFacets] {};
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* level = nullptr;
    } params;

    // ---- AudioProcessor
    const juce::String getName() const override { return "Spark"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;

protected:
    void writeExtraState (juce::ValueTree&) override;
    void readExtraState (const juce::ValueTree&) override;

private:
    void timerCallback() override;
    void installSource (SourceData::Ptr);
    static SourceData::Ptr makeBuiltInSource();

    juce::AudioFormatManager formats;
    juce::Synthesiser synth;
    std::atomic<float>* modeParam = nullptr;

    SourceData::Ptr source;
    mutable juce::SpinLock sourceLock;
    juce::ReferenceCountedArray<SourceData> retired; // freed on the message thread, never the audio thread

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> cutoffSmooth, driveSmooth, levelSmooth;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
} // namespace spark

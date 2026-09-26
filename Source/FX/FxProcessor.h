#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Common/SparkProcessorBase.h"
#include <array>

namespace spark
{
// Spark FX: live granular resynthesis of the track, with tempo-synced stutter,
// tone, space and dry/wet. Shares the core, facets and lineage with the instrument.
class FxProcessor : public SparkProcessorBase
{
public:
    enum Facet { size, density, spray, pitch, stutter, tone, space, mix };

    FxProcessor();

    void getCoreShape (std::vector<float>& out, int n) override;

    // Writes the last few seconds of incoming audio to a WAV file (message thread).
    juce::File captureToFile (juce::String& error);
    static juce::File getCaptureFolder();

    // Metering and scope (read from the UI)
    float getInputPeak() const noexcept  { return inPeak.load(); }
    float getOutputPeak() const noexcept { return outPeak.load(); }
    double getBpm() const noexcept       { return bpm.load(); }
    static constexpr int scopeSize = 512;
    void getScope (std::vector<float>& in, std::vector<float>& out) const;

    // ---- AudioProcessor
    const juce::String getName() const override { return "Spark FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0, rate = 1.0;
        int age = 0, length = 1;
        float gainL = 0.7f, gainR = 0.7f;
    };

    void spawnGrain (float grainSec, float sprayAmt, float semis);

    std::atomic<float>* facet[numFacets] {};
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* levelParam = nullptr;

    double sr = 44100.0;
    juce::AudioBuffer<float> history;   // circular record of the input
    int writePos = 0;
    int historyLength = 1;

    std::array<Grain, 64> grains;
    double samplesToNextGrain = 0.0;
    juce::Random random;

    // stutter
    juce::AudioBuffer<float> slice;
    int sliceLength = 1;
    double sampleInStep = 0.0;
    bool repeating = false, haveSlice = false;
    int repeatLength = 1, repeatIndex = 0;

    juce::AudioBuffer<float> wet;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> mixSmooth, levelSmooth, cutoffSmooth;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<double> bpm { 120.0 };
    std::array<std::atomic<float>, scopeSize> scopeIn {}, scopeOut {};
    std::atomic<int> scopeWrite { 0 };
    int scopeCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxProcessor)
};
} // namespace spark

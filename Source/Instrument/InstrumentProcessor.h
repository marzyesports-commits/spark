#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Common/SparkProcessorBase.h"
#include "Common/Wavetable.h"
#include "Common/Envelope.h"
#include "FxRack.h"
#include "Modulation.h"

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

    void computePeaks();

    // Lossless copy for saving inside the project, encoded once and reused.
    juce::String getEmbeddedAudio() const;

private:
    mutable juce::CriticalSection embedLock;
    mutable juce::String embedded;
};

class InstrumentProcessor;

// The synthesiser, with Mono and Legato modes on top of JUCE's polyphonic voice handling.
// Mono/Legato keep a stack of held keys and move one voice between them (glide, retrigger or not).
class SparkSynth : public juce::Synthesiser
{
public:
    explicit SparkSynth (InstrumentProcessor& p);
    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;
    void allNotesOff (int midiChannel, bool allowTailOff) override;

private:
    class SparkVoice* findMonoVoice() const;

    InstrumentProcessor& processor;
    std::vector<int> held;            // keys down, oldest first (mono modes)
    std::vector<float> heldVelocity;
    int soundingNote = -1;            // the note JUCE's voice bookkeeping knows the mono voice by
    int lastNote = -1;                // most recent key, where glides start from
    int lastMode = 0;
};

class InstrumentProcessor : public SparkProcessorBase,
                            private juce::Timer
{
public:
    enum Facet { pitch, position, grain, morph, tone, drive, motion, space, numFacetsInstrument };
    enum Mode { grainMode = 0, tableMode = 1, sampleMode = 2 };
    enum VoiceMode { poly = 0, mono = 1, legato = 2 };
    enum FilterType { lowPass = 0, highPass = 1, bandPass = 2, notch = 3 };

    InstrumentProcessor();
    ~InstrumentProcessor() override;

    // ---- source management (message thread)
    SourceData::Ptr getSource() const;
    bool loadFile (const juce::File&, juce::String& error);
    void makeTableFromSample();
    // Rebuilds a bounced note from another synth with Spark's engine. Empty file = use the current sound.
    bool shapeshift (const juce::File&, juce::String& summary);
    juce::String getLastShapeshiftSummary() const { return lastShapeshift; }
    bool exportTable (const juce::File&, juce::String& error) const;
    juce::AudioFormatManager& getFormatManager() noexcept { return formats; }
    juce::String getSupportedExtensions() const;

    Mode getMode() const noexcept { return (Mode) juce::roundToInt (modeParam->load()); }
    void setMode (Mode);

    // ---- effects rack
    std::vector<juce::RangedAudioParameter*> getRandomisableExtras() const override;
    float getFacetModulation (int facet) const override;
    juce::ReferenceCountedObjectPtr<juce::ReferenceCountedObject> getUndoObject() const override { return getSource().get(); }
    void restoreUndoObject (juce::ReferenceCountedObjectPtr<juce::ReferenceCountedObject>) override;
    bool isModuleLocked (const juce::String& moduleId) const;
    void setModuleLocked (const juce::String& moduleId, bool);
    void sparkEffects();            // roll only the enabled, unlocked effects
    void loadChain (int index);     // load an effect-chain preset
    juce::String getChainName() const { return chainName; }
    void getCoreShape (std::vector<float>& out, int n) override;

    // raw parameter access for voices (audio thread)
    struct EnvParams
    {
        std::atomic<float> *attack = nullptr, *hold = nullptr, *decay = nullptr, *sustain = nullptr, *release = nullptr;
        std::atomic<float> *attackCurve = nullptr, *decayCurve = nullptr, *releaseCurve = nullptr;

        Envelope::Settings settings() const
        {
            Envelope::Settings s;
            s.attack = fmt::envSeconds (attack->load());
            s.hold = hold->load() > 0.0f ? fmt::envSeconds (hold->load()) : 0.0f;
            s.decay = fmt::envSeconds (decay->load());
            s.sustain = sustain->load();
            s.release = fmt::envSeconds (release->load());
            s.attackCurve = attackCurve->load();
            s.decayCurve = decayCurve->load();
            s.releaseCurve = releaseCurve->load();
            return s;
        }
    };

    struct Params
    {
        std::atomic<float>* facet[numFacets] {};
        std::atomic<float>* mode = nullptr;
        EnvParams amp, toneEnv;
        std::atomic<float>* ampVelocity = nullptr;
        std::atomic<float>* toneAmount = nullptr;   // -1..1 = -5..+5 octaves
        std::atomic<float>* toneVelocity = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* scanTime = nullptr;
        std::atomic<float>* voiceMode = nullptr;
        std::atomic<float>* glide = nullptr;        // 0..1, see fmt::glideSeconds
        std::atomic<float>* bendRange = nullptr;    // semitones
        std::atomic<float>* filterType = nullptr;
        std::atomic<float>* resonance = nullptr;    // 0..1
        std::atomic<float>* keyTrack = nullptr;     // 0..1
        std::atomic<float>* subLevel = nullptr;
        std::atomic<float>* subOctave = nullptr;    // 0 = -1 octave, 1 = -2
        std::atomic<float>* noiseLevel = nullptr;
        std::atomic<float>* noiseColour = nullptr;  // 0 dark .. 1 bright
    } params;

    // Audio thread only: the note a new mono voice glides from (-1 = no glide). Set by SparkSynth.
    int glideFromNote = -1;

    // ---- modulation
    mod::Params modParams;
    mod::BlockState modState;                                   // audio thread: this block's routing and LFO phases
    std::array<std::atomic<float>, mod::numDests> liveMod {};   // for the UI: current offset on each destination
    std::array<std::atomic<float>, mod::numLfos> liveLfo {};    // for the UI: each LFO's current value
    std::array<std::atomic<float>, mod::numLfos> liveLfoPhase {};
    // Adds a routing in the first free slot (or updates an existing one). Returns the slot, or -1 if all 8 are used.
    int assignModulation (int source, int dest, float amount);
    void clearModulation (int slot);
    float defaultAmountFor (int dest) const;

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
    static SourceData::Ptr makeSource (juce::AudioBuffer<float> audio, double sampleRate, const juce::String& name,
                                       const juce::File& file, int tableFrameLength);
    static SourceData::Ptr makeBuiltInSource();

    juce::AudioFormatManager formats;
    SparkSynth synth;
    std::atomic<float>* modeParam = nullptr;

    SourceData::Ptr source;
    mutable juce::SpinLock sourceLock;
    juce::ReferenceCountedArray<SourceData> retired; // freed on the message thread, never the audio thread

    FxRack rack;
    juce::StringArray lockedModules;
    mutable juce::CriticalSection lockLock;
    juce::String chainName { "Clean" };
    bool effectsOnlyRoll = false;
    juce::String lastShapeshift;
    juce::SmoothedValue<float> levelSmooth;
    void updateModulation (int numSamples, const juce::MidiBuffer&, double bpm, double ppq, bool playing);
    juce::Random modRandom;
    double lastSyncCycle[mod::numLfos] {};
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
} // namespace spark

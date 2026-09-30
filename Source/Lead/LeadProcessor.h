#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Common/SparkProcessorBase.h"
#include "Common/Wavetable.h"
#include "Common/Envelope.h"
#include "Instrument/FxHost.h"
#include "Instrument/SourceData.h"
#include "Riff.h"

namespace spark
{
class LeadProcessor;

// Mono and Legato on top of JUCE's voice handling: a stack of held keys, one voice moving between them.
class LeadSynth : public juce::Synthesiser
{
public:
    explicit LeadSynth (LeadProcessor& p);
    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;
    void allNotesOff (int midiChannel, bool allowTailOff) override;

private:
    class LeadVoice* findMonoVoice() const;
    LeadProcessor& processor;
    std::vector<int> held;
    std::vector<float> heldVelocity;
    int soundingNote = -1, lastNote = -1, lastMode = -1;
};

// OBSDN: a lead synth. Unison wavetable oscillators, a second oscillator, sub and breath,
// a filter with its own envelope, and the expression that makes leads feel played:
// delayed vibrato, scoops into notes, fall-offs, glide. The riff writer (Riff.h) writes the lines.
class LeadProcessor : public SparkProcessorBase,
                      public FxHost,
                      private juce::Timer
{
public:
    enum Facet { wave, detune, tone, bite, drive, vibrato, glide, space };
    enum VoiceMode { poly = 0, mono = 1, legato = 2 };
    // Oscillator A's source: the built-in shapes, or a sound played as a wavetable, as grains, or as itself
    enum OscMode { waves = 0, table = 1, grain = 2, sample = 3 };

    LeadProcessor();
    ~LeadProcessor() override;

    // The eight shapes Wave sweeps through (Sine .. Reed), band-limited
    static const Wavetable& waveTable();
    static const juce::StringArray& waveNames();
    static juce::String describeWave (float normalised);

    struct Params
    {
        std::atomic<float>* facet[numFacets] {};
        std::atomic<float> *unison = nullptr, *width = nullptr;
        std::atomic<float> *oscAMode = nullptr, *scanTime = nullptr, *grainSize = nullptr, *grainSpray = nullptr;
        std::atomic<float> *oscBWave = nullptr, *oscBSemi = nullptr, *oscBFine = nullptr, *oscBLevel = nullptr;
        std::atomic<float> *subLevel = nullptr, *noiseLevel = nullptr;
        std::atomic<float> *voiceMode = nullptr, *bendRange = nullptr;
        std::atomic<float> *filterType = nullptr, *resonance = nullptr, *keyTrack = nullptr, *velTone = nullptr, *ampVel = nullptr;
        std::atomic<float> *ampA = nullptr, *ampD = nullptr, *ampS = nullptr, *ampR = nullptr;
        std::atomic<float> *fltA = nullptr, *fltD = nullptr, *fltS = nullptr, *fltR = nullptr;
        std::atomic<float> *vibRate = nullptr, *vibDelay = nullptr, *scoop = nullptr, *fall = nullptr;
        std::atomic<float> *level = nullptr;
        // the riff writer
        std::atomic<float> *riffOn = nullptr, *riffKey = nullptr, *riffScale = nullptr, *riffStyle = nullptr, *riffBars = nullptr;
        std::atomic<float> *riffDensity = nullptr, *riffRange = nullptr, *riffGate = nullptr, *riffSwing = nullptr;
        std::atomic<float> *riffOctave = nullptr, *riffFollow = nullptr, *riffLatch = nullptr;
    } params;

    Envelope::Settings ampSettings() const;
    Envelope::Settings filterSettings() const;

    // audio thread: the note a fresh mono voice glides from (-1 = none), set by LeadSynth
    int glideFromNote = -1;
    float modWheel = 0.0f, pressure = 0.0f;

    // ---- Riff (message thread unless noted)
    riff::Riff getRiff() const;
    void setRiff (const riff::Riff&, bool addToHistory = true);
    riff::Settings riffSettings() const;
    void generateRiff();                 // a brand new riff
    void mutateRiff();                   // same rhythm, new notes
    void newRiffRhythm();                // same notes, new rhythm
    void answerRiff();                   // second half answers the first
    void regenerateRiff();               // same seed with the current style/bars/density/range
    void toggleRiffNote (int tick, int degree);   // click in the roll: add or remove a note
    const std::vector<riff::Riff>& getRiffHistory() const noexcept { return riffHistory; }
    int getRiffHistoryIndex() const noexcept { return riffHistoryIndex; }
    void recallRiff (int index);
    juce::File exportRiffMidi() const;   // writes a .mid to a temp folder and returns it
    std::atomic<bool> riffPreview { false };   // PLAY button
    float getRiffPlayhead() const noexcept { return riffPlayhead.load(); }
    juce::String riffName() const;

    // ---- sound design: oscillator A's source (message thread unless noted)
    SourceData::Ptr getSource() const;                               // any thread: may be null (built-in waves)
    OscMode getOscMode() const noexcept { return (OscMode) juce::roundToInt (params.oscAMode->load()); }
    void setOscMode (OscMode);
    bool loadFile (const juce::File&, juce::String& error);          // drop any sound: tables become TABLE, the rest GRAIN
    bool loadFactorySound (const juce::String& id, bool fromPreset = false);
    bool shapeshift (const juce::File&, juce::String& summary);      // rebuild a bounced synth note as a table
    void clearSource();                                              // back to the built-in waves
    juce::String getSupportedExtensions() const { return formats.getWildcardForAllFormats(); }
    juce::String getLastShapeshiftSummary() const { return lastShapeshift; }
    juce::String sourceName() const;

    // ---- SparkProcessorBase
    std::vector<juce::RangedAudioParameter*> getRandomisableExtras() const override;
    void getCoreShape (std::vector<float>& out, int n) override;
    bool keepsValueOnPresetLoad (const juce::String& paramId) const override { return paramId.startsWith ("riff"); }

    // ---- AudioProcessor
    const juce::String getName() const override { return "OBSDN"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;

protected:
    void writeExtraState (juce::ValueTree&) override;
    void readExtraState (const juce::ValueTree&) override;
    void applyPresetSound (const Preset&) override;
    juce::String currentSoundId() const override;

private:
    void timerCallback() override;
    void installSource (SourceData::Ptr);
    juce::AudioFormatManager formats;
    SourceData::Ptr source;
    mutable juce::SpinLock sourceLock;
    juce::ReferenceCountedArray<SourceData> retired;   // freed on the message thread
    juce::String lastShapeshift;
    std::shared_ptr<std::atomic<int>> modeForDisplay;  // lets the Wave facet describe itself for the current mode
    void pushRiffToAudio();

    LeadSynth synth;
    juce::SmoothedValue<float> levelSmooth;
    double currentSampleRate = 44100.0;

    // the riff: edited on the message thread, read by the audio thread through a spin lock
    riff::Riff currentRiff, audioRiff, emptyRiff;
    std::atomic<bool> syncRiffSettings { false };
    mutable juce::SpinLock riffLock;
    riff::Player player;
    std::atomic<float> riffPlayhead { -1.0f };
    std::vector<riff::Riff> riffHistory;
    int riffHistoryIndex = -1;
    juce::Random riffRandom;
    std::array<float, 4> lastGenSettings { -1, -1, -1, -1 };   // style, bars, density, range as last used
    bool riffWasOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeadProcessor)
};

namespace leadfmt
{
    juce::String cutoff (float v);     // 300 Hz .. 18 kHz
    float cutoffHz (float v);
    float glideSeconds (float v);      // 0 .. 0.6 s
    float vibratoSemis (float v);      // 0 .. 0.6 semitones
}

PresetLibrary makeLeadPresets();
} // namespace spark

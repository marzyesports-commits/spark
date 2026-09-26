#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Lineage.h"
#include "UndoHistory.h"
#include <map>

namespace spark
{
struct FacetSpec
{
    juce::String id;
    juce::String name;       // upper-case label shown on the core ring
    float defaultValue = 0.5f;
    std::function<juce::String (float)> format;
    juce::String tooltip;
};

struct Preset
{
    juce::String category;
    juce::String name;
    juce::String hint;                    // what kind of sample it suits
    FacetValues facets {};
    std::map<juce::String, float> extras; // other parameter id -> normalised value
    juce::String sound;                   // factory sound this preset brings (instrument), empty = none
    juce::File file;                      // set for user presets
};

struct PresetCategory
{
    juce::String name;
    juce::String hint;
};

struct PresetLibrary
{
    std::vector<PresetCategory> categories;
    std::vector<Preset> presets;
};

// Value formatters shared by both plugins.
namespace fmt
{
    juce::String percent (float v);
    juce::String semitones (float v);   // -24..+24
    juce::String cutoff (float v);      // 20 Hz .. 20 kHz
    juce::String grainMs (float v);     // 5..500 ms
    juce::String driveDb (float v);     // 0..24 dB
    juce::String envTime (float v);     // 1 ms .. 5 s
    juce::String curve (float v);       // -1..1
    juce::String octaves (float v);     // -1..1 = -5..+5 octaves
    juce::String glideTime (float v);   // Off, then up to 2 s

    float cutoffHz (float v);
    float grainSeconds (float v);
    float envSeconds (float v);
    float semitoneValue (float v);
    float glideSeconds (float v);       // 0..1 -> 0..2 s (squared, so short glides get most of the travel)
    float filterQ (float resonance);    // 0..1 -> Q 0.65..9.4 (0.1 = 0.85, the classic Spark filter)
}

// Everything the two Spark plugins share: facet parameters, locks, the lineage
// (Spark / Breed / Keep / recall), presets and state saving.
class SparkProcessorBase : public juce::AudioProcessor,
                           public juce::ChangeBroadcaster
{
public:
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    SparkProcessorBase (const BusesProperties&, std::vector<FacetSpec> facets,
                        std::function<void (Layout&)> addExtraParameters,
                        PresetLibrary factoryPresets, juce::String pluginKind);

    juce::AudioProcessorValueTreeState apvts;

    // ---- facets
    const std::vector<FacetSpec>& getFacets() const noexcept { return facetSpecs; }
    juce::RangedAudioParameter& facetParam (int i) const { return *facetParams[(size_t) i]; }
    FacetValues currentFacetValues() const;
    void applyFacetValues (const FacetValues&);

    bool isLocked (int i) const noexcept { return locks[(size_t) i]; }
    void setLocked (int i, bool);
    int numLocked() const noexcept;

    // ---- randomiser (message thread)
    Lineage lineage;
    void spark();
    void breed();
    void recall (int index);
    void toggleKeepCurrent();
    int currentGeneration() const noexcept;
    juce::RangedAudioParameter& mutateParam() const { return *mutate; }
    juce::RangedAudioParameter& chaosParam() const  { return *chaos; }

    // ---- presets: factory presets first, then the user's saved ones
    static constexpr const char* userCategory = "User";
    int getNumPresets() const noexcept { return (int) (presets.size() + userPresets.size()); }
    int getNumFactoryPresets() const noexcept { return (int) presets.size(); }
    const Preset& getPreset (int index) const;
    int getPresetIndex() const noexcept { return presetIndex; }
    juce::String getPresetName() const;
    juce::String getPresetCategory() const;
    void loadPreset (int index);
    void loadRandomPreset (const juce::String& categoryOrEmpty);
    juce::StringArray getCategories() const;
    juce::String getCategoryHint (const juce::String& category) const;

    juce::File getUserPresetFolder() const;
    void rescanUserPresets();
    bool saveUserPreset (const juce::String& name, juce::String& error);
    bool deleteUserPreset (int index);

    // ---- other parameters Spark and Breed may move (e.g. enabled effects), and their current values
    virtual std::vector<juce::RangedAudioParameter*> getRandomisableExtras() const { return {}; }
    std::map<juce::String, float> currentExtraValues() const;
    void applyExtraValues (const std::map<juce::String, float>&);

    // ---- visuals: 'n' values in -1..1 describing the current sound as a ring
    virtual void getCoreShape (std::vector<float>& out, int n) = 0;
    // Live modulation offset on a facet (normalised units), for drawing. 0 when nothing modulates it.
    virtual float getFacetModulation (int) const { return 0.0f; }

    const juce::String& getKind() const noexcept { return kind; }

    // ---- undo / redo (message thread)
    UndoHistory& getUndo() { return *undoHistory; }
    // Non-parameter state that undo should also restore (the instrument's loaded sound)
    virtual juce::ReferenceCountedObjectPtr<juce::ReferenceCountedObject> getUndoObject() const { return nullptr; }
    virtual void restoreUndoObject (juce::ReferenceCountedObjectPtr<juce::ReferenceCountedObject>) {}

    // ---- AudioProcessor
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return juce::jmax (1, getNumFactoryPresets()); }
    int getCurrentProgram() override { return juce::jlimit (0, juce::jmax (0, getNumFactoryPresets() - 1), presetIndex); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

protected:
    virtual void writeExtraState (juce::ValueTree&) {}
    virtual void readExtraState (const juce::ValueTree&) {}
    virtual void migrateParameters (juce::ValueTree&) {}
    virtual void applyPresetSound (const Preset&) {}          // after a preset's parameters are set
    virtual juce::String currentSoundId() const { return {}; } // saved with user presets   // rename/convert parameters from older versions before loading
    void setParam (const juce::String& id, float normalised);

private:
    static Layout buildLayout (const std::vector<FacetSpec>&, const std::function<void (Layout&)>&);

    std::vector<FacetSpec> facetSpecs;
    std::vector<juce::RangedAudioParameter*> facetParams;
    juce::RangedAudioParameter* mutate = nullptr;
    juce::RangedAudioParameter* chaos = nullptr;
    FacetLocks locks {};
    std::vector<PresetCategory> categories;
    std::vector<Preset> presets, userPresets;
    int presetIndex = 0;
    juce::String kind;
    juce::Random random;
    std::unique_ptr<UndoHistory> undoHistory;   // last member: goes before the parameters it listens to

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SparkProcessorBase)
};
} // namespace spark

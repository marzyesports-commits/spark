#include "SparkProcessorBase.h"

namespace spark
{
namespace fmt
{
    float cutoffHz (float v)      { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, v)); }
    float grainSeconds (float v)  { return (5.0f + v * 495.0f) * 0.001f; }
    float envSeconds (float v)    { return (1.0f + v * v * 4999.0f) * 0.001f; }
    float semitoneValue (float v) { return std::round ((v - 0.5f) * 48.0f); }

    juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    juce::String semitones (float v)
    {
        const int n = (int) semitoneValue (v);
        return (n > 0 ? "+" : "") + juce::String (n) + " st";
    }

    juce::String cutoff (float v)
    {
        const float hz = cutoffHz (v);
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    juce::String grainMs (float v) { return juce::String (juce::roundToInt (grainSeconds (v) * 1000.0f)) + " ms"; }
    juce::String driveDb (float v) { return "+" + juce::String (v * 24.0f, 1) + " dB"; }

    juce::String curve (float v)
    {
        if (std::abs (v) < 0.03f) return "Linear";
        return (v > 0 ? "Punchy " : "Swell ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
    }

    juce::String octaves (float v)
    {
        if (std::abs (v) < 0.005f) return "Off";
        return (v > 0 ? "+" : "") + juce::String (v * 5.0f, 1) + " oct";
    }

    juce::String envTime (float v)
    {
        const float s = envSeconds (v);
        return s >= 1.0f ? juce::String (s, 2) + " s" : juce::String (juce::roundToInt (s * 1000.0f)) + " ms";
    }

    float glideSeconds (float v) { return 2.0f * v * v; }
    float filterQ (float resonance) { return 0.85f * std::pow (15.0f, juce::jlimit (0.0f, 1.0f, resonance) - 0.1f); }

    juce::String glideTime (float v)
    {
        const float s = glideSeconds (v);
        if (s < 0.001f) return "Off";
        return s >= 1.0f ? juce::String (s, 2) + " s" : juce::String (juce::roundToInt (s * 1000.0f)) + " ms";
    }
}

SparkProcessorBase::Layout SparkProcessorBase::buildLayout (const std::vector<FacetSpec>& facets,
                                                             const std::function<void (Layout&)>& addExtra)
{
    Layout layout;
    for (const auto& f : facets)
    {
        auto formatter = f.format;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { f.id, 1 },
            f.name.substring (0, 1) + f.name.substring (1).toLowerCase(),
            juce::NormalisableRange<float> (0.0f, 1.0f),
            f.defaultValue,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [formatter] (float v, int) { return formatter ? formatter (v) : fmt::percent (v); })));
    }

    auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::percent (v); });
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mutate", 1 }, "Mutate",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "chaos", 1 }, "Chaos",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    if (addExtra)
        addExtra (layout);
    return layout;
}

SparkProcessorBase::SparkProcessorBase (const BusesProperties& buses, std::vector<FacetSpec> facets,
                                        std::function<void (Layout&)> addExtra, PresetLibrary library,
                                        juce::String pluginKind)
    : juce::AudioProcessor (buses),
      apvts (*this, nullptr, "SPARK", buildLayout (facets, addExtra)),
      facetSpecs (std::move (facets)),
      categories (std::move (library.categories)),
      presets (std::move (library.presets)),
      kind (std::move (pluginKind))
{
    jassert ((int) facetSpecs.size() == numFacets);
    for (const auto& f : facetSpecs)
        facetParams.push_back (apvts.getParameter (f.id));
    mutate = apvts.getParameter ("mutate");
    chaos = apvts.getParameter ("chaos");

    rescanUserPresets();
    if (! presets.empty())
        loadPreset (0);
    else
        lineage.push (currentFacetValues(), (juce::uint32) random.nextInt());

    undoHistory = std::make_unique<UndoHistory> (*this);
}

FacetValues SparkProcessorBase::currentFacetValues() const
{
    FacetValues v {};
    for (int i = 0; i < numFacets; ++i)
        v[(size_t) i] = facetParams[(size_t) i]->getValue();
    return v;
}

void SparkProcessorBase::applyFacetValues (const FacetValues& v)
{
    for (int i = 0; i < numFacets; ++i)
    {
        auto* p = facetParams[(size_t) i];
        if (std::abs (p->getValue() - v[(size_t) i]) > 1.0e-5f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (v[(size_t) i]);
            p->endChangeGesture();
        }
    }
}

void SparkProcessorBase::setParam (const juce::String& id, float normalised)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (normalised);
        p->endChangeGesture();
    }
}

void SparkProcessorBase::setLocked (int i, bool shouldLock)
{
    locks[(size_t) i] = shouldLock;
    sendChangeMessage();
}

int SparkProcessorBase::numLocked() const noexcept
{
    return (int) std::count (locks.begin(), locks.end(), true);
}

std::map<juce::String, float> SparkProcessorBase::currentExtraValues() const
{
    std::map<juce::String, float> m;
    for (auto* p : getRandomisableExtras())
        m[p->getParameterID()] = p->getValue();
    return m;
}

void SparkProcessorBase::applyExtraValues (const std::map<juce::String, float>& extras)
{
    for (const auto& [id, v] : extras)
        if (auto* p = apvts.getParameter (id); p != nullptr && std::abs (p->getValue() - v) > 1.0e-5f)
            setParam (id, v);
}

void SparkProcessorBase::spark()
{
    const auto seed = (juce::uint32) random.nextInt();
    const auto vals = Lineage::roll (currentFacetValues(), seed, mutate->getValue(), chaos->getValue(), locks);
    const auto extras = Lineage::rollExtras (currentExtraValues(), seed, mutate->getValue(), chaos->getValue());
    lineage.push (vals, seed, extras);
    applyFacetValues (vals);
    applyExtraValues (extras);
    sendChangeMessage();
}

void SparkProcessorBase::breed()
{
    const auto* partner = lineage.breedPartner();
    const auto current = currentFacetValues();
    const auto currentExtras = currentExtraValues();
    const auto seed = (juce::uint32) random.nextInt();
    const auto vals = Lineage::breed (current, partner != nullptr ? partner->vals : current, seed, locks);
    const auto extras = Lineage::breedExtras (currentExtras, partner != nullptr ? partner->extras : currentExtras, seed);
    lineage.push (vals, seed, extras);
    applyFacetValues (vals);
    applyExtraValues (extras);
    sendChangeMessage();
}

void SparkProcessorBase::recall (int index)
{
    lineage.setCurrent (index);
    if (auto* n = lineage.currentNode())
    {
        // Locked facets stay where they are, even when travelling back in time.
        FacetValues v = n->vals;
        const auto now = currentFacetValues();
        for (int i = 0; i < numFacets; ++i)
            if (locks[(size_t) i]) v[(size_t) i] = now[(size_t) i];
        applyFacetValues (v);
        // Only restore effect settings that are still unlocked and in use.
        const auto allowed = currentExtraValues();
        std::map<juce::String, float> e;
        for (const auto& [id, val] : n->extras)
            if (allowed.count (id) > 0) e[id] = val;
        applyExtraValues (e);
    }
    sendChangeMessage();
}

void SparkProcessorBase::toggleKeepCurrent()
{
    const int i = lineage.currentIndex();
    if (auto* n = lineage.currentNode(); n != nullptr && ! n->kept)
        lineage.updateValues (i, currentFacetValues(), currentExtraValues()); // keep what you are actually hearing
    lineage.toggleKeep (i);
    sendChangeMessage();
}

int SparkProcessorBase::currentGeneration() const noexcept
{
    auto* n = lineage.currentNode();
    return n != nullptr ? n->gen : 1;
}

const Preset& SparkProcessorBase::getPreset (int index) const
{
    static const Preset empty { "Starters", "Init", {}, {}, {}, {} };
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index];
    index -= (int) presets.size();
    if (juce::isPositiveAndBelow (index, (int) userPresets.size()))
        return userPresets[(size_t) index];
    return empty;
}

juce::String SparkProcessorBase::getPresetName() const     { return getPreset (presetIndex).name; }
juce::String SparkProcessorBase::getPresetCategory() const { return getPreset (presetIndex).category; }

const juce::String SparkProcessorBase::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

juce::StringArray SparkProcessorBase::getCategories() const
{
    juce::StringArray names;
    for (const auto& c : categories)
        names.add (c.name);
    for (const auto& p : presets)
        names.addIfNotAlreadyThere (p.category);
    names.add (userCategory);
    return names;
}

juce::String SparkProcessorBase::getCategoryHint (const juce::String& category) const
{
    if (category == userCategory)
        return "Presets you've saved. They live in " + getUserPresetFolder().getFullPathName();
    for (const auto& c : categories)
        if (c.name == category)
            return c.hint;
    return {};
}

void SparkProcessorBase::loadPreset (int index)
{
    const int total = getNumPresets();
    if (total == 0)
        return;
    presetIndex = ((index % total) + total) % total;
    const auto& p = getPreset (presetIndex);

    // Reset the non-facet sound settings first so nothing leaks over from the previous preset,
    // then apply the preset's own values.
    const juce::StringArray untouched { "mutate", "chaos", "freeze", "bypass" };
    for (auto* param : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr || untouched.contains (ranged->getParameterID())
            || std::find (facetParams.begin(), facetParams.end(), ranged) != facetParams.end())
            continue;
        const auto it = p.extras.find (ranged->getParameterID());
        const float target = it != p.extras.end() ? it->second : ranged->getDefaultValue();
        if (std::abs (ranged->getValue() - target) > 1.0e-5f)
            setParam (ranged->getParameterID(), target);
    }

    // Presets load all facets, but respect locks so a locked facet survives preset browsing.
    FacetValues v = p.facets;
    const auto now = currentFacetValues();
    for (int i = 0; i < numFacets; ++i)
        if (locks[(size_t) i]) v[(size_t) i] = now[(size_t) i];

    applyFacetValues (v);
    lineage.push (v, (juce::uint32) juce::DefaultHashFunctions::generateHash (p.category + p.name, 1 << 30), currentExtraValues());
    sendChangeMessage();
}

void SparkProcessorBase::loadRandomPreset (const juce::String& category)
{
    juce::Array<int> candidates;
    for (int i = 0; i < getNumPresets(); ++i)
        if ((category.isEmpty() || getPreset (i).category == category) && i != presetIndex)
            candidates.add (i);
    if (! candidates.isEmpty())
        loadPreset (candidates[random.nextInt (candidates.size())]);
}

juce::File SparkProcessorBase::getUserPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Spark").getChildFile ("Presets").getChildFile (kind == "fx" ? "Spark FX" : "Spark");
}

void SparkProcessorBase::rescanUserPresets()
{
    const auto current = presetIndex >= getNumFactoryPresets() ? getPreset (presetIndex).file : juce::File();
    userPresets.clear();

    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.sparkpreset");
    files.sort();
    for (const auto& f : files)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("SparkPreset") || xml->getStringAttribute ("kind") != kind)
            continue;

        Preset p;
        p.category = userCategory;
        p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
        p.hint = xml->getStringAttribute ("hint", "Saved " + f.getLastModificationTime().formatted ("%d %b %Y"));
        p.file = f;
        p.facets = currentFacetValues();
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = e->getStringAttribute ("id");
            const float value = juce::jlimit (0.0f, 1.0f, (float) e->getDoubleAttribute ("value"));
            bool isFacet = false;
            for (int i = 0; i < numFacets; ++i)
                if (facetSpecs[(size_t) i].id == id) { p.facets[(size_t) i] = value; isFacet = true; }
            if (! isFacet && apvts.getParameter (id) != nullptr)
                p.extras[id] = value;
        }
        userPresets.push_back (std::move (p));
    }

    if (current != juce::File())
        for (int i = 0; i < (int) userPresets.size(); ++i)
            if (userPresets[(size_t) i].file == current)
                presetIndex = getNumFactoryPresets() + i;
    sendChangeMessage();
}

bool SparkProcessorBase::saveUserPreset (const juce::String& rawName, juce::String& error)
{
    const auto name = rawName.trim();
    if (name.isEmpty())
    {
        error = "Give the preset a name.";
        return false;
    }
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
    {
        error = "Couldn't create " + folder.getFullPathName();
        return false;
    }

    juce::XmlElement xml ("SparkPreset");
    xml.setAttribute ("kind", kind);
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    const juce::StringArray skip { "mutate", "chaos", "freeze", "bypass" };
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param); ranged != nullptr && ! skip.contains (ranged->getParameterID()))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", ranged->getParameterID());
            e->setAttribute ("value", ranged->getValue());
        }

    const auto file = folder.getChildFile (juce::File::createLegalFileName (name) + ".sparkpreset");
    if (! xml.writeTo (file))
    {
        error = "Couldn't write " + file.getFullPathName();
        return false;
    }

    rescanUserPresets();
    for (int i = 0; i < (int) userPresets.size(); ++i)
        if (userPresets[(size_t) i].file == file)
            presetIndex = getNumFactoryPresets() + i;
    sendChangeMessage();
    return true;
}

bool SparkProcessorBase::deleteUserPreset (int index)
{
    const auto& p = getPreset (index);
    if (p.file == juce::File() || ! (p.file.moveToTrash() || p.file.deleteFile()))
        return false;
    if (index == presetIndex)
        presetIndex = 0;
    rescanUserPresets();
    return true;
}

void SparkProcessorBase::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("pluginKind", kind, nullptr);
    state.setProperty ("presetIndex", presetIndex, nullptr);
    state.setProperty ("presetName", getPresetName(), nullptr);
    state.setProperty ("presetCategory", getPresetCategory(), nullptr);

    juce::String lockString;
    for (auto l : locks) lockString << (l ? "1" : "0");
    state.setProperty ("locks", lockString, nullptr);

    state.removeChild (state.getChildWithName ("LINEAGE"), nullptr);
    state.appendChild (lineage.toValueTree(), nullptr);

    auto extra = state.getOrCreateChildWithName ("EXTRA", nullptr);
    extra.removeAllProperties (nullptr);
    writeExtraState (extra);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SparkProcessorBase::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    // Find the preset by name first (indices shift when presets are added), then fall back to the index.
    presetIndex = juce::jlimit (0, juce::jmax (0, getNumPresets() - 1), (int) state.getProperty ("presetIndex", 0));
    const auto savedName = state.getProperty ("presetName").toString();
    const auto savedCategory = state.getProperty ("presetCategory").toString();
    for (int i = 0; i < getNumPresets(); ++i)
        if (getPreset (i).name == savedName && (savedCategory.isEmpty() || getPreset (i).category == savedCategory))
        {
            presetIndex = i;
            break;
        }
    const auto lockString = state.getProperty ("locks").toString();
    for (int i = 0; i < numFacets; ++i)
        locks[(size_t) i] = lockString.length() > i && lockString[i] == '1';

    lineage.fromValueTree (state.getChildWithName ("LINEAGE"));
    readExtraState (state.getChildWithName ("EXTRA"));

    auto paramsOnly = state.createCopy();
    paramsOnly.removeChild (paramsOnly.getChildWithName ("LINEAGE"), nullptr);
    paramsOnly.removeChild (paramsOnly.getChildWithName ("EXTRA"), nullptr);
    migrateParameters (paramsOnly);
    apvts.replaceState (paramsOnly);

    sendChangeMessage(); // async and thread-safe
    if (undoHistory != nullptr)
        undoHistory->requestReset();   // a loaded project starts a fresh history (hosts may call this off the message thread)
}
} // namespace spark

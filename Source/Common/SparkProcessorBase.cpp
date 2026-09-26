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

    juce::String envTime (float v)
    {
        const float s = envSeconds (v);
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
                                        std::function<void (Layout&)> addExtra, std::vector<Preset> presetList,
                                        juce::String pluginKind)
    : juce::AudioProcessor (buses),
      apvts (*this, nullptr, "SPARK", buildLayout (facets, addExtra)),
      facetSpecs (std::move (facets)),
      presets (std::move (presetList)),
      kind (std::move (pluginKind))
{
    jassert ((int) facetSpecs.size() == numFacets);
    for (const auto& f : facetSpecs)
        facetParams.push_back (apvts.getParameter (f.id));
    mutate = apvts.getParameter ("mutate");
    chaos = apvts.getParameter ("chaos");

    if (! presets.empty())
        loadPreset (0);
    else
        lineage.push (currentFacetValues(), (juce::uint32) random.nextInt());
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

void SparkProcessorBase::spark()
{
    const auto seed = (juce::uint32) random.nextInt();
    const auto vals = Lineage::roll (currentFacetValues(), seed, mutate->getValue(), chaos->getValue(), locks);
    lineage.push (vals, seed);
    applyFacetValues (vals);
    sendChangeMessage();
}

void SparkProcessorBase::breed()
{
    const auto* partner = lineage.breedPartner();
    const auto current = currentFacetValues();
    const auto seed = (juce::uint32) random.nextInt();
    const auto vals = Lineage::breed (current, partner != nullptr ? partner->vals : current, seed, locks);
    lineage.push (vals, seed);
    applyFacetValues (vals);
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
    }
    sendChangeMessage();
}

void SparkProcessorBase::toggleKeepCurrent()
{
    const int i = lineage.currentIndex();
    if (auto* n = lineage.currentNode(); n != nullptr && ! n->kept)
        lineage.updateValues (i, currentFacetValues()); // keep what you are actually hearing
    lineage.toggleKeep (i);
    sendChangeMessage();
}

int SparkProcessorBase::currentGeneration() const noexcept
{
    auto* n = lineage.currentNode();
    return n != nullptr ? n->gen : 1;
}

juce::String SparkProcessorBase::getPresetName() const
{
    return juce::isPositiveAndBelow (presetIndex, (int) presets.size()) ? presets[(size_t) presetIndex].name : juce::String ("Init");
}

const juce::String SparkProcessorBase::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void SparkProcessorBase::loadPreset (int index)
{
    if (presets.empty())
        return;
    presetIndex = ((index % (int) presets.size()) + (int) presets.size()) % (int) presets.size();
    const auto& p = presets[(size_t) presetIndex];

    for (const auto& [id, value] : p.extras)
        setParam (id, value);

    // Presets load all facets, but respect locks so a locked facet survives preset browsing.
    FacetValues v = p.facets;
    const auto now = currentFacetValues();
    for (int i = 0; i < numFacets; ++i)
        if (locks[(size_t) i]) v[(size_t) i] = now[(size_t) i];

    applyFacetValues (v);
    lineage.push (v, (juce::uint32) juce::DefaultHashFunctions::generateHash (p.name, 1 << 30));
    sendChangeMessage();
}

void SparkProcessorBase::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("pluginKind", kind, nullptr);
    state.setProperty ("presetIndex", presetIndex, nullptr);

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

    presetIndex = juce::jlimit (0, juce::jmax (0, (int) presets.size() - 1), (int) state.getProperty ("presetIndex", 0));
    const auto lockString = state.getProperty ("locks").toString();
    for (int i = 0; i < numFacets; ++i)
        locks[(size_t) i] = lockString.length() > i && lockString[i] == '1';

    lineage.fromValueTree (state.getChildWithName ("LINEAGE"));
    readExtraState (state.getChildWithName ("EXTRA"));

    auto paramsOnly = state.createCopy();
    paramsOnly.removeChild (paramsOnly.getChildWithName ("LINEAGE"), nullptr);
    paramsOnly.removeChild (paramsOnly.getChildWithName ("EXTRA"), nullptr);
    apvts.replaceState (paramsOnly);

    sendChangeMessage(); // async and thread-safe
}
} // namespace spark

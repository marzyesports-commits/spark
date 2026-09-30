#include "FxHost.h"

namespace spark
{
bool FxHost::isModuleLocked (const juce::String& id) const
{
    const juce::ScopedLock sl (lockLock);
    return lockedModules.contains (id);
}

void FxHost::setModuleLocked (const juce::String& id, bool locked)
{
    {
        const juce::ScopedLock sl (lockLock);
        if (locked) lockedModules.addIfNotAlreadyThere (id); else lockedModules.removeString (id);
    }
    fxOwner.sendChangeMessage();
}

void FxHost::addRackExtras (std::vector<juce::RangedAudioParameter*>& out) const
{
    for (const auto& m : FxRack::modules())
    {
        if (fxOwner.apvts.getRawParameterValue (m.onParam)->load() < 0.5f || isModuleLocked (m.id))
            continue;
        for (const auto& id : m.params)
            if (id != "space")
                out.push_back (fxOwner.apvts.getParameter (id));
    }
}

void FxHost::sparkEffects()
{
    const auto seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
    std::vector<juce::RangedAudioParameter*> params;
    addRackExtras (params);
    std::map<juce::String, float> current;
    for (auto* p : params)
        current[p->paramID] = p->getValue();
    const auto extras = Lineage::rollExtras (current, seed, 1.0f, fxOwner.chaosParam().getValue());
    fxOwner.lineage.push (fxOwner.currentFacetValues(), seed, extras);
    fxOwner.applyExtraValues (extras);
    fxOwner.sendChangeMessage();
}

void FxHost::loadChain (int index)
{
    const auto& chains = FxRack::chains();
    if (! juce::isPositiveAndBelow (index, (int) chains.size()))
        return;
    const auto& chain = chains[(size_t) index];
    for (const auto& m : FxRack::modules())
    {
        juce::StringArray ids (m.params);
        ids.add (m.onParam);
        for (const auto& id : ids)
        {
            if (id == "space") continue;
            if (auto* prm = fxOwner.apvts.getParameter (id))
            {
                const auto it = chain.values.find (id);
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (it != chain.values.end() ? prm->convertTo0to1 (it->second) : prm->getDefaultValue());
                prm->endChangeGesture();
            }
        }
    }
    chainName = chain.name;
    fxOwner.sendChangeMessage();
}

void FxHost::writeRackState (juce::ValueTree& extra) const
{
    {
        const juce::ScopedLock sl (lockLock);
        extra.setProperty ("fxLocks", lockedModules.joinIntoString (","), nullptr);
    }
    extra.setProperty ("chain", chainName, nullptr);
}

void FxHost::readRackState (const juce::ValueTree& extra)
{
    {
        const juce::ScopedLock sl (lockLock);
        lockedModules = juce::StringArray::fromTokens (extra.getProperty ("fxLocks").toString(), ",", "");
        lockedModules.removeEmptyStrings();
    }
    chainName = extra.getProperty ("chain", "Clean").toString();
}
} // namespace spark

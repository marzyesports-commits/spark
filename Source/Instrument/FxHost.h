#pragma once

#include "Common/SparkProcessorBase.h"
#include "FxRack.h"

namespace spark
{
// The effects rack as a processor sees it: the rack itself, per-module locks, chain presets and
// a Spark for just the effects. Shared by Spark and SparkLead so both get the same FX page.
class FxHost
{
public:
    explicit FxHost (SparkProcessorBase& owner) : fxOwner (owner) {}
    virtual ~FxHost() = default;

    SparkProcessorBase& fxProcessor() noexcept { return fxOwner; }

    bool isModuleLocked (const juce::String& moduleId) const;
    void setModuleLocked (const juce::String& moduleId, bool);
    void sparkEffects();            // roll only the enabled, unlocked effects
    void loadChain (int index);     // load an effect-chain preset
    juce::String getChainName() const { return chainName; }

    // Enabled, unlocked effect parameters (the Space facet is a facet, so it's skipped)
    void addRackExtras (std::vector<juce::RangedAudioParameter*>&) const;

protected:
    void writeRackState (juce::ValueTree&) const;
    void readRackState (const juce::ValueTree&);

    FxRack rack;

private:
    SparkProcessorBase& fxOwner;
    juce::StringArray lockedModules;
    mutable juce::CriticalSection lockLock;
    juce::String chainName { "Clean" };
};
} // namespace spark

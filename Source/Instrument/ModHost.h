#pragma once

#include "Common/SparkProcessorBase.h"
#include "FxHost.h"
#include "Modulation.h"

namespace spark
{
// Modulation as a processor sees it: 2 LFOs, 4 macros, mod wheel, aftertouch and velocity routed through
// 8 slots. Shared by Spark and OBSDN, so both get the same LFO, macro and matrix cards and drag-to-assign.
class ModHost
{
public:
    ModHost (SparkProcessorBase& owner, FxHost& fx) : modOwner (owner), fxHost (fx) {}
    virtual ~ModHost() = default;

    SparkProcessorBase& modProcessor() noexcept { return modOwner; }

    mod::Params modParams;
    mod::BlockState modState;                                   // audio thread: this block's routing and LFO phases
    std::array<std::atomic<float>, mod::numDests> liveMod {};   // for the UI: current offset on each destination
    std::array<std::atomic<float>, mod::numLfos> liveLfo {};    // for the UI: each LFO's current value
    std::array<std::atomic<float>, mod::numLfos> liveLfoPhase {};

    // Adds a routing in the first free slot (or updates an existing one). Returns the slot, or -1 if all 8 are used.
    int assignModulation (int source, int dest, float amount);
    void clearModulation (int slot);
    virtual float defaultAmountFor (int dest) const;

    // The lock on the matrix card: Spark and Breed leave the routings alone
    bool isModLocked() const { return fxHost.isModuleLocked ("mod"); }
    void setModLocked (bool l) { fxHost.setModuleLocked ("mod", l); }

    // Routing amounts, the macros they read and the speed of the LFOs they use (unless locked)
    void addModExtras (std::vector<juce::RangedAudioParameter*>&) const;

protected:
    void attachModulation (juce::AudioProcessorValueTreeState&);
    // audio thread: before the voices render, then after
    void updateModulation (const juce::MidiBuffer&, double bpm, double ppq, bool playing, double sampleRate);
    void advanceLfos (int numSamples);

private:
    SparkProcessorBase& modOwner;
    FxHost& fxHost;
    juce::Random modRandom;
    double lastSyncCycle[mod::numLfos] {};
};
} // namespace spark

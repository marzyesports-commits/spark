#include "ModHost.h"

namespace spark
{
void ModHost::attachModulation (juce::AudioProcessorValueTreeState& state)
{
    modParams.attach (state);
    for (int l = 0; l < mod::numLfos; ++l)
    {
        modState.held[l] = modRandom.nextFloat() * 2.0f - 1.0f;
        modState.next[l] = modRandom.nextFloat() * 2.0f - 1.0f;
    }
}

void ModHost::advanceLfos (int n)
{
    // move the free-running LFOs past this block
    for (int l = 0; l < mod::numLfos; ++l)
    {
        double ph = modState.lfoPhase[l] + modState.lfoInc[l] * n;
        if (ph >= 1.0)
        {
            ph -= std::floor (ph);
            modState.held[l] = modState.next[l];
            modState.next[l] = modRandom.nextFloat() * 2.0f - 1.0f;
        }
        modState.lfoPhase[l] = ph;
    }
}

void ModHost::addModExtras (std::vector<juce::RangedAudioParameter*>& out) const
{
    if (isModLocked())
        return;
    auto& apvts = modOwner.apvts;
    bool lfoUsed[mod::numLfos] {}, macroUsed[mod::numMacros] {};
    for (int s = 0; s < mod::numSlots; ++s)
    {
        const int src = juce::roundToInt (modParams.src[s]->load());
        if (src == mod::none) continue;
        out.push_back (apvts.getParameter (mod::slotParam (s, "Amt")));
        if (src == mod::lfo1 || src == mod::lfo2) lfoUsed[src - mod::lfo1] = true;
        if (src >= mod::macro1 && src <= mod::macro4) macroUsed[src - mod::macro1] = true;
    }
    for (int l = 0; l < mod::numLfos; ++l)
        if (lfoUsed[l])
            out.push_back (apvts.getParameter (mod::lfoParam (l, modParams.sync[l]->load() > 0.5f ? "Div" : "Rate")));
    for (int m = 0; m < mod::numMacros; ++m)
        if (macroUsed[m])
            out.push_back (apvts.getParameter (mod::macroParam (m)));
}

void ModHost::updateModulation (const juce::MidiBuffer& midi, double bpm, double ppq, bool playing, double sampleRate)
{
    auto& st = modState;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isController() && msg.getControllerNumber() == 1) st.modWheel = (float) msg.getControllerValue() / 127.0f;
        else if (msg.isChannelPressure()) st.aftertouch = (float) msg.getChannelPressureValue() / 127.0f;
        else if (msg.isAftertouch()) st.aftertouch = (float) msg.getAfterTouchValue() / 127.0f;
    }
    for (int m = 0; m < mod::numMacros; ++m)
        st.macro[m] = modParams.macro[m]->load();

    st.numActive = 0;
    for (auto& u : st.usesLfo) u = false;
    for (int s = 0; s < mod::numSlots; ++s)
    {
        const int src = juce::roundToInt (modParams.src[s]->load());
        const float amt = modParams.amt[s]->load();
        if (src == mod::none || std::abs (amt) < 1.0e-4f)
            continue;
        auto& slot = st.slots[st.numActive++];
        slot.src = src;
        slot.dst = juce::jlimit (0, mod::numDests - 1, juce::roundToInt (modParams.dst[s]->load()));
        slot.amount = amt;
        if (src == mod::lfo1 || src == mod::lfo2) st.usesLfo[src - mod::lfo1] = true;
    }

    float sources[mod::numSources] {};
    for (int l = 0; l < mod::numLfos; ++l)
    {
        st.shape[l] = juce::roundToInt (modParams.shape[l]->load());
        st.retrigger[l] = modParams.retrig[l]->load() > 0.5f;
        const bool synced = modParams.sync[l]->load() > 0.5f;
        double hz = mod::rateHz (modParams.rate[l]->load());
        if (synced)
        {
            const double beats = mod::divisionBeats (juce::roundToInt (modParams.division[l]->load()));
            hz = bpm / 60.0 / beats;
            if (playing)
            {
                // lock to the host's bar position
                const double cycles = ppq / beats;
                if (std::floor (cycles) != std::floor (lastSyncCycle[l]))
                {
                    st.held[l] = st.next[l];
                    st.next[l] = modRandom.nextFloat() * 2.0f - 1.0f;
                }
                lastSyncCycle[l] = cycles;
                st.lfoPhase[l] = cycles - std::floor (cycles);
            }
        }
        st.lfoInc[l] = hz / juce::jmax (1.0, sampleRate);
        const float v = mod::shapeValue (st.shape[l], (float) st.lfoPhase[l], st.held[l], st.next[l]);
        sources[mod::lfo1 + l] = v;
        liveLfo[(size_t) l].store (v);
        liveLfoPhase[(size_t) l].store ((float) st.lfoPhase[l]);
    }
    for (int m = 0; m < mod::numMacros; ++m) sources[mod::macro1 + m] = st.macro[m];
    sources[mod::modWheel] = st.modWheel;
    sources[mod::aftertouch] = st.aftertouch;
    sources[mod::velocity] = 0.8f;   // global view only; voices use their own velocity

    float offsets[mod::numDests] {};
    st.route (sources, offsets);
    for (int d = 0; d < mod::numDests; ++d)
        liveMod[(size_t) d].store (offsets[d]);
}

float ModHost::defaultAmountFor (int dest) const
{
    switch (dest)
    {
        case mod::pitch:  return 0.02f;   // about +/-1 semitone: vibrato
        case mod::volume: return -0.5f;   // tremolo / ducking
        default:          return 0.3f;
    }
}

int ModHost::assignModulation (int source, int dest, float amount)
{
    if (source <= mod::none || source >= mod::numSources || dest < 0 || dest >= mod::numDests)
        return -1;
    auto setChoice = [this] (const juce::String& id, int index, int count)
    {
        if (auto* p = modOwner.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost ((float) index / (float) juce::jmax (1, count - 1));
            p->endChangeGesture();
        }
    };
    auto setAmount = [this] (int slot, float a)
    {
        if (auto* p = modOwner.apvts.getParameter (mod::slotParam (slot, "Amt")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (-1.0f, 1.0f, a)));
            p->endChangeGesture();
        }
    };
    // already routed? just set the amount
    for (int s = 0; s < mod::numSlots; ++s)
        if (juce::roundToInt (modParams.src[s]->load()) == source && juce::roundToInt (modParams.dst[s]->load()) == dest)
        {
            setAmount (s, amount);
            return s;
        }
    for (int s = 0; s < mod::numSlots; ++s)
        if (juce::roundToInt (modParams.src[s]->load()) == mod::none)
        {
            setChoice (mod::slotParam (s, "Dst"), dest, mod::numDests);
            setAmount (s, amount);
            setChoice (mod::slotParam (s, "Src"), source, mod::numSources);
            modOwner.sendChangeMessage();
            return s;
        }
    return -1;
}

void ModHost::clearModulation (int slot)
{
    if (slot < 0 || slot >= mod::numSlots)
        return;
    for (auto* what : { "Src", "Dst", "Amt" })
        if (auto* p = modOwner.apvts.getParameter (mod::slotParam (slot, what)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    modOwner.sendChangeMessage();
}
} // namespace spark

#include "UndoHistory.h"
#include "SparkProcessorBase.h"

namespace spark
{
UndoHistory::UndoHistory (SparkProcessorBase& p) : processor (p)
{
    for (auto* prm : processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (prm); ranged != nullptr && ranged->getParameterID() == "bypass")
            continue;
        params.add (prm);
        prm->addListener (this);
    }
    startTimer (200);
}

UndoHistory::~UndoHistory()
{
    stopTimer();
    for (auto* prm : params)
        prm->removeListener (this);
}

UndoHistory::Snapshot UndoHistory::capture() const
{
    Snapshot s;
    s.values.reserve ((size_t) params.size());
    for (auto* prm : params)
        s.values.push_back (prm->getValue());
    s.extra = processor.getUndoObject();
    return s;
}

bool UndoHistory::same (const Snapshot& a, const Snapshot& b)
{
    if (a.extra != b.extra || a.values.size() != b.values.size())
        return false;
    for (size_t i = 0; i < a.values.size(); ++i)
        if (std::abs (a.values[i] - b.values[i]) > 1.0e-5f)
            return false;
    return true;
}

juce::String UndoHistory::describe (const Snapshot& from, const Snapshot& to) const
{
    if (from.extra != to.extra)
        return "New sound";
    int changed = 0, last = -1;
    for (size_t i = 0; i < from.values.size() && i < to.values.size(); ++i)
        if (std::abs (from.values[i] - to.values[i]) > 1.0e-5f) { ++changed; last = (int) i; }
    if (changed == 1)
        return params[last]->getName (40);
    return juce::String (changed) + " changes";
}

void UndoHistory::push (Snapshot s)
{
    if (index >= 0 && index + 1 < (int) states.size())
    {
        states.erase (states.begin() + index + 1, states.end());   // a new edit drops the redo steps
        labels.erase (labels.begin() + index + 1, labels.end());
    }
    labels.push_back (index >= 0 ? describe (states[(size_t) index], s) : juce::String());
    states.push_back (std::move (s));
    // keep at most maxSteps, and at most 8 different sounds (a long sample can be tens of MB)
    auto distinctSounds = [this]
    {
        juce::Array<juce::ReferenceCountedObject*> seen;
        for (auto& st : states) seen.addIfNotAlreadyThere (st.extra.get());
        return seen.size();
    };
    while ((int) states.size() > maxSteps + 1 || (states.size() > 1 && distinctSounds() > 8))
    {
        states.erase (states.begin());
        labels.erase (labels.begin());
    }
    index = (int) states.size() - 1;
    sendChangeMessage();
}

void UndoHistory::timerCallback()
{
    if (resetRequested.exchange (false))
    {
        reset();
        return;
    }
    auto now = capture();
    // settled = unchanged since the last look and nobody is mid-drag
    // (a drag whose end we never heard about still counts after ~3 s without movement)
    const bool still = havePoll && same (now, lastPoll);
    stillTicks = still ? stillTicks + 1 : 0;
    const bool settled = still && (gestures.load() <= 0 || stillTicks >= 15);
    lastPoll = now;
    havePoll = true;
    if (index < 0)
    {
        push (std::move (now));   // the starting point
        return;
    }
    if (settled && ! same (now, states[(size_t) index]))
        push (std::move (now));
}

void UndoHistory::commitNow()
{
    auto now = capture();
    lastPoll = now;
    havePoll = true;
    if (index < 0 || ! same (now, states[(size_t) index]))
        push (std::move (now));
}

void UndoHistory::reset()
{
    states.clear();
    labels.clear();
    index = -1;
    havePoll = false;
    gestures = 0;
    commitNow();
}

bool UndoHistory::canUndo() const
{
    return index > 0 || (index >= 0 && ! same (capture(), states[(size_t) index]));
}

bool UndoHistory::canRedo() const
{
    return index >= 0 && index + 1 < (int) states.size();
}

juce::String UndoHistory::getUndoDescription() const
{
    if (index < 0) return {};
    const auto now = capture();
    if (! same (now, states[(size_t) index])) return describe (states[(size_t) index], now);
    return index > 0 ? labels[(size_t) index] : juce::String();
}

juce::String UndoHistory::getRedoDescription() const
{
    return canRedo() ? labels[(size_t) index + 1] : juce::String();
}

void UndoHistory::apply (const Snapshot& s)
{
    processor.restoreUndoObject (s.extra);
    for (int i = 0; i < params.size() && i < (int) s.values.size(); ++i)
    {
        auto* prm = params[i];
        if (std::abs (prm->getValue() - s.values[(size_t) i]) > 1.0e-6f)
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (s.values[(size_t) i]);
            prm->endChangeGesture();
        }
    }
    lastPoll = capture();
    havePoll = true;
    processor.sendChangeMessage();
    sendChangeMessage();
}

void UndoHistory::undo()
{
    commitNow();   // an edit made in the last moment becomes its own step first
    if (index <= 0)
        return;
    --index;
    apply (states[(size_t) index]);
}

void UndoHistory::redo()
{
    if (! canRedo())
        return;
    ++index;
    apply (states[(size_t) index]);
}
} // namespace spark

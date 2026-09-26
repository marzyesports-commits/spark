#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace spark
{
class SparkProcessorBase;

// Undo/redo for everything you can edit: every parameter (facets, envelopes, filter, modulation,
// effects...) plus the loaded sound. A step is recorded once an edit settles: a whole knob drag,
// a Spark, a Breed, a preset change or a sample drop each become one step.
// Message thread only.
class UndoHistory : public juce::ChangeBroadcaster,
                    private juce::Timer,
                    private juce::AudioProcessorParameter::Listener
{
public:
    explicit UndoHistory (SparkProcessorBase&);
    ~UndoHistory() override;

    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    juce::String getUndoDescription() const;   // e.g. "Tone", "Spark (5 changes)", "New sound"
    juce::String getRedoDescription() const;

    void commitNow();    // record any settled edit right away (the timer does this on its own)
    void reset();        // forget the history (message thread)
    void requestReset() { resetRequested = true; }   // any thread: reset on the next tick

    static constexpr int maxSteps = 200;

private:
    struct Snapshot
    {
        std::vector<float> values;
        juce::ReferenceCountedObjectPtr<juce::ReferenceCountedObject> extra;
    };

    void timerCallback() override;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool starting) override { gestures += starting ? 1 : -1; }

    Snapshot capture() const;
    static bool same (const Snapshot&, const Snapshot&);
    void apply (const Snapshot&);
    juce::String describe (const Snapshot& from, const Snapshot& to) const;
    void push (Snapshot);

    SparkProcessorBase& processor;
    juce::Array<juce::AudioProcessorParameter*> params;
    std::vector<Snapshot> states;
    std::vector<juce::String> labels;   // labels[i] describes the step from states[i-1] to states[i]
    int index = -1;
    Snapshot lastPoll;
    bool havePoll = false;
    int stillTicks = 0;
    std::atomic<int> gestures { 0 };
    std::atomic<bool> resetRequested { false };
};
} // namespace spark

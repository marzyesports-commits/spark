#pragma once

#include "Common/Components.h"
#include "InstrumentProcessor.h"

namespace spark
{
// One effect: title, on switch, lock, and its knobs.
class ModuleCard : public juce::Component,
                   private juce::ChangeListener
{
public:
    ModuleCard (InstrumentProcessor&, const FxRack::ModuleInfo&);
    ~ModuleCard() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    bool isOn() const;

    InstrumentProcessor& processor;
    FxRack::ModuleInfo info;
    PillButton onButton { "OFF", PillButton::Style::lockToggle };
    PillButton lockButton { {}, PillButton::Style::lockToggle, Icon::unlock };
    juce::ParameterAttachment onAttachment;
    juce::OwnedArray<ArcKnob> knobs;
};

// The output card: master level.
class OutputCard : public juce::Component
{
public:
    explicit OutputCard (InstrumentProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ArcKnob level;
};

// The FX page: the whole rack, chain presets and a Spark button just for the effects.
class FxPage : public juce::Component,
               private juce::ChangeListener
{
public:
    explicit FxPage (InstrumentProcessor&);
    ~FxPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showChainMenu();

    InstrumentProcessor& processor;
    juce::OwnedArray<ModuleCard> cards;
    OutputCard output;
    PillButton chainButton { "Chain", PillButton::Style::outline, Icon::chevronDown };
    PillButton sparkFx { "SPARK FX", PillButton::Style::goldSolid };
    PillButton allOff { "ALL OFF", PillButton::Style::outline };
};
} // namespace spark

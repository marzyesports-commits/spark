#pragma once

#include "Common/Components.h"
#include "InstrumentProcessor.h"

namespace spark
{
// A sliding power switch: gold with "ON" when the effect runs, grey with "OFF" when it doesn't.
class PowerSwitch : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    PowerSwitch() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setOn (bool);
    bool isShownOn() const noexcept { return on; }
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }

private:
    bool on = false;
};

// One effect: power switch, title, lock, and its knobs.
class ModuleCard : public juce::Component,
                   private juce::ChangeListener
{
public:
    ModuleCard (InstrumentProcessor&, const FxRack::ModuleInfo&);
    ~ModuleCard() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;   // clicking the title toggles too

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    bool isOn() const;

    InstrumentProcessor& processor;
    FxRack::ModuleInfo info;
    PowerSwitch onButton;
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

// The signal chain at a glance: every effect in order, lit when it's on. Click one to toggle it.
class ChainStrip : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    explicit ChainStrip (InstrumentProcessor&);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }

private:
    std::vector<juce::Rectangle<float>> pillBounds() const;
    InstrumentProcessor& processor;
    int hovered = -1;
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
    ChainStrip chain;
    PillButton chainButton { "Chain", PillButton::Style::outline, Icon::chevronDown };
    PillButton sparkFx { "SPARK FX", PillButton::Style::goldSolid };
    PillButton allOff { "ALL OFF", PillButton::Style::outline };
};
} // namespace spark

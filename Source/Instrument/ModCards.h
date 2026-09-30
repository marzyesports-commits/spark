#pragma once

#include "Common/Components.h"
#include "Common/Cards.h"
#include "ModHost.h"

namespace spark
{
// The little handle you drag from a modulation source onto a facet or knob.
class ModGrip : public juce::Component,
                public juce::SettableTooltipClient
{
public:
    explicit ModGrip (int source);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;

private:
    int source;
    bool hover = false;
};

class MacroCard : public SynthCard
{
public:
    explicit MacroCard (ModHost&);
    void resized() override;

private:
    juce::OwnedArray<ModGrip> grips;
};

class LfoCard : public juce::Component,
                private juce::Timer
{
public:
    LfoCard (ModHost&, int lfo);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void showShapeMenu();
    juce::Rectangle<float> waveArea() const;

    ModHost& processor;
    int lfo;
    ModGrip grip;
    PillButton shapeButton { "Sine", PillButton::Style::outline, Icon::chevronDown };
    PillButton syncButton { "SYNC", PillButton::Style::lockToggle };
    PillButton retrigButton { "RETRIG", PillButton::Style::lockToggle };
    ArcKnob rate, division;
    juce::ParameterAttachment shapeAttachment, syncAttachment, retrigAttachment;
    float shownPhase = -1.0f;
};

// One routing: SOURCE -> DESTINATION, amount bar, clear.
class MatrixRow : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    MatrixRow (ModHost&, int slot);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> srcArea() const, dstArea() const, amountArea() const, clearArea() const;
    int source() const, dest() const;
    void showAddMenu();

    ModHost& processor;
    int slot;
    juce::ParameterAttachment srcAttachment, dstAttachment, amtAttachment;
    bool draggingAmount = false;
};

class MatrixCard : public juce::Component,
                   private juce::ChangeListener
{
public:
    explicit MatrixCard (ModHost&);
    ~MatrixCard() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    ModHost& processor;
    juce::OwnedArray<MatrixRow> rows;
    PillButton lockButton { {}, PillButton::Style::lockToggle, Icon::unlock };
};

} // namespace spark

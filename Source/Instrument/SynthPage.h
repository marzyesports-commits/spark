#pragma once

#include "Common/Components.h"
#include "InstrumentProcessor.h"

namespace spark
{
// A row of segment buttons bound to a choice parameter (e.g. LP | HP | BP | NOTCH).
class ChoiceSegments : public juce::Component
{
public:
    ChoiceSegments (juce::RangedAudioParameter&, const juce::StringArray& labels, const juce::StringArray& tooltips);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onChange;

private:
    void refresh();

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::OwnedArray<PillButton> buttons;
};

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

// A titled card of knobs on the SYNTH page.
class SynthCard : public juce::Component
{
public:
    SynthCard (const juce::String& title, const juce::String& blurb);
    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    ArcKnob& addKnob (juce::RangedAudioParameter&, const juce::String& label, const juce::String& tooltip);
    ChoiceSegments* segments = nullptr;   // optional, shown under the title
    juce::OwnedArray<ArcKnob> knobs;
    juce::String title, blurb;
};

class FilterCard : public SynthCard
{
public:
    FilterCard (InstrumentProcessor&, ModDropHandler);

private:
    ChoiceSegments type;
};

class LayersCard : public SynthCard
{
public:
    explicit LayersCard (InstrumentProcessor&);
};

class PlayCard : public SynthCard
{
public:
    explicit PlayCard (InstrumentProcessor&);

private:
    InstrumentProcessor& processor;
    ChoiceSegments mode;
};

class MacroCard : public SynthCard
{
public:
    explicit MacroCard (InstrumentProcessor&);
    void resized() override;

private:
    juce::OwnedArray<ModGrip> grips;
};

class LfoCard : public juce::Component,
                private juce::Timer
{
public:
    LfoCard (InstrumentProcessor&, int lfo);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void showShapeMenu();
    juce::Rectangle<float> waveArea() const;

    InstrumentProcessor& processor;
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
    MatrixRow (InstrumentProcessor&, int slot);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> srcArea() const, dstArea() const, amountArea() const, clearArea() const;
    int source() const, dest() const;
    void showAddMenu();

    InstrumentProcessor& processor;
    int slot;
    juce::ParameterAttachment srcAttachment, dstAttachment, amtAttachment;
    bool draggingAmount = false;
};

class MatrixCard : public juce::Component,
                   private juce::ChangeListener
{
public:
    explicit MatrixCard (InstrumentProcessor&);
    ~MatrixCard() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    InstrumentProcessor& processor;
    juce::OwnedArray<MatrixRow> rows;
    PillButton lockButton { {}, PillButton::Style::lockToggle, Icon::unlock };
};

// The SYNTH page: filter, voice mode, and modulation.
class SynthPage : public juce::Component
{
public:
    SynthPage (InstrumentProcessor&, ModDropHandler onModDrop);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FilterCard filter;
    LayersCard layers;
    PlayCard play;
    MacroCard macros;
    LfoCard lfo1, lfo2;
    MatrixCard matrix;
};
} // namespace spark

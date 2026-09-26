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

private:
    juce::String title, blurb;
    juce::OwnedArray<ArcKnob> knobs;
};

class FilterCard : public SynthCard
{
public:
    explicit FilterCard (InstrumentProcessor&);

private:
    ChoiceSegments type;
};

class PlayCard : public SynthCard
{
public:
    explicit PlayCard (InstrumentProcessor&);

private:
    InstrumentProcessor& processor;
    ChoiceSegments mode;
};

// The SYNTH page: how notes play and how the filter shapes them.
class SynthPage : public juce::Component
{
public:
    explicit SynthPage (InstrumentProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FilterCard filter;
    PlayCard play;
};
} // namespace spark

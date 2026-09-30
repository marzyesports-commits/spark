#pragma once

#include "Components.h"

namespace spark
{
// Rounded card background with a title on the left and an optional blurb on the right.
void drawCard (juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& title, const juce::String& blurb, float titleWidth);

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
    juce::OwnedArray<ArcKnob> knobs;
    juce::String title, blurb;
};

} // namespace spark

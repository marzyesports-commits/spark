#include "Cards.h"

namespace spark
{
void drawCard (juce::Graphics& g, juce::Rectangle<float> b, const juce::String& title, const juce::String& blurb, float titleWidth)
{
    using namespace colours;
    g.setColour (panel);
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
    g.setColour (text);
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.12f));
    g.drawText (title, juce::Rectangle<float> (14.0f, 10.0f, titleWidth, 26.0f), juce::Justification::centredLeft, false);
    if (blurb.isNotEmpty())
    {
        g.setColour (muted);
        g.setFont (fonts::body (11.0f));
        g.drawText (blurb, juce::Rectangle<float> (14.0f + titleWidth, 10.0f, b.getWidth() - titleWidth - 28.0f, 26.0f), juce::Justification::centredRight, true);
    }
}


// =====================================================================================
ChoiceSegments::ChoiceSegments (juce::RangedAudioParameter& p, const juce::StringArray& labels, const juce::StringArray& tooltips)
    : param (p), attachment (p, [this] (float) { refresh(); })
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new PillButton (labels[i], PillButton::Style::segment));
        b->setFontHeight (10.0f);
        b->setLetterSpacing (0.12f);
        b->setTooltip (tooltips[i]);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void ChoiceSegments::refresh()
{
    const int index = juce::roundToInt (param.convertFrom0to1 (param.getValue()));
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
    if (onChange) onChange();
}

void ChoiceSegments::resized()
{
    auto b = getLocalBounds().reduced (3);
    const int w = b.getWidth() / juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i == buttons.size() - 1 ? b : b.removeFromLeft (w));
}

void ChoiceSegments::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
}

// =====================================================================================
SynthCard::SynthCard (const juce::String& t, const juce::String& b) : title (t), blurb (b) {}

ArcKnob& SynthCard::addKnob (juce::RangedAudioParameter& p, const juce::String& label, const juce::String& tooltip)
{
    auto* k = knobs.add (new ArcKnob (p, label));
    k->setTooltip (tooltip);
    addAndMakeVisible (k);
    return *k;
}

void SynthCard::resized()
{
    int top = 44;
    if (segments != nullptr)
    {
        segments->setBounds (12, 42, getWidth() - 24, 30);
        top = 80;
    }
    const int n = knobs.size();
    const int w = (getWidth() - 16) / juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (8 + i * w, top, w, getHeight() - top - 6);
}

void SynthCard::paint (juce::Graphics& g)
{
    // with no blurb the title can use the whole width (narrow cards like OUTPUT)
    drawCard (g, getLocalBounds().toFloat(), title, blurb, blurb.isEmpty() ? (float) getWidth() - 28.0f : (float) getWidth() * 0.4f);
}

} // namespace spark

#include "SynthPage.h"

namespace spark
{
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
        segments->setBounds (12, 44, getWidth() - 24, 30);
        top = 84;
    }
    const int n = knobs.size();
    const int w = (getWidth() - 16) / juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (8 + i * w, top, w, getHeight() - top - 8);
}

void SynthCard::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
    g.setColour (text);
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.12f));
    g.drawText (title, juce::Rectangle<float> (14.0f, 10.0f, b.getWidth() * 0.4f, 26.0f), juce::Justification::centredLeft, false);
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText (blurb, juce::Rectangle<float> (b.getWidth() * 0.4f, 10.0f, b.getWidth() * 0.6f - 14.0f, 26.0f), juce::Justification::centredRight, true);
}

// =====================================================================================
FilterCard::FilterCard (InstrumentProcessor& p)
    : SynthCard ("FILTER", "Cutoff is the Tone facet"),
      type (*p.apvts.getParameter ("filterType"), { "LOW", "HIGH", "BAND", "NOTCH" },
            { "Low-pass: keeps the lows, darkens as you close it",
              "High-pass: removes the lows, thins the sound out",
              "Band-pass: keeps a band around the cutoff",
              "Notch: cuts a band around the cutoff" })
{
    segments = &type;
    addAndMakeVisible (type);
    addKnob (*p.apvts.getParameter ("tone"), "CUTOFF", "Filter cutoff (the Tone facet)");
    addKnob (*p.apvts.getParameter ("resonance"), "RES", "Resonance: a peak at the cutoff. High settings whistle");
    addKnob (*p.apvts.getParameter ("keyTrack"), "KEY", "Key tracking: higher notes open the filter more. 100% follows the keyboard exactly");
    addKnob (*p.apvts.getParameter ("toneAmount"), "ENV", "How far the Tone envelope sweeps the cutoff, in octaves (edit its shape on the SOUND page)");
}

// =====================================================================================
PlayCard::PlayCard (InstrumentProcessor& p)
    : SynthCard ("PLAY", "How keys become notes"),
      processor (p),
      mode (*p.apvts.getParameter ("voiceMode"), { "POLY", "MONO", "LEGATO" },
            { "Poly: every key plays its own note",
              "Mono: one note at a time, each key restarts the envelopes",
              "Legato: one note at a time, overlapping keys slide without restarting" })
{
    segments = &mode;
    addAndMakeVisible (mode);
    auto& glide = addKnob (*p.apvts.getParameter ("glide"), "GLIDE", "Glide time between notes, in Mono and Legato");
    glide.isActive = [this] { return juce::roundToInt (processor.params.voiceMode->load()) != InstrumentProcessor::poly; };
    mode.onChange = [&glide] { glide.repaint(); };
    addKnob (*p.apvts.getParameter ("bendRange"), "BEND", "Pitch-bend range in semitones");
    addKnob (*p.apvts.getParameter ("ampVelocity"), "VELOCITY", "How much playing harder makes notes louder");
}

// =====================================================================================
SynthPage::SynthPage (InstrumentProcessor& p) : filter (p), play (p)
{
    addAndMakeVisible (filter);
    addAndMakeVisible (play);
}

void SynthPage::resized()
{
    const int gap = 12, top = 64;
    const int w = (getWidth() - 32 - 3 * gap) / 4;
    const int h = (getHeight() - top - 16 - gap) / 2;
    // Top row: FILTER and PLAY take two columns each
    filter.setBounds (16, top, 2 * w + gap, h);
    play.setBounds (filter.getRight() + gap, top, 2 * w + gap, h);
}

void SynthPage::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);
    drawSectionLabel (g, "SYNTH", { 20.0f, 16.0f, 120.0f, 32.0f });
    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    g.drawText ("How the filter shapes your sound and how the keys play it",
                juce::Rectangle<float> (110.0f, 16.0f, b.getWidth() - 130.0f, 32.0f), juce::Justification::centredLeft, true);
}
} // namespace spark

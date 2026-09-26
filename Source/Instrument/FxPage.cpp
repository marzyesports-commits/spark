#include "FxPage.h"

namespace spark
{
// =====================================================================================
ModuleCard::ModuleCard (InstrumentProcessor& p, const FxRack::ModuleInfo& m)
    : processor (p), info (m),
      onAttachment (*p.apvts.getParameter (m.onParam), [this] (float) { refresh(); })
{
    addAndMakeVisible (onButton);
    addAndMakeVisible (lockButton);
    onButton.setFontHeight (10.0f);
    onButton.setTooltip ("Switch " + m.name.toLowerCase() + " on or off");
    lockButton.setTooltip ("Lock: Spark and Breed leave this effect alone");
    onButton.onClick = [this] { onAttachment.setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f); };
    lockButton.onClick = [this] { processor.setModuleLocked (info.id, ! processor.isModuleLocked (info.id)); };

    for (int i = 0; i < m.params.size(); ++i)
    {
        auto* k = knobs.add (new ArcKnob (*p.apvts.getParameter (m.params[i]), m.labels[i]));
        k->isActive = [this] { return isOn(); };
        addAndMakeVisible (k);
    }
    processor.addChangeListener (this);
    onAttachment.sendInitialUpdate();
}

ModuleCard::~ModuleCard() { processor.removeChangeListener (this); }

bool ModuleCard::isOn() const
{
    return processor.apvts.getRawParameterValue (info.onParam)->load() > 0.5f;
}

void ModuleCard::refresh()
{
    const bool on = isOn();
    onButton.setToggleState (on, juce::dontSendNotification);
    onButton.setButtonText (on ? "ON" : "OFF");
    const bool locked = processor.isModuleLocked (info.id);
    lockButton.setToggleState (locked, juce::dontSendNotification);
    lockButton.setIcon (locked ? Icon::lock : Icon::unlock);
    for (auto* k : knobs) k->repaint();
    repaint();
}

void ModuleCard::resized()
{
    onButton.setBounds (getWidth() - 12 - 50, 12, 50, 26);
    lockButton.setBounds (onButton.getX() - 6 - 26, 12, 26, 26);
    const int n = knobs.size();
    const int area = getWidth() - 16;
    const int w = area / juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (8 + i * w, 66, w, getHeight() - 74);
}

void ModuleCard::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    const bool on = isOn();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (on ? gold.withAlpha (0.55f) : line);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);

    g.setColour (on ? text : text2);
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.12f));
    g.drawText (info.name, juce::Rectangle<float> (14.0f, 12.0f, (float) lockButton.getX() - 18.0f, 26.0f), juce::Justification::centredLeft, true);
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText (info.blurb, juce::Rectangle<float> (14.0f, 38.0f, b.getWidth() - 28.0f, 18.0f), juce::Justification::centredLeft, true);
}

// =====================================================================================
OutputCard::OutputCard (InstrumentProcessor& p)
    : level (*p.apvts.getParameter ("level"), "LEVEL")
{
    addAndMakeVisible (level);
}

void OutputCard::resized()
{
    level.setBounds (getWidth() / 2 - 40, 66, 80, getHeight() - 74);
}

void OutputCard::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
    g.setColour (text);
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.12f));
    g.drawText ("OUTPUT", juce::Rectangle<float> (14.0f, 12.0f, 150.0f, 26.0f), juce::Justification::centredLeft, false);
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText ("Final level, with a safety limiter", juce::Rectangle<float> (14.0f, 38.0f, b.getWidth() - 28.0f, 18.0f), juce::Justification::centredLeft, true);
}

// =====================================================================================
FxPage::FxPage (InstrumentProcessor& p) : processor (p), output (p)
{
    for (const auto& m : FxRack::modules())
        addAndMakeVisible (cards.add (new ModuleCard (p, m)));
    addAndMakeVisible (output);
    for (auto* b : { &chainButton, &sparkFx, &allOff })
        addAndMakeVisible (b);

    chainButton.setTooltip ("Effect chains: ready-made rack settings");
    sparkFx.setTooltip ("Roll new settings for the effects that are on (locked ones stay put)");
    allOff.setTooltip ("Switch every effect off (the Space reverb stays)");
    chainButton.onClick = [this] { showChainMenu(); };
    sparkFx.onClick = [this] { processor.sparkEffects(); };
    allOff.onClick = [this]
    {
        for (const auto& m : FxRack::modules())
            if (m.id != "reverb")
                if (auto* prm = processor.apvts.getParameter (m.onParam))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (0.0f);
                    prm->endChangeGesture();
                }
    };
    processor.addChangeListener (this);
    changeListenerCallback (nullptr);
}

FxPage::~FxPage() { processor.removeChangeListener (this); }

void FxPage::changeListenerCallback (juce::ChangeBroadcaster*)
{
    chainButton.setButtonText ("CHAIN: " + processor.getChainName().toUpperCase());
    repaint();
}

void FxPage::showChainMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    const auto& chains = FxRack::chains();
    for (int i = 0; i < (int) chains.size(); ++i)
        m.addItem (chains[(size_t) i].name + "   " + juce::String::fromUTF8 ("\xc2\xb7") + "   " + chains[(size_t) i].hint, true,
                   chains[(size_t) i].name == processor.getChainName(), [this, i] { processor.loadChain (i); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chainButton));
}

void FxPage::resized()
{
    allOff.setBounds (getWidth() - 16 - 88, 16, 88, 32);
    sparkFx.setBounds (allOff.getX() - 8 - 116, 16, 116, 32);
    chainButton.setBounds (sparkFx.getX() - 8 - 250, 16, 250, 32);

    const int gap = 12, top = 64;
    const int w = (getWidth() - 32 - 3 * gap) / 4;
    const int h = (getHeight() - top - 16 - gap) / 2;
    for (int i = 0; i < 8; ++i)
    {
        juce::Rectangle<int> r (16 + (i % 4) * (w + gap), top + (i / 4) * (h + gap), w, h);
        if (i < cards.size()) cards[i]->setBounds (r);
        else output.setBounds (r);
    }
}

void FxPage::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);
    drawSectionLabel (g, "EFFECTS", { 20.0f, 16.0f, 120.0f, 32.0f });
    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    g.drawText (juce::String::fromUTF8 ("Signal flows left to right, top to bottom \xc2\xb7 Spark and Breed also move effects that are on"),
                juce::Rectangle<float> (110.0f, 16.0f, (float) chainButton.getX() - 120.0f, 32.0f), juce::Justification::centredLeft, true);
}
} // namespace spark

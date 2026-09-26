#include "FxPage.h"

namespace spark
{
// =====================================================================================
void PowerSwitch::setOn (bool o)
{
    if (o != on) { on = o; repaint(); }
}

void PowerSwitch::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    auto track = b.removeFromLeft (40.0f).withSizeKeepingCentre (40.0f, 22.0f);
    g.setColour (on ? gold : raised);
    g.fillRoundedRectangle (track, 11.0f);
    if (! on)
    {
        g.setColour (line2);
        g.drawRoundedRectangle (track.reduced (0.5f), 11.0f, 1.0f);
    }
    const auto knob = juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ on ? track.getRight() - 11.0f : track.getX() + 11.0f, track.getCentreY() });
    g.setColour (on ? bg : muted);
    g.fillEllipse (knob);
    g.setColour (on ? gold : muted);
    g.setFont (fonts::mono (10.5f).withExtraKerningFactor (0.1f));
    g.drawText (on ? "ON" : "OFF", b.withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
}

// =====================================================================================
ChainStrip::ChainStrip (InstrumentProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("The signal chain: lit effects are on. Click one to switch it on or off");
}

std::vector<juce::Rectangle<float>> ChainStrip::pillBounds() const
{
    static const char* shortNames[] = { "DIST", "EQ", "CHORUS", "GRAINS", "STUTTER", "DELAY", "REVERB" };
    const auto font = fonts::mono (9.5f).withExtraKerningFactor (0.08f);
    std::vector<float> widths;
    float total = 0.0f;
    for (auto* n : shortNames) { widths.push_back (juce::GlyphArrangement::getStringWidth (font, n) + 16.0f); total += widths.back(); }
    const float arrow = juce::jmax (8.0f, ((float) getWidth() - total) / 6.0f);
    std::vector<juce::Rectangle<float>> out;
    float x = 0.0f;
    for (auto w : widths)
    {
        out.push_back ({ x, (float) getHeight() * 0.5f - 11.0f, w, 22.0f });
        x += w + arrow;
    }
    return out;
}

void ChainStrip::paint (juce::Graphics& g)
{
    using namespace colours;
    static const char* shortNames[] = { "DIST", "EQ", "CHORUS", "GRAINS", "STUTTER", "DELAY", "REVERB" };
    const auto pills = pillBounds();
    const auto& mods = FxRack::modules();
    for (size_t i = 0; i < pills.size() && i < mods.size(); ++i)
    {
        const bool on = processor.apvts.getRawParameterValue (mods[i].onParam)->load() > 0.5f;
        auto r = pills[i];
        if (i + 1 < pills.size())
        {
            const float x0 = r.getRight() + 2.0f, x1 = pills[i + 1].getX() - 2.0f, y = r.getCentreY();
            g.setColour (line2);
            g.drawLine (x0, y, x1, y, 1.0f);
            juce::Path head;
            head.addTriangle (x1, y, x1 - 4.0f, y - 3.0f, x1 - 4.0f, y + 3.0f);
            g.fillPath (head);
        }
        g.setColour (on ? gold : (hovered == (int) i ? raised : panel));
        g.fillRoundedRectangle (r, 11.0f);
        if (! on)
        {
            g.setColour (hovered == (int) i ? muted : line2);
            g.drawRoundedRectangle (r.reduced (0.5f), 11.0f, 1.0f);
        }
        g.setColour (on ? bg : (hovered == (int) i ? text2 : ember));
        g.setFont (fonts::mono (9.5f).withExtraKerningFactor (0.08f));
        g.drawText (shortNames[i], r, juce::Justification::centred, false);
    }
}

void ChainStrip::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    const auto pills = pillBounds();
    for (size_t i = 0; i < pills.size(); ++i)
        if (pills[i].contains (e.position)) h = (int) i;
    if (h != hovered) { hovered = h; repaint(); }
}

void ChainStrip::mouseUp (const juce::MouseEvent& e)
{
    const auto pills = pillBounds();
    const auto& mods = FxRack::modules();
    for (size_t i = 0; i < pills.size() && i < mods.size(); ++i)
        if (pills[i].contains (e.position))
            if (auto* prm = processor.apvts.getParameter (mods[i].onParam))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
                prm->endChangeGesture();
            }
}

// =====================================================================================
ModuleCard::ModuleCard (InstrumentProcessor& p, const FxRack::ModuleInfo& m)
    : processor (p), info (m),
      onAttachment (*p.apvts.getParameter (m.onParam), [this] (float) { refresh(); })
{
    addAndMakeVisible (onButton);
    addAndMakeVisible (lockButton);
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
    onButton.setOn (on);
    for (auto* k : knobs) k->setAlpha (on ? 1.0f : 0.4f);
    if (auto* page = getParentComponent()) page->repaint();   // the chain strip follows
    const bool locked = processor.isModuleLocked (info.id);
    lockButton.setToggleState (locked, juce::dontSendNotification);
    lockButton.setIcon (locked ? Icon::lock : Icon::unlock);
    for (auto* k : knobs) k->repaint();
    repaint();
}

void ModuleCard::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && e.position.y < 40.0f && e.position.x < (float) lockButton.getX())
        onAttachment.setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
}

void ModuleCard::resized()
{
    onButton.setBounds (getWidth() - 12 - 72, 12, 72, 26);
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
    // On: lit card with a gold edge and accent stripe. Off: dark, flat, faded.
    g.setColour (on ? panel.interpolatedWith (gold, 0.07f) : bg);
    g.fillRoundedRectangle (b, 14.0f);
    if (on)
    {
        g.setColour (gold);
        g.drawRoundedRectangle (b.reduced (0.75f), 14.0f, 1.5f);
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 18.0f, 3.0f, b.getHeight() - 36.0f), 1.5f);
    }
    else
    {
        g.setColour (line);
        g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);
    }

    g.setColour (on ? text : ember);
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.12f));
    g.drawFittedText (info.name, juce::Rectangle<float> (14.0f, 12.0f, (float) lockButton.getX() - 20.0f, 26.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.7f);
    g.setColour (on ? text2 : ember);
    g.setFont (fonts::body (11.0f));
    g.drawText (on ? info.blurb : juce::String ("Off, click to switch on"), juce::Rectangle<float> (14.0f, 38.0f, b.getWidth() - 28.0f, 18.0f), juce::Justification::centredLeft, true);
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
FxPage::FxPage (InstrumentProcessor& p) : processor (p), output (p), chain (p)
{
    addAndMakeVisible (chain);
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
    allOff.setBounds (getWidth() - 16 - 80, 16, 80, 32);
    sparkFx.setBounds (allOff.getX() - 8 - 100, 16, 100, 32);
    chainButton.setBounds (sparkFx.getX() - 8 - 196, 16, 196, 32);
    chain.setBounds (112, 16, chainButton.getX() - 124, 32);

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
}
} // namespace spark

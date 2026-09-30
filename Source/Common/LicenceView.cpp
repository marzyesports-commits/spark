#include "LicenceView.h"

namespace spark
{
// =====================================================================================
LicenceFooter::LicenceFooter (Licence& l) : licence (l)
{
    licence.addChangeListener (this);
    startTimer (60000);   // the day count moves on its own
    update();
}

LicenceFooter::~LicenceFooter()
{
    licence.removeChangeListener (this);
}

void LicenceFooter::update()
{
    const auto s = licence.getState();
    setVisible (s != Licence::State::disabled && s != Licence::State::active);
    repaint();
}

juce::Rectangle<float> LicenceFooter::linkArea() const
{
    const auto text = licence.statusLine();
    const float w = juce::GlyphArrangement::getStringWidth (fonts::label (11.0f), text);
    return { w + 18.0f, 0.0f, 96.0f, (float) getHeight() };
}

void LicenceFooter::paint (juce::Graphics& g)
{
    const auto s = licence.getState();
    const bool urgent = s == Licence::State::trialOver || s == Licence::State::needsRecheck;
    auto r = getLocalBounds().toFloat();
    g.setFont (fonts::label (11.0f));
    g.setColour (urgent ? colours::goldHi : colours::muted);
    g.drawText (licence.statusLine(), r, juce::Justification::centredLeft, false);
    const auto link = linkArea();
    g.setColour (hover ? colours::goldHi : colours::gold);
    g.drawText (s == Licence::State::needsRecheck ? "CHECK NOW" : "ACTIVATE", link, juce::Justification::centredLeft, false);
    g.fillRect (link.getX(), link.getBottom() - 4.0f,
                juce::GlyphArrangement::getStringWidth (fonts::label (11.0f), "ACTIVATE"), 1.0f);
}

void LicenceFooter::mouseMove (const juce::MouseEvent& e)
{
    const bool h = linkArea().contains (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void LicenceFooter::mouseUp (const juce::MouseEvent& e)
{
    if (linkArea().contains (e.position) && onActivate)
        onActivate();
}

// =====================================================================================
LicencePanel::LicencePanel (Licence& l) : licence (l)
{
    licence.addChangeListener (this);
    keyField.setFont (fonts::mono (16.0f));
    keyField.setJustification (juce::Justification::centred);
    keyField.setTextToShowWhenEmpty ("XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX", colours::ember);
    keyField.setColour (juce::TextEditor::backgroundColourId, colours::bg);
    keyField.setColour (juce::TextEditor::textColourId, colours::text);
    keyField.setColour (juce::TextEditor::outlineColourId, colours::line2);
    keyField.setColour (juce::TextEditor::focusedOutlineColourId, colours::gold);
    keyField.setColour (juce::TextEditor::highlightColourId, colours::gold.withAlpha (0.35f));
    keyField.setColour (juce::CaretComponent::caretColourId, colours::goldHi);
    keyField.setIndents (12, 10);
    keyField.onReturnKey = [this] { submit(); };
    keyField.setTitle ("Licence key");
    addAndMakeVisible (keyField);

    activateButton.onClick = [this] { submit(); };
    buyButton.onClick = [this] { juce::URL (licence.getConfig().buyUrl).launchInDefaultBrowser(); };
    closeButton.onClick = [this] { setVisible (false); if (onClose) onClose(); };
    removeButton.onClick = [this]
    {
        licence.removeFromThisComputer();
        message = "Removed. You can activate the key again on this or another computer.";
        messageIsError = false;
        refresh();
    };
    for (auto* b : { &activateButton, &buyButton, &closeButton, &removeButton })
        addAndMakeVisible (b);
    setWantsKeyboardFocus (false);
    refresh();
}

LicencePanel::~LicencePanel()
{
    licence.removeChangeListener (this);
}

void LicencePanel::open()
{
    message = {};
    refresh();
    setVisible (true);
    toFront (true);
    if (keyField.isVisible())
        keyField.grabKeyboardFocus();
}

juce::Rectangle<int> LicencePanel::card() const
{
    return getLocalBounds().withSizeKeepingCentre (560, 330);
}

void LicencePanel::refresh()
{
    const bool active = licence.getState() == Licence::State::active;
    keyField.setVisible (! active);
    activateButton.setVisible (! active);
    buyButton.setVisible (! active && licence.getConfig().buyUrl.isNotEmpty());
    removeButton.setVisible (active);
    closeButton.setButtonText (active ? "DONE" : (licence.getState() == Licence::State::trialOver ? "CONTINUE IN DEMO" : "NOT NOW"));
    activateButton.setButtonText (licence.isBusy() ? "CHECKING..." : "ACTIVATE");
    activateButton.setEnabled (! licence.isBusy());
    resized();
    repaint();
}

void LicencePanel::submit()
{
    if (licence.isBusy()) return;
    message = "Checking your key with Gumroad...";
    messageIsError = false;
    licence.activate (keyField.getText(), [safe = juce::Component::SafePointer<LicencePanel> (this)] (bool ok, const juce::String& text)
    {
        if (safe == nullptr) return;
        safe->message = text;
        safe->messageIsError = ! ok;
        if (ok) safe->keyField.clear();
        safe->refresh();
    });
    refresh();
}

void LicencePanel::resized()
{
    auto c = card().reduced (32);
    c.removeFromTop (118);                       // title and text
    auto row = c.removeFromTop (48);
    keyField.setBounds (row);
    removeButton.setBounds (row.withSizeKeepingCentre (300, 40));
    c.removeFromTop (18);
    auto buttons = c.removeFromTop (40);
    activateButton.setBounds (buttons.removeFromLeft (170));
    buttons.removeFromLeft (12);
    if (buyButton.isVisible()) { buyButton.setBounds (buttons.removeFromLeft (110)); buttons.removeFromLeft (12); }
    closeButton.setBounds (buttons.removeFromRight (190));
    if (licence.getState() == Licence::State::active)
        closeButton.setBounds (card().reduced (32).removeFromBottom (40).withSizeKeepingCentre (140, 40));
}

void LicencePanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg.withAlpha (0.82f));
    const auto c = card().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (c, 18.0f);
    g.setColour (colours::gold.withAlpha (0.55f));
    g.drawRoundedRectangle (c.reduced (0.5f), 18.0f, 1.2f);

    auto area = c.reduced (32.0f);
    auto titleRow = area.removeFromTop (40.0f);
    auto gemArea = titleRow.removeFromLeft (34.0f).withSizeKeepingCentre (28.0f, 28.0f);
    g.setColour (colours::gold);
    g.fillPath (makeLogoPath (gemArea));
    titleRow.removeFromLeft (12.0f);
    const auto state = licence.getState();
    const auto name = licence.getConfig().productName;
    g.setColour (colours::text);
    g.setFont (fonts::display (24.0f));
    g.drawText (state == Licence::State::active ? name + " is activated" : "Activate " + name, titleRow, juce::Justification::centredLeft, false);

    juce::String body;
    switch (state)
    {
        case Licence::State::trial:
            body = "Your trial has " + juce::String (licence.trialDaysLeft()) + (licence.trialDaysLeft() == 1 ? " day" : " days")
                 + " left, with everything unlocked. Paste the licence key from your Gumroad receipt to keep it that way.";
            break;
        case Licence::State::trialOver:
            body = "Your trial has ended. " + name + " still works, but the sound drops out every 30 seconds and DRAG MIDI is locked "
                   "until you activate. Paste the licence key from your Gumroad receipt.";
            break;
        case Licence::State::needsRecheck:
            body = name + " hasn't been able to confirm your licence for a while. Connect to the internet and press ACTIVATE with your key.";
            break;
        case Licence::State::active:
            body = "Licensed to " + (licence.getEmail().isNotEmpty() ? licence.getEmail() : juce::String ("you"))
                 + " with key " + licence.getMaskedKey() + ". Thanks for supporting independent plugins.";
            break;
        case Licence::State::disabled: break;
    }
    area.removeFromTop (14.0f);
    g.setColour (colours::text2);
    g.setFont (fonts::body (15.0f));
    g.drawFittedText (body, area.removeFromTop (60.0f).toNearestInt(), juce::Justification::topLeft, 3, 1.0f);

    if (message.isNotEmpty())
    {
        auto msgArea = c.reduced (32.0f).removeFromBottom (26.0f);
        if (state != Licence::State::active)
            msgArea = c.reduced (32.0f).withTrimmedTop (118 + 48 + 18 + 40 + 10).removeFromTop (40.0f);
        else
            msgArea = c.reduced (32.0f).withTrimmedTop (118 + 48 + 8).removeFromTop (40.0f);
        g.setColour (messageIsError ? juce::Colour (0xffff8a7a) : colours::goldHi);
        g.setFont (fonts::body (13.5f));
        g.drawFittedText (message, msgArea.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
    }
}

void LicencePanel::mouseUp (const juce::MouseEvent& e)
{
    if (! card().contains (e.getPosition()))
    {
        setVisible (false);
        if (onClose) onClose();
    }
}
} // namespace spark

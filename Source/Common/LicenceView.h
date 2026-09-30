#pragma once

#include "Components.h"
#include "Licence.h"

namespace spark
{
// A single line along the bottom of the window: "TRIAL · 12 DAYS LEFT   ACTIVATE". Hidden once licensed.
class LicenceFooter : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit LicenceFooter (Licence&);
    ~LicenceFooter() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    std::function<void()> onActivate;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { update(); }
    void timerCallback() override { update(); }
    void update();
    juce::Rectangle<float> linkArea() const;
    Licence& licence;
    bool hover = false;
};

// The activation screen: a card over the dimmed window. Paste the key from the Gumroad receipt, press ACTIVATE.
class LicencePanel : public juce::Component,
                     private juce::ChangeListener
{
public:
    explicit LicencePanel (Licence&);
    ~LicencePanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;   // a click outside the card closes it
    void open();
    std::function<void()> onClose;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void submit();
    juce::Rectangle<int> card() const;

    Licence& licence;
    juce::TextEditor keyField;
    PillButton activateButton { "ACTIVATE", PillButton::Style::goldSolid };
    PillButton buyButton { "BUY", PillButton::Style::goldOutline };
    PillButton closeButton { "NOT NOW", PillButton::Style::ghost };
    PillButton removeButton { "REMOVE FROM THIS COMPUTER", PillButton::Style::outline };
    juce::String message;
    bool messageIsError = false;
};
} // namespace spark

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SparkLookAndFeel.h"
#include "SparkProcessorBase.h"

namespace spark
{
enum class Icon { none, chevronLeft, chevronRight, settings, undo, star, lock, unlock, upload, snowflake };
juce::Path makeIcon (Icon, juce::Rectangle<float> area);

// Rounded pill button in Spark's styles.
class PillButton : public juce::Button
{
public:
    enum class Style { outline, goldOutline, goldSolid, segment, lockToggle, ghost };

    PillButton (const juce::String& text, Style = Style::outline, Icon = Icon::none);
    void setIcon (Icon i, bool filled = false) { icon = i; iconFilled = filled; repaint(); }
    void setStyle (Style s) { style = s; repaint(); }
    void setFontHeight (float h) { fontHeight = h; repaint(); }
    void setLetterSpacing (float k) { kerning = k; repaint(); }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Style style;
    Icon icon;
    bool iconFilled = false;
    float fontHeight = 11.0f;
    float kerning = 0.12f;
};

// Top bar: logo, preset browser with generation counter, plugin kind, settings.
class Header : public juce::Component,
               private juce::ChangeListener
{
public:
    Header (SparkProcessorBase&, bool isFx);
    ~Header() override;
    std::function<void (juce::Component& anchor)> onSettings;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    SparkProcessorBase& processor;
    bool fx;
    PillButton prev { {}, PillButton::Style::ghost, Icon::chevronLeft };
    PillButton next { {}, PillButton::Style::ghost, Icon::chevronRight };
    PillButton settings { {}, PillButton::Style::outline, Icon::settings };
    juce::Rectangle<int> presetArea, kindArea;
};

// The round centrepiece: the live sound as a ring, eight facet arcs you can drag,
// and the SPARK button in the middle.
class CoreView : public juce::Component,
                 public juce::SettableTooltipClient,
                 private juce::Timer,
                 private juce::ChangeListener
{
public:
    explicit CoreView (SparkProcessorBase&);
    ~CoreView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    class SparkButton : public juce::Button
    {
    public:
        SparkButton() : juce::Button ("Spark") { setTitle ("Spark: roll a new variation"); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool hitTest (int x, int y) override;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    int facetAt (juce::Point<float>) const;
    juce::Path ringPath (const std::vector<float>& shape, float radius, float amp) const;
    void updateTooltip();

    SparkProcessorBase& processor;
    SparkButton sparkButton;
    std::vector<float> target, shown, ghost;
    float ghostAlpha = 0.0f;
    int hovered = -1, dragging = -1;
    float dragStartValue = 0.0f;
    juce::Point<float> centre;
    static constexpr int ringPoints = 360;
};

// Mutate / Chaos pill sliders.
class AmountSlider : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    AmountSlider (juce::RangedAudioParameter&, const juce::String& label, const juce::String& tooltip);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float value = 0.0f, dragStart = 0.0f;
};

// Right-hand facet list with value bars and per-facet locks.
class FacetList : public juce::Component,
                  private juce::Timer,
                  private juce::ChangeListener
{
public:
    explicit FacetList (SparkProcessorBase&);
    ~FacetList() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    juce::Rectangle<int> rowBounds (int i) const;
    juce::Rectangle<int> barBounds (int i) const;
    int rowAt (juce::Point<int>) const;
    void setFromX (int row, int x);

    SparkProcessorBase& processor;
    juce::OwnedArray<PillButton> lockButtons;
    int dragging = -1;
};

// Bottom strip: recent variations as glyphs; click to go back, Keep and Breed.
class LineageStrip : public juce::Component,
                     private juce::ChangeListener
{
public:
    explicit LineageStrip (SparkProcessorBase&);
    ~LineageStrip() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    int emberAt (juce::Point<int>) const;
    juce::Rectangle<int> emberBounds (int slot) const;
    int firstVisible() const;

    SparkProcessorBase& processor;
    PillButton keep { "KEEP", PillButton::Style::outline, Icon::star };
    PillButton breed { "BREED", PillButton::Style::goldOutline };
    int hovered = -1;
    static constexpr int maxVisible = 10;
};

// Base editor shared by both plugins: scales a fixed 1120x720 design to the window,
// and lays out the header, core, amounts, facets and lineage. Subclasses fill the left column.
class SparkEditorBase : public juce::AudioProcessorEditor
{
public:
    SparkEditorBase (SparkProcessorBase&, bool isFx);
    ~SparkEditorBase() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1120, designHeight = 720;

protected:
    // area for the left column inside the design (296 x 440)
    juce::Rectangle<int> leftColumn() const { return { 24, 86, 296, 440 }; }
    juce::Component& content() { return contentComponent; }
    void showSettingsMenu (juce::Component& anchor);
    virtual void addExtraMenuItems (juce::PopupMenu&) {}
    void showMessage (const juce::String& title, const juce::String& text);
    SparkLookAndFeel& getSparkLookAndFeel() noexcept { return lookAndFeel; }

    SparkProcessorBase& sparkProcessor;

private:
    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
    };

    SparkLookAndFeel lookAndFeel;
    Content contentComponent;
    Header header;
    CoreView core;
    AmountSlider mutate, chaos;
    FacetList facets;
    LineageStrip lineage;
    juce::TooltipWindow tooltips { nullptr, 600 };
};
} // namespace spark

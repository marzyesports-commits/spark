#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SparkLookAndFeel.h"
#include "SparkProcessorBase.h"

namespace spark
{
enum class Icon { none, chevronLeft, chevronRight, chevronDown, settings, undo, star, lock, unlock, upload, snowflake, cross, shuffle, search, expand };
juce::Path makeIcon (Icon, juce::Rectangle<float> area);

// Rounded pill button in Spark's styles.
// Modulation drags carry "mod:<source index>" as their description.
bool isModDrag (const juce::DragAndDropTarget::SourceDetails&);
int modDragSource (const juce::DragAndDropTarget::SourceDetails&);
using ModDropHandler = std::function<void (int source, int dest)>;

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
               public juce::DragAndDropTarget,
               private juce::ChangeListener,
               private juce::Timer
{
public:
    Header (SparkProcessorBase&, bool isFx);
    ~Header() override;
    std::function<void (juce::Component& anchor)> onSettings;
    std::function<void()> onBrowse;
    std::function<void (int page)> onPage;   // 0 = SOUND, 1 = SYNTH, 2 = FX
    void setPage (int page);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Dragging a modulation source over a page tab opens that page, so you can drop onto what's there.
    bool isInterestedInDragSource (const SourceDetails& d) override { return isModDrag (d); }
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { stopTimer(); springTab = -1; }
    void itemDropped (const SourceDetails&) override { stopTimer(); springTab = -1; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void timerCallback() override;
    int springTab = -1;
    bool presetHover = false;

    SparkProcessorBase& processor;
    bool fx;
    PillButton prev { {}, PillButton::Style::ghost, Icon::chevronLeft };
    PillButton next { {}, PillButton::Style::ghost, Icon::chevronRight };
    PillButton settings { {}, PillButton::Style::outline, Icon::settings };
    PillButton soundTab { "SOUND", PillButton::Style::segment };
    PillButton synthTab { "SYNTH", PillButton::Style::segment };
    PillButton fxTab { "FX", PillButton::Style::segment };
    juce::Rectangle<int> presetArea, kindArea;
};

// The round centrepiece: the live sound as a ring, eight facet arcs you can drag,
// and the SPARK button in the middle.
class CoreView : public juce::Component,
                 public juce::SettableTooltipClient,
                 public juce::DragAndDropTarget,
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

    bool isInterestedInDragSource (const SourceDetails& d) override { return isModDrag (d); }
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { dropHover = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;
    ModDropHandler onModDrop;
    void flashFacet (int facet);   // sparks and a bolt on a facet (e.g. when modulation lands on it)

private:
    int dropHover = -1;
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

    // ---- Storm: sparks and lightning when facets move. Fixed pools, no allocation, idle = no repaints.
    struct Particle
    {
        juce::Point<float> pos, vel;
        float life = 0.0f, maxLife = 1.0f, size = 1.0f;
        bool hot = false;
    };
    struct Bolt
    {
        static constexpr int mainPoints = 17, branchPoints = 9;
        juce::Point<float> from, to, pts[mainPoints], branch[branchPoints];
        float life = 0.0f, maxLife = 0.2f, nextReshape = 0.0f, strength = 1.0f;
        bool hasBranch = false;
    };
    juce::Point<float> facetTip (int facet, float value) const;
    void spawnSparks (juce::Point<float> at, juce::Point<float> outward, int count, float speed, float spread);
    void spawnBolt (juce::Point<float> from, juce::Point<float> to, float strength);
    void shapeBolt (Bolt&);
    void burst();
    void stepStorm (float dt);
    void paintStorm (juce::Graphics&);
    bool stormActive() const;

    std::array<Particle, 120> particles;
    std::array<Bolt, 8> bolts;
    std::array<float, numFacets> lastFacet {};
    std::array<float, numFacets> travel {};     // movement since the last bolt, per facet
    std::array<double, numFacets> lastBoltTime {};
    juce::Random storm;
    float glow = 0.0f;
    double lastTick = 0.0, lastBurst = 0.0;
    bool wasIdle = false;

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

// A labelled number you drag up/down to change (Shift = fine, double-click = reset, wheel works too).
class ValueBox : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    ValueBox (juce::RangedAudioParameter&, const juce::String& label, const juce::String& tooltip = {});
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool framed = false; // draw a rounded box behind it

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float dragStart = 0.0f;
    bool hover = false, dragging = false, dropHover = false;
};

// Small arc knob for effect controls (drag up/down, Shift = fine, double-click = reset).
class ArcKnob : public juce::Component,
                public juce::SettableTooltipClient,
                public juce::DragAndDropTarget
{
public:
    ArcKnob (juce::RangedAudioParameter&, const juce::String& label);
    std::function<bool()> isActive; // gold when true, grey when false

    // Set both to make this knob a modulation drop target
    int modDest = -1;
    ModDropHandler onModDrop;
    bool isInterestedInDragSource (const SourceDetails& d) override { return modDest >= 0 && onModDrop != nullptr && isModDrag (d); }
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override { dropHover = false; repaint(); onModDrop (modDragSource (d), modDest); }
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float dragStart = 0.0f;
    bool hover = false, dragging = false, dropHover = false;
};

// Right-hand facet list with value bars and per-facet locks.
class FacetList : public juce::Component,
                  public juce::DragAndDropTarget,
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

    bool isInterestedInDragSource (const SourceDetails& d) override { return isModDrag (d); }
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { dropHover = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;
    ModDropHandler onModDrop;

private:
    void timerCallback() override;   // repaints only when a value or its modulation changed
    std::array<float, numFacets * 2> shownValues {};
    int dropHover = -1;
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

// Full preset browser: categories, search, tiles with "best for" hints, save and surprise.
class PresetBrowser : public juce::Component,
                      private juce::ChangeListener
{
public:
    explicit PresetBrowser (SparkProcessorBase&);
    ~PresetBrowser() override;

    std::function<void()> onClose;
    std::function<void()> onSave;
    void refresh (bool jumpToCurrentCategory);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    class Tiles : public juce::Component
    {
    public:
        explicit Tiles (PresetBrowser& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        juce::Rectangle<int> tileBounds (int slot) const;
        int slotAt (juce::Point<int>) const;
        int requiredHeight (int count) const;
        PresetBrowser& owner;
        int hovered = -1;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildList();
    juce::Rectangle<int> categoryRow (int i) const;
    juce::Rectangle<int> listArea() const;
    int countIn (const juce::String& category) const;

    SparkProcessorBase& processor;
    juce::StringArray categories;
    juce::String selectedCategory;
    std::vector<int> shown;
    int hoveredCategory = -1;

    juce::TextEditor search;
    PillButton surprise { "SURPRISE ME", PillButton::Style::outline, Icon::shuffle };
    PillButton save { "SAVE", PillButton::Style::goldOutline };
    PillButton close { {}, PillButton::Style::outline, Icon::cross };
    juce::Viewport viewport;
    Tiles tiles { *this };
};

// Base editor shared by both plugins: scales a fixed 1120x720 design to the window,
// and lays out the header, core, amounts, facets and lineage. Subclasses fill the left column.
class SparkEditorBase : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer
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
    void setBrowserVisible (bool);
    Header& getHeader() noexcept { return header; }
    virtual void hideOtherOverlays() {}
    virtual void handleModDrop (int /*source*/, int /*dest*/) {}
    CoreView& getCore() noexcept { return core; }
    void promptToSavePreset();

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
    PresetBrowser browser;
    juce::TooltipWindow tooltips { nullptr, 600 };
};
} // namespace spark

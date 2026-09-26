#pragma once

#include "Common/Components.h"
#include "InstrumentProcessor.h"
#include "FxPage.h"
#include "SynthPage.h"

namespace spark
{
// Left column, top: the dropped sound, engine switch and wavetable tools.
class SourcePanel : public juce::Component,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    explicit SourcePanel (InstrumentProcessor&);
    ~SourcePanel() override;

    std::function<void()> onImport, onShapeshift, onExport;
    void setDragHover (bool h) { dragHover = h; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    InstrumentProcessor& processor;
    PillButton grainTab { "GRAIN", PillButton::Style::segment };
    PillButton tableTab { "TABLE", PillButton::Style::segment };
    PillButton sampleTab { "SAMPLE", PillButton::Style::segment };
    PillButton importButton { "Import" }, shapeshiftButton { "Shapeshift", PillButton::Style::goldSolid };
    PillButton exportButton { "Export", PillButton::Style::goldOutline };
    juce::ParameterAttachment modeAttachment;
    bool dragHover = false;
    std::vector<float> frameShape;
};

// The parameters of one DAHDSR envelope with curves and sustain slope.
struct EnvelopeRefs
{
    juce::RangedAudioParameter *attack, *hold, *decay, *sustain, *release, *attackCurve, *decayCurve, *releaseCurve, *delay, *sustainSlope;
    static EnvelopeRefs from (juce::AudioProcessorValueTreeState&, const juce::String& prefix);
    std::vector<juce::RangedAudioParameter*> all() const { return { attack, hold, decay, sustain, release, attackCurve, decayCurve, releaseCurve, delay, sustainSlope }; }
};

// Draggable envelope drawing. Points set times and levels; the small circles bend each curve
// (and tilt the sustain). While notes play, a glowing dot per voice runs along the shape.
class EnvelopeGraph : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    // extended = also show and edit Delay and Hold
    EnvelopeGraph (EnvelopeRefs, bool extended, const InstrumentProcessor* voices = nullptr, bool toneEnvelope = false);
    std::function<bool()> isDimmed; // e.g. tone envelope with Amount at zero

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Target { none = -1, delayPt, attackPt, holdPt, decayPt, sustainPt, releasePt,
                  attackHandle, decayHandle, slopeHandle, releaseHandle };
    struct Geometry
    {
        float x0, xDl, xA, xH, xD, xS, xR, top, bottom, segW, minW, zoom = 1.0f;
        float sustainEnd = 0.0f;               // level at the end of the drawn sustain (after the slope)
        juce::Point<float> handle[4];          // attack, decay, slope, release
        juce::Point<float> point[6];           // delay, attack, hold, decay, sustain, release
    };
    Geometry geometry() const;
    juce::Path curvePath (const Geometry&) const;
    Target targetAt (juce::Point<float>) const;
    void timerCallback() override;
    std::vector<juce::RangedAudioParameter*> paramsFor (Target) const;
    float valueOf (juce::RangedAudioParameter* p) const { return p->convertFrom0to1 (p->getValue()); }

    EnvelopeRefs env;
    bool extended;
    const InstrumentProcessor* voices;
    bool toneEnvelope;
    Target hovered = none, dragging = none;
    std::map<juce::RangedAudioParameter*, float> dragStart;
    float frozenZoom = 1.0f; // zoom is held still while dragging
    std::vector<float> lastSeen;   // parameter values + playheads last drawn: repaint only when they change
};

// Left column, bottom: the envelopes in compact form, with a button to open the big editor.
class ShapePanel : public juce::Component
{
public:
    explicit ShapePanel (InstrumentProcessor&);
    std::function<void()> onExpand;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showTone (bool);

    InstrumentProcessor& processor;
    EnvelopeRefs ampRefs, toneRefs;
    PillButton ampTab { "AMP", PillButton::Style::segment };
    PillButton toneTab { "TONE", PillButton::Style::segment };
    PillButton expand { {}, PillButton::Style::outline, Icon::expand };
    EnvelopeGraph ampGraph, toneGraph;
    juce::OwnedArray<ValueBox> ampBoxes, toneBoxes;
    bool toneShown = false;
};

// Full-size envelope editor: both envelopes with every control.
class ShapeEditor : public juce::Component
{
public:
    explicit ShapeEditor (InstrumentProcessor&);
    std::function<void()> onClose;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override { if (isVisible()) grabKeyboardFocus(); }

private:
    InstrumentProcessor& processor;
    EnvelopeRefs ampRefs, toneRefs;
    EnvelopeGraph ampGraph, toneGraph;
    juce::OwnedArray<ValueBox> ampBoxes, toneBoxes;
    PillButton close { {}, PillButton::Style::outline, Icon::cross };
};

class InstrumentEditor : public SparkEditorBase,
                         public juce::FileDragAndDropTarget
{
public:
    explicit InstrumentEditor (InstrumentProcessor&);

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { source.setDragHover (true); }
    void fileDragExit (const juce::StringArray&) override { source.setDragHover (false); }
    void filesDropped (const juce::StringArray&, int, int) override;

    // 0 = Sound, 1 = Synth, 2 = FX
    void showPage (int page);

protected:
    void addExtraMenuItems (juce::PopupMenu&) override;
    void hideOtherOverlays() override;
    void handleModDrop (int source, int dest) override;

private:
    void loadFile (const juce::File&);
    void chooseFileToImport();
    void exportWavetable();
    void startShapeshift();
    void runShapeshift (const juce::File&);

    InstrumentProcessor& processor;
    SourcePanel source;
    ShapePanel shape;
    ShapeEditor shapeEditor;
    SynthPage synthPage;
    FxPage fxPage;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace spark

#pragma once

#include "Common/Components.h"
#include "InstrumentProcessor.h"

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

    std::function<void()> onImport, onMakeTable, onExport;
    void setDragHover (bool h) { dragHover = h; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    InstrumentProcessor& processor;
    PillButton grainTab { "GRAIN", PillButton::Style::segment };
    PillButton tableTab { "TABLE", PillButton::Style::segment };
    PillButton importButton { "Import" }, makeTableButton { "Make table" };
    PillButton exportButton { "Export", PillButton::Style::goldOutline };
    juce::ParameterAttachment modeAttachment;
    bool dragHover = false;
    std::vector<float> frameShape;
};

// Left column, bottom: the amp envelope, drawn and draggable.
class ShapePanel : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    explicit ShapePanel (InstrumentProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    struct Geometry { juce::Point<float> attack, decay, release; float left, right, top, bottom; };
    Geometry geometry() const;
    int pointAt (juce::Point<float>) const;

    InstrumentProcessor& processor;
    juce::RangedAudioParameter *attack, *decay, *sustain, *release;
    int dragging = -1, hovered = -1;
    juce::Rectangle<float> graph { 16.0f, 44.0f, 264.0f, 84.0f };
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

protected:
    void addExtraMenuItems (juce::PopupMenu&) override;

private:
    void loadFile (const juce::File&);
    void chooseFileToImport();
    void exportWavetable();

    InstrumentProcessor& processor;
    SourcePanel source;
    ShapePanel shape;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace spark

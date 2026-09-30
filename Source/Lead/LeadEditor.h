#pragma once

#include "Common/Components.h"
#include "Common/Cards.h"
#include "Instrument/FxPage.h"
#include "Instrument/ModCards.h"
#include "LeadProcessor.h"

namespace spark
{
// A labelled box for a choice parameter: click for a menu, drag or scroll to step through.
class ChoiceBox : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    ChoiceBox (juce::RangedAudioParameter&, const juce::String& label, const juce::String& tooltip = {});
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool framed = true;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    bool hover = false;
};

// One oscillator's current shape, drawn from the wavetable. Repaints only when the shape changes.
class WaveView : public juce::Component,
                 private juce::Timer
{
public:
    // followsSource: this is oscillator A, which can also show a loaded sound (table, grains or sample)
    WaveView (LeadProcessor&, juce::RangedAudioParameter& wave, std::function<juce::String()> caption, bool followsSource = false);
    void paint (juce::Graphics&) override;
    bool dragHover = false;

private:
    void timerCallback() override;
    juce::String signature() const;
    LeadProcessor& processor;
    juce::RangedAudioParameter& wave;
    std::function<juce::String()> caption;
    bool followsSource;
    juce::String shown;
};

// LEAD page, left column: oscillator A (built-in waves or a sound), oscillator B, layers and play mode.
class OscPanel : public juce::Component,
                 private juce::Timer
{
public:
    explicit OscPanel (LeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setDragHover (bool h) { waveA.dragHover = h; waveA.repaint(); }
    std::function<void (const juce::String& title, const juce::String& text)> onMessage;
    void showSoundsMenu();
    void importSound();
    void startShapeshift();

private:
    void timerCallback() override;   // shows the controls that go with oscillator A's mode
    void layoutModeRow();
    LeadProcessor& processor;
    PillButton sounds { "SOUNDS", PillButton::Style::goldOutline, Icon::chevronDown };
    ChoiceSegments oscMode;
    WaveView waveA, waveB;
    ValueBox unison, width, scan, grainSize, grainSpray, bWave, bSemi, bFine, bLevel, sub, breath;
    ChoiceSegments voiceMode;
    int shownMode = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

// A SynthCard that the OBSDN pages fill in from outside.
class LeadCard : public SynthCard
{
public:
    using SynthCard::SynthCard;
    using SynthCard::addKnob;
    void setSegments (ChoiceSegments& s) { segments = &s; addAndMakeVisible (s); }
};

// An ADSR card: a small graph of the envelope over its four knobs.
class AdsrCard : public LeadCard,
                 private juce::Timer
{
public:
    AdsrCard (const juce::String& title, const juce::String& blurb, juce::AudioProcessorValueTreeState&, const juce::String& prefix);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::RangedAudioParameter *a, *d, *s, *r;
    std::array<float, 4> shown { -1, -1, -1, -1 };
};

// SYNTH page: filter, both envelopes, expression and play settings.
class LeadSynthPage : public juce::Component
{
public:
    LeadSynthPage (LeadProcessor&, ModDropHandler onKnobDrop);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChoiceSegments filterType, voiceMode;
    LeadCard filter { "FILTER", {} };
    AdsrCard filterEnv, ampEnv;
    LeadCard expression { "EXPRESSION", "Vibrato waits, then fades in" };
    LeadCard play { "PLAY", {} };
    LeadCard output { "OUTPUT", {} };
};

// MOD page: two LFOs, four macros and the 8-slot matrix. Drag a source's handle onto a facet or knob.
class LeadModPage : public juce::Component
{
public:
    explicit LeadModPage (LeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LfoCard lfo1, lfo2;
    MacroCard macros;
    MatrixCard matrix;
};

// ---- Riff ---------------------------------------------------------------------
// The riff as a piano roll on the key's scale: click an empty cell to add a note, click a note to remove it.
class RiffRoll : public juce::Component,
                 private juce::Timer,
                 private juce::ChangeListener
{
public:
    explicit RiffRoll (LeadProcessor&);
    ~RiffRoll() override;
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverTick = -1; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    juce::Rectangle<float> gridArea() const;
    int gridTicks() const;   // click grid: 16ths, or 8th-note triplets for Drill
    float xForTick (double tick) const;
    float yForDegree (int degree) const;
    bool cellAt (juce::Point<float>, int& tick, int& degree) const;

    LeadProcessor& processor;
    riff::Riff riff;
    int lo = -2, hi = 12, key = 9, scale = 1, octave = 0;
    float playhead = -1.0f;
    int hoverTick = -1, hoverDegree = 0;
};

// Recent riffs as tiny rolls: click one to go back to it.
class RiffHistory : public juce::Component,
                    private juce::ChangeListener
{
public:
    explicit RiffHistory (LeadProcessor&);
    ~RiffHistory() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    int firstShown() const;
    juce::Rectangle<float> chip (int slot) const;
    static constexpr int maxChips = 12;
    LeadProcessor& processor;
    int hovered = -1;
};

// Drag this onto a MIDI track to drop the riff into your DAW. Click it to save a .mid file.
class MidiDragTile : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit MidiDragTile (LeadProcessor&);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    LeadProcessor& processor;
    bool hover = false, dragged = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

class RiffPage : public juce::Component,
                 private juce::ChangeListener,
                 private juce::Timer
{
public:
    explicit RiffPage (LeadProcessor&);
    ~RiffPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void timerCallback() override { refresh(); }
    void refresh();
    bool isOn() const;

    LeadProcessor& processor;
    PowerSwitch onSwitch;
    juce::ParameterAttachment onAttachment;
    PillButton generate { "GENERATE", PillButton::Style::goldSolid, Icon::shuffle };
    PillButton mutate { "MUTATE", PillButton::Style::outline };
    PillButton rhythm { "RHYTHM", PillButton::Style::outline };
    PillButton answer { "ANSWER", PillButton::Style::outline };
    PillButton playButton { "PLAY", PillButton::Style::goldOutline };
    ChoiceBox key, scale, style, bars, follow, latch;
    ValueBox density, range, gate, swing, octave;
    RiffRoll roll;
    RiffHistory history;
    MidiDragTile dragTile;
    juce::String hintShown;
};

class LeadEditor : public SparkEditorBase,
                   public juce::FileDragAndDropTarget
{
public:
    explicit LeadEditor (LeadProcessor&);
    ~LeadEditor() override;
    void showPage (int page);   // 0 LEAD, 1 SYNTH, 2 MOD, 3 RIFF, 4 FX
    enum Page { leadPage = 0, synthPage_ = 1, modPage_ = 2, riffPage_ = 3, fxPage_ = 4 };

    // drop a sound anywhere on OBSDN: it becomes oscillator A
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { osc.setDragHover (true); }
    void fileDragExit (const juce::StringArray&) override { osc.setDragHover (false); }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void hideOtherOverlays() override;
    void handleModDrop (int source, int facet) override;   // from the ring or the facet list
    void assignModulation (int source, int dest);          // from a knob
    LeadProcessor& processor;
    OscPanel osc;
    LeadSynthPage synthPage;
    LeadModPage modPage;
    RiffPage riffPage;
    FxPage fxPage;
};
} // namespace spark

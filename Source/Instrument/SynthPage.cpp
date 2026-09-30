#include "SynthPage.h"

namespace spark
{
// =====================================================================================
FilterCard::FilterCard (InstrumentProcessor& p, ModDropHandler onDrop)
    : SynthCard ("FILTER", {}),
      type (*p.apvts.getParameter ("filterType"), { "LOW", "HIGH", "BAND", "NOTCH" },
            { "Low-pass: keeps the lows, darkens as you close it",
              "High-pass: removes the lows, thins the sound out",
              "Band-pass: keeps a band around the cutoff",
              "Notch: cuts a band around the cutoff" })
{
    segments = &type;
    addAndMakeVisible (type);
    auto& cutoff = addKnob (*p.apvts.getParameter ("tone"), "CUTOFF", "Filter cutoff (the Tone facet). Drop an LFO or macro here to modulate it");
    auto& res = addKnob (*p.apvts.getParameter ("resonance"), "RES", "Resonance: a peak at the cutoff. High settings whistle. Drop an LFO or macro here to modulate it");
    cutoff.modDest = mod::tone;
    res.modDest = mod::resonance;
    cutoff.onModDrop = res.onModDrop = onDrop;
    addKnob (*p.apvts.getParameter ("keyTrack"), "KEY", "Key tracking: higher notes open the filter more. 100% follows the keyboard exactly");
    addKnob (*p.apvts.getParameter ("toneAmount"), "ENV", "How far the Tone envelope sweeps the cutoff, in octaves (edit its shape on the SOUND page)");
}

// =====================================================================================
LayersCard::LayersCard (InstrumentProcessor& p)
    : SynthCard ("LAYERS", "Sub and noise, under every note")
{
    addKnob (*p.apvts.getParameter ("subLevel"), "SUB", "A clean sine under every note, for weight. Goes through the filter and envelope with the rest");
    addKnob (*p.apvts.getParameter ("subTune"), "PITCH", "How far below the note the sub plays: any interval down to 3 octaves (-12 st = one octave)");
    addKnob (*p.apvts.getParameter ("noiseLevel"), "NOISE", "Noise layer for breath, air and grit. Goes through the filter and envelope with the rest");
    addKnob (*p.apvts.getParameter ("noiseColour"), "COLOUR", "Noise colour: dark rumble to bright hiss");
}

// =====================================================================================
PlayCard::PlayCard (InstrumentProcessor& p)
    : SynthCard ("PLAY", {}),
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
    addKnob (*p.apvts.getParameter ("ampVelocity"), "VEL", "How much playing harder makes notes louder");
}

// =====================================================================================
SynthPage::SynthPage (InstrumentProcessor& p, ModDropHandler onModDrop)
    : filter (p, onModDrop), layers (p), play (p), macros (p), lfo1 (p, 0), lfo2 (p, 1), matrix (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &filter, &layers, &play, &macros, &lfo1, &lfo2, &matrix })
        addAndMakeVisible (c);
}

void SynthPage::resized()
{
    const int gap = 12, top = 60;
    const int w = (getWidth() - 32 - 3 * gap) / 4;
    const int h = (getHeight() - top - 16 - gap) / 2;
    const int y2 = top + h + gap;
    filter.setBounds (16, top, w, h);
    layers.setBounds (filter.getRight() + gap, top, w, h);
    play.setBounds (layers.getRight() + gap, top, w, h);
    macros.setBounds (play.getRight() + gap, top, w, h);
    lfo1.setBounds (16, y2, w, h);
    lfo2.setBounds (lfo1.getRight() + gap, y2, w, h);
    matrix.setBounds (lfo2.getRight() + gap, y2, 2 * w + gap, h);
}

void SynthPage::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);
    drawSectionLabel (g, "SYNTH", { 20.0f, 14.0f, 120.0f, 32.0f });
    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    g.drawText ("Filter, how the keys play, and modulation. Drag an LFO or macro handle onto anything you want it to move",
                juce::Rectangle<float> (110.0f, 14.0f, b.getWidth() - 130.0f, 32.0f), juce::Justification::centredLeft, true);
}
} // namespace spark

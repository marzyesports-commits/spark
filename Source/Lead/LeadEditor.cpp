#include "LeadEditor.h"
#include "Instrument/FactorySounds.h"

namespace spark
{
namespace
{
    juce::RangedAudioParameter& param (LeadProcessor& p, const char* id) { return *p.apvts.getParameter (id); }

    juce::String noteName (int midi) { return juce::MidiMessage::getMidiNoteName (midi, true, true, 4); }
}

// =====================================================================================
ChoiceBox::ChoiceBox (juce::RangedAudioParameter& p, const juce::String& l, const juce::String& tip)
    : param (p), attachment (p, [this] (float) { repaint(); }), label (l)
{
    setTooltip (tip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void ChoiceBox::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    if (framed || hover)
    {
        g.setColour (hover ? raised : panel);
        g.fillRoundedRectangle (b.reduced (0.5f), 8.0f);
        g.setColour (hover ? line2 : line);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }
    auto top = b.withHeight (b.getHeight() * 0.45f);
    auto bottom = b.withTrimmedTop (b.getHeight() * 0.42f);
    g.setColour (hover ? gold : muted);
    g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.12f));
    g.drawText (label, top.translated (0.0f, 2.0f), juce::Justification::centred, false);
    g.setColour (text);
    g.setFont (fonts::mono (11.0f));
    g.drawText (param.getCurrentValueAsText(), bottom.translated (0.0f, -1.0f).reduced (14.0f, 0.0f), juce::Justification::centred, true);
    g.setColour (hover ? gold : ember);
    g.strokePath (makeIcon (Icon::chevronDown, juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ b.getRight() - 11.0f, bottom.getCentreY() - 1.0f })),
                  juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ChoiceBox::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&param);
    if (choice == nullptr)
        return;
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    for (int i = 0; i < choice->choices.size(); ++i)
        m.addItem (choice->choices[i], true, i == choice->getIndex(), [this, i] { attachment.setValueAsCompleteGesture ((float) i); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()));
}

void ChoiceBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&param))
    {
        const int dir = w.deltaY > 0 ? -1 : 1;
        attachment.setValueAsCompleteGesture ((float) juce::jlimit (0, choice->choices.size() - 1, choice->getIndex() + dir));
    }
}

// =====================================================================================
WaveView::WaveView (LeadProcessor& p, juce::RangedAudioParameter& w, std::function<juce::String()> c, bool follows)
    : processor (p), wave (w), caption (std::move (c)), followsSource (follows)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (15);
}

juce::String WaveView::signature() const
{
    juce::String sig = juce::String (wave.getValue(), 4) + caption();
    if (followsSource)
    {
        sig << "|" << (int) processor.getOscMode() << "|" << juce::String::toHexString ((juce::pointer_sized_int) processor.getSource().get())
            << "|" << juce::String (processor.params.grainSize->load(), 3) << (dragHover ? "d" : "");
    }
    return sig;
}

void WaveView::timerCallback()
{
    if (signature() != shown)
        repaint();
}

void WaveView::paint (juce::Graphics& g)
{
    using namespace colours;
    shown = signature();
    const float v = wave.getValue();
    auto b = getLocalBounds().toFloat();
    g.setColour (dragHover ? selected : bg);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (dragHover ? gold : line);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, dragHover ? 1.5f : 1.0f);

    auto plot = b.reduced (12.0f, 10.0f).withTrimmedTop (14.0f);
    g.setColour (faint);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    const int mode = followsSource ? (int) processor.getOscMode() : LeadProcessor::waves;
    const auto src = followsSource ? processor.getSource() : nullptr;
    auto drawShape = [&] (const std::vector<float>& shape)
    {
        float peak = 1.0e-6f;
        for (auto s : shape) peak = juce::jmax (peak, std::abs (s));
        juce::Path p;
        for (int i = 0; i < (int) shape.size(); ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (shape.size() - 1);
            const float y = plot.getCentreY() - shape[(size_t) i] / peak * plot.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (gold.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (gold);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (dragHover)
    {
        g.setColour (gold);
        g.setFont (fonts::body (13.0f, true));
        g.drawText ("Drop to play it as oscillator A", plot, juce::Justification::centred, false);
    }
    else if (mode != LeadProcessor::waves && src == nullptr)
    {
        g.setColour (muted);
        g.setFont (fonts::body (12.0f));
        g.drawFittedText ("Drop a sound here, or pick one from SOUNDS", plot.toNearestInt(), juce::Justification::centred, 2);
    }
    else if (mode == LeadProcessor::table && src != nullptr && src->table != nullptr)
    {
        std::vector<float> shape;
        src->table->getFrameShape (v * (float) (src->table->getNumFrames() - 1), shape, 128);
        drawShape (shape);
    }
    else if ((mode == LeadProcessor::grain || mode == LeadProcessor::sample) && src != nullptr && ! src->peaks.empty())
    {
        // the whole sound as a waveform, with the play position (and the grain's width)
        const int bins = (int) src->peaks.size();
        g.setColour (gold.withAlpha (0.55f));
        for (int i = 0; i < bins; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) bins;
            const float h = juce::jmax (1.0f, src->peaks[(size_t) i] * plot.getHeight() * 0.9f);
            g.fillRect (x, plot.getCentreY() - h * 0.5f, juce::jmax (1.0f, plot.getWidth() / (float) bins - 0.5f), h);
        }
        const float px = plot.getX() + plot.getWidth() * v;
        if (mode == LeadProcessor::grain)
        {
            const float secs = fmt::grainSeconds (processor.params.grainSize->load());
            const float w = juce::jmax (3.0f, plot.getWidth() * secs * (float) src->sampleRate / (float) juce::jmax (1, src->audio.getNumSamples()));
            g.setColour (goldHi.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (w, plot.getHeight()).withCentre ({ px, plot.getCentreY() }));
        }
        else
        {
            g.setColour (goldHi.withAlpha (0.12f));
            g.fillRect (px, plot.getY(), plot.getRight() - px, plot.getHeight());
        }
        g.setColour (goldHi);
        g.fillRect (px - 0.75f, plot.getY(), 1.5f, plot.getHeight());
    }
    else
    {
        const auto& table = LeadProcessor::waveTable();
        std::vector<float> shape;
        table.getFrameShape (v * (float) (table.getNumFrames() - 1), shape, 128);
        drawShape (shape);
    }

    g.setColour (text2);
    g.setFont (fonts::mono (10.0f));
    g.drawText (caption(), b.reduced (12.0f, 6.0f).withHeight (14.0f), juce::Justification::centredLeft, true);
}

// =====================================================================================
OscPanel::OscPanel (LeadProcessor& p)
    : processor (p),
      oscMode (param (p, "oscAMode"), { "WAVES", "TABLE", "GRAIN", "SAMPLE" },
               { "The built-in shapes: sine to reed",
                 "A wavetable from your sound (or a wavetable file). Wave moves through its frames",
                 "Grains of your sound, pitched to the key. Wave sets where in the sound they come from",
                 "Your sound itself, pitched to the key and looping from the Wave position" }),
      waveA (p, p.facetParam (LeadProcessor::wave), [&p]
      {
          const int n = juce::roundToInt (p.params.unison->load());
          const auto mult = n > 1 ? juce::String::fromUTF8 ("  \xc3\x97") + juce::String (n) : juce::String();
          const int mode = p.getOscMode();
          if (mode != LeadProcessor::waves)
          {
              const auto name = p.sourceName();
              return "A  " + (name.isNotEmpty() ? name.upToLastOccurrenceOf (".", false, false).toUpperCase() : juce::String ("NO SOUND")) + mult;
          }
          return "A  " + LeadProcessor::describeWave (p.facetParam (LeadProcessor::wave).getValue()).toUpperCase() + mult;
      }, true),
      waveB (p, param (p, "oscBWave"), [&p]
      {
          if (p.params.oscBLevel->load() < 0.005f) return juce::String ("B  OFF");
          return "B  " + LeadProcessor::describeWave (p.params.oscBWave->load()).toUpperCase();
      }),
      unison (param (p, "unison"), "UNISON", "How many copies of oscillator A are stacked (Detune spreads them)"),
      width (param (p, "width"), "WIDTH", "How far the unison copies spread across the stereo field"),
      scan (param (p, "scanTime"), "SCAN", "Sweeps through the table on every note, from Wave to the last frame, over this time"),
      grainSize (param (p, "grainSize"), "SIZE", "Grain length: short is buzzy and vocal, long is smooth"),
      grainSpray (param (p, "grainSpray"), "SPRAY", "Scatters where grains come from: more movement and air"),
      bWave (param (p, "oscBWave"), "WAVE", "Oscillator B's shape"),
      bSemi (param (p, "oscBSemi"), "PITCH", "Oscillator B's pitch in semitones (+12 = an octave up)"),
      bFine (param (p, "oscBFine"), "FINE", "Oscillator B's fine tune in cents"),
      bLevel (param (p, "oscBLevel"), "LEVEL", "Oscillator B's level (0 = off)"),
      sub (param (p, "subLevel"), "SUB", "A sine an octave below, for weight"),
      breath (param (p, "noiseLevel"), "BREATH", "Breath noise: airy flutes, whistles and pads"),
      voiceMode (param (p, "voiceMode"), { "POLY", "MONO", "LEGATO" },
                 { "Chords: every key sounds",
                   "One note at a time; every note restarts and glides from the last",
                   "One note at a time; overlapping notes glide without a new attack (best for leads)" })
{
    for (auto* c : std::initializer_list<juce::Component*> { &sounds, &oscMode, &waveA, &waveB, &unison, &width, &scan, &grainSize, &grainSpray,
                                                             &bWave, &bSemi, &bFine, &bLevel, &sub, &breath, &voiceMode })
        addAndMakeVisible (c);
    for (auto* v : { &unison, &width, &scan, &grainSize, &grainSpray, &bWave, &bSemi, &bFine, &bLevel, &sub, &breath })
        v->framed = true;
    sounds.setFontHeight (10.0f);
    sounds.setTooltip ("Play a sound as oscillator A: Spark's library, your own file, or Shapeshift a synth note. Or drag a file onto OBSDN");
    sounds.onClick = [this] { showSoundsMenu(); };
    startTimerHz (8);
}

void OscPanel::timerCallback()
{
    const int m = processor.getOscMode();
    if (m != shownMode)
    {
        shownMode = m;
        layoutModeRow();
    }
}

void OscPanel::layoutModeRow()
{
    // the row under oscillator A: unison and width, plus Scan for tables, or grain size and spray
    const int x = 16, w = getWidth() - 32, y = 152, h = 38;
    const int m = processor.getOscMode();
    scan.setVisible (m == LeadProcessor::table);
    grainSize.setVisible (m == LeadProcessor::grain);
    grainSpray.setVisible (m == LeadProcessor::grain);
    width.setVisible (m != LeadProcessor::grain);
    juce::Array<juce::Component*> row { &unison };
    if (m == LeadProcessor::grain) { row.add (&grainSize); row.add (&grainSpray); }
    else { row.add (&width); if (m == LeadProcessor::table) row.add (&scan); }
    const int gap = 6, each = (w - gap * (row.size() - 1)) / row.size();
    for (int i = 0; i < row.size(); ++i)
        row[i]->setBounds (x + i * (each + gap), y, i == row.size() - 1 ? w - i * (each + gap) : each, h);
}

void OscPanel::resized()
{
    const int x = 16, w = getWidth() - 32;
    sounds.setBounds (getWidth() - 16 - 96, 8, 96, 26);
    oscMode.setBounds (x, 40, w, 28);
    waveA.setBounds (x, 72, w, 76);
    layoutModeRow();
    waveB.setBounds (x, 200, w, 50);
    const int q = (w - 18) / 4;
    bWave.setBounds (x, 256, q, 38);
    bSemi.setBounds (x + (q + 6), 256, q, 38);
    bFine.setBounds (x + 2 * (q + 6), 256, q, 38);
    bLevel.setBounds (x + 3 * (q + 6), 256, w - 3 * (q + 6), 38);
    sub.setBounds (x, 322, w / 2 - 4, 38);
    breath.setBounds (x + w / 2 + 4, 322, w - w / 2 - 4, 38);
    voiceMode.setBounds (x, 390, w, 32);
}

void OscPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "OSCILLATORS", { 16.0f, 10.0f, 160.0f, 24.0f });
    drawSectionLabel (g, "LAYERS", { 16.0f, 298.0f, 200.0f, 22.0f }, juce::Justification::centredLeft, colours::muted);
    drawSectionLabel (g, "PLAY", { 16.0f, 364.0f, 200.0f, 22.0f }, juce::Justification::centredLeft, colours::muted);
}

void OscPanel::showSoundsMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    m.addItem ("Import a sound...", [this] { importSound(); });
    m.addItem ("Shapeshift a synth note...", [this] { startShapeshift(); });
    m.addSeparator();
    m.addSectionHeader ("SOUND LIBRARY");
    const auto current = processor.getSource();
    for (const auto& cat : factory::categories())
    {
        juce::PopupMenu sub;
        for (const auto& s : factory::sounds())
            if (s.category == cat)
            {
                const auto id = s.id;
                sub.addItem (s.name, true, current != nullptr && current->factoryId == id,
                             [this, id] { processor.loadFactorySound (id); });
            }
        m.addSubMenu (cat, sub);
    }
    m.addSeparator();
    m.addItem ("Built-in waves", true, processor.getOscMode() == LeadProcessor::waves, [this] { processor.clearSource(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sounds));
}

void OscPanel::importSound()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sound for oscillator A", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   processor.getSupportedExtensions());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        juce::String error;
        if (! processor.loadFile (f, error) && onMessage) onMessage ("Couldn't load that sound", error);
    });
}

void OscPanel::startShapeshift()
{
    chooser = std::make_unique<juce::FileChooser> ("Shapeshift: choose one note bounced from Serum, Serum 2, Vital or any synth",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), processor.getSupportedExtensions());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        juce::String summary;
        const bool ok = processor.shapeshift (f, summary);
        if (onMessage) onMessage (ok ? "Shapeshifted" : "Couldn't shapeshift that", summary);
    });
}

// =====================================================================================
AdsrCard::AdsrCard (const juce::String& cardTitle, const juce::String& cardBlurb, juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
    : LeadCard (cardTitle, cardBlurb),
      a (state.getParameter (prefix + "A")), d (state.getParameter (prefix + "D")),
      s (state.getParameter (prefix + "S")), r (state.getParameter (prefix + "R"))
{
    addKnob (*a, "ATTACK", "Attack time");
    addKnob (*d, "DECAY", "Decay time");
    addKnob (*s, "SUSTAIN", "Level while the key is held");
    addKnob (*r, "RELEASE", "Fade after the key is let go");
    startTimerHz (15);
}

void AdsrCard::timerCallback()
{
    const std::array<float, 4> now { a->getValue(), d->getValue(), s->getValue(), r->getValue() };
    if (now != shown)
    {
        shown = now;
        repaint (4, 40, getWidth() - 8, 76);
    }
}

void AdsrCard::resized()
{
    const int top = 122;
    const int n = knobs.size();
    const int w = (getWidth() - 16) / juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (8 + i * w, top, w, getHeight() - top - 6);
}

void AdsrCard::paint (juce::Graphics& g)
{
    using namespace colours;
    LeadCard::paint (g);
    auto area = juce::Rectangle<float> (14.0f, 44.0f, (float) getWidth() - 28.0f, 70.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (area, 8.0f);
    auto plot = area.reduced (10.0f, 10.0f);
    // time axis: square-root so short attacks stay visible next to long releases
    auto t = [] (juce::RangedAudioParameter* p) { return std::sqrt (fmt::envSeconds (p->getValue())); };
    const float ta = t (a), td = t (d), tr = t (r), hold = 0.35f;
    const float total = ta + td + hold + tr;
    const float sx = plot.getWidth() / juce::jmax (0.01f, total);
    const float sus = s->getValue();
    const float x0 = plot.getX(), yb = plot.getBottom(), yt = plot.getY(), h = plot.getHeight();
    juce::Path p;
    p.startNewSubPath (x0, yb);
    p.lineTo (x0 + ta * sx, yt);
    p.quadraticTo (x0 + (ta + td * 0.2f) * sx, yb - sus * h, x0 + (ta + td) * sx, yb - sus * h);
    p.lineTo (x0 + (ta + td + hold) * sx, yb - sus * h);
    p.quadraticTo (x0 + (ta + td + hold + tr * 0.2f) * sx, yb, x0 + total * sx, yb);
    juce::Path fill (p);
    fill.lineTo (x0, yb);
    fill.closeSubPath();
    g.setColour (gold.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (gold);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// =====================================================================================
LeadSynthPage::LeadSynthPage (LeadProcessor& p, ModDropHandler onKnobDrop)
    : filterType (param (p, "filterType"), { "LOW", "HIGH", "BAND", "NOTCH" },
                  { "Low-pass: keeps the lows, darkens as you close it", "High-pass: thins the sound out",
                    "Band-pass: a nasal band around the cutoff", "Notch: cuts a band around the cutoff" }),
      voiceMode (param (p, "voiceMode"), { "POLY", "MONO", "LEGATO" },
                 { "Chords: every key sounds", "One note at a time; every note restarts",
                   "One note at a time; overlapping notes glide without a new attack" }),
      filterEnv ("FILTER ENV", "Bite sets how far it sweeps", p.apvts, "flt"),
      ampEnv ("AMP ENV", {}, p.apvts, "amp")
{
    filter.setSegments (filterType);
    auto& cutoff = filter.addKnob (p.facetParam (LeadProcessor::tone), "CUTOFF", "Filter cutoff (the Tone facet). Drop an LFO or macro here to modulate it");
    auto& res = filter.addKnob (param (p, "resonance"), "RES", "Resonance: a peak at the cutoff (Bite adds more). Drop an LFO or macro here to modulate it");
    cutoff.modDest = mod::tone;
    res.modDest = mod::resonance;
    filter.addKnob (param (p, "keyTrack"), "KEY", "Key tracking: higher notes open the filter more");
    filter.addKnob (param (p, "velTone"), "VEL", "Play harder for a brighter note");

    auto& depth = expression.addKnob (p.facetParam (LeadProcessor::vibrato), "DEPTH", "Vibrato depth (the Vibrato facet). The mod wheel adds more");
    depth.modDest = mod::leadVibrato;
    expression.addKnob (param (p, "vibRate"), "RATE", "Vibrato speed");
    expression.addKnob (param (p, "vibDelay"), "DELAY", "How long a note is held before vibrato fades in");
    expression.addKnob (param (p, "scoop"), "SCOOP", "Each new note starts a little flat and bends up into pitch");
    expression.addKnob (param (p, "fall"), "FALL", "Notes drop in pitch as they're released");

    play.setSegments (voiceMode);
    play.addKnob (p.facetParam (LeadProcessor::glide), "GLIDE", "Glide time (the Glide facet)");
    play.addKnob (param (p, "bendRange"), "BEND", "Pitch bend range in semitones");
    play.addKnob (param (p, "ampVel"), "VEL", "Play harder for a louder note");

    auto& level = output.addKnob (param (p, "level"), "LEVEL", "Final level, with a safety limiter. Drop an LFO here for tremolo");
    level.modDest = mod::volume;
    for (auto* k : { &cutoff, &res, &depth, &level })
        k->onModDrop = onKnobDrop;

    for (auto* c : std::initializer_list<juce::Component*> { &filter, &filterEnv, &ampEnv, &expression, &play, &output })
        addAndMakeVisible (c);
}

void LeadSynthPage::resized()
{
    const int gap = 12, pad = 16;
    const int w = getWidth() - 2 * pad, h = getHeight() - 2 * pad - gap;
    const int topH = h / 2 + 10, bottomH = h - topH;
    const int third = (w - 2 * gap) / 3;
    filter.setBounds (pad, pad, third, topH);
    filterEnv.setBounds (pad + third + gap, pad, third, topH);
    ampEnv.setBounds (pad + 2 * (third + gap), pad, w - 2 * (third + gap), topH);
    const int y = pad + topH + gap;
    const int expW = w * 5 / 12, outW = w / 6;
    expression.setBounds (pad, y, expW, bottomH);
    play.setBounds (pad + expW + gap, y, w - expW - outW - 2 * gap, bottomH);
    output.setBounds (pad + w - outW, y, outW, bottomH);
}

void LeadSynthPage::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (colours::line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);
}

// =====================================================================================
LeadModPage::LeadModPage (LeadProcessor& p) : lfo1 (p, 0), lfo2 (p, 1), macros (p), matrix (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &lfo1, &lfo2, &macros, &matrix })
        addAndMakeVisible (c);
}

void LeadModPage::resized()
{
    const int gap = 12, pad = 16;
    const int w = getWidth() - 2 * pad, h = getHeight() - 2 * pad - gap;
    const int topH = h / 2 + 12, third = (w - 2 * gap) / 3;
    lfo1.setBounds (pad, pad, third, topH);
    lfo2.setBounds (pad + third + gap, pad, third, topH);
    macros.setBounds (pad + 2 * (third + gap), pad, w - 2 * (third + gap), topH);
    matrix.setBounds (pad, pad + topH + gap, w, h - topH);
}

void LeadModPage::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (colours::line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);
}

// =====================================================================================
RiffRoll::RiffRoll (LeadProcessor& p) : processor (p)
{
    processor.addChangeListener (this);
    refresh();
    startTimerHz (30);
}

RiffRoll::~RiffRoll() { processor.removeChangeListener (this); }

void RiffRoll::refresh()
{
    riff = processor.getRiff();
    key = juce::roundToInt (processor.params.riffKey->load());
    scale = juce::roundToInt (processor.params.riffScale->load());
    octave = juce::roundToInt (processor.params.riffOctave->load());
    const int size = (int) riff::scaleSteps (scale).size();
    lo = -2;
    hi = size + size / 2 + 2;
    for (const auto& n : riff.notes)
    {
        lo = juce::jmin (lo, n.degree - 1);
        hi = juce::jmax (hi, n.degree + 1);
    }
    repaint();
}

void RiffRoll::timerCallback()
{
    // follow the key/scale/octave boxes, and move the playhead only when it moved
    const int k = juce::roundToInt (processor.params.riffKey->load());
    const int s = juce::roundToInt (processor.params.riffScale->load());
    const int o = juce::roundToInt (processor.params.riffOctave->load());
    if (k != key || s != scale || o != octave)
        refresh();
    const float ph = processor.getRiffPlayhead();
    if (std::abs (ph - playhead) > 0.05f)
    {
        playhead = ph;
        repaint();
    }
}

juce::Rectangle<float> RiffRoll::gridArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f).withTrimmedLeft (40.0f).withTrimmedTop (16.0f);
}

int RiffRoll::gridTicks() const
{
    return riff.style == riff::drill ? riff::ticksPerBeat / 3 : riff::ticksPerBeat / 4;
}

float RiffRoll::xForTick (double tick) const
{
    const auto a = gridArea();
    return a.getX() + a.getWidth() * (float) (tick / juce::jmax (1, riff.lengthTicks()));
}

float RiffRoll::yForDegree (int degree) const
{
    const auto a = gridArea();
    const float rowH = a.getHeight() / (float) (hi - lo + 1);
    return a.getBottom() - (float) (degree - lo + 1) * rowH;
}

bool RiffRoll::cellAt (juce::Point<float> pos, int& tick, int& degree) const
{
    const auto a = gridArea();
    if (! a.contains (pos))
        return false;
    const float rowH = a.getHeight() / (float) (hi - lo + 1);
    degree = lo + (int) ((a.getBottom() - pos.y) / rowH);
    const int grid = gridTicks();
    tick = (int) ((pos.x - a.getX()) / a.getWidth() * (float) riff.lengthTicks());
    // a click on an existing note hits that note; otherwise snap to the grid
    for (const auto& n : riff.notes)
        if (n.degree == degree && tick >= n.start && tick < n.start + juce::jmax (grid, juce::jmin (n.span, grid * 2)))
        {
            tick = n.start;
            return true;
        }
    tick = tick / grid * grid;
    return true;
}

void RiffRoll::mouseMove (const juce::MouseEvent& e)
{
    int t = -1, d = 0;
    if (! cellAt (e.position, t, d)) t = -1;
    if (t != hoverTick || d != hoverDegree)
    {
        hoverTick = t;
        hoverDegree = d;
        repaint();
    }
}

void RiffRoll::mouseUp (const juce::MouseEvent& e)
{
    int t = 0, d = 0;
    if (e.mouseWasClicked() && cellAt (e.position, t, d))
        processor.toggleRiffNote (t, d);
}

void RiffRoll::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

    const auto a = gridArea();
    const auto& steps = riff::scaleSteps (scale);
    const int size = (int) steps.size();
    const int root = riff::rootNote (key, octave);
    const float rowH = a.getHeight() / (float) (hi - lo + 1);

    // rows: roots shaded, note names on the left
    g.setFont (fonts::mono (9.0f));
    for (int d = lo; d <= hi; ++d)
    {
        const float y = yForDegree (d);
        const bool isRoot = ((d % size) + size) % size == 0;
        if (isRoot)
        {
            g.setColour (raised);
            g.fillRect (a.getX(), y, a.getWidth(), rowH);
        }
        if (rowH >= 9.0f || isRoot)
        {
            g.setColour (isRoot ? text2 : ember);
            g.drawText (noteName (riff::degreeToNote (d, root, steps)), juce::Rectangle<float> (b.getX() + 8.0f, y, 38.0f, rowH),
                        juce::Justification::centredLeft, false);
        }
    }
    // columns: beats and bars
    const int beats = riff.bars * 4;
    for (int i = 0; i <= beats * 4; ++i)
    {
        const float x = xForTick (i * riff::ticksPerBeat / 4.0);
        g.setColour (i % 16 == 0 ? line2 : (i % 4 == 0 ? line : faint));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    }
    g.setColour (muted);
    g.setFont (fonts::mono (9.0f));
    for (int bar = 0; bar < riff.bars; ++bar)
        g.drawText (juce::String (bar + 1), juce::Rectangle<float> (xForTick (bar * riff::ticksPerBar) + 4.0f, b.getY() + 6.0f, 20.0f, 12.0f),
                    juce::Justification::centredLeft, false);

    // hover cell
    if (hoverTick >= 0)
    {
        g.setColour (gold.withAlpha (0.14f));
        g.fillRoundedRectangle (juce::Rectangle<float> (xForTick (hoverTick), yForDegree (hoverDegree), xForTick (hoverTick + gridTicks()) - xForTick (hoverTick), rowH).reduced (1.0f), 3.0f);
    }

    // notes, with slides drawn as a curve into the next note
    const float gate = processor.params.riffGate->load();
    for (size_t i = 0; i < riff.notes.size(); ++i)
    {
        const auto& n = riff.notes[i];
        const float x0 = xForTick (n.start), x1 = xForTick (n.start + juce::jmin (riff::soundingTicks (n, gate), n.span));
        const auto r = juce::Rectangle<float> (x0, yForDegree (n.degree), juce::jmax (4.0f, x1 - x0), rowH).reduced (1.0f, juce::jmin (2.0f, rowH * 0.15f));
        const bool lit = playhead >= (float) n.start && playhead < (float) (n.start + n.span);
        g.setColour ((lit ? goldHi : gold).withAlpha (0.35f + 0.65f * n.velocity));
        g.fillRoundedRectangle (r, 3.0f);
        if (n.slide && i + 1 < riff.notes.size())
        {
            const auto& m = riff.notes[i + 1];
            juce::Path s;
            s.startNewSubPath (r.getRight() - 2.0f, r.getCentreY());
            const float xe = xForTick (m.start) + 2.0f, ye = yForDegree (m.degree) + rowH * 0.5f;
            s.cubicTo (r.getRight() + 6.0f, r.getCentreY(), xe - 6.0f, ye, xe, ye);
            g.setColour (goldHi.withAlpha (0.8f));
            g.strokePath (s, juce::PathStrokeType (1.5f));
        }
    }

    if (playhead >= 0.0f)
    {
        const float x = xForTick (playhead);
        g.setColour (goldHi);
        g.fillRect (x - 0.75f, a.getY(), 1.5f, a.getHeight());
    }
    if (riff.notes.empty())
    {
        g.setColour (muted);
        g.setFont (fonts::body (13.0f));
        g.drawText ("Click to add notes, or press GENERATE", a, juce::Justification::centred, false);
    }
}

// =====================================================================================
RiffHistory::RiffHistory (LeadProcessor& p) : processor (p) { processor.addChangeListener (this); }
RiffHistory::~RiffHistory() { processor.removeChangeListener (this); }

int RiffHistory::firstShown() const
{
    return juce::jmax (0, (int) processor.getRiffHistory().size() - maxChips);
}

juce::Rectangle<float> RiffHistory::chip (int slot) const
{
    const float w = 64.0f, gap = 6.0f;
    return { 70.0f + (float) slot * (w + gap), 2.0f, w, (float) getHeight() - 4.0f };
}

void RiffHistory::paint (juce::Graphics& g)
{
    using namespace colours;
    g.setColour (muted);
    g.setFont (fonts::label (10.0f));
    g.drawText ("RECENT", juce::Rectangle<float> (0.0f, 0.0f, 64.0f, (float) getHeight()), juce::Justification::centredLeft, false);
    const auto& list = processor.getRiffHistory();
    const int first = firstShown();
    for (int i = first; i < (int) list.size(); ++i)
    {
        const auto r = chip (i - first);
        const bool current = i == processor.getRiffHistoryIndex();
        g.setColour (current ? selected : (i - first == hovered ? raised : panel));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (current ? gold : line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        const auto& rf = list[(size_t) i];
        if (rf.notes.empty()) continue;
        int lo = 99, hi = -99;
        for (const auto& n : rf.notes) { lo = juce::jmin (lo, n.degree); hi = juce::jmax (hi, n.degree); }
        const auto in = r.reduced (5.0f, 5.0f);
        g.setColour (current ? gold : text2.withAlpha (0.7f));
        for (const auto& n : rf.notes)
        {
            const float x = in.getX() + in.getWidth() * (float) n.start / (float) rf.lengthTicks();
            const float w = juce::jmax (1.5f, in.getWidth() * (float) n.span / (float) rf.lengthTicks() - 1.0f);
            const float y = in.getBottom() - in.getHeight() * (float) (n.degree - lo) / (float) juce::jmax (1, hi - lo) - 1.0f;
            g.fillRect (x, y - 1.0f, w, 2.0f);
        }
    }
}

void RiffHistory::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int s = 0; s < maxChips; ++s)
        if (chip (s).contains (e.position) && firstShown() + s < (int) processor.getRiffHistory().size()) h = s;
    if (h != hovered) { hovered = h; repaint(); }
}

void RiffHistory::mouseUp (const juce::MouseEvent& e)
{
    for (int s = 0; s < maxChips; ++s)
        if (chip (s).contains (e.position))
            processor.recallRiff (firstShown() + s);
}

// =====================================================================================
MidiDragTile::MidiDragTile (LeadProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto a MIDI track to drop this riff into your song. Click to save it as a .mid file");
}

void MidiDragTile::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hover ? selected : panel);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (hover ? gold : line2);
    const float dash[] { 4.0f, 3.0f };
    juce::Path outline;
    outline.addRoundedRectangle (b, b.getHeight() * 0.5f);
    juce::Path dashed;
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dash, 2);
    g.fillPath (dashed);
    g.setColour (hover ? gold : text);
    g.setFont (fonts::body (11.0f, true).withExtraKerningFactor (0.1f));
    g.drawText ("DRAG MIDI", b, juce::Justification::centred, false);
}

void MidiDragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged || e.getDistanceFromDragStart() < 6)
        return;
    dragged = true;
    const auto file = processor.exportRiffMidi();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void MidiDragTile::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDrag = dragged;
    dragged = false;
    if (wasDrag || ! e.mouseWasClicked())
        return;
    chooser = std::make_unique<juce::FileChooser> ("Save the riff as a MIDI file",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                       .getChildFile (processor.riffName() + ".mid"),
                                                   "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto target = fc.getResult();
                              if (target == juce::File()) return;
                              const auto made = processor.exportRiffMidi();
                              made.copyFileTo (target.withFileExtension ("mid"));
                          });
}

// =====================================================================================
RiffPage::RiffPage (LeadProcessor& p)
    : processor (p),
      onAttachment (param (p, "riffOn"), [this] (float) { refresh(); }),
      key (param (p, "riffKey"), "KEY", "The riff's key"),
      scale (param (p, "riffScale"), "SCALE", "The riff's scale. Changing it re-voices the riff without losing the tune"),
      style (param (p, "riffStyle"), "STYLE", "What kind of lines GENERATE writes"),
      bars (param (p, "riffBars"), "LENGTH", "Riff length in bars"),
      follow (param (p, "riffFollow"), "FOLLOW",
              "How held keys move the riff. In key: along the scale, so it follows your chord roots. "
              "Chromatic: transposes exactly. Fixed: any key plays it as written"),
      latch (param (p, "riffLatch"), "LATCH", "On: the riff plays whenever your DAW plays, no key needed"),
      density (param (p, "riffDensity"), "DENSITY", "How busy the riff is"),
      range (param (p, "riffRange"), "RANGE", "How far the line travels"),
      gate (param (p, "riffGate"), "GATE", "Note length: short and plucky to long and smooth"),
      swing (param (p, "riffSwing"), "SWING", "Pushes off-beat 16ths late for groove"),
      octave (param (p, "riffOctave"), "OCTAVE", "Plays the riff higher or lower"),
      roll (p), history (p), dragTile (p)
{
    addAndMakeVisible (onSwitch);
    onSwitch.setTooltip ("Riff on: held keys play the riff. Off: keys play notes as normal");
    onSwitch.onClick = [this] { onAttachment.setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f); };

    for (auto* b : { &generate, &mutate, &rhythm, &answer, &playButton })
    {
        addAndMakeVisible (b);
        b->setFontHeight (11.0f);
    }
    generate.setTooltip ("Write a brand new riff in this style");
    mutate.setTooltip ("Keep the rhythm, change some notes");
    rhythm.setTooltip ("Keep the notes in order, try a new rhythm");
    answer.setTooltip ("Make the second half answer the first (call and response)");
    playButton.setTooltip ("Hear the riff without holding a key");
    playButton.setClickingTogglesState (false);
    auto switchOn = [this]
    {
        if (! isOn()) onAttachment.setValueAsCompleteGesture (1.0f);
    };
    generate.onClick = [this, switchOn] { switchOn(); processor.generateRiff(); };
    mutate.onClick = [this, switchOn] { switchOn(); processor.mutateRiff(); };
    rhythm.onClick = [this, switchOn] { switchOn(); processor.newRiffRhythm(); };
    answer.onClick = [this, switchOn] { switchOn(); processor.answerRiff(); };
    playButton.onClick = [this, switchOn]
    {
        switchOn();
        processor.riffPreview = ! processor.riffPreview.load();
        refresh();
    };

    for (auto* c : std::initializer_list<juce::Component*> { &key, &scale, &style, &bars, &follow, &latch,
                                                             &density, &range, &gate, &swing, &octave, &roll, &history, &dragTile })
        addAndMakeVisible (c);
    for (auto* v : { &density, &range, &gate, &swing, &octave })
        v->framed = true;

    processor.addChangeListener (this);
    onAttachment.sendInitialUpdate();
    startTimerHz (4);
}

RiffPage::~RiffPage()
{
    processor.removeChangeListener (this);
    processor.riffPreview = false;
}

bool RiffPage::isOn() const
{
    return processor.apvts.getParameter ("riffOn")->getValue() > 0.5f;
}

void RiffPage::refresh()
{
    const bool on = isOn();
    onSwitch.setOn (on);
    if (! on && processor.riffPreview.load())
        processor.riffPreview = false;
    const bool previewing = processor.riffPreview.load();
    playButton.setButtonText (previewing ? "STOP" : "PLAY");
    playButton.setStyle (previewing ? PillButton::Style::goldSolid : PillButton::Style::goldOutline);

    const int k = juce::roundToInt (processor.params.riffKey->load());
    const int o = juce::roundToInt (processor.params.riffOctave->load());
    const int followMode = juce::roundToInt (processor.params.riffFollow->load());
    const int st = juce::roundToInt (processor.params.riffStyle->load());
    juce::String hint = riff::styleHints()[st] + ".  ";
    if (! on)
        hint += "The riff is off: switch it on, then hold a key to play it.";
    else if (followMode == riff::fixedRoot)
        hint += "Hold any key to play the riff as written.";
    else
        hint += "Hold " + noteName (riff::rootNote (k, o)) + " to play it as written; other keys move it"
                + (followMode == riff::inKey ? " along the scale." : ".");
    if (hint != hintShown)
    {
        hintShown = hint;
        repaint();
    }
}

void RiffPage::resized()
{
    onSwitch.setBounds (112, 18, 64, 24);
    int x = 192;
    for (auto* b : { &generate, &mutate, &rhythm, &answer })
    {
        const int w = b == &generate ? 124 : 88;
        b->setBounds (x, 14, w, 32);
        x += w + 8;
    }
    dragTile.setBounds (getWidth() - 16 - 120, 14, 120, 32);
    playButton.setBounds (dragTile.getX() - 8 - 84, 14, 84, 32);

    // settings: two columns on the left
    const int colW = 104, rowH = 44, left = 16, top = 60;
    juce::Component* cells[] { &key, &scale, &style, &bars, &density, &range, &gate, &swing, &octave, &follow, &latch };
    for (int i = 0; i < 11; ++i)
        cells[i]->setBounds (left + (i % 2) * (colW + 8), top + (i / 2) * (rowH + 6), colW, rowH);

    const int rollX = left + 2 * colW + 8 + 16;
    roll.setBounds (rollX, top, getWidth() - rollX - 16, getHeight() - top - 76);
    history.setBounds (rollX, roll.getBottom() + 8, getWidth() - rollX - 16, 30);
}

void RiffPage::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);

    g.setColour (gold);
    g.fillPath (makeLogoPath ({ 18.0f, 20.0f, 18.0f, 18.0f }));
    g.setColour (text);
    g.setFont (fonts::display (13.0f).withExtraKerningFactor (0.1f));
    g.drawText ("RIFF", juce::Rectangle<float> (42.0f, 14.0f, 140.0f, 32.0f), juce::Justification::centredLeft, false);

    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText (hintShown, juce::Rectangle<float> ((float) roll.getX(), b.getBottom() - 30.0f, (float) roll.getWidth(), 20.0f),
                juce::Justification::centredLeft, true);
}

// =====================================================================================
namespace
{
    EditorStyle leadStyle()
    {
        EditorStyle s;
        s.badge = {};
        s.tabs = { "LEAD", "SYNTH", "MOD", "RIFF", "FX" };
        s.tabTips = { "The sound: oscillators and facets", "Filter, envelopes, vibrato, scoop and glide",
                      "LFOs, macros and the mod matrix: drag a handle onto a facet or knob",
                      "Riff: write and play lead lines", "The effects rack" };
        s.aboutTitle = "OBSDN";
        s.aboutText = "Hooks on demand. Pick a sound, let the riff writer find the line, strike the stone until it's yours.";
        return s;
    }
}

LeadEditor::LeadEditor (LeadProcessor& p)
    : SparkEditorBase (p, leadStyle()), processor (p), osc (p),
      synthPage (p, [this] (int src, int dest) { assignModulation (src, dest); }), modPage (p), riffPage (p), fxPage (p)
{
    auto& c = content();
    c.addAndMakeVisible (osc);
    osc.setBounds (leftColumn());
    for (auto* page : std::initializer_list<juce::Component*> { &synthPage, &modPage, &riffPage, &fxPage })
    {
        c.addChildComponent (page);
        page->setBounds (24, 86, 1072, 440);
    }
    getHeader().onPage = [this] (int page) { showPage (page); };
    osc.onMessage = [this] (const juce::String& title, const juce::String& text) { showMessage (title, text); };
    showPage (0);
}

LeadEditor::~LeadEditor() = default;

bool LeadEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    const auto exts = juce::StringArray::fromTokens (processor.getSupportedExtensions().removeCharacters ("*"), ";", "");
    for (const auto& f : files)
        for (const auto& e : exts)
            if (e.isNotEmpty() && f.endsWithIgnoreCase (e)) return true;
    return false;
}

void LeadEditor::filesDropped (const juce::StringArray& files, int, int)
{
    osc.setDragHover (false);
    juce::String error;
    if (! processor.loadFile (juce::File (files[0]), error))
        showMessage ("Couldn't load that sound", error);
    else
        showPage (0);
}

void LeadEditor::hideOtherOverlays() {}

void LeadEditor::assignModulation (int source, int dest)
{
    if (processor.assignModulation (source, dest, processor.defaultAmountFor (dest)) < 0)
        showMessage ("All 8 modulation slots are in use", "Clear one in the MOD MATRIX on the MOD page, then try again.");
}

void LeadEditor::handleModDrop (int source, int facet)
{
    const int dest = LeadProcessor::destForFacet (facet);
    if (dest < 0)
    {
        showMessage ("Glide can't be modulated", "Try Wave, Detune, Tone, Bite, Drive, Vibrato or Space.");
        return;
    }
    if (processor.assignModulation (source, dest, processor.defaultAmountFor (dest)) < 0)
    {
        showMessage ("All 8 modulation slots are in use", "Clear one in the MOD MATRIX on the MOD page, then try again.");
        return;
    }
    getCore().flashFacet (facet);
}

void LeadEditor::showPage (int page)
{
    setBrowserVisible (false);
    synthPage.setVisible (page == 1);
    modPage.setVisible (page == 2);
    riffPage.setVisible (page == 3);
    fxPage.setVisible (page == 4);
    for (auto* c : std::initializer_list<juce::Component*> { &synthPage, &modPage, &riffPage, &fxPage })
        if (c->isVisible()) c->toFront (false);
    getHeader().setPage (page);
}
} // namespace spark

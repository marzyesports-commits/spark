#include "InstrumentEditor.h"
#include "Shapeshift.h"

namespace spark
{
// =====================================================================================
SourcePanel::SourcePanel (InstrumentProcessor& p)
    : processor (p),
      modeAttachment (*p.apvts.getParameter ("mode"), [this] (float v)
      {
          const bool table = v > 0.5f;
          const int m = juce::roundToInt (v);
          grainTab.setToggleState (m == 0, juce::dontSendNotification);
          tableTab.setToggleState (m == 1, juce::dontSendNotification);
          sampleTab.setToggleState (m == 2, juce::dontSendNotification);
          repaint();
      })
{
    for (auto* b : { &grainTab, &tableTab, &sampleTab, &importButton, &shapeshiftButton, &exportButton })
        addAndMakeVisible (b);
    for (auto* b : { &grainTab, &tableTab, &sampleTab })
    {
        b->setFontHeight (10.0f);
        b->setLetterSpacing (0.14f);
    }
    grainTab.setTooltip ("Grain: play the sound as a cloud of tiny slices");
    tableTab.setTooltip ("Table: play the wavetable made from the sound");
    importButton.setTooltip ("Load a sample or wavetable (you can also drag one onto Spark)");
    sampleTab.setTooltip ("Sample: play the recording itself (after Shapeshift, this is the original to compare with)");
    shapeshiftButton.setTooltip ("Shapeshift: rebuild a note bounced from Serum, Serum 2, Vital or any synth, then reshape it");
    exportButton.setTooltip ("Save the wavetable, with Drive and Tone baked in, for Serum or Vital");

    grainTab.onClick = [this] { modeAttachment.setValueAsCompleteGesture (0.0f); };
    tableTab.onClick = [this] { modeAttachment.setValueAsCompleteGesture (1.0f); };
    sampleTab.onClick = [this] { modeAttachment.setValueAsCompleteGesture (2.0f); };
    importButton.onClick = [this] { if (onImport) onImport(); };
    shapeshiftButton.onClick = [this] { if (onShapeshift) onShapeshift(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };

    modeAttachment.sendInitialUpdate();
    processor.addChangeListener (this);
    startTimerHz (24);
}

SourcePanel::~SourcePanel() { processor.removeChangeListener (this); }

void SourcePanel::resized()
{
    sampleTab.setBounds (getWidth() - 16 - 2 - 64, 16, 64, 26);
    tableTab.setBounds (sampleTab.getX() - 56, 16, 56, 26);
    grainTab.setBounds (tableTab.getX() - 58, 16, 58, 26);
    const int w = (getWidth() - 32 - 12) / 3;
    importButton.setBounds (16, 204, w, 32);
    shapeshiftButton.setBounds (16 + w + 6, 204, w, 32);
    exportButton.setBounds (16 + 2 * (w + 6), 204, w, 32);
}

void SourcePanel::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "SOURCE", { 16.0f, 16.0f, 100.0f, 26.0f });

    // segmented control frame
    auto seg = grainTab.getBounds().getUnion (sampleTab.getBounds()).toFloat().expanded (2.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (seg, seg.getHeight() * 0.5f, 1.0f);

    // drop zone
    auto zone = juce::Rectangle<float> (16.0f, 54.0f, (float) getWidth() - 32.0f, 108.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (zone, 12.0f);
    {
        juce::Path outline;
        outline.addRoundedRectangle (zone.reduced (0.5f), 12.0f);
        juce::Path dashPath;
        const float dashLengths[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashPath, outline, dashLengths, 2);
        g.setColour (dragHover ? gold : colours::dashed);
        g.fillPath (dashPath);
    }

    auto s = processor.getSource();
    auto wave = zone.withHeight (78.0f).reduced (1.0f, 0.0f);
    auto info = zone.withTrimmedTop (78.0f);
    g.setColour (line);
    g.fillRect (info.getX() + 1.0f, info.getY(), info.getWidth() - 2.0f, 1.0f);

    const bool tableMode = processor.getMode() == InstrumentProcessor::tableMode;
    if (s != nullptr)
    {
        if (tableMode && s->table != nullptr)
        {
            const float morph = processor.facetParam (InstrumentProcessor::morph).getValue();
            const int frames = s->table->getNumFrames();
            s->table->getFrameShape (morph * (float) (frames - 1), frameShape, 128);
            juce::Path p;
            for (int i = 0; i < 128; ++i)
            {
                const float x = wave.getX() + 8.0f + (float) i / 127.0f * (wave.getWidth() - 16.0f);
                const float y = wave.getCentreY() - frameShape[(size_t) i] * 30.0f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (gold);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
            g.setColour (muted);
            g.setFont (fonts::mono (10.0f));
            g.drawText ("FRAME " + juce::String (juce::roundToInt (morph * (float) (frames - 1)) + 1) + " / " + juce::String (frames),
                        wave.reduced (8.0f, 6.0f), juce::Justification::topRight, false);
        }
        else if (! s->peaks.empty())
        {
            const float pos = processor.facetParam (InstrumentProcessor::position).getValue();
            const float grainSec = fmt::grainSeconds (processor.facetParam (InstrumentProcessor::grain).getValue());
            const float duration = (float) (s->audio.getNumSamples() / s->sampleRate);
            const float grainW = juce::jlimit (3.0f, wave.getWidth(), grainSec / juce::jmax (0.01f, duration) * wave.getWidth());
            const float px = wave.getX() + pos * wave.getWidth();

            g.setColour (gold.withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (px - grainW * 0.5f, wave.getY(), grainW, wave.getHeight()).getIntersection (wave));

            const int bars = 86;
            juce::Path bp;
            for (int i = 0; i < bars; ++i)
            {
                const int bin = (int) ((float) i / (float) bars * (float) s->peaks.size());
                const float h = juce::jmax (1.5f, s->peaks[(size_t) bin] * 34.0f);
                const float x = wave.getX() + 3.0f + (float) i * (wave.getWidth() - 6.0f) / (float) (bars - 1);
                bp.startNewSubPath (x, wave.getCentreY() - h);
                bp.lineTo (x, wave.getCentreY() + h);
            }
            g.setColour (gold);
            g.strokePath (bp, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

            g.setColour (text);
            g.fillRect (px - 0.6f, wave.getY(), 1.2f, wave.getHeight());
        }

        g.setColour (text);
        g.setFont (fonts::body (12.0f));
        g.drawText (s->name, info.reduced (10.0f, 0.0f).withTrimmedRight (96.0f), juce::Justification::centredLeft, true);
        const auto secs = s->audio.getNumSamples() / s->sampleRate;
        juce::String right = s->loadedAsWavetable ? juce::String (s->table->getNumFrames()) + " fr"
                                                  : shapeshift::noteName (s->rootNote) + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (secs, 1) + " s";
        g.setColour (s->shapeshifted ? gold : muted);
        g.setFont (fonts::mono (10.0f));
        g.drawText (right, info.reduced (10.0f, 0.0f), juce::Justification::centredRight, false);
    }

    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    auto src = processor.getSource();
    g.drawText (src != nullptr && src->shapeshifted ? "Shapeshifted: TABLE is the rebuild, SAMPLE the original"
                                                    : "Drop a sound, or Shapeshift a synth note",
                juce::Rectangle<float> (16.0f, 170.0f, (float) getWidth() - 32.0f, 22.0f),
                juce::Justification::centredLeft, false);
}

// =====================================================================================
EnvelopeRefs EnvelopeRefs::from (juce::AudioProcessorValueTreeState& s, const juce::String& prefix)
{
    auto get = [&] (const juce::String& stage)
    {
        const auto id = prefix.isEmpty() ? stage.substring (0, 1).toLowerCase() + stage.substring (1) : prefix + stage;
        auto* p = s.getParameter (id);
        jassert (p != nullptr);
        return p;
    };
    return { get ("Attack"), get ("Hold"), get ("Decay"), get ("Sustain"), get ("Release"),
             get ("AttackCurve"), get ("DecayCurve"), get ("ReleaseCurve"), get ("Delay"), get ("SustainSlope") };
}

EnvelopeGraph::EnvelopeGraph (EnvelopeRefs e, bool ext, const InstrumentProcessor* v, bool tone)
    : env (e), extended (ext), voices (v), toneEnvelope (tone)
{
    startTimerHz (30);
}

void EnvelopeGraph::timerCallback()
{
    // Repaint only when something visible changed: a parameter, the dimming, or a playing voice
    std::vector<float> now;
    now.reserve (48);
    for (auto* p : env.all()) now.push_back (p->getValue());
    now.push_back (isDimmed && isDimmed() ? 1.0f : 0.0f);
    if (voices != nullptr && isShowing())
        for (const auto& d : voices->envDisplay)
        {
            now.push_back (toneEnvelope ? d.tone.load() : d.amp.load());
            now.push_back (toneEnvelope ? d.toneLevel.load() : d.ampLevel.load());
        }
    if (now != lastSeen)
    {
        lastSeen = std::move (now);
        repaint();
    }
}

EnvelopeGraph::Geometry EnvelopeGraph::geometry() const
{
    Geometry g;
    auto b = getLocalBounds().toFloat().reduced (8.0f, 8.0f);
    g.top = b.getY() + 2.0f;
    g.bottom = b.getBottom() - (getHeight() > 120 ? 14.0f : 0.0f);
    const float h = g.bottom - g.top;
    const float sustainW = b.getWidth() * 0.14f;
    const float extraFactor = extended ? 0.5f : 0.0f;   // delay and hold each get half a segment
    g.minW = 6.0f;
    g.segW = (b.getWidth() - sustainW - 3.0f * g.minW) / (3.0f + 2.0f * extraFactor);

    // Zoom so the envelope fills most of the width (short plucks get room to edit).
    const float wdl = extended ? env.delay->getValue() * g.segW * extraFactor : 0.0f;
    const float wa = g.minW + env.attack->getValue() * g.segW;
    const float wh = extended ? env.hold->getValue() * g.segW * extraFactor : 0.0f;
    const float wd = g.minW + env.decay->getValue() * g.segW;
    const float wr = g.minW + env.release->getValue() * g.segW;
    const float natural = wdl + wa + wh + wd + sustainW + wr;
    const float zoom = dragging != none ? frozenZoom
                                        : juce::jlimit (1.0f, 4.0f, b.getWidth() * 0.9f / juce::jmax (1.0f, natural));
    g.segW *= zoom;
    g.minW *= zoom;

    g.x0 = b.getX();
    g.xDl = g.x0 + wdl * zoom;
    g.xA = g.xDl + wa * zoom;
    g.xH = g.xA + wh * zoom;
    g.xD = g.xH + wd * zoom;
    g.xS = g.xD + sustainW * zoom;
    g.xR = g.xS + wr * zoom;
    g.zoom = zoom;

    const float s = env.sustain->getValue();
    const float slope = valueOf (env.sustainSlope);
    g.sustainEnd = Envelope::sustainAfter (s, slope, 2.0f);
    const float sy = g.bottom - s * h, syEnd = g.bottom - g.sustainEnd * h;
    const float ac = valueOf (env.attackCurve), dc = valueOf (env.decayCurve), rc = valueOf (env.releaseCurve);

    g.point[0] = { g.xDl, g.bottom };
    g.point[1] = { g.xA, g.top };
    g.point[2] = { g.xH, g.top };
    g.point[3] = { g.xD, sy };
    g.point[4] = { g.xS, syEnd };
    g.point[5] = { g.xR, g.bottom };
    g.handle[0] = { (g.xDl + g.xA) * 0.5f, g.bottom - Envelope::shape (0.5f, ac) * h };
    g.handle[1] = { (g.xH + g.xD) * 0.5f, g.bottom - (s + (1.0f - s) * (1.0f - Envelope::shape (0.5f, dc))) * h };
    g.handle[2] = { (g.xD + g.xS) * 0.5f, g.bottom - Envelope::sustainAfter (s, slope, 1.0f) * h };
    g.handle[3] = { (g.xS + g.xR) * 0.5f, g.bottom - g.sustainEnd * (1.0f - Envelope::shape (0.5f, rc)) * h };
    return g;
}

juce::Path EnvelopeGraph::curvePath (const Geometry& g) const
{
    const float h = g.bottom - g.top;
    const float s = env.sustain->getValue();
    const float slope = valueOf (env.sustainSlope);
    const float ac = valueOf (env.attackCurve), dc = valueOf (env.decayCurve), rc = valueOf (env.releaseCurve);
    const int steps = 32;

    juce::Path p;
    p.startNewSubPath (g.x0, g.bottom);
    p.lineTo (g.xDl, g.bottom);
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float) i / steps;
        p.lineTo (g.xDl + (g.xA - g.xDl) * t, g.bottom - Envelope::shape (t, ac) * h);
    }
    p.lineTo (g.xH, g.top);
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float) i / steps;
        p.lineTo (g.xH + (g.xD - g.xH) * t, g.bottom - (s + (1.0f - s) * (1.0f - Envelope::shape (t, dc))) * h);
    }
    for (int i = 1; i <= 16; ++i)   // sustain, tilted by the slope (2 s shown)
    {
        const float t = (float) i / 16;
        p.lineTo (g.xD + (g.xS - g.xD) * t, g.bottom - Envelope::sustainAfter (s, slope, 2.0f * t) * h);
    }
    for (int i = 1; i <= steps; ++i)
    {
        const float t = (float) i / steps;
        p.lineTo (g.xS + (g.xR - g.xS) * t, g.bottom - g.sustainEnd * (1.0f - Envelope::shape (t, rc)) * h);
    }
    return p;
}

void EnvelopeGraph::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    const auto geo = geometry();
    const bool dim = isDimmed && isDimmed();
    const float alpha = dim ? 0.35f : 1.0f;
    const float h = geo.bottom - geo.top;

    // stage dividers
    g.setColour (faint);
    for (float x : { geo.xDl, geo.xA, geo.xH, geo.xD, geo.xS })
        if (x > geo.x0 + 0.5f)
            g.fillRect (x - 0.5f, geo.top, 1.0f, geo.bottom - geo.top);

    auto path = curvePath (geo);
    juce::Path fill (path);
    fill.lineTo (geo.xR, geo.bottom);
    fill.closeSubPath();
    g.setColour (gold.withAlpha (0.12f * alpha));
    g.fillPath (fill);
    g.setColour (gold.withAlpha (alpha));
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // curve handles (the slope handle only matters once there's a sustain to tilt)
    for (int i = 0; i < 4; ++i)
    {
        const bool active = hovered == attackHandle + i || dragging == attackHandle + i;
        auto r = juce::Rectangle<float> (active ? 11.0f : 8.0f, active ? 11.0f : 8.0f).withCentre (geo.handle[i]);
        g.setColour (bg);
        g.fillEllipse (r);
        g.setColour ((active ? gold : text2).withAlpha (alpha));
        if (i == 2) // slope: a diamond, so it reads differently from the curve circles
        {
            juce::Path d;
            const auto c = r.getCentre();
            const float k = r.getWidth() * 0.6f;
            d.addQuadrilateral (c.x, c.y - k, c.x + k, c.y, c.x, c.y + k, c.x - k, c.y);
            g.setColour (bg);
            g.fillPath (d);
            g.setColour ((active ? gold : text2).withAlpha (alpha));
            g.strokePath (d, juce::PathStrokeType (1.5f));
        }
        else
        {
            g.drawEllipse (r, 1.5f);
        }
    }
    // points
    for (int i = 0; i < 6; ++i)
    {
        if ((i == delayPt || i == holdPt) && ! extended) continue;
        const bool active = hovered == i || dragging == i;
        const float rad = active ? 5.5f : 4.0f;
        g.setColour ((active ? gold : text).withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (geo.point[i]));
    }

    // live playheads: one glowing dot per sounding voice, at its stage and level
    if (voices != nullptr)
    {
        const float xs[] = { geo.x0, geo.xDl, geo.xA, geo.xH, geo.xD, geo.xS, geo.xR };
        for (const auto& d : voices->envDisplay)
        {
            const float pos = toneEnvelope ? d.tone.load() : d.amp.load();
            if (pos < 0.0f) continue;
            const int stage = juce::jlimit (0, 5, (int) pos);
            const float frac = pos - (float) stage;
            const float x = xs[stage] + (xs[stage + 1] - xs[stage]) * frac;
            const float level = juce::jlimit (0.0f, 1.0f, toneEnvelope ? d.toneLevel.load() : d.ampLevel.load());
            const juce::Point<float> c (x, geo.bottom - level * h);
            g.setColour (goldHi.withAlpha (0.18f));
            g.fillRect (x - 0.5f, geo.top, 1.0f, geo.bottom - geo.top);
            g.setColour (goldHi.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
            g.setColour (juce::Colour (0xfffffbf0));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
        }
    }

    if (getHeight() > 120)
    {
        g.setColour (muted);
        g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.14f));
        auto lab = [&] (const juce::String& t, float x0, float x1)
        {
            if (x1 - x0 > 12.0f)
                g.drawText (t, juce::Rectangle<float> (x0, geo.bottom + 2.0f, x1 - x0, 12.0f), juce::Justification::centred, false);
        };
        if (extended) lab ("DL", geo.x0, geo.xDl);
        lab ("A", geo.xDl, geo.xA);
        if (extended) lab ("H", geo.xA, geo.xH);
        lab ("D", geo.xH, geo.xD);
        lab ("S", geo.xD, geo.xS);
        lab ("R", geo.xS, geo.xR);
    }

    if (dim)
    {
        g.setColour (muted);
        g.setFont (fonts::body (12.0f));
        g.drawText ("Turn up Amount to hear this envelope", b, juce::Justification::centred, false);
    }
}

EnvelopeGraph::Target EnvelopeGraph::targetAt (juce::Point<float> p) const
{
    const auto geo = geometry();
    Target best = none;
    float bestD = 12.0f;
    for (int i = 0; i < 6; ++i)
    {
        if ((i == delayPt || i == holdPt) && ! extended) continue;
        if (auto d = geo.point[i].getDistanceFrom (p); d < bestD) { bestD = d; best = (Target) i; }
    }
    for (int i = 0; i < 4; ++i)
        if (auto d = geo.handle[i].getDistanceFrom (p); d < bestD) { bestD = d; best = (Target) (attackHandle + i); }
    return best;
}

std::vector<juce::RangedAudioParameter*> EnvelopeGraph::paramsFor (Target t) const
{
    switch (t)
    {
        case delayPt:       return { env.delay };
        case attackPt:      return { env.attack };
        case holdPt:        return { env.hold };
        case decayPt:       return { env.decay, env.sustain };
        case sustainPt:     return { env.sustain };
        case releasePt:     return { env.release };
        case attackHandle:  return { env.attackCurve };
        case decayHandle:   return { env.decayCurve };
        case slopeHandle:   return { env.sustainSlope };
        case releaseHandle: return { env.releaseCurve };
        case none:          break;
    }
    return {};
}

void EnvelopeGraph::mouseMove (const juce::MouseEvent& e)
{
    const auto t = targetAt (e.position);
    if (t == hovered)
        return;
    hovered = t;
    const bool isHandle = t >= attackHandle;
    setMouseCursor (t == none ? juce::MouseCursor::NormalCursor
                              : (isHandle || t == sustainPt ? juce::MouseCursor::UpDownResizeCursor
                                                             : (t == decayPt ? juce::MouseCursor::DraggingHandCursor
                                                                             : juce::MouseCursor::LeftRightResizeCursor)));
    static const char* tips[] = { "Delay: silence before the attack starts. Drag sideways",
                                  "Attack time: drag sideways", "Hold time: drag sideways",
                                  "Decay time (sideways) and sustain level (up/down)", "Sustain level: drag up or down",
                                  "Release time: drag sideways", "Attack curve: drag up for punchy, down for a slow swell",
                                  "Decay curve: drag down for a snappy drop, up for a slow fade",
                                  "Sustain slope: drag down to fade out while held, up to swell",
                                  "Release curve: drag down for a snappy tail, up for a slow fade" };
    setTooltip (t == none ? juce::String() : juce::String (tips[t]) + ". Double-click to reset.");
    repaint();
}

void EnvelopeGraph::mouseExit (const juce::MouseEvent&)
{
    hovered = none;
    repaint();
}

void EnvelopeGraph::mouseDown (const juce::MouseEvent& e)
{
    frozenZoom = geometry().zoom;
    dragging = targetAt (e.position);
    dragStart.clear();
    for (auto* p : paramsFor (dragging))
    {
        dragStart[p] = p->getValue();
        p->beginChangeGesture();
    }
}

void EnvelopeGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == none)
        return;
    const auto geo = geometry();
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float dx = (float) e.getDistanceFromDragStartX() * fine;
    const float dy = (float) e.getDistanceFromDragStartY() * fine;
    const float h = geo.bottom - geo.top;
    auto set = [&] (juce::RangedAudioParameter* p, float delta)
    {
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStart[p] + delta));
    };

    switch (dragging)
    {
        case delayPt:   set (env.delay, dx / (geo.segW * 0.5f)); break;
        case attackPt:  set (env.attack, dx / geo.segW); break;
        case holdPt:    set (env.hold, dx / (geo.segW * 0.5f)); break;
        case decayPt:   set (env.decay, dx / geo.segW); set (env.sustain, -dy / h); break;
        case sustainPt: set (env.sustain, -dy / h); break;
        case releasePt: set (env.release, dx / geo.segW); break;
        case attackHandle:  set (env.attackCurve, -dy / h); break;   // up = punchier rise
        case decayHandle:   set (env.decayCurve, dy / h); break;     // down = snappier drop
        case slopeHandle:   set (env.sustainSlope, -dy / h); break;  // up = swell, down = fade
        case releaseHandle: set (env.releaseCurve, dy / h); break;
        case none: break;
    }
    repaint();
}

void EnvelopeGraph::mouseUp (const juce::MouseEvent&)
{
    for (auto* p : paramsFor (dragging))
        p->endChangeGesture();
    dragging = none;
    repaint();
}

void EnvelopeGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (auto* p : paramsFor (targetAt (e.position)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

// =====================================================================================
ShapePanel::ShapePanel (InstrumentProcessor& p)
    : processor (p),
      ampRefs (EnvelopeRefs::from (p.apvts, {})),
      toneRefs (EnvelopeRefs::from (p.apvts, "tone")),
      ampGraph (ampRefs, true, &p, false),
      toneGraph (toneRefs, false, &p, true)
{
    for (auto* b : { &ampTab, &toneTab })
    {
        addAndMakeVisible (b);
        b->setFontHeight (10.0f);
        b->setLetterSpacing (0.14f);
    }
    addAndMakeVisible (expand);
    addChildComponent (ampGraph);
    addChildComponent (toneGraph);
    ampTab.setTooltip ("Amp envelope: the volume of each note");
    toneTab.setTooltip ("Tone envelope: sweeps the Tone filter on each note");
    expand.setTooltip ("Open the full shape editor");
    expand.setTitle ("Open shape editor");
    toneGraph.isDimmed = [this] { return std::abs (processor.apvts.getRawParameterValue ("toneAmount")->load()) < 0.005f; };

    auto& s = p.apvts;
    ampBoxes.add (new ValueBox (*ampRefs.attack, "A", "Attack time"));
    ampBoxes.add (new ValueBox (*ampRefs.hold, "H", "Hold time at full level"));
    ampBoxes.add (new ValueBox (*ampRefs.decay, "D", "Decay time"));
    ampBoxes.add (new ValueBox (*ampRefs.sustain, "S", "Sustain level"));
    ampBoxes.add (new ValueBox (*ampRefs.release, "R", "Release time"));
    toneBoxes.add (new ValueBox (*s.getParameter ("toneAmount"), "AMT", "How far the envelope opens (or closes) the Tone filter"));
    toneBoxes.add (new ValueBox (*toneRefs.attack, "A", "Tone attack"));
    toneBoxes.add (new ValueBox (*toneRefs.decay, "D", "Tone decay"));
    toneBoxes.add (new ValueBox (*toneRefs.sustain, "S", "Tone sustain"));
    toneBoxes.add (new ValueBox (*toneRefs.release, "R", "Tone release"));
    for (auto* b : ampBoxes) addChildComponent (b);
    for (auto* b : toneBoxes) addChildComponent (b);

    ampTab.onClick = [this] { showTone (false); };
    toneTab.onClick = [this] { showTone (true); };
    expand.onClick = [this] { if (onExpand) onExpand(); };
    showTone (false);
}

void ShapePanel::showTone (bool tone)
{
    toneShown = tone;
    ampTab.setToggleState (! tone, juce::dontSendNotification);
    toneTab.setToggleState (tone, juce::dontSendNotification);
    ampGraph.setVisible (! tone);
    toneGraph.setVisible (tone);
    for (auto* b : ampBoxes) b->setVisible (! tone);
    for (auto* b : toneBoxes) b->setVisible (tone);
    repaint();
}

void ShapePanel::resized()
{
    expand.setBounds (getWidth() - 16 - 28, 11, 28, 28);
    toneTab.setBounds (expand.getX() - 10 - 2 - 50, 13, 50, 24);
    ampTab.setBounds (toneTab.getX() - 48, 13, 48, 24);
    ampGraph.setBounds (16, 46, getWidth() - 32, 74);
    toneGraph.setBounds (ampGraph.getBounds());
    const int w = (getWidth() - 32) / 5;
    for (int i = 0; i < 5; ++i)
    {
        ampBoxes[i]->setBounds (16 + i * w, 124, w, 36);
        toneBoxes[i]->setBounds (16 + i * w, 124, w, 36);
    }
}

void ShapePanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "SHAPE", { 16.0f, 12.0f, 100.0f, 26.0f });
    auto seg = ampTab.getBounds().getUnion (toneTab.getBounds()).toFloat().expanded (2.0f);
    g.setColour (colours::line2);
    g.drawRoundedRectangle (seg, seg.getHeight() * 0.5f, 1.0f);
}

// =====================================================================================
ShapeEditor::ShapeEditor (InstrumentProcessor& p)
    : processor (p),
      ampRefs (EnvelopeRefs::from (p.apvts, {})),
      toneRefs (EnvelopeRefs::from (p.apvts, "tone")),
      ampGraph (ampRefs, true, &p, false),
      toneGraph (toneRefs, true, &p, true)
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (ampGraph);
    addAndMakeVisible (toneGraph);
    addAndMakeVisible (close);
    close.setTooltip ("Close (Esc)");
    close.setTitle ("Close shape editor");
    close.onClick = [this] { if (onClose) onClose(); };
    toneGraph.isDimmed = [this] { return std::abs (processor.apvts.getRawParameterValue ("toneAmount")->load()) < 0.005f; };

    auto& s = p.apvts;
    auto addBoxes = [] (juce::OwnedArray<ValueBox>& arr, const EnvelopeRefs& r, const juce::String& what)
    {
        arr.add (new ValueBox (*r.delay, "DELAY", what + " delay: silence before the attack"));
        arr.add (new ValueBox (*r.attack, "ATTACK", what + " attack time"));
        arr.add (new ValueBox (*r.hold, "HOLD", what + " hold time at full level"));
        arr.add (new ValueBox (*r.decay, "DECAY", what + " decay time"));
        arr.add (new ValueBox (*r.sustain, "SUSTAIN", what + " sustain level"));
        arr.add (new ValueBox (*r.release, "RELEASE", what + " release time"));
        arr.add (new ValueBox (*r.attackCurve, "A CURVE", what + " attack curve"));
        arr.add (new ValueBox (*r.decayCurve, "D CURVE", what + " decay curve"));
        arr.add (new ValueBox (*r.sustainSlope, "S SLOPE", what + " sustain slope: fade out or swell up while the key is held"));
        arr.add (new ValueBox (*r.releaseCurve, "R CURVE", what + " release curve"));
    };
    addBoxes (ampBoxes, ampRefs, "Amp");
    ampBoxes.add (new ValueBox (*s.getParameter ("ampVelocity"), "VELOCITY", "How much playing harder makes notes louder"));
    addBoxes (toneBoxes, toneRefs, "Tone");
    toneBoxes.add (new ValueBox (*s.getParameter ("toneAmount"), "AMOUNT", "How far the envelope opens (positive) or closes (negative) the Tone filter"));
    toneBoxes.add (new ValueBox (*s.getParameter ("toneVelocity"), "VELOCITY", "How much playing harder deepens the sweep"));
    for (auto* b : ampBoxes) { b->framed = true; addAndMakeVisible (b); }
    for (auto* b : toneBoxes) { b->framed = true; addAndMakeVisible (b); }
}

void ShapeEditor::resized()
{
    close.setBounds (getWidth() - 16 - 36, 14, 36, 36);
    const int cardW = (getWidth() - 48) / 2;
    auto layoutCard = [&] (int x, EnvelopeGraph& graph, juce::OwnedArray<ValueBox>& boxes)
    {
        graph.setBounds (x + 16, 110, cardW - 32, 170);
        const int bw = (cardW - 32 - 5 * 6) / 6;
        for (int i = 0; i < boxes.size(); ++i)
        {
            const int row = i < 6 ? 0 : 1;
            const int col = i < 6 ? i : i - 6;
            boxes[i]->setBounds (x + 16 + col * (bw + 6), 290 + row * 50, bw, 42);
        }
    };
    layoutCard (16, ampGraph, ampBoxes);
    layoutCard (32 + cardW, toneGraph, toneBoxes);
}

void ShapeEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);

    drawSectionLabel (g, "SHAPE", { 20.0f, 16.0f, 100.0f, 32.0f });
    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    g.drawText ("Drag points for times and levels, circles to bend curves, the diamond to tilt the sustain. Double-click a number to type it.",
                juce::Rectangle<float> (106.0f, 16.0f, (float) close.getX() - 120.0f, 32.0f), juce::Justification::centredLeft, true);
    g.setColour (line);
    g.fillRect (16, 62, getWidth() - 32, 1);

    const int cardW = (getWidth() - 48) / 2;
    auto card = [&] (int x, const juce::String& title, const juce::String& sub)
    {
        auto r = juce::Rectangle<float> ((float) x, 74.0f, (float) cardW, (float) getHeight() - 90.0f);
        drawPanel (g, r, 14.0f);
        g.setColour (text);
        g.setFont (fonts::display (15.0f).withExtraKerningFactor (0.12f));
        g.drawText (title, juce::Rectangle<float> (r.getX() + 16.0f, r.getY() + 8.0f, 120.0f, 24.0f), juce::Justification::centredLeft, false);
        g.setColour (muted);
        g.setFont (fonts::body (12.0f));
        g.drawText (sub, juce::Rectangle<float> (r.getX() + 90.0f, r.getY() + 8.0f, r.getWidth() - 106.0f, 24.0f), juce::Justification::centredRight, false);
    };
    card (16, "AMP", "The volume of each note");
    card (32 + cardW, "TONE", "Sweeps the Tone filter on each note");
}

bool ShapeEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    return false;
}

// =====================================================================================
InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : SparkEditorBase (p, false), processor (p), source (p), shape (p), shapeEditor (p), synthPage (p, [this] (int src, int dest) { handleModDrop (src, dest); }), fxPage (p)
{
    auto col = leftColumn();
    source.setBounds (col.removeFromTop (252));
    col.removeFromTop (14);
    shape.setBounds (col);
    content().addAndMakeVisible (source);
    content().addAndMakeVisible (shape);

    content().addChildComponent (synthPage);
    synthPage.setBounds (24, 86, 1072, 440);
    content().addChildComponent (fxPage);
    fxPage.setBounds (24, 86, 1072, 440);
    getHeader().onPage = [this] (int page) { showPage (page); };

    content().addChildComponent (shapeEditor);
    shapeEditor.setBounds (24, 86, 1072, 440);
    shapeEditor.onClose = [this] { shapeEditor.setVisible (false); };
    shape.onExpand = [this]
    {
        const bool show = ! shapeEditor.isVisible();
        if (show) setBrowserVisible (false);
        shapeEditor.setVisible (show);
        if (show) shapeEditor.toFront (true);
    };

    source.onImport = [this] { chooseFileToImport(); };
    source.onShapeshift = [this] { startShapeshift(); };
    source.onExport = [this] { exportWavetable(); };
}

bool InstrumentEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (processor.getFormatManager().findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;
    return false;
}

void InstrumentEditor::filesDropped (const juce::StringArray& files, int, int)
{
    source.setDragHover (false);
    for (auto& f : files)
    {
        const juce::File file (f);
        if (processor.getFormatManager().findFormatForFileExtension (file.getFileExtension()) != nullptr)
        {
            loadFile (file);
            return;
        }
    }
}

void InstrumentEditor::loadFile (const juce::File& file)
{
    juce::String error;
    if (! processor.loadFile (file, error))
        showMessage ("Couldn't load that sound", error);
}

void InstrumentEditor::chooseFileToImport()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sample or wavetable",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   processor.getSupportedExtensions());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (auto f = fc.getResult(); f.existsAsFile())
                                  loadFile (f);
                          });
}

void InstrumentEditor::exportWavetable()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Spark").getChildFile ("Wavetables");
    folder.createDirectory();
    auto s = processor.getSource();
    const auto base = (s != nullptr ? juce::File::createLegalFileName (s->name).upToLastOccurrenceOf (".", false, false) : juce::String ("Spark"));
    const auto suggested = folder.getNonexistentChildFile ((base.isEmpty() ? juce::String ("Spark") : base) + " Spark", ".wav", false);

    chooser = std::make_unique<juce::FileChooser> ("Export wavetable", suggested, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;
                              f = f.withFileExtension (".wav");
                              juce::String error;
                              if (processor.exportTable (f, error))
                                  showMessage ("Wavetable saved", f.getFileName() + " is ready for Serum, Vital or Spark.\n\n" + f.getParentDirectory().getFullPathName());
                              else
                                  showMessage ("Couldn't export", error);
                          });
}

void InstrumentEditor::hideOtherOverlays()
{
    shapeEditor.setVisible (false);
    synthPage.setVisible (false);
    fxPage.setVisible (false);
    getHeader().setPage (0);
}

void InstrumentEditor::handleModDrop (int source, int dest)
{
    const int slot = processor.assignModulation (source, dest, processor.defaultAmountFor (dest));
    if (slot < 0)
    {
        showMessage ("All 8 modulation slots are in use", "Clear one in the MOD MATRIX on the SYNTH page, then try again.");
        return;
    }
    if (dest < InstrumentProcessor::numFacetsInstrument)
        getCore().flashFacet (dest);
}

void InstrumentEditor::showPage (int page)
{
    setBrowserVisible (false);
    shapeEditor.setVisible (false);
    synthPage.setVisible (page == 1);
    fxPage.setVisible (page == 2);
    if (page == 1) synthPage.toFront (false);
    if (page == 2) fxPage.toFront (false);
    getHeader().setPage (page);
}

void InstrumentEditor::startShapeshift()
{
    auto current = processor.getSource();
    const bool haveSound = current != nullptr && current->name != "Spark Init";
    if (! haveSound)
    {
        chooser = std::make_unique<juce::FileChooser> ("Shapeshift: choose one note bounced from Serum, Serum 2, Vital or any synth",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       processor.getSupportedExtensions());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (auto f = fc.getResult(); f.existsAsFile()) runShapeshift (f); });
        return;
    }

    juce::PopupMenu m;
    m.setLookAndFeel (&getSparkLookAndFeel());
    m.addItem ("Shapeshift \"" + current->name + "\"", [this] { runShapeshift ({}); });
    m.addItem ("Shapeshift another note...", [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Shapeshift: choose one note bounced from Serum, Serum 2, Vital or any synth",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       processor.getSupportedExtensions());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (auto f = fc.getResult(); f.existsAsFile()) runShapeshift (f); });
    });
    m.addSeparator();
    m.addItem ("How to bounce a note for Shapeshift", [this]
    {
        showMessage ("Shapeshift", "In Serum, Serum 2 or Vital, play and hold one note for about two seconds, then let go and let the tail ring out. "
                                   "Export or bounce that as a WAV and give it to Shapeshift.\n\n"
                                   "Spark finds the note, turns the way the timbre changes into a wavetable it plays through, and matches the envelope and width. "
                                   "Then shape it however you like with the facets, the Shape editor, the effects and Spark.");
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

void InstrumentEditor::runShapeshift (const juce::File& file)
{
    juce::String summary;
    if (processor.shapeshift (file, summary))
        showMessage ("Shapeshifted", summary + "\n\nCompare: TABLE is the rebuild, SAMPLE plays the original. "
                                               "Everything is now yours to reshape: facets, Shape, effects, Spark and Breed.");
    else
        showMessage ("Couldn't Shapeshift that", summary);
}

void InstrumentEditor::addExtraMenuItems (juce::PopupMenu& m)
{
    m.addItem ("Shapeshift...", [this] { startShapeshift(); });
    m.addItem ("Shape editor...", [this] { setBrowserVisible (false); shapeEditor.setVisible (true); shapeEditor.toFront (true); });
    m.addItem ("Load sample or wavetable...", [this] { chooseFileToImport(); });
    m.addItem ("Make wavetable from sample", [this] { processor.makeTableFromSample(); });
    m.addItem ("Export wavetable...", [this] { exportWavetable(); });
}
} // namespace spark

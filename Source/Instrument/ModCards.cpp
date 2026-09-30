#include "ModCards.h"

namespace spark
{
namespace
{
    juce::String shortSource (int s)
    {
        static const juce::StringArray n { "-", "LFO 1", "LFO 2", "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4", "WHEEL", "PRESS", "VEL" };
        return n[s];
    }
}

// =====================================================================================
ModGrip::ModGrip (int s) : source (s)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip (juce::String ("Drag onto a facet (on the ring or in the list) or a knob to modulate it. Hover over the ") + (brand::isObsdn ? "LEAD" : "SOUND") + " tab while dragging to reach the facets.");
}

void ModGrip::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (hover ? gold : raised);
    g.fillEllipse (b);
    g.setColour (hover ? bg : gold);
    // four-way arrows
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.3f, h = r * 0.45f;
    juce::Path p;
    for (int k = 0; k < 4; ++k)
    {
        const float a = juce::MathConstants<float>::halfPi * (float) k;
        const juce::Point<float> dir (std::sin (a), -std::cos (a)), side (std::cos (a), std::sin (a));
        const auto tip = c + dir * r;
        p.addTriangle (tip, tip - dir * h + side * h * 0.8f, tip - dir * h - side * h * 0.8f);
    }
    g.fillPath (p);
    g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre (c));
}

void ModGrip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4)
        return;
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;
    // the drag image: a gold pill with the source's name
    const auto name = mod::sourceNames()[source].toUpperCase();
    const auto font = fonts::body (12.0f, true).withExtraKerningFactor (0.1f);
    const int w = (int) juce::GlyphArrangement::getStringWidth (font, name) + 28;
    juce::Image img (juce::Image::ARGB, w * 2, 56, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        g.setColour (colours::gold);
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, 28.0f, 14.0f);
        g.setColour (colours::bg);
        g.setFont (font);
        g.drawText (name, juce::Rectangle<float> (0.0f, 0.0f, (float) w, 28.0f), juce::Justification::centred, false);
    }
    const juce::Point<int> offset (-w / 2, -14);
    container->startDragging ("mod:" + juce::String (source), this, juce::ScaledImage (img, 2.0), true, &offset);
}

// =====================================================================================
MacroCard::MacroCard (ModHost& p) : SynthCard ("MACROS", {})
{
    for (int m = 0; m < mod::numMacros; ++m)
    {
        addKnob (*p.modProcessor().apvts.getParameter (mod::macroParam (m)), "M" + juce::String (m + 1),
                 "Macro " + juce::String (m + 1) + ": drag its handle onto facets or knobs, then turn this to move them all at once");
        auto* grip = grips.add (new ModGrip (mod::macro1 + m));
        addAndMakeVisible (grip);
    }
}

void MacroCard::resized()
{
    const int n = knobs.size();
    const int w = (getWidth() - 16) / n;
    for (int i = 0; i < n; ++i)
    {
        knobs[i]->setBounds (8 + i * w, 70, w, getHeight() - 76);
        grips[i]->setBounds (8 + i * w + w / 2 - 11, 42, 22, 22);
    }
}

// =====================================================================================
LfoCard::LfoCard (ModHost& p, int l)
    : processor (p), lfo (l), grip (mod::lfo1 + l),
      rate (*p.modProcessor().apvts.getParameter (mod::lfoParam (l, "Rate")), "RATE"),
      division (*p.modProcessor().apvts.getParameter (mod::lfoParam (l, "Div")), "RATE"),
      shapeAttachment (*p.modProcessor().apvts.getParameter (mod::lfoParam (l, "Shape")), [this] (float) { refresh(); }),
      syncAttachment (*p.modProcessor().apvts.getParameter (mod::lfoParam (l, "Sync")), [this] (float) { refresh(); }),
      retrigAttachment (*p.modProcessor().apvts.getParameter (mod::lfoParam (l, "Retrig")), [this] (float) { refresh(); })
{
    for (auto* c : std::initializer_list<juce::Component*> { &grip, &shapeButton, &syncButton, &retrigButton, &rate, &division })
        addAndMakeVisible (c);
    for (auto* b : { &shapeButton, &syncButton, &retrigButton })
        b->setFontHeight (10.0f);
    shapeButton.setTooltip ("LFO shape");
    syncButton.setTooltip ("Sync the rate to your project's tempo");
    retrigButton.setTooltip ("Retrigger: restart the LFO on every note. Off: one LFO shared by all notes");
    rate.setTooltip ("LFO speed");
    division.setTooltip ("LFO speed, in beats and bars");
    shapeButton.onClick = [this] { showShapeMenu(); };
    syncButton.onClick = [this] { syncAttachment.setValueAsCompleteGesture (syncButton.getToggleState() ? 0.0f : 1.0f); };
    retrigButton.onClick = [this] { retrigAttachment.setValueAsCompleteGesture (retrigButton.getToggleState() ? 0.0f : 1.0f); };
    shapeAttachment.sendInitialUpdate();
    syncAttachment.sendInitialUpdate();
    retrigAttachment.sendInitialUpdate();
    startTimerHz (30);
}

void LfoCard::refresh()
{
    // read the parameters themselves (raw values can lag behind while listeners are called)
    auto value = [this] (const char* what)
    {
        auto* p = processor.modProcessor().apvts.getParameter (mod::lfoParam (lfo, what));
        return p->convertFrom0to1 (p->getValue());
    };
    shapeButton.setButtonText (mod::shapeNames()[juce::roundToInt (value ("Shape"))]);
    const bool synced = value ("Sync") > 0.5f;
    syncButton.setToggleState (synced, juce::dontSendNotification);
    retrigButton.setToggleState (value ("Retrig") > 0.5f, juce::dontSendNotification);
    rate.setVisible (! synced);
    division.setVisible (synced);
    repaint();
}

void LfoCard::showShapeMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    const int current = juce::roundToInt (processor.modParams.shape[lfo]->load());
    for (int i = 0; i < mod::numShapes; ++i)
        m.addItem (mod::shapeNames()[i], true, i == current, [this, i] { shapeAttachment.setValueAsCompleteGesture ((float) i); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapeButton));
}

void LfoCard::timerCallback()
{
    // only the wave display moves; skip when hidden or when the phase hasn't moved
    if (! isShowing())
        return;
    const float ph = processor.liveLfoPhase[(size_t) lfo].load();
    if (std::abs (ph - shownPhase) > 1.0e-4f)
    {
        shownPhase = ph;
        repaint (waveArea().expanded (6.0f).toNearestInt());
    }
}

juce::Rectangle<float> LfoCard::waveArea() const
{
    return { 88.0f, 84.0f, (float) getWidth() - 102.0f, (float) getHeight() - 104.0f };
}

void LfoCard::resized()
{
    grip.setBounds (getWidth() - 36, 10, 24, 24);
    shapeButton.setBounds (12, 44, 96, 28);
    syncButton.setBounds (114, 44, 58, 28);
    retrigButton.setBounds (178, 44, juce::jmin (64, getWidth() - 190), 28);
    const juce::Rectangle<int> knob (8, 80, 76, getHeight() - 86);
    rate.setBounds (knob);
    division.setBounds (knob);
}

void LfoCard::paint (juce::Graphics& g)
{
    using namespace colours;
    drawCard (g, getLocalBounds().toFloat(), "LFO " + juce::String (lfo + 1), {}, 120.0f);

    // one cycle of the shape, with a dot at the current phase
    const auto area = waveArea();
    g.setColour (line);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    const int shape = juce::roundToInt (processor.modParams.shape[lfo]->load());
    const float held[] = { 0.6f, -0.3f, 0.9f, -0.7f, 0.2f };
    juce::Path wave;
    const int pts = 96;
    for (int i = 0; i <= pts; ++i)
    {
        const float p = (float) i / pts;
        float v;
        if (shape == mod::sampleHold || shape == mod::drift)
        {
            // show a few random steps / drifts
            const float x = p * 4.0f;
            const int k = juce::jmin (3, (int) x);
            v = mod::shapeValue (shape, x - (float) k, held[k], held[k + 1]);
        }
        else
        {
            v = mod::shapeValue (shape, p, 0.0f, 0.0f);
        }
        const juce::Point<float> pt (area.getX() + area.getWidth() * p, area.getCentreY() - v * area.getHeight() * 0.45f);
        if (i == 0) wave.startNewSubPath (pt); else wave.lineTo (pt);
    }
    g.setColour (gold);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool retrig = processor.modParams.retrig[lfo]->load() > 0.5f;
    if (! retrig && shape != mod::sampleHold && shape != mod::drift)
    {
        const float p = processor.liveLfoPhase[(size_t) lfo].load();
        const float v = mod::shapeValue (shape, p, 0.0f, 0.0f);
        const juce::Point<float> dot (area.getX() + area.getWidth() * p, area.getCentreY() - v * area.getHeight() * 0.45f);
        g.setColour (text);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
    }
    g.setColour (muted);
    g.setFont (fonts::mono (9.5f));
    g.drawText (retrig ? "RESTARTS EACH NOTE" : "SHARED BY ALL NOTES",
                juce::Rectangle<float> (area.getX(), area.getBottom() + 4.0f, area.getWidth(), 14.0f), juce::Justification::centredLeft, false);
}

// =====================================================================================
MatrixRow::MatrixRow (ModHost& p, int s)
    : processor (p), slot (s),
      srcAttachment (*p.modProcessor().apvts.getParameter (mod::slotParam (s, "Src")), [this] (float) { repaint(); }),
      dstAttachment (*p.modProcessor().apvts.getParameter (mod::slotParam (s, "Dst")), [this] (float) { repaint(); }),
      amtAttachment (*p.modProcessor().apvts.getParameter (mod::slotParam (s, "Amt")), [this] (float) { repaint(); })
{
    setTooltip ("Click the source or destination to change it. Drag the bar to set the amount (double-click resets). x clears the slot.");
}

int MatrixRow::source() const { return juce::roundToInt (processor.modParams.src[slot]->load()); }
int MatrixRow::dest() const { return juce::roundToInt (processor.modParams.dst[slot]->load()); }

juce::Rectangle<float> MatrixRow::srcArea() const { return { 0.0f, 3.0f, 62.0f, (float) getHeight() - 6.0f }; }
juce::Rectangle<float> MatrixRow::dstArea() const { return { 76.0f, 3.0f, 70.0f, (float) getHeight() - 6.0f }; }
juce::Rectangle<float> MatrixRow::amountArea() const { return { 152.0f, 3.0f, (float) getWidth() - 176.0f, (float) getHeight() - 6.0f }; }
juce::Rectangle<float> MatrixRow::clearArea() const { return { (float) getWidth() - 20.0f, 3.0f, 20.0f, (float) getHeight() - 6.0f }; }

void MatrixRow::paint (juce::Graphics& g)
{
    using namespace colours;
    const int src = source();
    if (src == mod::none)
    {
        g.setColour (line);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f, 3.5f), 8.0f, 1.0f);
        g.setColour (ember);
        g.setFont (fonts::body (11.0f));
        g.drawText ("+ add", getLocalBounds().toFloat(), juce::Justification::centred, false);
        return;
    }
    auto pill = [&] (juce::Rectangle<float> r, const juce::String& t, bool goldPill)
    {
        g.setColour (goldPill ? gold : raised);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (goldPill ? bg : text);
        g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.06f));
        g.drawText (t, r, juce::Justification::centred, false);
    };
    pill (srcArea(), shortSource (src), true);
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), juce::Rectangle<float> (62.0f, 0.0f, 14.0f, (float) getHeight()), juce::Justification::centred, false);
    pill (dstArea(), mod::destNames()[dest()].toUpperCase(), false);

    // bipolar amount bar with the value
    const auto bar = amountArea();
    const float amt = processor.modParams.amt[slot]->load();
    const auto track = bar.withSizeKeepingCentre (bar.getWidth(), 4.0f).translated (0.0f, 5.0f);
    g.setColour (line);
    g.fillRoundedRectangle (track, 2.0f);
    const float mid = track.getCentreX(), end = mid + amt * track.getWidth() * 0.5f;
    g.setColour (gold);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, end), track.getY(), juce::jmax (2.0f, std::abs (end - mid)), track.getHeight()), 2.0f);
    g.setColour (text2);
    g.fillRect (juce::Rectangle<float> (1.0f, 8.0f).withCentre ({ mid, track.getCentreY() }));
    g.setColour (text);
    g.setFont (fonts::mono (10.0f));
    const int pct = juce::roundToInt (amt * 100.0f);
    g.drawText ((pct > 0 ? "+" : "") + juce::String (pct) + "%", bar.withTrimmedBottom (bar.getHeight() * 0.45f), juce::Justification::centred, false);

    g.setColour (muted);
    const auto c = clearArea().getCentre();
    g.drawLine (c.x - 3.5f, c.y - 3.5f, c.x + 3.5f, c.y + 3.5f, 1.4f);
    g.drawLine (c.x - 3.5f, c.y + 3.5f, c.x + 3.5f, c.y - 3.5f, 1.4f);
}

void MatrixRow::showAddMenu()
{
    // Source > Destination, all in one menu
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    for (int s = 1; s < mod::numSources; ++s)
    {
        juce::PopupMenu sub;
        for (int d = 0; d < mod::numDests; ++d)
            sub.addItem (mod::destNames()[d], [this, s, d]
            {
                auto& dp = *processor.modProcessor().apvts.getParameter (mod::slotParam (slot, "Dst"));
                auto& ap = *processor.modProcessor().apvts.getParameter (mod::slotParam (slot, "Amt"));
                dp.setValueNotifyingHost ((float) d / (float) (mod::numDests - 1));
                ap.setValueNotifyingHost (ap.convertTo0to1 (processor.defaultAmountFor (d)));
                srcAttachment.setValueAsCompleteGesture ((float) s);
            });
        m.addSubMenu (mod::sourceNames()[s], sub);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MatrixRow::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (source() == mod::none)
    {
        showAddMenu();
        return;
    }
    if (clearArea().contains (p))
    {
        processor.clearModulation (slot);
        return;
    }
    if (srcArea().contains (p))
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        for (int s = 1; s < mod::numSources; ++s)
            m.addItem (mod::sourceNames()[s], true, s == source(), [this, s] { srcAttachment.setValueAsCompleteGesture ((float) s); });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
        return;
    }
    if (dstArea().contains (p))
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        for (int d = 0; d < mod::numDests; ++d)
            m.addItem (mod::destNames()[d], true, d == dest(), [this, d] { dstAttachment.setValueAsCompleteGesture ((float) d); });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
        return;
    }
    if (amountArea().expanded (4.0f).contains (p))
    {
        draggingAmount = true;
        amtAttachment.beginGesture();
        mouseDrag (e);
    }
}

void MatrixRow::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingAmount)
        return;
    const auto bar = amountArea();
    float v = juce::jlimit (-1.0f, 1.0f, (e.position.x - bar.getCentreX()) / (bar.getWidth() * 0.5f));
    if (e.mods.isShiftDown()) v = std::round (v * 100.0f) / 100.0f;
    if (std::abs (v) < 0.02f) v = 0.0f;   // easy to find zero
    amtAttachment.setValueAsPartOfGesture (v);
}

void MatrixRow::mouseUp (const juce::MouseEvent&)
{
    if (draggingAmount)
        amtAttachment.endGesture();
    draggingAmount = false;
}

void MatrixRow::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (source() != mod::none && amountArea().contains (e.position))
        amtAttachment.setValueAsCompleteGesture (processor.defaultAmountFor (dest()));
}

// =====================================================================================
MatrixCard::MatrixCard (ModHost& p) : processor (p)
{
    for (int s = 0; s < mod::numSlots; ++s)
        addAndMakeVisible (rows.add (new MatrixRow (p, s)));
    addAndMakeVisible (lockButton);
    lockButton.setClickingTogglesState (false);
    lockButton.onClick = [this] { processor.setModLocked (! processor.isModLocked()); };
    processor.modProcessor().addChangeListener (this);
    changeListenerCallback (nullptr);
}

MatrixCard::~MatrixCard() { processor.modProcessor().removeChangeListener (this); }

void MatrixCard::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const bool locked = processor.isModLocked();
    lockButton.setToggleState (locked, juce::dontSendNotification);
    lockButton.setIcon (locked ? Icon::lock : Icon::unlock);
    lockButton.setTooltip (locked ? "Locked: Spark and Breed leave modulation alone" : "Lock: stop Spark and Breed changing modulation amounts, macros and LFO speeds");
    for (auto* r : rows) r->repaint();
}

void MatrixCard::resized()
{
    lockButton.setBounds (getWidth() - 38, 8, 26, 26);
    const int colW = (getWidth() - 24 - 16) / 2;
    const int rowH = (getHeight() - 44 - 8) / 4;
    for (int s = 0; s < mod::numSlots; ++s)
        rows[s]->setBounds (12 + (s / 4) * (colW + 16), 42 + (s % 4) * rowH, colW, rowH);
}

void MatrixCard::paint (juce::Graphics& g)
{
    drawCard (g, getLocalBounds().toFloat(), "MOD MATRIX", {}, 170.0f);
    g.setColour (colours::muted);
    g.setFont (fonts::body (11.0f));
    g.drawText ("Drag a handle onto a facet or knob, or click + add",
                juce::Rectangle<float> (170.0f, 10.0f, (float) lockButton.getX() - 178.0f, 26.0f), juce::Justification::centredRight, true);
}

} // namespace spark

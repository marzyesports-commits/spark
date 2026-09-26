#include "Components.h"

namespace spark
{
// =====================================================================================
juce::Path makeIcon (Icon icon, juce::Rectangle<float> a)
{
    auto sx = [a] (float x) { return a.getX() + x / 24.0f * a.getWidth(); };
    auto sy = [a] (float y) { return a.getY() + y / 24.0f * a.getHeight(); };
    juce::Path p;
    switch (icon)
    {
        case Icon::chevronLeft:
            p.startNewSubPath (sx (15), sy (6)); p.lineTo (sx (9), sy (12)); p.lineTo (sx (15), sy (18));
            break;
        case Icon::chevronRight:
            p.startNewSubPath (sx (9), sy (6)); p.lineTo (sx (15), sy (12)); p.lineTo (sx (9), sy (18));
            break;
        case Icon::chevronDown:
            p.startNewSubPath (sx (6), sy (9)); p.lineTo (sx (12), sy (15)); p.lineTo (sx (18), sy (9));
            break;
        case Icon::cross:
            p.startNewSubPath (sx (6), sy (6)); p.lineTo (sx (18), sy (18));
            p.startNewSubPath (sx (18), sy (6)); p.lineTo (sx (6), sy (18));
            break;
        case Icon::shuffle:
            p.startNewSubPath (sx (3), sy (7)); p.lineTo (sx (7), sy (7));
            p.cubicTo (sx (12), sy (7), sx (12), sy (17), sx (17), sy (17)); p.lineTo (sx (21), sy (17));
            p.startNewSubPath (sx (3), sy (17)); p.lineTo (sx (7), sy (17));
            p.cubicTo (sx (12), sy (17), sx (12), sy (7), sx (17), sy (7)); p.lineTo (sx (21), sy (7));
            p.startNewSubPath (sx (18), sy (4)); p.lineTo (sx (21), sy (7)); p.lineTo (sx (18), sy (10));
            p.startNewSubPath (sx (18), sy (14)); p.lineTo (sx (21), sy (17)); p.lineTo (sx (18), sy (20));
            break;
        case Icon::search:
            p.addEllipse (sx (4), sy (4), sx (16) - sx (4), sy (16) - sy (4));
            p.startNewSubPath (sx (14.5f), sy (14.5f)); p.lineTo (sx (20), sy (20));
            break;
        case Icon::expand:
            p.startNewSubPath (sx (14), sy (4)); p.lineTo (sx (20), sy (4)); p.lineTo (sx (20), sy (10));
            p.startNewSubPath (sx (20), sy (4)); p.lineTo (sx (13), sy (11));
            p.startNewSubPath (sx (10), sy (20)); p.lineTo (sx (4), sy (20)); p.lineTo (sx (4), sy (14));
            p.startNewSubPath (sx (4), sy (20)); p.lineTo (sx (11), sy (13));
            break;
        case Icon::settings:
            p.startNewSubPath (sx (4), sy (7)); p.lineTo (sx (14), sy (7));
            p.startNewSubPath (sx (18), sy (7)); p.lineTo (sx (20), sy (7));
            p.startNewSubPath (sx (4), sy (17)); p.lineTo (sx (8), sy (17));
            p.startNewSubPath (sx (12), sy (17)); p.lineTo (sx (20), sy (17));
            p.addEllipse (sx (14), sy (5), sx (18) - sx (14), sy (9) - sy (5));
            p.addEllipse (sx (8), sy (15), sx (12) - sx (8), sy (19) - sy (15));
            break;
        case Icon::undo:
            p.startNewSubPath (sx (9), sy (14)); p.lineTo (sx (4), sy (9)); p.lineTo (sx (9), sy (4));
            p.startNewSubPath (sx (4), sy (9)); p.lineTo (sx (15), sy (9));
            p.cubicTo (sx (21.7f), sy (9), sx (21.7f), sy (19), sx (15), sy (19));
            p.lineTo (sx (12), sy (19));
            break;
        case Icon::star:
            p = makeFivePointStar (a.reduced (a.getWidth() * 0.08f));
            break;
        case Icon::lock:   p = makeLockPath (a, true); break;
        case Icon::unlock: p = makeLockPath (a, false); break;
        case Icon::upload:
            p.startNewSubPath (sx (12), sy (15)); p.lineTo (sx (12), sy (3));
            p.startNewSubPath (sx (7), sy (8)); p.lineTo (sx (12), sy (3)); p.lineTo (sx (17), sy (8));
            p.startNewSubPath (sx (4), sy (15)); p.lineTo (sx (4), sy (19));
            p.quadraticTo (sx (4), sy (21), sx (6), sy (21)); p.lineTo (sx (18), sy (21));
            p.quadraticTo (sx (20), sy (21), sx (20), sy (19)); p.lineTo (sx (20), sy (15));
            break;
        case Icon::snowflake:
            for (int k = 0; k < 3; ++k)
            {
                const float ang = (float) k * juce::MathConstants<float>::pi / 3.0f;
                const float dx = std::sin (ang) * 9.0f, dy = std::cos (ang) * 9.0f;
                p.startNewSubPath (sx (12 - dx), sy (12 - dy)); p.lineTo (sx (12 + dx), sy (12 + dy));
            }
            break;
        case Icon::none: break;
    }
    return p;
}

// =====================================================================================
PillButton::PillButton (const juce::String& buttonText, Style s, Icon i)
    : juce::Button (buttonText), style (s), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (buttonText.isEmpty())
        setTitle ("button");
}

void PillButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float radius = b.getHeight() * 0.5f;
    juce::Colour fill = juce::Colours::transparentBlack, border = juce::Colours::transparentBlack, ink = colours::text;

    const bool on = getToggleState();
    switch (style)
    {
        case Style::outline:     border = highlighted ? colours::text2 : colours::line2; ink = colours::text; break;
        case Style::goldOutline: border = colours::gold; ink = highlighted ? colours::goldHi : colours::gold; if (highlighted) fill = colours::gold.withAlpha (0.08f); break;
        case Style::goldSolid:   fill = highlighted ? colours::goldHi : colours::gold; ink = colours::bg; break;
        case Style::segment:     if (on) { fill = colours::text; ink = colours::bg; } else { ink = highlighted ? colours::text2 : colours::muted; } break;
        case Style::lockToggle:
            if (on) { fill = colours::text; border = colours::text; ink = colours::bg; }
            else    { border = highlighted ? colours::text2 : colours::line2; ink = highlighted ? colours::text2 : colours::muted; }
            break;
        case Style::ghost:       ink = highlighted ? colours::text : colours::muted; if (highlighted) fill = colours::raised; break;
    }
    if (down) fill = fill.isTransparent() ? colours::raised : fill.darker (0.15f);
    if (! isEnabled()) { ink = ink.withAlpha (0.35f); border = border.withAlpha (0.35f); }

    if (! fill.isTransparent())
    {
        g.setColour (fill);
        g.fillRoundedRectangle (b, radius);
    }
    if (! border.isTransparent())
    {
        g.setColour (border);
        g.drawRoundedRectangle (b, radius, 1.0f);
    }

    const auto label = getButtonText();
    auto font = fonts::body (fontHeight, true).withExtraKerningFactor (kerning);
    const float iconSize = juce::jmin (b.getHeight() * 0.42f, 14.0f);
    const float textWidth = label.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font, label);
    const float gap = (icon != Icon::none && label.isNotEmpty()) ? 6.0f : 0.0f;
    const float total = (icon != Icon::none ? iconSize : 0.0f) + gap + textWidth;
    float x = b.getCentreX() - total * 0.5f;

    g.setColour (ink);
    if (icon != Icon::none)
    {
        auto ia = juce::Rectangle<float> (x, b.getCentreY() - iconSize * 0.5f, iconSize, iconSize);
        auto path = makeIcon (icon, ia);
        if (iconFilled) g.fillPath (path);
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        x += iconSize + gap;
    }
    if (label.isNotEmpty())
    {
        g.setFont (font);
        g.drawText (label, juce::Rectangle<float> (x, b.getY(), textWidth + 2.0f, b.getHeight()), juce::Justification::centredLeft, false);
    }
}

// =====================================================================================
Header::Header (SparkProcessorBase& p, bool isFx) : processor (p), fx (isFx)
{
    for (auto* b : { &prev, &next, &settings })
        addAndMakeVisible (b);
    prev.setTitle ("Previous preset");
    next.setTitle ("Next preset");
    settings.setTitle ("Settings");
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    settings.setTooltip ("Menu");
    for (auto* b : { &soundTab, &synthTab, &fxTab })
    {
        addAndMakeVisible (b);
        b->setFontHeight (11.0f);
        b->setLetterSpacing (0.14f);
    }
    soundTab.setTooltip ("The sound: source, shape, facets");
    synthTab.setTooltip ("Filter, voice mode and glide");
    fxTab.setTooltip ("The effects rack");
    soundTab.onClick = [this] { setPage (0); if (onPage) onPage (0); };
    synthTab.onClick = [this] { setPage (1); if (onPage) onPage (1); };
    fxTab.onClick = [this] { setPage (2); if (onPage) onPage (2); };
    setPage (0);
    prev.onClick = [this] { processor.loadPreset (processor.getPresetIndex() - 1); };
    next.onClick = [this] { processor.loadPreset (processor.getPresetIndex() + 1); };
    settings.onClick = [this] { if (onSettings) onSettings (settings); };
    processor.addChangeListener (this);
}

Header::~Header() { processor.removeChangeListener (this); }

void Header::resized()
{
    auto b = getLocalBounds();
    presetArea = juce::Rectangle<int> (360, 44).withCentre (b.getCentre());
    prev.setBounds (presetArea.getX() + 4, presetArea.getY() + 4, 36, 36);
    next.setBounds (presetArea.getRight() - 40, presetArea.getY() + 4, 36, 36);
    settings.setBounds (b.getRight() - 44, b.getCentreY() - 22, 44, 44);
    kindArea = juce::Rectangle<int> (222, 40).withCentre ({ 0, b.getCentreY() });
    kindArea.setX (settings.getX() - 12 - kindArea.getWidth());
    soundTab.setBounds (kindArea.getX() + 3, kindArea.getY() + 3, 80, 34);
    synthTab.setBounds (soundTab.getRight(), kindArea.getY() + 3, 80, 34);
    fxTab.setBounds (synthTab.getRight(), kindArea.getY() + 3, kindArea.getRight() - 3 - synthTab.getRight(), 34);
}

void Header::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();

    // logo
    g.setColour (gold);
    g.fillPath (makeStarPath ({ 0.0f, b.getCentreY() - 13.0f, 26.0f, 26.0f }));
    g.setColour (text);
    const auto wordmark = fonts::display (26.0f).withExtraKerningFactor (0.22f);
    const float wordWidth = juce::GlyphArrangement::getStringWidth (wordmark, "SPARK");
    g.setFont (wordmark);
    g.drawText ("SPARK", juce::Rectangle<float> (36.0f, 0.0f, wordWidth + 10.0f, b.getHeight()), juce::Justification::centredLeft, false);
    if (fx)
    {
        auto badge = juce::Rectangle<float> (36.0f + wordWidth + 8.0f, b.getCentreY() - 10.0f, 34.0f, 20.0f);
        g.setColour (gold);
        g.drawRoundedRectangle (badge, 10.0f, 1.0f);
        g.setFont (fonts::mono (11.0f).withExtraKerningFactor (0.16f));
        g.drawText ("FX", badge.translated (1.0f, 0.0f), juce::Justification::centred, false);
    }

    // preset pill
    auto pill = presetArea.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (pill, 22.0f);
    g.setColour (line);
    g.drawRoundedRectangle (pill.reduced (0.5f), 22.0f, 1.0f);
    if (presetHover)
    {
        g.setColour (raised);
        g.fillRoundedRectangle (pill.reduced (44.0f, 4.0f), 16.0f);
    }
    g.setColour (muted);
    g.setFont (fonts::mono (10.0f).withExtraKerningFactor (0.18f));
    g.drawText (processor.getPresetCategory().toUpperCase() + juce::String::fromUTF8 ("  \xc2\xb7  GEN ")
                    + juce::String (processor.currentGeneration()).paddedLeft ('0', 2),
                pill.withTrimmedTop (6.0f).withHeight (14.0f), juce::Justification::centred, false);
    const auto nameFont = fonts::body (15.0f, true);
    const auto name = processor.getPresetName();
    const float nameWidth = juce::jmin (220.0f, juce::GlyphArrangement::getStringWidth (nameFont, name));
    g.setColour (presetHover ? gold : text);
    g.setFont (nameFont);
    g.drawText (name, pill.withTrimmedTop (20.0f).withHeight (20.0f).withSizeKeepingCentre (nameWidth + 4.0f, 20.0f),
                juce::Justification::centred, true);
    g.setColour (presetHover ? gold : muted);
    auto chev = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pill.getCentreX() + nameWidth * 0.5f + 12.0f, pill.getY() + 30.0f });
    g.strokePath (makeIcon (Icon::chevronDown, chev), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // SOUND | FX page switch
    auto tag = kindArea.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (tag, 20.0f);
    g.setColour (line);
    g.drawRoundedRectangle (tag.reduced (0.5f), 20.0f, 1.0f);
}

void Header::setPage (int page)
{
    soundTab.setToggleState (page == 0, juce::dontSendNotification);
    synthTab.setToggleState (page == 1, juce::dontSendNotification);
    fxTab.setToggleState (page == 2, juce::dontSendNotification);
}

void Header::mouseMove (const juce::MouseEvent& e)
{
    const bool h = presetArea.reduced (44, 0).contains (e.getPosition());
    if (h != presetHover)
    {
        presetHover = h;
        setMouseCursor (h ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void Header::mouseExit (const juce::MouseEvent&)
{
    presetHover = false;
    repaint();
}

void Header::mouseUp (const juce::MouseEvent& e)
{
    if (presetArea.reduced (44, 0).contains (e.getPosition()) && onBrowse)
        onBrowse();
}

// =====================================================================================
void CoreView::SparkButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    auto circle = b.reduced (9.0f);
    g.setColour (line2);
    g.drawEllipse (b.reduced (0.5f), 1.0f);
    g.setColour (down ? gold.darker (0.2f) : (highlighted ? goldHi : gold));
    g.fillEllipse (circle);

    g.setColour (bg);
    g.fillPath (makeStarPath (juce::Rectangle<float> (26.0f, 26.0f).withCentre ({ circle.getCentreX(), circle.getCentreY() - 12.0f })));
    g.setFont (fonts::display (15.0f).withExtraKerningFactor (0.28f));
    g.drawText ("SPARK", juce::Rectangle<float> (circle.getX() + 4.0f, circle.getCentreY() + 8.0f, circle.getWidth(), 18.0f),
                juce::Justification::centred, false);
}

bool CoreView::SparkButton::hitTest (int x, int y)
{
    return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) <= (float) getWidth() * 0.5f;
}

CoreView::CoreView (SparkProcessorBase& p) : processor (p)
{
    addAndMakeVisible (sparkButton);
    sparkButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    sparkButton.setTooltip ("Roll a new variation. Locked facets stay put.");
    sparkButton.onClick = [this] { processor.spark(); };
    processor.addChangeListener (this);
    processor.getCoreShape (target, ringPoints);
    shown = target;
    startTimerHz (30);
}

CoreView::~CoreView() { processor.removeChangeListener (this); }

void CoreView::resized()
{
    centre = getLocalBounds().toFloat().getCentre();
    sparkButton.setBounds (juce::Rectangle<int> (138, 138).withCentre (centre.toInt()));
}

void CoreView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    ghost = shown;
    ghostAlpha = 0.55f;
    repaint();
}

void CoreView::timerCallback()
{
    processor.getCoreShape (target, ringPoints);
    if (shown.size() != target.size())
        shown = target;
    for (size_t i = 0; i < shown.size(); ++i)
        shown[i] += (target[i] - shown[i]) * 0.3f;
    ghostAlpha = juce::jmax (0.14f, ghostAlpha * 0.97f);
    repaint();
}

juce::Path CoreView::ringPath (const std::vector<float>& shape, float radius, float amp) const
{
    juce::Path p;
    const int n = (int) shape.size();
    for (int i = 0; i < n; ++i)
    {
        const float th = (float) i / (float) n * juce::MathConstants<float>::twoPi;
        const float r = radius + amp * juce::jlimit (-1.2f, 1.2f, shape[(size_t) i]);
        const juce::Point<float> pt (centre.x + r * std::sin (th), centre.y - r * std::cos (th));
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    return p;
}

void CoreView::paint (juce::Graphics& g)
{
    using namespace colours;
    const float arcR = 186.0f, labelR = 164.0f;

    g.setColour (faint);
    g.drawEllipse (juce::Rectangle<float> (210.0f, 210.0f).withCentre (centre), 1.0f);

    if (! ghost.empty())
    {
        g.setColour (text.withAlpha (ghostAlpha * 0.35f));
        g.strokePath (ringPath (ghost, 105.0f, 32.0f), juce::PathStrokeType (1.0f));
    }
    g.setColour (gold);
    g.strokePath (ringPath (shown, 105.0f, 32.0f), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));

    const auto& facets = processor.getFacets();
    for (int i = 0; i < numFacets; ++i)
    {
        const float c = juce::degreesToRadians ((float) i * 45.0f);
        const float a0 = c - juce::degreesToRadians (19.0f);
        const float span = juce::degreesToRadians (38.0f);
        const float v = processor.facetParam (i).getValue();
        const bool locked = processor.isLocked (i);
        const bool active = (i == hovered || i == dragging);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a0 + span, true);
        g.setColour (active ? line2 : line);
        g.strokePath (track, juce::PathStrokeType (active ? 10.0f : 8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a0 + juce::jmax (0.001f, span * v), true);
        g.setColour (locked ? text2 : gold);
        g.strokePath (fill, juce::PathStrokeType (active ? 10.0f : 8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const juce::Point<float> lp (centre.x + labelR * std::sin (c), centre.y - labelR * std::cos (c));
        g.setFont (fonts::mono (10.0f).withExtraKerningFactor (0.15f));
        g.setColour (active ? gold : (locked ? muted : text));
        const auto labelArea = juce::Rectangle<float> (90.0f, 14.0f).withCentre (lp);
        if (active)
        {
            g.drawText (facets[(size_t) i].name, labelArea.translated (0.0f, -6.0f), juce::Justification::centred, false);
            g.setColour (text);
            g.drawText (processor.facetParam (i).getCurrentValueAsText(), labelArea.translated (0.0f, 7.0f), juce::Justification::centred, false);
        }
        else
        {
            g.drawText (facets[(size_t) i].name, labelArea, juce::Justification::centred, false);
        }
        if (locked)
        {
            g.setColour (muted);
            auto lockArea = juce::Rectangle<float> (9.0f, 9.0f).withCentre (lp.translated (0.0f, active ? 19.0f : 12.0f));
            g.strokePath (makeLockPath (lockArea, true), juce::PathStrokeType (1.3f));
        }
    }
}

int CoreView::facetAt (juce::Point<float> p) const
{
    const float d = p.getDistanceFrom (centre);
    if (d < 148.0f || d > 200.0f)
        return -1;
    float angle = std::atan2 (p.x - centre.x, centre.y - p.y); // 0 at top, clockwise
    if (angle < 0) angle += juce::MathConstants<float>::twoPi;
    return juce::roundToInt (juce::radiansToDegrees (angle) / 45.0f) % numFacets;
}

void CoreView::updateTooltip()
{
    if (hovered >= 0)
    {
        const auto& f = processor.getFacets()[(size_t) hovered];
        setTooltip (f.tooltip + ". Drag up or down, double-click to reset.");
    }
    else
    {
        setTooltip ({});
    }
}

void CoreView::mouseMove (const juce::MouseEvent& e)
{
    const int h = facetAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
        updateTooltip();
        repaint();
    }
}

void CoreView::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    updateTooltip();
    repaint();
}

void CoreView::mouseDown (const juce::MouseEvent& e)
{
    dragging = facetAt (e.position);
    if (dragging >= 0)
    {
        dragStartValue = processor.facetParam (dragging).getValue();
        processor.facetParam (dragging).beginChangeGesture();
    }
}

void CoreView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const float sensitivity = e.mods.isShiftDown() ? 0.001f : 0.004f;
    const float v = juce::jlimit (0.0f, 1.0f, dragStartValue - (float) e.getDistanceFromDragStartY() * sensitivity);
    processor.facetParam (dragging).setValueNotifyingHost (v);
    repaint();
}

void CoreView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        processor.facetParam (dragging).endChangeGesture();
    dragging = -1;
    repaint();
}

void CoreView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int f = facetAt (e.position);
    if (f < 0)
        return;
    auto& p = processor.facetParam (f);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.getDefaultValue());
    p.endChangeGesture();
}

void CoreView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int f = facetAt (e.position);
    if (f < 0)
        return;
    auto& p = processor.facetParam (f);
    p.beginChangeGesture();
    p.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p.getValue() + w.deltaY * 0.05f));
    p.endChangeGesture();
}

// =====================================================================================
AmountSlider::AmountSlider (juce::RangedAudioParameter& p, const juce::String& l, const juce::String& tip)
    : param (p),
      attachment (p, [this] (float v) { value = param.convertTo0to1 (v); repaint(); }),
      label (l)
{
    setTooltip (tip);
    setTitle (l);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    attachment.sendInitialUpdate();
}

void AmountSlider::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (panel);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (line);
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

    auto inner = b.reduced (14.0f, 0.0f);
    g.setColour (text);
    g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.16f));
    g.drawText (label, inner.removeFromLeft (56.0f), juce::Justification::centredLeft, false);
    g.setColour (text2);
    g.setFont (fonts::mono (11.0f));
    g.drawText (juce::String (juce::roundToInt (value * 100.0f)) + "%", inner.removeFromRight (36.0f), juce::Justification::centredRight, false);

    auto bar = inner.reduced (4.0f, 0.0f).withSizeKeepingCentre (inner.getWidth() - 8.0f, 4.0f);
    g.setColour (line);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (gold);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * value), 2.0f);
}

void AmountSlider::mouseDown (const juce::MouseEvent&)
{
    dragStart = value;
    attachment.beginGesture();
}

void AmountSlider::mouseDrag (const juce::MouseEvent& e)
{
    const float v = juce::jlimit (0.0f, 1.0f, dragStart + (float) e.getDistanceFromDragStartX() / 180.0f);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (v));
}

void AmountSlider::mouseUp (const juce::MouseEvent&) { attachment.endGesture(); }

void AmountSlider::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

// =====================================================================================
ValueBox::ValueBox (juce::RangedAudioParameter& p, const juce::String& l, const juce::String& tip)
    : param (p),
      attachment (p, [this] (float) { repaint(); }),
      label (l)
{
    setTooltip (tip.isNotEmpty() ? tip + ". Drag up or down, Shift for fine, double-click to reset." : juce::String());
    setTitle (l);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ValueBox::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    if (framed || hover || dragging)
    {
        g.setColour (dragging ? selected : (hover ? raised : panel));
        g.fillRoundedRectangle (b.reduced (0.5f), 8.0f);
        g.setColour (dragging ? gold : (hover ? line2 : line));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }
    auto top = b.withHeight (b.getHeight() * 0.45f);
    auto bottom = b.withTrimmedTop (b.getHeight() * 0.42f);
    g.setColour (dragging || hover ? gold : muted);
    g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.12f));
    g.drawText (label, top.translated (0.0f, 2.0f), juce::Justification::centred, false);
    g.setColour (text);
    g.setFont (fonts::mono (11.0f));
    g.drawText (param.getCurrentValueAsText(), bottom.translated (0.0f, -1.0f), juce::Justification::centred, false);
}

void ValueBox::mouseDown (const juce::MouseEvent&)
{
    dragStart = param.getValue();
    dragging = true;
    attachment.beginGesture();
    repaint();
}

void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    const float sensitivity = e.mods.isShiftDown() ? 0.0008f : 0.005f;
    const float v = juce::jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() * sensitivity);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (v));
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
    dragging = false;
    repaint();
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void ValueBox::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float step = e.mods.isShiftDown() ? 0.005f : 0.03f;
    const float v = juce::jlimit (0.0f, 1.0f, param.getValue() + (w.deltaY > 0 ? step : -step));
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (v));
}

// =====================================================================================
ArcKnob::ArcKnob (juce::RangedAudioParameter& p, const juce::String& l)
    : param (p), attachment (p, [this] (float) { repaint(); }), label (l)
{
    setTitle (p.getName (64));
    setTooltip (p.getName (64) + ". Drag up or down, Shift for fine, double-click to reset.");
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ArcKnob::paint (juce::Graphics& g)
{
    using namespace colours;
    const bool active = ! isActive || isActive();
    auto b = getLocalBounds().toFloat();
    const float size = juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 30.0f, 52.0f);
    const juce::Point<float> c (b.getCentreX(), b.getY() + 4.0f + size * 0.5f);
    const float r = size * 0.5f - 3.0f;
    const float a0 = juce::degreesToRadians (-135.0f), a1 = juce::degreesToRadians (135.0f);
    const float v = param.getValue();

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val;
    val.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a0 + (a1 - a0) * juce::jmax (0.002f, v), true);
    g.setColour (active ? (dragging || hover ? goldHi : gold) : muted);
    g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (raised);
    g.fillEllipse (juce::Rectangle<float> (r * 1.3f, r * 1.3f).withCentre (c));
    const float ang = a0 + (a1 - a0) * v;
    const juce::Point<float> tip (c.x + std::sin (ang) * r * 0.5f, c.y - std::cos (ang) * r * 0.5f);
    g.setColour (active ? text : muted);
    g.drawLine ({ c.getPointOnCircumference (r * 0.2f, ang), tip }, 2.0f);

    auto textArea = b.withTrimmedTop (size + 6.0f);
    g.setColour (active ? text2 : muted);
    g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.12f));
    g.drawText (label, textArea.removeFromTop (13.0f), juce::Justification::centred, false);
    g.setColour (active ? text : muted);
    g.setFont (fonts::mono (10.0f));
    g.drawText (param.getCurrentValueAsText(), textArea.removeFromTop (13.0f), juce::Justification::centred, false);
}

void ArcKnob::mouseDown (const juce::MouseEvent&)
{
    dragStart = param.getValue();
    dragging = true;
    attachment.beginGesture();
    repaint();
}

void ArcKnob::mouseDrag (const juce::MouseEvent& e)
{
    const float sensitivity = e.mods.isShiftDown() ? 0.0008f : 0.005f;
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() * sensitivity)));
}

void ArcKnob::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
    dragging = false;
    repaint();
}

void ArcKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void ArcKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float step = e.mods.isShiftDown() ? 0.005f : 0.03f;
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, param.getValue() + (w.deltaY > 0 ? step : -step))));
}

// =====================================================================================
FacetList::FacetList (SparkProcessorBase& p) : processor (p)
{
    for (int i = 0; i < numFacets; ++i)
    {
        auto* b = lockButtons.add (new PillButton ({}, PillButton::Style::lockToggle, Icon::unlock));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { processor.setLocked (i, ! processor.isLocked (i)); };
        addAndMakeVisible (b);
    }
    processor.addChangeListener (this);
    changeListenerCallback (nullptr);
    startTimerHz (20);
}

FacetList::~FacetList() { processor.removeChangeListener (this); }

void FacetList::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto& facets = processor.getFacets();
    for (int i = 0; i < numFacets; ++i)
    {
        const bool locked = processor.isLocked (i);
        auto* b = lockButtons[i];
        b->setToggleState (locked, juce::dontSendNotification);
        b->setIcon (locked ? Icon::lock : Icon::unlock);
        const auto name = facets[(size_t) i].name.toLowerCase();
        b->setTitle ((locked ? "Unlock " : "Lock ") + name);
        b->setTooltip (locked ? "Locked: Spark and Breed leave " + name + " alone" : "Lock " + name + " so Spark leaves it alone");
    }
    repaint();
}

juce::Rectangle<int> FacetList::rowBounds (int i) const
{
    return { 16, 50 + i * 47, getWidth() - 32, 47 };
}

juce::Rectangle<int> FacetList::barBounds (int i) const
{
    auto r = rowBounds (i);
    return { r.getX(), r.getY() + 30, r.getWidth() - 44, 4 };
}

void FacetList::resized()
{
    for (int i = 0; i < numFacets; ++i)
    {
        auto r = rowBounds (i);
        lockButtons[i]->setBounds (r.getRight() - 32, r.getCentreY() - 16, 32, 32);
    }
}

void FacetList::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "FACETS", { 16.0f, 14.0f, 120.0f, 26.0f });
    const int locked = processor.numLocked();
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText (locked > 0 ? juce::String (locked) + " locked" : "All free",
                juce::Rectangle<float> ((float) getWidth() - 136.0f, 14.0f, 120.0f, 26.0f), juce::Justification::centredRight, false);

    const auto& facets = processor.getFacets();
    for (int i = 0; i < numFacets; ++i)
    {
        auto r = rowBounds (i);
        g.setColour (faint);
        g.fillRect (r.getX(), r.getY(), r.getWidth(), 1);

        const auto& f = facets[(size_t) i];
        const auto name = f.name.substring (0, 1) + f.name.substring (1).toLowerCase();
        auto textRow = juce::Rectangle<int> (r.getX(), r.getY() + 8, r.getWidth() - 44, 18);
        g.setColour (text);
        g.setFont (fonts::body (13.0f, true));
        g.drawText (name, textRow, juce::Justification::centredLeft, false);
        g.setColour (text2);
        g.setFont (fonts::mono (11.0f));
        g.drawText (processor.facetParam (i).getCurrentValueAsText(), textRow, juce::Justification::centredRight, false);

        auto bar = barBounds (i).toFloat();
        g.setColour (line);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (processor.isLocked (i) ? text2 : gold);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * processor.facetParam (i).getValue())), 2.0f);
    }
}

int FacetList::rowAt (juce::Point<int> p) const
{
    for (int i = 0; i < numFacets; ++i)
        if (rowBounds (i).withTrimmedRight (44).contains (p))
            return i;
    return -1;
}

void FacetList::setFromX (int row, int x)
{
    auto bar = barBounds (row);
    const float v = juce::jlimit (0.0f, 1.0f, (float) (x - bar.getX()) / (float) bar.getWidth());
    processor.facetParam (row).setValueNotifyingHost (v);
}

void FacetList::mouseDown (const juce::MouseEvent& e)
{
    dragging = rowAt (e.getPosition());
    if (dragging >= 0)
    {
        processor.facetParam (dragging).beginChangeGesture();
        setFromX (dragging, e.x);
    }
}

void FacetList::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging >= 0)
        setFromX (dragging, e.x);
}

void FacetList::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        processor.facetParam (dragging).endChangeGesture();
    dragging = -1;
}

void FacetList::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    if (row < 0)
        return;
    auto& p = processor.facetParam (row);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.getDefaultValue());
    p.endChangeGesture();
}

// =====================================================================================
LineageStrip::LineageStrip (SparkProcessorBase& p) : processor (p)
{
    addAndMakeVisible (keep);
    addAndMakeVisible (breed);
    keep.setTooltip ("Keep this variation. Kept ones survive and become Breed partners.");
    breed.setTooltip ("Cross this variation with your most recent kept one");
    keep.onClick = [this] { processor.toggleKeepCurrent(); };
    breed.onClick = [this] { processor.breed(); };
    processor.addChangeListener (this);
    changeListenerCallback (nullptr);
}

LineageStrip::~LineageStrip() { processor.removeChangeListener (this); }

void LineageStrip::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto* n = processor.lineage.currentNode();
    const bool kept = n != nullptr && n->kept;
    keep.setButtonText (kept ? "KEPT" : "KEEP");
    keep.setIcon (Icon::star, kept);
    repaint();
}

void LineageStrip::resized()
{
    breed.setBounds (getWidth() - 16 - 84, 12, 84, 30);
    keep.setBounds (breed.getX() - 8 - 84, 12, 84, 30);
}

int LineageStrip::firstVisible() const
{
    const int count = (int) processor.lineage.nodes().size();
    // keep the current variation in view
    int first = juce::jmax (0, count - maxVisible);
    const int cur = processor.lineage.currentIndex();
    if (cur < first) first = cur;
    return first;
}

juce::Rectangle<int> LineageStrip::emberBounds (int slot) const
{
    return { 16 + slot * 100, 52, 90, 86 };
}

int LineageStrip::emberAt (juce::Point<int> p) const
{
    const int count = (int) processor.lineage.nodes().size();
    const int first = firstVisible();
    for (int s = 0; s < maxVisible && first + s < count; ++s)
        if (emberBounds (s).contains (p))
            return first + s;
    return -1;
}

void LineageStrip::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "LINEAGE", { 16.0f, 12.0f, 90.0f, 30.0f });
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText ("Click a spark to go back to it. Keep the ones you love, then breed them.",
                juce::Rectangle<int> (110, 12, keep.getX() - 120, 30), juce::Justification::centredLeft, true);

    const auto& nodes = processor.lineage.nodes();
    const int count = (int) nodes.size();
    const int first = firstVisible();
    const int cur = processor.lineage.currentIndex();

    for (int s = 0; s < maxVisible && first + s < count; ++s)
    {
        const int idx = first + s;
        const auto& n = nodes[(size_t) idx];
        auto r = emberBounds (s).toFloat();
        const bool isCur = idx == cur;

        if (isCur || idx == hovered)
        {
            g.setColour (isCur ? selected : raised);
            g.fillRoundedRectangle (r, 14.0f);
        }
        g.setColour (isCur ? gold : line);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        // glyph
        const auto glyph = makeGlyph (n, 90);
        const juce::Point<float> c (r.getCentreX(), r.getY() + 34.0f);
        juce::Path p;
        for (int i = 0; i < 90; ++i)
        {
            const float th = (float) i / 90.0f * juce::MathConstants<float>::twoPi;
            const float rad = 15.5f + 8.0f * glyph[(size_t) i];
            const juce::Point<float> pt (c.x + rad * std::sin (th), c.y - rad * std::cos (th));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        g.setColour (isCur ? gold : (n.kept ? text : ember));
        g.strokePath (p, juce::PathStrokeType (1.4f));

        juce::String label = juce::String (n.gen).paddedLeft ('0', 2);
        g.setFont (fonts::mono (10.0f));
        g.setColour (isCur ? gold : muted);
        auto labelArea = juce::Rectangle<float> (r.getX(), r.getBottom() - 20.0f, r.getWidth(), 14.0f);
        if (n.kept)
        {
            const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label);
            auto starArea = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ labelArea.getCentreX() - w * 0.5f - 4.0f, labelArea.getCentreY() });
            g.fillPath (makeFivePointStar (starArea));
            labelArea.translate (5.0f, 0.0f);
        }
        g.drawText (label, labelArea, juce::Justification::centred, false);
    }
}

void LineageStrip::mouseMove (const juce::MouseEvent& e)
{
    const int h = emberAt (e.getPosition());
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void LineageStrip::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void LineageStrip::mouseUp (const juce::MouseEvent& e)
{
    const int idx = emberAt (e.getPosition());
    if (idx < 0)
        return;
    if (e.mods.isPopupMenu())
    {
        processor.lineage.toggleKeep (idx);
        processor.sendChangeMessage();
        return;
    }
    processor.recall (idx);
}

// =====================================================================================
PresetBrowser::PresetBrowser (SparkProcessorBase& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    for (auto* b : { &surprise, &save, &close })
        addAndMakeVisible (b);
    addAndMakeVisible (search);
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&tiles, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, colours::line2);

    search.setTextToShowWhenEmpty ("Search presets", colours::muted);
    search.setFont (fonts::body (13.0f));
    search.setIndents (34, 9);
    search.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    search.onFocusLost = [this] { repaint(); };
    search.setColour (juce::TextEditor::textColourId, colours::text);
    search.setColour (juce::TextEditor::highlightColourId, colours::gold.withAlpha (0.3f));
    search.setColour (juce::CaretComponent::caretColourId, colours::gold);
    search.onTextChange = [this] { rebuildList(); };
    search.onReturnKey = [this] { if (! shown.empty()) processor.loadPreset (shown.front()); };
    search.onEscapeKey = [this] { if (search.isEmpty()) { if (onClose) onClose(); } else search.clear(); rebuildList(); };

    surprise.setTooltip ("Load a random preset from this category");
    save.setTooltip ("Save the current sound as your own preset");
    close.setTooltip ("Close (Esc)");
    close.setTitle ("Close presets");
    surprise.onClick = [this]
    {
        processor.loadRandomPreset (search.isEmpty() && selectedCategory != "All" ? selectedCategory : juce::String());
    };
    save.onClick = [this] { if (onSave) onSave(); };
    close.onClick = [this] { if (onClose) onClose(); };

    processor.addChangeListener (this);
}

PresetBrowser::~PresetBrowser() { processor.removeChangeListener (this); }

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        processor.rescanUserPresets();
        refresh (true);
        grabKeyboardFocus();
    }
}

void PresetBrowser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (isVisible())
        refresh (false);
}

void PresetBrowser::refresh (bool jumpToCurrentCategory)
{
    categories = processor.getCategories();
    if (jumpToCurrentCategory || ! categories.contains (selectedCategory))
        selectedCategory = processor.getPresetCategory();
    if (! categories.contains (selectedCategory))
        selectedCategory = categories[0];
    rebuildList();
}

int PresetBrowser::countIn (const juce::String& category) const
{
    int n = 0;
    for (int i = 0; i < processor.getNumPresets(); ++i)
        if (processor.getPreset (i).category == category) ++n;
    return n;
}

void PresetBrowser::rebuildList()
{
    shown.clear();
    const auto query = search.getText().trim().toLowerCase();
    for (int i = 0; i < processor.getNumPresets(); ++i)
    {
        const auto& p = processor.getPreset (i);
        if (query.isNotEmpty())
        {
            if ((p.name + " " + p.hint + " " + p.category).toLowerCase().contains (query))
                shown.push_back (i);
        }
        else if (p.category == selectedCategory)
        {
            shown.push_back (i);
        }
    }
    resized();
    repaint();
    tiles.repaint();
}

juce::Rectangle<int> PresetBrowser::categoryRow (int i) const
{
    return { 16, 70 + i * 30, 196, 28 };
}

juce::Rectangle<int> PresetBrowser::listArea() const
{
    return getLocalBounds().withTrimmedLeft (228).withTrimmedTop (64).reduced (0, 0).withTrimmedRight (16).withTrimmedBottom (16);
}

void PresetBrowser::resized()
{
    close.setBounds (getWidth() - 16 - 36, 14, 36, 36);
    save.setBounds (close.getX() - 10 - 78, 16, 78, 32);
    surprise.setBounds (save.getX() - 8 - 128, 16, 128, 32);
    search.setBounds (228, 15, juce::jmin (300, surprise.getX() - 240), 34);

    auto list = listArea().withTrimmedTop (44);
    viewport.setBounds (list);
    tiles.setSize (list.getWidth() - 8, juce::jmax (list.getHeight(), tiles.requiredHeight ((int) shown.size())));
}

void PresetBrowser::paint (juce::Graphics& g)
{
    using namespace colours;
    auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);

    drawSectionLabel (g, "PRESETS", { 20.0f, 16.0f, 120.0f, 32.0f });
    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText (juce::String (processor.getNumPresets()) + " sounds", juce::Rectangle<float> (106.0f, 16.0f, 110.0f, 32.0f), juce::Justification::centredLeft, false);

    // search field (the editor itself is transparent and sits on top)
    auto field = search.getBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (field, field.getHeight() * 0.5f);
    g.setColour (search.hasKeyboardFocus (true) ? gold : line2);
    g.drawRoundedRectangle (field.reduced (0.5f), field.getHeight() * 0.5f, 1.0f);
    g.setColour (muted);
    auto si = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ (float) search.getX() + 18.0f, (float) search.getBounds().getCentreY() });
    g.strokePath (makeIcon (Icon::search, si), juce::PathStrokeType (1.6f));

    g.setColour (line);
    g.fillRect (16, 62, getWidth() - 32, 1);

    // categories
    const bool searching = search.getText().trim().isNotEmpty();
    for (int i = 0; i < categories.size(); ++i)
    {
        auto r = categoryRow (i).toFloat();
        const bool sel = ! searching && categories[i] == selectedCategory;
        if (sel || i == hoveredCategory)
        {
            g.setColour (sel ? selected : raised);
            g.fillRoundedRectangle (r, 14.0f);
        }
        if (sel)
        {
            g.setColour (gold);
            g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
        }
        g.setColour (sel ? gold : text);
        g.setFont (fonts::body (13.0f, sel));
        g.drawText (categories[i], r.reduced (14.0f, 0.0f), juce::Justification::centredLeft, false);
        g.setColour (sel ? gold : muted);
        g.setFont (fonts::mono (10.0f));
        g.drawText (juce::String (countIn (categories[i])), r.reduced (14.0f, 0.0f), juce::Justification::centredRight, false);
    }

    // heading for the list
    auto head = listArea().removeFromTop (40).toFloat();
    g.setColour (text);
    g.setFont (fonts::display (18.0f).withExtraKerningFactor (0.06f));
    const auto title = searching ? "Results" : selectedCategory;
    g.drawText (title, head.removeFromTop (22.0f), juce::Justification::centredLeft, false);
    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    juce::String sub = searching ? juce::String ((int) shown.size()) + " matching \"" + search.getText().trim() + "\""
                                 : processor.getCategoryHint (selectedCategory);
    if (! searching && selectedCategory == SparkProcessorBase::userCategory && shown.empty())
        sub = "Nothing saved yet. Shape a sound you love and press SAVE.";
    g.drawText (sub, head, juce::Justification::centredLeft, true);
}

void PresetBrowser::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < categories.size(); ++i)
        if (categoryRow (i).contains (e.getPosition())) h = i;
    if (h != hoveredCategory)
    {
        hoveredCategory = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void PresetBrowser::mouseExit (const juce::MouseEvent&)
{
    hoveredCategory = -1;
    repaint();
}

void PresetBrowser::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < categories.size(); ++i)
        if (categoryRow (i).contains (e.getPosition()))
        {
            selectedCategory = categories[i];
            search.clear();
            rebuildList();
            viewport.setViewPosition (0, 0);
            return;
        }
}

bool PresetBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::rightKey || k == juce::KeyPress::upKey || k == juce::KeyPress::leftKey)
    {
        if (shown.empty()) return true;
        auto it = std::find (shown.begin(), shown.end(), processor.getPresetIndex());
        int pos = it == shown.end() ? -1 : (int) (it - shown.begin());
        const int step = (k == juce::KeyPress::downKey) ? 3 : (k == juce::KeyPress::upKey) ? -3 : (k == juce::KeyPress::rightKey ? 1 : -1);
        pos = juce::jlimit (0, (int) shown.size() - 1, pos + step);
        processor.loadPreset (shown[(size_t) pos]);
        return true;
    }
    return false;
}

// ---- tiles
juce::Rectangle<int> PresetBrowser::Tiles::tileBounds (int slot) const
{
    const int cols = 3, gap = 8, h = 58;
    const int w = (getWidth() - gap * (cols - 1)) / cols;
    return { (slot % cols) * (w + gap), (slot / cols) * (h + gap), w, h };
}

int PresetBrowser::Tiles::requiredHeight (int count) const
{
    const int rows = (count + 2) / 3;
    return rows * 66;
}

int PresetBrowser::Tiles::slotAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) owner.shown.size(); ++i)
        if (tileBounds (i).contains (p))
            return i;
    return -1;
}

void PresetBrowser::Tiles::paint (juce::Graphics& g)
{
    using namespace colours;
    const int current = owner.processor.getPresetIndex();
    const bool searching = owner.search.getText().trim().isNotEmpty();
    for (int i = 0; i < (int) owner.shown.size(); ++i)
    {
        const int idx = owner.shown[(size_t) i];
        const auto& p = owner.processor.getPreset (idx);
        auto r = tileBounds (i).toFloat();
        const bool isCur = idx == current;

        g.setColour (isCur ? selected : (i == hovered ? raised : panel));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (isCur ? gold : (i == hovered ? line2 : line));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        auto inner = r.reduced (14.0f, 9.0f);
        if (isCur)
        {
            g.setColour (gold);
            g.fillPath (makeStarPath (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ inner.getRight() - 6.0f, inner.getY() + 9.0f })));
        }
        g.setColour (isCur ? gold : text);
        g.setFont (fonts::body (14.0f, true));
        g.drawText (p.name, inner.removeFromTop (20.0f).withTrimmedRight (isCur ? 18.0f : 0.0f), juce::Justification::centredLeft, true);
        g.setColour (muted);
        g.setFont (fonts::body (11.0f));
        g.drawText (searching ? p.category + juce::String::fromUTF8 (" \xc2\xb7 ") + p.hint : p.hint, inner, juce::Justification::centredLeft, true);
    }
}

void PresetBrowser::Tiles::mouseMove (const juce::MouseEvent& e)
{
    const int h = slotAt (e.getPosition());
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void PresetBrowser::Tiles::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void PresetBrowser::Tiles::mouseUp (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());
    if (slot < 0)
        return;
    const int idx = owner.shown[(size_t) slot];
    const auto& preset = owner.processor.getPreset (idx);

    if (e.mods.isPopupMenu() && preset.file != juce::File())
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        m.addItem ("Show in Finder", [f = preset.file] { f.revealToUser(); });
        m.addItem ("Delete preset", [this, idx] { owner.processor.deleteUserPreset (idx); owner.refresh (false); });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
        return;
    }
    owner.processor.loadPreset (idx);
}

void PresetBrowser::Tiles::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (slotAt (e.getPosition()) >= 0 && owner.onClose)
        owner.onClose();
}

// =====================================================================================
void SparkEditorBase::Content::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
}

SparkEditorBase::SparkEditorBase (SparkProcessorBase& p, bool isFx)
    : juce::AudioProcessorEditor (p),
      sparkProcessor (p),
      header (p, isFx),
      core (p),
      mutate (p.mutateParam(), "MUTATE", "Mutate: how many facets move on each Spark"),
      chaos (p.chaosParam(), "CHAOS", "Chaos: how far they move"),
      facets (p),
      lineage (p),
      browser (p)
{
    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (contentComponent);
    for (auto* c : std::initializer_list<juce::Component*> { &header, &core, &mutate, &chaos, &facets, &lineage })
        contentComponent.addAndMakeVisible (c);

    header.setBounds (24, 20, 1072, 52);
    core.setBounds (360, 86, 400, 400);
    mutate.setBounds (360, 492, 192, 34);
    chaos.setBounds (568, 492, 192, 34);
    facets.setBounds (800, 86, 296, 440);
    lineage.setBounds (24, 540, 1072, 150);

    header.onSettings = [this] (juce::Component& anchor) { showSettingsMenu (anchor); };
    header.onBrowse = [this] { setBrowserVisible (! browser.isVisible()); };

    contentComponent.addChildComponent (browser);
    browser.setBounds (24, 86, 1072, 440);
    browser.onClose = [this] { setBrowserVisible (false); };
    browser.onSave = [this] { promptToSavePreset(); };

    setResizable (true, true);
    setResizeLimits (designWidth * 7 / 10, designHeight * 7 / 10, designWidth * 3 / 2, designHeight * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth, designHeight);
}

SparkEditorBase::~SparkEditorBase()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SparkEditorBase::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
}

void SparkEditorBase::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    contentComponent.setBounds (0, 0, designWidth, designHeight);
    contentComponent.setTransform (juce::AffineTransform::scale (scale));
}

void SparkEditorBase::showSettingsMenu (juce::Component& anchor)
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lookAndFeel);
    addExtraMenuItems (m);
    if (m.getNumItems() > 0)
        m.addSeparator();
    m.addItem ("Browse presets...", [this] { setBrowserVisible (true); });
    m.addItem ("Save preset...", [this] { promptToSavePreset(); });
    juce::PopupMenu presets;
    for (const auto& category : sparkProcessor.getCategories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < sparkProcessor.getNumPresets(); ++i)
            if (sparkProcessor.getPreset (i).category == category)
                sub.addItem (sparkProcessor.getPreset (i).name, true, i == sparkProcessor.getPresetIndex(),
                             [this, i] { sparkProcessor.loadPreset (i); });
        if (sub.getNumItems() > 0)
            presets.addSubMenu (category, sub);
    }
    m.addSubMenu ("Presets", presets);
    m.addItem ("Unlock all facets", [this] { for (int i = 0; i < numFacets; ++i) sparkProcessor.setLocked (i, false); });
    m.addItem ("Clear lineage", [this]
    {
        sparkProcessor.lineage.clear();
        sparkProcessor.lineage.push (sparkProcessor.currentFacetValues(), 1);
        sparkProcessor.sendChangeMessage();
    });
    m.addSeparator();
    m.addItem ("About Spark", [this]
    {
        showMessage ("Spark 1.0", "Drop a sound, hit Spark, keep what you love.\n\n"
                                  "Fonts: Syne, Manrope and JetBrains Mono (SIL Open Font License).");
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&anchor));
}

void SparkEditorBase::setBrowserVisible (bool shouldShow)
{
    if (shouldShow)
        hideOtherOverlays();
    browser.setVisible (shouldShow);
    if (shouldShow)
        browser.toFront (true);
}

void SparkEditorBase::promptToSavePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your sound. It will appear under User in the preset browser.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lookAndFeel);
    const auto current = sparkProcessor.getPresetName();
    w->addTextEditor ("name", sparkProcessor.getPresetCategory() == SparkProcessorBase::userCategory ? current : current + " " + juce::String (sparkProcessor.currentGeneration()), "Name");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            juce::String error;
            if (! sparkProcessor.saveUserPreset (w->getTextEditorContents ("name"), error))
                showMessage ("Couldn't save", error);
            else if (browser.isVisible())
                browser.refresh (true);
        }
    }), true);
}

void SparkEditorBase::showMessage (const juce::String& title, const juce::String& text)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::NoIcon)
                                      .withTitle (title)
                                      .withMessage (text)
                                      .withButton ("OK")
                                      .withAssociatedComponent (this),
                                  nullptr);
}
} // namespace spark

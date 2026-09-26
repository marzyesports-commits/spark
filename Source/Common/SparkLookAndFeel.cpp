#include "SparkLookAndFeel.h"
#include <BinaryData.h>

namespace spark
{
namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr display = juce::Typeface::createSystemTypefaceFor (BinaryData::SyneExtraBold_ttf, BinaryData::SyneExtraBold_ttfSize);
        juce::Typeface::Ptr body    = juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeMedium_ttf, BinaryData::ManropeMedium_ttfSize);
        juce::Typeface::Ptr bold    = juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize);
        juce::Typeface::Ptr mono    = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    };

    Typefaces& typefaces()
    {
        static Typefaces t;
        return t;
    }

    juce::Font fontFrom (juce::Typeface::Ptr tf, float height)
    {
        if (tf == nullptr)
            return juce::Font (juce::FontOptions (height));
        return juce::Font (juce::FontOptions (tf).withHeight (height));
    }
}

namespace fonts
{
    juce::Font display (float h)          { return fontFrom (typefaces().display, h); }
    juce::Font body (float h, bool bold)  { return fontFrom (bold ? typefaces().bold : typefaces().body, h); }
    juce::Font mono (float h)             { return fontFrom (typefaces().mono, h); }
    juce::Font label (float h)            { return mono (h).withExtraKerningFactor (0.2f); }
}

SparkLookAndFeel::SparkLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, selected);
    setColour (juce::PopupMenu::highlightedTextColourId, gold);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line2);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::TextEditor::backgroundColourId, bg);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, line2);
    setColour (juce::Label::textColourId, text);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, selected);
}

juce::Typeface::Ptr SparkLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && typefaces().body != nullptr)
        return f.isBold() ? typefaces().bold : typefaces().body;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void SparkLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line2);
    g.drawRect (0, 0, w, h, 1);
}

void SparkLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                          bool isActive, bool isHighlighted, bool isTicked, bool /*hasSubMenu*/,
                                          const juce::String& text, const juce::String& /*shortcut*/,
                                          const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour (colours::line);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::selected);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
    }

    g.setColour (! isActive ? colours::muted : (isHighlighted ? colours::gold : colours::text));
    g.setFont (fonts::body (14.0f));
    auto textArea = r.reduced (12, 0);
    if (isTicked)
    {
        g.setColour (colours::gold);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 8.0f, (float) r.getCentreY() }));
        g.setColour (isHighlighted ? colours::gold : colours::text);
    }
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);
}

juce::Font SparkLookAndFeel::getPopupMenuFont() { return fonts::body (14.0f); }

void SparkLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line2);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (colours::text);
    g.setFont (fonts::body (13.0f));
    g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 3);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> b, float radius)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (b, radius);
    g.setColour (colours::line);
    g.drawRoundedRectangle (b.reduced (0.5f), radius, 1.0f);
}

void drawSectionLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                       juce::Justification j, juce::Colour c)
{
    g.setColour (c);
    g.setFont (fonts::label (11.0f));
    g.drawText (text, area, j, false);
}

juce::Path makeStarPath (juce::Rectangle<float> a)
{
    // 24x24 design: M12 1 L14.2 9.8 L23 12 L14.2 14.2 L12 23 L9.8 14.2 L1 12 L9.8 9.8 Z
    const float pts[][2] = { { 12, 1 }, { 14.2f, 9.8f }, { 23, 12 }, { 14.2f, 14.2f },
                             { 12, 23 }, { 9.8f, 14.2f }, { 1, 12 }, { 9.8f, 9.8f } };
    juce::Path p;
    for (int i = 0; i < 8; ++i)
    {
        const float x = a.getX() + pts[i][0] / 24.0f * a.getWidth();
        const float y = a.getY() + pts[i][1] / 24.0f * a.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    p.closeSubPath();
    return p;
}

juce::Path makeFivePointStar (juce::Rectangle<float> a)
{
    juce::Path p;
    p.addStar (a.getCentre(), 5, a.getWidth() * 0.22f, a.getWidth() * 0.5f, 0.0f);
    return p;
}

juce::Path makeLockPath (juce::Rectangle<float> a, bool locked)
{
    // 24x24 design, stroked
    auto sx = [a] (float x) { return a.getX() + x / 24.0f * a.getWidth(); };
    auto sy = [a] (float y) { return a.getY() + y / 24.0f * a.getHeight(); };
    juce::Path p;
    p.addRoundedRectangle (sx (5), sy (11), sx (19) - sx (5), sy (21) - sy (11), a.getWidth() * 2.0f / 24.0f);
    p.startNewSubPath (sx (8), sy (11));
    p.lineTo (sx (8), sy (7));
    if (locked)
    {
        p.cubicTo (sx (8), sy (1.7f), sx (16), sy (1.7f), sx (16), sy (7));
        p.lineTo (sx (16), sy (11));
    }
    else
    {
        p.cubicTo (sx (8), sy (2.5f), sx (13.5f), sy (1.5f), sx (15.5f), sy (5));
    }
    return p;
}
} // namespace spark

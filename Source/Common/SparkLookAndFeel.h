#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spark
{
namespace colours
{
    const juce::Colour bg       { 0xff0a0a09 };
    const juce::Colour panel    { 0xff121210 };
    const juce::Colour raised   { 0xff191916 };
    const juce::Colour line     { 0xff24231f };
    const juce::Colour line2    { 0xff34322c };
    const juce::Colour dashed   { 0xff4a473f };
    const juce::Colour faint    { 0xff1c1b18 };
    const juce::Colour text     { 0xfff4f1ea };
    const juce::Colour text2    { 0xffbdb8ad };
    const juce::Colour muted    { 0xff8e897d };
    const juce::Colour ember    { 0xff6e6a61 };
    const juce::Colour selected { 0xff1a1812 };
    const juce::Colour gold     { 0xffd4af37 };
    const juce::Colour goldHi   { 0xfff0cf62 };
}

namespace fonts
{
    juce::Font display (float height);                 // Syne ExtraBold
    juce::Font body    (float height, bool bold = false); // Manrope
    juce::Font mono    (float height);                 // JetBrains Mono
    juce::Font label   (float height = 11.0f);         // spaced mono caps for section labels
}

class SparkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SparkLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};

// Shared drawing helpers
void drawPanel (juce::Graphics&, juce::Rectangle<float> bounds, float radius = 16.0f);
void drawSectionLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                       juce::Justification = juce::Justification::centredLeft,
                       juce::Colour = colours::text2);
juce::Path makeStarPath (juce::Rectangle<float> area);      // the Spark four-point star
juce::Path makeFivePointStar (juce::Rectangle<float> area); // "keep" star
juce::Path makeLockPath (juce::Rectangle<float> area, bool locked);
} // namespace spark

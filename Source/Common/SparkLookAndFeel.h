#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace spark
{
// Two brands share this code. SPARK_THEME_OBSDN (set on the OBSDN target) turns the gold-on-black
// Spark look into OBSDN's jade-on-black; "gold" then means the jade accent.
namespace colours
{
#if SPARK_THEME_OBSDN
    const juce::Colour bg       { 0xff060908 };
    const juce::Colour panel    { 0xff0c110f };
    const juce::Colour raised   { 0xff121916 };
    const juce::Colour line     { 0xff1b2420 };
    const juce::Colour line2    { 0xff29342f };
    const juce::Colour dashed   { 0xff3d4a44 };
    const juce::Colour faint    { 0xff151d1a };
    const juce::Colour text     { 0xffecf5f0 };
    const juce::Colour text2    { 0xffb3c4bb };
    const juce::Colour muted    { 0xff839489 };
    const juce::Colour ember    { 0xff5e6c65 };
    const juce::Colour selected { 0xff0d1c16 };
    const juce::Colour gold     { 0xff1fbf85 };   // jade
    const juce::Colour goldHi   { 0xff72f2c2 };   // lit jade
    const juce::Colour flash    { 0xffeafff6 };   // the white-hot core of a bolt
#else
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
    const juce::Colour flash    { 0xfffffbf0 };
#endif
}

// Words that change with the brand
namespace brand
{
#if SPARK_THEME_OBSDN
    constexpr bool isObsdn = true;
    inline const char* wordmark = "OBSDN";
    inline const char* verb = "Strike";          // what the centre button does
    inline const char* verbCaps = "STRIKE";
    inline const char* variationWord = "shard";  // one variation in the lineage
#else
    constexpr bool isObsdn = false;
    inline const char* wordmark = "SPARK";
    inline const char* verb = "Spark";
    inline const char* verbCaps = "SPARK";
    inline const char* variationWord = "spark";
#endif
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
juce::Path makeGemPath (juce::Rectangle<float> area);       // OBSDN's hex ring mark (even-odd: the centre is hollow)
juce::Path makeLogoPath (juce::Rectangle<float> area);      // the brand mark: star (Spark) or gem (OBSDN)
// OBSDN's centrepiece: a bevelled jade hex ring (the O of OBSDN), lit from the top left.
// 'lift' 0..1 brightens it (hover, a strike). Its outline corners, in units of its radius, are gemPoints().
void drawGem (juce::Graphics&, juce::Rectangle<float> area, float lift);
const std::array<juce::Point<float>, 6>& gemPoints();
juce::Path makeFivePointStar (juce::Rectangle<float> area); // "keep" star
juce::Path makeLockPath (juce::Rectangle<float> area, bool locked);
} // namespace spark

#include "Modulation.h"
#include "Common/SparkLookAndFeel.h"

namespace spark::mod
{
const juce::StringArray& sourceNames()
{
    static const juce::StringArray n { "None", "LFO 1", "LFO 2", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Mod Wheel", "Aftertouch", "Velocity" };
    return n;
}

const juce::StringArray& destNames()
{
#if SPARK_THEME_OBSDN
    // OBSDN's facets sit in the same slots: Pitch, Wave, Detune, Bite, Tone, Drive, Vibrato, Space, Resonance, Volume
    static const juce::StringArray n { "Pitch", "Wave", "Detune", "Bite", "Tone", "Drive", "Vibrato", "Space", "Resonance", "Volume" };
#else
    static const juce::StringArray n { "Pitch", "Position", "Grain", "Morph", "Tone", "Drive", "Motion", "Space", "Resonance", "Volume" };
#endif
    return n;
}

const juce::StringArray& shapeNames()
{
    static const juce::StringArray n { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Drift" };
    return n;
}

const juce::StringArray& divisionNames()
{
    static const juce::StringArray n { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T", "1/4 D", "1/8 D" };
    return n;
}

double divisionBeats (int index)
{
    static const double beats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

float rateHz (float v) { return 0.02f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, v)); }

bool isBipolar (int source) { return source == lfo1 || source == lfo2; }

float shapeValue (int shape, float p, float held, float next) noexcept
{
    switch (shape)
    {
        case sine:       return std::sin (juce::MathConstants<float>::twoPi * p);
        case triangle:   return 1.0f - 4.0f * std::abs (p - 0.5f);
        case sawUp:      return 2.0f * p - 1.0f;
        case sawDown:    return 1.0f - 2.0f * p;
        case square:     return p < 0.5f ? 1.0f : -1.0f;
        case sampleHold: return held;
        case drift:
        {
            const float s = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p);   // smooth glide to the next value
            return held + (next - held) * s;
        }
        default:         return 0.0f;
    }
}

juce::String slotParam (int slot, const char* what) { return "mod" + juce::String (slot + 1) + what; }
juce::String lfoParam (int lfo, const char* what) { return "lfo" + juce::String (lfo + 1) + what; }
juce::String macroParam (int macro) { return "macro" + juce::String (macro + 1); }

void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
    auto signedPct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const int p = roundToInt (v * 100.0f);
        return (p > 0 ? "+" : "") + String (p) + "%";
    });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const float f = rateHz (v);
        return f < 1.0f ? String (f, 2) + " Hz" : String (f, f < 10.0f ? 1 : 0) + " Hz";
    });

    for (int l = 0; l < numLfos; ++l)
    {
        const String name = "LFO " + String (l + 1) + " ";
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { lfoParam (l, "Shape"), 1 }, name + "Shape", shapeNames(), 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lfoParam (l, "Rate"), 1 }, name + "Rate", NormalisableRange<float> (0.0f, 1.0f), 0.5f, hz));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { lfoParam (l, "Sync"), 1 }, name + "Sync", StringArray { "Free", "Sync" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { lfoParam (l, "Div"), 1 }, name + "Division", divisionNames(), 4));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { lfoParam (l, "Retrig"), 1 }, name + "Retrigger", StringArray { "Free", "Retrigger" }, 0));
    }
    for (int m = 0; m < numMacros; ++m)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { macroParam (m), 1 }, "Macro " + String (m + 1), NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    for (int s = 0; s < numSlots; ++s)
    {
        const String name = "Mod " + String (s + 1) + " ";
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { slotParam (s, "Src"), 1 }, name + "Source", sourceNames(), 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { slotParam (s, "Dst"), 1 }, name + "Destination", destNames(), 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { slotParam (s, "Amt"), 1 }, name + "Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, signedPct));
    }
}

void Params::attach (juce::AudioProcessorValueTreeState& apvts)
{
    for (int s = 0; s < numSlots; ++s)
    {
        src[s] = apvts.getRawParameterValue (slotParam (s, "Src"));
        dst[s] = apvts.getRawParameterValue (slotParam (s, "Dst"));
        amt[s] = apvts.getRawParameterValue (slotParam (s, "Amt"));
    }
    for (int l = 0; l < numLfos; ++l)
    {
        shape[l] = apvts.getRawParameterValue (lfoParam (l, "Shape"));
        rate[l] = apvts.getRawParameterValue (lfoParam (l, "Rate"));
        sync[l] = apvts.getRawParameterValue (lfoParam (l, "Sync"));
        division[l] = apvts.getRawParameterValue (lfoParam (l, "Div"));
        retrig[l] = apvts.getRawParameterValue (lfoParam (l, "Retrig"));
    }
    for (int m = 0; m < numMacros; ++m)
        macro[m] = apvts.getRawParameterValue (macroParam (m));
}
} // namespace spark::mod

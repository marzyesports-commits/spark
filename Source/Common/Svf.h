#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace spark
{
// State-variable filter (Simper/Cytomic topology): low-pass, high-pass, band-pass and notch from one core.
struct Svf
{
    float ic1[2] {}, ic2[2] {};
    float a1 = 0, a2 = 0, a3 = 0, k = 1;
    void reset() noexcept { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }
    void set (float hz, float q, double sr) noexcept
    {
        const float g = std::tan (juce::MathConstants<float>::pi * hz / (float) sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    float process (int ch, float v0, int type) noexcept
    {
        const float v3 = v0 - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        switch (type)
        {
            case 1:  return v0 - k * v1 - v2;   // high-pass
            case 2:  return k * v1;             // band-pass, unity gain at the centre
            case 3:  return v0 - k * v1;        // notch
            default: return v2;                 // low-pass
        }
    }
};
} // namespace spark

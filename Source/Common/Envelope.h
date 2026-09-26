#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace spark
{
// Delay-Attack-Hold-Decay-Sustain-Release envelope with a shape control for each moving stage,
// and a sustain slope (fade out or swell up while the key is held).
// Curve 0 = linear; positive = fast start then easing (punchy); negative = slow start then rushing (swelling).
class Envelope
{
public:
    struct Settings
    {
        float attack = 0.01f, hold = 0.0f, decay = 0.3f, sustain = 1.0f, release = 0.2f; // seconds, sustain 0..1
        float attackCurve = 0.0f, decayCurve = 0.0f, releaseCurve = 0.0f;                // -1..1
        float delay = 0.0f;          // seconds before the attack starts
        float sustainSlope = 0.0f;   // -1..1: negative fades towards silence, positive swells towards full
    };

    // Time constant of the sustain slope: gentle drift at small amounts, ~0.25 s at the extremes
    static float slopeSeconds (float slope) noexcept
    {
        const float a = 1.0f - juce::jlimit (0.0f, 1.0f, std::abs (slope));
        return 0.25f + 20.0f * a * a * a;
    }
    // Level after holding the sustain for 'seconds', starting from 'sustain' (for drawing)
    static float sustainAfter (float sustain, float slope, float seconds) noexcept
    {
        if (std::abs (slope) < 1.0e-3f) return sustain;
        const float target = slope > 0.0f ? 1.0f : 0.0f;
        return target + (sustain - target) * std::exp (-seconds / slopeSeconds (slope));
    }

    // For the playhead: stage index (0 delay, 1 attack, 2 hold, 3 decay, 4 sustain, 5 release) plus progress 0..1,
    // or -1 when idle
    float displayPosition() const noexcept
    {
        if (stage == Stage::idle) return -1.0f;
        return (float) ((int) stage - 1) + juce::jlimit (0.0f, 0.999f, progress);
    }

    static float shape (float t, float curve) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (std::abs (curve) < 1.0e-3f)
            return t;
        const float k = curve * 6.0f;
        return (1.0f - std::exp (-k * t)) / (1.0f - std::exp (-k));
    }

    void setSampleRate (double sr) noexcept { sampleRate = sr > 0 ? sr : 44100.0; }
    void reset() noexcept { stage = Stage::idle; level = 0.0f; position = 0.0; }
    bool isActive() const noexcept { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    float getLevel() const noexcept { return level; }

    void noteOn() noexcept
    {
        startLevel = level;
        stage = Stage::delay;
        position = 0.0;
        progress = 0.0f;
    }

    void noteOff() noexcept
    {
        if (stage == Stage::idle)
            return;
        startLevel = level;
        stage = Stage::release;
        position = 0.0;
    }

    float next (const Settings& s) noexcept
    {
        switch (stage)
        {
            case Stage::idle:
                level = 0.0f;
                break;

            case Stage::delay:
            {
                const double len = s.delay * sampleRate;
                if (position >= len)
                {
                    advance (Stage::attack);
                    return next (s);
                }
                level = startLevel;
                progress = (float) (position / juce::jmax (1.0, len));
                ++position;
                break;
            }
            case Stage::attack:
            {
                const double len = juce::jmax (1.0, s.attack * sampleRate);
                progress = (float) (position / len);
                level = startLevel + (1.0f - startLevel) * shape (progress, s.attackCurve);
                if (++position >= len) advance (Stage::hold);
                break;
            }
            case Stage::hold:
            {
                level = 1.0f;
                progress = (float) (position / juce::jmax (1.0, (double) s.hold * sampleRate));
                if (++position >= s.hold * sampleRate) advance (Stage::decay);
                break;
            }
            case Stage::decay:
            {
                const double len = juce::jmax (1.0, s.decay * sampleRate);
                progress = (float) (position / len);
                level = s.sustain + (1.0f - s.sustain) * (1.0f - shape (progress, s.decayCurve));
                if (++position >= len) { advance (Stage::sustain); level = s.sustain; }
                break;
            }
            case Stage::sustain:
            {
                if (std::abs (s.sustainSlope) < 1.0e-3f)
                {
                    level = s.sustain;   // flat: follows the Sustain setting live
                }
                else
                {
                    if (s.sustainSlope != cachedSlope)
                    {
                        cachedSlope = s.sustainSlope;
                        slopeCoeff = 1.0f - std::exp (-1.0f / (slopeSeconds (cachedSlope) * (float) sampleRate));
                    }
                    const float target = s.sustainSlope > 0.0f ? 1.0f : 0.0f;
                    level += (target - level) * slopeCoeff;
                }
                progress = (float) juce::jmin (1.0, position / (2.0 * sampleRate));   // the drawing shows 2 s of sustain
                ++position;
                if (level <= 1.0e-4f) advance (Stage::idle);
                break;
            }
            case Stage::release:
            {
                const double len = juce::jmax (1.0, s.release * sampleRate);
                progress = (float) (position / len);
                level = startLevel * (1.0f - shape (progress, s.releaseCurve));
                if (++position >= len) { advance (Stage::idle); level = 0.0f; }
                break;
            }
        }
        return level;
    }

private:
    enum class Stage { idle, delay, attack, hold, decay, sustain, release };

    void advance (Stage s) noexcept
    {
        stage = s;
        position = 0.0;
        progress = 0.0f;
    }

    Stage stage = Stage::idle;
    double sampleRate = 44100.0, position = 0.0;
    float level = 0.0f, startLevel = 0.0f, progress = 0.0f;
    float cachedSlope = 0.0f, slopeCoeff = 0.0f;
};
} // namespace spark

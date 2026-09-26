#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace spark
{
// Attack-Hold-Decay-Sustain-Release envelope with a shape control for each moving stage.
// Curve 0 = linear; positive = fast start then easing (punchy); negative = slow start then rushing (swelling).
class Envelope
{
public:
    struct Settings
    {
        float attack = 0.01f, hold = 0.0f, decay = 0.3f, sustain = 1.0f, release = 0.2f; // seconds, sustain 0..1
        float attackCurve = 0.0f, decayCurve = 0.0f, releaseCurve = 0.0f;                // -1..1
    };

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
        stage = Stage::attack;
        position = 0.0;
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

            case Stage::attack:
            {
                const double len = juce::jmax (1.0, s.attack * sampleRate);
                level = startLevel + (1.0f - startLevel) * shape ((float) (position / len), s.attackCurve);
                if (++position >= len) advance (Stage::hold);
                break;
            }
            case Stage::hold:
            {
                level = 1.0f;
                if (++position >= s.hold * sampleRate) advance (Stage::decay);
                break;
            }
            case Stage::decay:
            {
                const double len = juce::jmax (1.0, s.decay * sampleRate);
                level = s.sustain + (1.0f - s.sustain) * (1.0f - shape ((float) (position / len), s.decayCurve));
                if (++position >= len) advance (Stage::sustain);
                break;
            }
            case Stage::sustain:
                level = s.sustain;
                if (s.sustain <= 1.0e-4f) advance (Stage::idle);
                break;

            case Stage::release:
            {
                const double len = juce::jmax (1.0, s.release * sampleRate);
                level = startLevel * (1.0f - shape ((float) (position / len), s.releaseCurve));
                if (++position >= len) { advance (Stage::idle); level = 0.0f; }
                break;
            }
        }
        return level;
    }

private:
    enum class Stage { idle, attack, hold, decay, sustain, release };

    void advance (Stage s) noexcept
    {
        stage = s;
        position = 0.0;
    }

    Stage stage = Stage::idle;
    double sampleRate = 44100.0, position = 0.0;
    float level = 0.0f, startLevel = 0.0f;
};
} // namespace spark

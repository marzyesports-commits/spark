#include "Riff.h"
#include <algorithm>

namespace spark::riff
{
// =====================================================================================
const juce::StringArray& styleNames()
{
    static const juce::StringArray n { "Pop", "Trance", "Future", "Drill", "Afro", "Chip", "Anthem",
                                       "Liquid DnB", "Dancefloor DnB", "Neuro DnB", "Jump Up", "Dubstep", "UK Garage" };
    return n;
}

const juce::StringArray& styleHints()
{
    static const juce::StringArray n {
        "Syncopated 8ths and 16ths that move mostly by step: a singable topline",
        "Driving 16ths that jump between chord notes: gated trance and EDM hooks",
        "Dotted, off-beat rhythms with octave jumps and slides: future bass and melodic dubstep",
        "Triplet rolls, slides and tight minor lines: drill and trap",
        "3-3-2 bounce and call-and-response: afrobeats and amapiano",
        "Fast arpeggios with big leaps: chiptune and video-game leads",
        "Long notes and wide intervals: festival and sing-along hooks",
        "Flowing, soulful lines that move by step: liquid drum & bass (170-176 bpm)",
        "Catchy repeated-note hooks with octave drops: dancefloor DnB (170-176 bpm)",
        "Stabbing 16ths on the two-step, with rolls: neurofunk (170-176 bpm)",
        "Bouncy call and response with drops and slides: jump up (170-176 bpm)",
        "Half-time hits and 16th rolls: dubstep and riddim (140-150 bpm)",
        "Shuffled 2-step skips and stabs: UK garage and bass house (130-135 bpm)" };
    return n;
}

const juce::StringArray& scaleNames()
{
    static const juce::StringArray n { "Major", "Minor", "Dorian", "Phrygian", "Mixolydian", "Harmonic Minor",
                                       "Minor Pentatonic", "Major Pentatonic", "Blues" };
    return n;
}

const juce::StringArray& keyNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

const std::vector<int>& scaleSteps (int scale)
{
    static const std::vector<std::vector<int>> s {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 }, { 0, 3, 5, 6, 7, 10 } };
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, scale)];
}

// =====================================================================================
namespace
{
    int floorDiv (int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

    struct ScaleInfo
    {
        int size = 7;
        std::vector<int> chordTones;   // degrees 0..size-1 that are root, third or fifth
        bool isChordTone (int degree) const
        {
            const int d = degree - floorDiv (degree, size) * size;
            return std::find (chordTones.begin(), chordTones.end(), d) != chordTones.end();
        }
        int fifth() const { return chordTones.size() > 2 ? chordTones[2] : chordTones.back(); }
    };

    ScaleInfo infoFor (int scale)
    {
        ScaleInfo s;
        const auto& steps = scaleSteps (scale);
        s.size = (int) steps.size();
        for (int i = 0; i < s.size; ++i)
            if (steps[(size_t) i] == 0 || steps[(size_t) i] == 3 || steps[(size_t) i] == 4 || steps[(size_t) i] == 7)
                s.chordTones.push_back (i);
        return s;
    }

    // How each style moves: probabilities of repeat / step / skip / leap / octave
    struct Motion { float repeat, step, skip, leap, octave, slide; };
    Motion motionFor (int style)
    {
        switch (style)
        {
            case trance: return { 0.10f, 0.25f, 0.35f, 0.20f, 0.10f, 0.03f };
            case future: return { 0.15f, 0.30f, 0.20f, 0.15f, 0.20f, 0.25f };
            case drill:  return { 0.25f, 0.45f, 0.15f, 0.10f, 0.05f, 0.30f };
            case afro:   return { 0.20f, 0.45f, 0.25f, 0.08f, 0.02f, 0.06f };
            case chip:   return { 0.05f, 0.20f, 0.35f, 0.25f, 0.15f, 0.02f };
            case anthem: return { 0.15f, 0.35f, 0.20f, 0.25f, 0.05f, 0.10f };
            case liquid: return { 0.12f, 0.55f, 0.20f, 0.10f, 0.03f, 0.14f };
            case dancefloor: return { 0.32f, 0.28f, 0.14f, 0.08f, 0.18f, 0.16f };
            case neuro:  return { 0.35f, 0.18f, 0.15f, 0.12f, 0.20f, 0.12f };
            case jumpUp: return { 0.38f, 0.15f, 0.12f, 0.10f, 0.25f, 0.30f };
            case dubstep:return { 0.40f, 0.15f, 0.12f, 0.13f, 0.20f, 0.35f };
            case garage: return { 0.22f, 0.40f, 0.23f, 0.10f, 0.05f, 0.08f };
            default:     return { 0.18f, 0.50f, 0.20f, 0.10f, 0.02f, 0.08f };
        }
    }

    // Onset weights for one bar. Most styles use 16 sixteenths; Drill uses 12 eighth-note triplets.
    const std::vector<float>& gridFor (int style)
    {
        static const std::vector<std::vector<float>> g {
            { 1, 0, .35f, .25f, .7f, .1f, .55f, .3f, .8f, 0, .45f, .25f, .6f, .15f, .5f, .3f },   // pop
            { 1, .4f, .7f, .5f, .9f, .4f, .7f, .5f, 1, .4f, .7f, .5f, .9f, .4f, .7f, .5f },     // trance
            { 1, 0, 0, .7f, 0, 0, .8f, 0, .5f, 0, .7f, 0, .6f, .2f, .3f, .2f },                  // future
            { 1, 0, .6f, .5f, .4f, .3f, .8f, 0, .5f, .4f, .6f, .5f },                             // drill (triplets)
            { 1, 0, 0, .85f, 0, 0, .85f, 0, .3f, 0, .7f, 0, .6f, 0, .4f, 0 },                   // afro
            { 1, .8f, .8f, .8f, 1, .8f, .8f, .8f, 1, .8f, .8f, .8f, 1, .8f, .8f, .8f },         // chip
            { 1, 0, 0, 0, .6f, 0, .3f, 0, .9f, 0, 0, 0, .6f, 0, .4f, .1f },                    // anthem
            { 1, 0, .2f, .5f, .3f, 0, .6f, .2f, .45f, 0, .7f, .2f, .5f, .1f, .4f, .2f },        // liquid dnb
            { 1, 0, 0, .55f, 0, 0, .85f, 0, .5f, 0, .85f, .3f, 0, .5f, .75f, .3f },            // dancefloor dnb: off-beat bounce with 16th pickups
            { 1, 0, .5f, .7f, .2f, .3f, .7f, .2f, .3f, .4f, .9f, .6f, .2f, .3f, .8f, .5f },     // neuro: around the two-step kick (1, 3&) and snare (2, 4)
            { 1, 0, 0, .6f, 0, 0, .9f, 0, .4f, 0, .9f, 0, .3f, .4f, .7f, 0 },                   // jump up
            { 1, 0, 0, .5f, 0, 0, .7f, 0, .15f, 0, .6f, .5f, 0, .5f, .7f, .6f },               // dubstep: half-time, busy into the bar end
            { 1, 0, 0, .7f, 0, .3f, .6f, 0, .4f, 0, .8f, 0, 0, .6f, .5f, 0 } };                 // uk garage: 2-step skips
        return g[(size_t) juce::jlimit (0, (int) g.size() - 1, style)];
    }

    // One bar of onsets (ticks from the bar start)
    std::vector<int> makeBarRhythm (juce::Random& r, int style, float density)
    {
        const auto& grid = gridFor (style);
        const int steps = (int) grid.size();
        const int stepTicks = ticksPerBar / steps;
        const float scale = 0.3f + density * 1.4f;
        std::vector<int> out;
        for (int s = 0; s < steps; ++s)
        {
            const float p = juce::jlimit (0.0f, 1.0f, grid[(size_t) s] * scale);
            if ((s == 0 && density > 0.15f) || r.nextFloat() < p)
                out.push_back (s * stepTicks);
        }
        // Drill: some triplets roll into 16th triplets
        if (style == drill)
        {
            std::vector<int> rolled;
            for (auto t : out)
            {
                rolled.push_back (t);
                if (r.nextFloat() < 0.18f + density * 0.2f)
                    rolled.push_back (t + stepTicks / 2);
            }
            out = rolled;
        }
        // Dubstep and neuro: some 16ths roll into 32nds
        if (style == dubstep || style == neuro)
        {
            std::vector<int> rolled;
            const float chance = (style == dubstep ? 0.14f : 0.08f) + density * 0.14f;
            for (auto t : out)
            {
                rolled.push_back (t);
                if (t % ticksPerBeat != 0 && r.nextFloat() < chance)
                    rolled.push_back (t + stepTicks / 2);
            }
            out = rolled;
        }
        // at least three notes, so there's a tune
        while ((int) out.size() < 3)
        {
            const int t = r.nextInt (steps) * stepTicks;
            if (std::find (out.begin(), out.end(), t) == out.end())
                out.push_back (t);
        }
        std::sort (out.begin(), out.end());
        return out;
    }

    // A variation: keep the first half of the bar, re-roll some of the second half
    std::vector<int> varyBarRhythm (const std::vector<int>& bar, juce::Random& r, int style, float density)
    {
        auto fresh = makeBarRhythm (r, style, density);
        std::vector<int> out;
        const int split = ticksPerBar * 3 / 4;
        for (auto t : bar) if (t < split) out.push_back (t);
        for (auto t : fresh) if (t >= split) out.push_back (t);
        if (out.size() < 3) return bar;
        return out;
    }

    struct Walker
    {
        ScaleInfo info;
        Motion motion;
        int lo = -2, hi = 12;

        int clampDegree (int d) const
        {
            while (d > hi) d -= (d - hi > info.size / 2 ? info.size : 2);
            while (d < lo) d += (lo - d > info.size / 2 ? info.size : 2);
            return juce::jlimit (lo, hi, d);
        }

        int nearestChordTone (int d, juce::Random& r) const
        {
            for (int k = 0; k <= info.size; ++k)
            {
                const bool upFirst = r.nextBool();
                const int a = d + (upFirst ? k : -k), b = d + (upFirst ? -k : k);
                if (info.isChordTone (a) && a >= lo && a <= hi) return a;
                if (info.isChordTone (b) && b >= lo && b <= hi) return b;
            }
            return d;
        }

        // Next degree from 'from', leaning towards 'target'
        int step (int from, int target, bool strongBeat, juce::Random& r) const
        {
            const float x = r.nextFloat();
            int size = 0;
            if (x < motion.repeat) size = 0;
            else if (x < motion.repeat + motion.step) size = 1;
            else if (x < motion.repeat + motion.step + motion.skip) size = 2;
            else if (x < motion.repeat + motion.step + motion.skip + motion.leap) size = 3 + r.nextInt (2);
            else size = info.size;
            const float upChance = target > from ? 0.8f : (target < from ? 0.2f : 0.5f);
            int d = from + (r.nextFloat() < upChance ? size : -size);
            if (d > hi || d < lo) d = from - (d - from);   // bounce off the edges
            d = clampDegree (d);
            if (strongBeat && ! info.isChordTone (d) && r.nextFloat() < 0.65f)
                d = nearestChordTone (d, r);
            return d;
        }

        // Pitches for a run of onsets. 'resolveTo' >= -100: land on that degree at the end.
        std::vector<int> walk (const std::vector<int>& onsets, int startDegree, float archHeight, int resolveTo, juce::Random& r) const
        {
            std::vector<int> out;
            int d = startDegree;
            const int n = (int) onsets.size();
            for (int i = 0; i < n; ++i)
            {
                if (i > 0)
                {
                    const float t = (float) i / (float) juce::jmax (1, n - 1);
                    const int target = startDegree + juce::roundToInt (archHeight * std::sin (juce::MathConstants<float>::pi * t));
                    d = step (d, target, onsets[(size_t) i] % ticksPerBeat == 0, r);
                }
                out.push_back (d);
            }
            if (resolveTo > -100 && ! out.empty())
            {
                // approach the landing note by step if we're far away
                auto& last = out.back();
                if (n > 1 && std::abs (out[(size_t) n - 2] - resolveTo) > 3)
                    out[(size_t) n - 2] = clampDegree (resolveTo + (out[(size_t) n - 2] > resolveTo ? 1 : -1));
                last = resolveTo;
            }
            return out;
        }
    };

    Walker makeWalker (const Settings& s)
    {
        Walker w;
        w.info = infoFor (s.scale);
        w.motion = motionFor (s.style);
        w.lo = -2;
        w.hi = juce::roundToInt ((float) w.info.size * (1.0f + juce::jlimit (0.0f, 1.0f, s.range) * 1.5f));
        return w;
    }

    int pickStart (const Walker& w, juce::Random& r)
    {
        const float x = r.nextFloat();
        if (x < 0.35f) return 0;
        if (x < 0.60f) return w.info.fifth();
        if (x < 0.80f) return w.info.chordTones.size() > 1 ? w.info.chordTones[1] : 0;
        return w.info.size;
    }

    // a 'call' ends somewhere open (third or fifth); an 'answer' ends on the root
    int openEnding (const Walker& w, juce::Random& r)
    {
        return r.nextBool() ? w.info.fifth() : (w.info.chordTones.size() > 1 ? w.info.chordTones[1] : w.info.fifth());
    }

    void addVelocities (Riff& riff, juce::Random& r)
    {
        for (auto& n : riff.notes)
        {
            float v = 0.74f;
            if (n.start % ticksPerBeat == 0) v += 0.16f;
            else if (n.start % (ticksPerBeat / 2) == 0) v += 0.07f;
            if (n.start % ticksPerBar == 0) v += 0.05f;
            v += (r.nextFloat() - 0.5f) * 0.08f;
            n.velocity = juce::jlimit (0.3f, 1.0f, v);
        }
    }

    void addSlides (Riff& riff, const Motion& m, juce::Random& r)
    {
        const int n = (int) riff.notes.size();
        for (int i = 0; i < n; ++i)
        {
            auto& a = riff.notes[(size_t) i];
            const auto& b = riff.notes[(size_t) ((i + 1) % n)];
            a.slide = i + 1 < n && a.span <= ticksPerBeat / 2 && std::abs (b.degree - a.degree) <= 4
                      && b.degree != a.degree && r.nextFloat() < m.slide;
        }
    }

    // Lays out pitches over the whole riff's onsets with repetition: A A' (A B)
    Riff compose (const std::vector<std::vector<int>>& barOnsets, const Settings& s, juce::uint32 seed, juce::Random& r)
    {
        const auto w = makeWalker (s);
        Riff riff;
        riff.bars = (int) barOnsets.size();
        riff.seed = seed;
        riff.style = s.style;
        const float arch = (float) w.info.size * (0.3f + s.range * 0.6f) * (r.nextBool() ? 1.0f : 0.6f);
        const int start = pickStart (w, r);

        auto place = [&riff] (const std::vector<int>& onsets, const std::vector<int>& degrees, int barOffset)
        {
            for (size_t i = 0; i < onsets.size(); ++i)
                riff.notes.push_back ({ onsets[i] + barOffset * ticksPerBar, 6, degrees[i], 0.85f, false });
        };

        // the answer copies the call's opening, then heads home
        auto answerTo = [&] (const std::vector<int>& onsets, const std::vector<int>& call, int ending)
        {
            std::vector<int> out;
            const size_t keep = juce::jmax ((size_t) 1, onsets.size() * 2 / 5);
            for (size_t i = 0; i < onsets.size(); ++i)
                out.push_back (i < keep && i < call.size() ? call[i] : 0);
            if (keep < onsets.size())
            {
                std::vector<int> rest (onsets.begin() + (long) keep - 1, onsets.end());
                auto tail = w.walk (rest, out[keep - 1], arch * 0.5f, ending, r);
                for (size_t i = 1; i < tail.size(); ++i)
                    out[keep - 1 + i] = tail[i];
            }
            return out;
        };

        if (riff.bars == 1)
        {
            // split the bar into two halves: call then answer
            std::vector<int> first, second;
            for (auto t : barOnsets[0]) (t < ticksPerBar / 2 ? first : second).push_back (t);
            if (first.empty() || second.empty())
            {
                place (barOnsets[0], w.walk (barOnsets[0], start, arch, 0, r), 0);
            }
            else
            {
                const auto call = w.walk (first, start, arch, -1000, r);
                std::vector<int> shifted;
                for (auto t : second) shifted.push_back (t - ticksPerBar / 2);
                const auto ans = answerTo (shifted, call, 0);
                place (first, call, 0);
                place (second, ans, 0);
            }
        }
        else
        {
            const auto callA = w.walk (barOnsets[0], start, arch, openEnding (w, r), r);
            place (barOnsets[0], callA, 0);
            place (barOnsets[1], answerTo (barOnsets[1], callA, riff.bars == 2 ? 0 : openEnding (w, r)), 1);
            if (riff.bars == 4)
            {
                place (barOnsets[2], barOnsets[2] == barOnsets[0] ? callA : w.walk (barOnsets[2], start, arch, openEnding (w, r), r), 2);
                const int bStart = callA.size() > 1 ? callA[1] : start;
                place (barOnsets[3], w.walk (barOnsets[3], bStart, arch * 1.2f, 0, r), 3);
            }
        }
        recomputeSpans (riff);
        addSlides (riff, w.motion, r);
        addVelocities (riff, r);
        return riff;
    }

    std::vector<std::vector<int>> makeRhythm (const Settings& s, juce::Random& r)
    {
        const int bars = s.bars <= 1 ? 1 : (s.bars >= 4 ? 4 : 2);
        std::vector<std::vector<int>> out;
        const auto a = makeBarRhythm (r, s.style, s.density);
        out.push_back (a);
        if (bars >= 2) out.push_back (r.nextFloat() < 0.5f ? a : varyBarRhythm (a, r, s.style, s.density));
        if (bars == 4)
        {
            out.push_back (a);
            out.push_back (varyBarRhythm (a, r, s.style, juce::jmin (1.0f, s.density + 0.15f)));
        }
        return out;
    }
}

// =====================================================================================
bool Riff::operator== (const Riff& o) const
{
    if (bars != o.bars || notes.size() != o.notes.size()) return false;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& a = notes[i];
        const auto& b = o.notes[i];
        if (a.start != b.start || a.span != b.span || a.degree != b.degree || a.slide != b.slide || std::abs (a.velocity - b.velocity) > 0.006f)
            return false;
    }
    return true;
}

juce::String Riff::toString() const
{
    juce::StringArray parts;
    for (const auto& n : notes)
        parts.add (juce::String (n.start) + "," + juce::String (n.span) + "," + juce::String (n.degree) + ","
                   + juce::String (juce::roundToInt (n.velocity * 100.0f)) + "," + (n.slide ? "1" : "0"));
    return "r1|" + juce::String (bars) + "|" + juce::String ((juce::int64) seed) + "|" + juce::String (style) + "|" + parts.joinIntoString (";");
}

Riff Riff::fromString (const juce::String& text)
{
    Riff r;
    const auto fields = juce::StringArray::fromTokens (text, "|", "");
    if (fields.size() < 5 || fields[0] != "r1")
        return r;
    r.bars = juce::jlimit (1, 4, fields[1].getIntValue());
    if (r.bars == 3) r.bars = 4;
    r.seed = (juce::uint32) fields[2].getLargeIntValue();
    r.style = juce::jlimit (0, numStyles - 1, fields[3].getIntValue());
    for (const auto& item : juce::StringArray::fromTokens (fields[4], ";", ""))
    {
        const auto v = juce::StringArray::fromTokens (item, ",", "");
        if (v.size() < 5) continue;
        Note n;
        n.start = juce::jlimit (0, r.lengthTicks() - 1, v[0].getIntValue());
        n.span = juce::jlimit (1, r.lengthTicks(), v[1].getIntValue());
        n.degree = juce::jlimit (-24, 36, v[2].getIntValue());
        n.velocity = juce::jlimit (0.05f, 1.0f, (float) v[3].getIntValue() / 100.0f);
        n.slide = v[4] == "1";
        r.notes.push_back (n);
    }
    std::sort (r.notes.begin(), r.notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    return r;
}

void recomputeSpans (Riff& riff)
{
    auto& notes = riff.notes;
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    notes.erase (std::unique (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start == b.start; }), notes.end());
    const int len = riff.lengthTicks();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const int next = i + 1 < notes.size() ? notes[i + 1].start : len + (notes.empty() ? 0 : notes[0].start);
        notes[i].span = juce::jlimit (1, ticksPerBar, next - notes[i].start);
    }
    if (! notes.empty()) notes.back().slide = false;
}

Riff generate (const Settings& s, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 7919 + 17);
    return compose (makeRhythm (s, r), s, seed, r);
}

Riff mutate (const Riff& from, const Settings& s, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 104729 + 3);
    auto w = makeWalker (s);
    Riff out = from;
    out.seed = seed;
    const int n = (int) out.notes.size();
    if (n < 2) return generate (s, seed);
    bool changed = false;
    for (int attempt = 0; attempt < 4 && ! changed; ++attempt)
        for (int i = 1; i < n - 1; ++i)
        {
            if (r.nextFloat() > 0.35f) continue;
            auto& note = out.notes[(size_t) i];
            const int prev = out.notes[(size_t) i - 1].degree;
            int d = note.degree + (r.nextBool() ? 1 : -1) * (1 + (r.nextFloat() < 0.3f ? 1 : 0));
            if (std::abs (d - prev) > w.info.size) d = note.degree;
            d = w.clampDegree (d);
            if (note.start % ticksPerBeat == 0 && ! w.info.isChordTone (d) && r.nextFloat() < 0.5f)
                d = w.nearestChordTone (d, r);
            if (d != note.degree) { note.degree = d; changed = true; }
        }
    addSlides (out, w.motion, r);
    return out;
}

Riff newRhythm (const Riff& from, const Settings& s, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 15485863 + 11);
    Settings settings = s;
    settings.bars = from.bars;
    const auto bars = makeRhythm (settings, r);
    Riff out;
    out.bars = from.bars;
    out.seed = seed;
    out.style = s.style;
    std::vector<int> degrees;
    for (const auto& n : from.notes) degrees.push_back (n.degree);
    if (degrees.empty()) return generate (s, seed);
    int total = 0;
    for (const auto& b : bars) total += (int) b.size();
    int i = 0;
    for (size_t b = 0; b < bars.size(); ++b)
        for (auto t : bars[b])
        {
            // keep the ending: the last new note takes the old last note
            const int d = (i == total - 1) ? degrees.back() : degrees[(size_t) (i % (int) degrees.size())];
            out.notes.push_back ({ t + (int) b * ticksPerBar, 6, d, 0.85f, false });
            ++i;
        }
    recomputeSpans (out);
    addSlides (out, motionFor (s.style), r);
    addVelocities (out, r);
    return out;
}

Riff answer (const Riff& from, const Settings& s, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 32452843 + 5);
    const auto w = makeWalker (s);
    Riff out;
    out.bars = from.bars;
    out.seed = seed;
    out.style = from.style;
    const int half = from.lengthTicks() / 2;
    std::vector<Note> call;
    for (const auto& n : from.notes) if (n.start < half) call.push_back (n);
    if (call.size() < 2) return mutate (from, s, seed);

    // the call ends open; the answer echoes its opening and comes home to the root
    std::vector<int> onsets, degrees;
    for (const auto& n : call) { onsets.push_back (n.start); degrees.push_back (n.degree); }
    if (w.info.isChordTone (degrees.back()) && degrees.back() % w.info.size == 0)
        degrees.back() = w.clampDegree (degrees.back() + (r.nextBool() ? w.info.fifth() : -1));
    const size_t keep = juce::jmax ((size_t) 1, call.size() / 2);
    std::vector<int> rest (onsets.begin() + (long) keep - 1, onsets.end());
    auto tail = w.walk (rest, degrees[keep - 1], (float) w.info.size * 0.3f, 0, r);
    for (size_t i = 0; i < call.size(); ++i)
    {
        auto n = call[i];
        n.degree = degrees[i];
        out.notes.push_back (n);
    }
    for (size_t i = 0; i < call.size(); ++i)
    {
        auto n = call[i];
        n.start += half;
        n.degree = i < keep ? degrees[i] : tail[i - keep + 1];
        out.notes.push_back (n);
    }
    recomputeSpans (out);
    addSlides (out, w.motion, r);
    return out;
}

// =====================================================================================
int rootNote (int key, int octave)
{
    key = juce::jlimit (0, 11, key);
    return juce::jlimit (12, 108, 60 + key - (key > 6 ? 12 : 0) + 12 * juce::jlimit (-2, 2, octave));
}

int degreeToNote (int degree, int root, const std::vector<int>& scale)
{
    const int size = (int) scale.size();
    const int oct = floorDiv (degree, size);
    return root + 12 * oct + scale[(size_t) (degree - oct * size)];
}

int noteToDegree (int midiNote, int root, const std::vector<int>& scale)
{
    const int diff = midiNote - root;
    const int oct = floorDiv (diff, 12);
    const int rem = diff - 12 * oct;
    int idx = 0;
    for (int i = 0; i < (int) scale.size(); ++i)
        if (scale[(size_t) i] <= rem) idx = i;
    return oct * (int) scale.size() + idx;
}

int soundingTicks (const Note& n, float gate) noexcept
{
    if (n.slide)
        return n.span + 2;   // overlaps the next note so Legato glides into it
    return juce::jmax (2, juce::roundToInt ((float) n.span * juce::jlimit (0.05f, 1.2f, gate)));
}

namespace
{
    double swingOffset (int start, float swing)
    {
        // off-beat 16ths lean late: at full swing they land on the triplet
        return (start % 12) == 6 ? (double) juce::jlimit (0.0f, 1.0f, swing) * 2.0 : 0.0;
    }
}

juce::MidiFile toMidiFile (const Riff& riff, int key, int scale, int octave, float gate, float swing)
{
    constexpr int ppq = 96;
    constexpr double k = (double) ppq / ticksPerBeat;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    const auto& steps = scaleSteps (scale);
    const int root = rootNote (key, octave);
    for (const auto& n : riff.notes)
    {
        const int note = juce::jlimit (0, 127, degreeToNote (n.degree, root, steps));
        const double on = (n.start + swingOffset (n.start, swing)) * k;
        const double off = juce::jmin ((double) riff.lengthTicks() * k, on + soundingTicks (n, gate) * k);
        const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f));
        seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, note), off);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), riff.lengthTicks() * k);
    seq.updateMatchedPairs();
    seq.sort();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);
    return file;
}

juce::File writeMidiFile (const Riff& riff, int key, int scale, int octave, float gate, float swing, const juce::String& name)
{
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("OBSDN Riffs");
    folder.createDirectory();
    auto file = folder.getChildFile (juce::File::createLegalFileName (name) + ".mid");
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! toMidiFile (riff, key, scale, octave, gate, swing).writeTo (out))
        return {};
    out.flush();
    return file;
}

// =====================================================================================
void Player::reset()
{
    held.reserve (128);
    sounding.reserve (64);
    scratch.ensureSize (8192);
    held.clear();
    sounding.clear();
    freeTick = 0.0;
    lastEndTick = -1.0;
    wasActive = false;
    shownTick = -1.0f;
}

void Player::allOff (juce::MidiBuffer& out, int sample)
{
    for (const auto& s : sounding)
        out.addEvent (juce::MidiMessage::noteOff (1, s.note), sample);
    sounding.clear();
}

void Player::process (const Riff& riff, const Context& ctx, juce::MidiBuffer& midi, int numSamples)
{
    scratch.clear();
    int firstKeySample = -1, releaseSample = 0;

    // keys drive the riff; everything else (bend, mod wheel, pressure) passes through
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            const int k = msg.getNoteNumber();
            held.erase (std::remove (held.begin(), held.end(), k), held.end());
            held.push_back (k);
            lastVelocityKey = msg.getVelocity();
            if (firstKeySample < 0) firstKeySample = m.samplePosition;
        }
        else if (msg.isNoteOff())
        {
            held.erase (std::remove (held.begin(), held.end(), msg.getNoteNumber()), held.end());
            releaseSample = m.samplePosition;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            held.clear();
            scratch.addEvent (msg, m.samplePosition);
        }
        else
        {
            scratch.addEvent (msg, m.samplePosition);
        }
    }

    const bool active = ! held.empty() || ctx.preview || (ctx.latch && ctx.hostPlaying);
    const double tps = ctx.bpm / 60.0 * ticksPerBeat / juce::jmax (1.0, ctx.sampleRate);
    const double loop = (double) riff.lengthTicks();

    if (! active)
    {
        if (wasActive)
            allOff (scratch, juce::jlimit (0, numSamples - 1, releaseSample));
        wasActive = false;
        shownTick = -1.0f;
        lastEndTick = -1.0;
        midi.swapWith (scratch);
        return;
    }

    // Where we are: the host's bar-locked position, or our own clock from the moment you pressed a key
    int startSample = 0;
    double t0;
    if (ctx.hostPlaying)
    {
        t0 = ctx.ppq * ticksPerBeat;
    }
    else
    {
        if (! wasActive || lastEndTick < 0.0)
        {
            freeTick = 0.0;
            startSample = juce::jmax (0, firstKeySample);
        }
        t0 = freeTick - startSample * tps;
    }
    const double t1 = t0 + tps * numSamples;
    if (wasActive && lastEndTick >= 0.0 && std::abs (t0 - lastEndTick) > tps * 4.0 + 0.5)
        allOff (scratch, 0);   // the host jumped: don't leave notes hanging

    // transposition from the latest held key
    const auto& steps = scaleSteps (ctx.scale);
    const int root = rootNote (ctx.key, ctx.octave);
    int degreeShift = 0;
    const int semis = transposeFor (ctx, steps, root, degreeShift);
    const float keyVel = held.empty() ? 1.0f : 0.6f + 0.4f * (float) lastVelocityKey / 127.0f;

    auto toSample = [&] (double tick) { return juce::jlimit (startSample, numSamples - 1, (int) std::floor ((tick - t0) / tps)); };

    // note-offs that fall in this block (before any new note-ons at the same sample)
    struct Ev { int sample; bool on; int note; float vel; };
    Ev events[256];
    int numEvents = 0;
    for (size_t i = 0; i < sounding.size();)
    {
        if (sounding[i].endTick < t1)
        {
            if (numEvents < 256) events[numEvents++] = { toSample (sounding[i].endTick), false, sounding[i].note, 0.0f };
            sounding.erase (sounding.begin() + (long) i);
        }
        else ++i;
    }

    if (loop > 0.0 && ! riff.notes.empty())
    {
        const double tFrom = t0 + startSample * tps;
        const auto firstLoop = (juce::int64) std::floor (tFrom / loop) - 1;
        const auto lastLoop = (juce::int64) std::floor (t1 / loop);
        for (auto k = firstLoop; k <= lastLoop; ++k)
            for (const auto& n : riff.notes)
            {
                const double s = (double) k * loop + n.start + swingOffset (n.start, ctx.swing);
                if (s < tFrom || s >= t1)
                    continue;
                const int note = juce::jlimit (0, 127, degreeToNote (n.degree + degreeShift, root, steps) + semis);
                const int at = toSample (s);
                // the same note still ringing: end it first so it retriggers cleanly
                for (size_t i = 0; i < sounding.size(); ++i)
                    if (sounding[i].note == note)
                    {
                        if (numEvents < 256) events[numEvents++] = { at, false, note, 0.0f };
                        sounding.erase (sounding.begin() + (long) i);
                        break;
                    }
                if (numEvents < 256) events[numEvents++] = { at, true, note, juce::jlimit (0.05f, 1.0f, n.velocity * keyVel) };
                if (sounding.size() < 64)
                    sounding.push_back ({ note, s + soundingTicks (n, ctx.gate) });
            }
        // a note that starts and ends inside this block
        for (size_t i = 0; i < sounding.size();)
        {
            if (sounding[i].endTick < t1)
            {
                if (numEvents < 256) events[numEvents++] = { toSample (sounding[i].endTick), false, sounding[i].note, 0.0f };
                sounding.erase (sounding.begin() + (long) i);
            }
            else ++i;
        }
    }

    std::stable_sort (events, events + numEvents, [] (const Ev& a, const Ev& b) { return a.sample < b.sample; });
    for (int i = 0; i < numEvents; ++i)
        scratch.addEvent (events[i].on ? juce::MidiMessage::noteOn (1, events[i].note, events[i].vel)
                                       : juce::MidiMessage::noteOff (1, events[i].note),
                          events[i].sample);

    if (! ctx.hostPlaying)
        freeTick = t1;
    lastEndTick = t1;
    wasActive = true;
    shownTick = (float) std::fmod (juce::jmax (0.0, t1), juce::jmax (1.0, loop));
    midi.swapWith (scratch);
}

int Player::transposeFor (const Context& ctx, const std::vector<int>& scale, int root, int& degreeShift) const
{
    degreeShift = 0;
    if (held.empty() || ctx.follow == fixedRoot)
        return 0;
    const int key = held.back();
    if (ctx.follow == chromatic)
        return key - root;
    // In key: the held note's place in the scale moves the riff along the scale, so it follows chord roots
    degreeShift = noteToDegree (key, root, scale);
    return 0;
}
} // namespace spark::riff

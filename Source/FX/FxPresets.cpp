#include "Common/Presets.h"

namespace spark
{
namespace
{
    struct Row
    {
        const char* name;
        const char* hint;
        FacetValues f; // Size, Density, Spray, Pitch, Stutter, Tone, Space, Mix
        float levelDb = 0.0f;
    };

    void add (PresetLibrary& lib, const char* category, std::initializer_list<Row> rows)
    {
        for (const auto& r : rows)
        {
            Preset p;
            p.category = category;
            p.name = r.name;
            p.hint = r.hint;
            p.facets = r.f;
            p.extras = { { "level", (r.levelDb + 24.0f) / 30.0f } };
            lib.presets.push_back (std::move (p));
        }
    }
}

PresetLibrary makeFxPresets()
{
    PresetLibrary lib;
    lib.categories = {
        { "Starters",          "Balanced starting points for any track" },
        { "Subtle Polish",     "Gentle width, air and depth for any track" },
        { "Shimmer & Space",   "Big, bright, spacious tails" },
        { "Rhythmic Stutter",  "Tempo-synced repeats and rolls" },
        { "Glitch & Chaos",    "Broken, scattered and unpredictable" },
        { "Pitch & Harmony",   "Octaves, fifths and harmonic layers" },
        { "Freeze & Drone",    "Turn anything into a sustained drone" },
        { "Lo-fi & Dark",      "Dusty, muffled and warm" },
        { "Vocal FX",          "Made for vocal tracks" },
        { "Drum Bus",          "For drum loops and drum groups" },
        { "Bass Tools",        "For bass tracks: keeps the low end solid" },
    };

    add (lib, "Starters", {
        { "Shattered Gold",   "Any track: the Spark FX signature",      { .40f, .55f, .30f, .50f, .25f, .72f, .48f, .58f } },
        { "Subtle Shine",     "A light touch on anything",               { .45f, .40f, .20f, .50f, .05f, .90f, .30f, .25f } },
    });

    add (lib, "Subtle Polish", {
        { "Air Lift",         "Adds air to vocals and synths",           { .45f, .40f, .15f, .50f, .00f, .95f, .25f, .20f } },
        { "Gentle Blur",      "Softens harsh sounds",                    { .60f, .60f, .20f, .50f, .00f, .70f, .30f, .25f } },
        { "Width Wash",       "Stereo spread for mono sources",          { .35f, .50f, .45f, .50f, .00f, .85f, .20f, .25f } },
        { "Silk Tail",        "Lush tails without clutter",              { .70f, .50f, .20f, .50f, .00f, .80f, .55f, .30f }, 1.5f },
        { "Grain Sheen",      "Fine grain sparkle",                      { .20f, .55f, .10f, .50f, .00f, .90f, .15f, .20f } },
        { "Warm Haze",        "Warm, soft depth",                        { .60f, .50f, .20f, .50f, .00f, .50f, .35f, .25f } },
        { "Soft Double",      "Doubling for vocals and leads",           { .25f, .45f, .08f, .50f, .00f, .85f, .15f, .30f } },
        { "Room Glue",        "Puts a mix in one space",                 { .40f, .50f, .20f, .50f, .00f, .70f, .40f, .18f }, 1.0f },
    });

    add (lib, "Shimmer & Space", {
        { "Octave Shimmer",   "Pads, keys and vocals",                   { .50f, .65f, .35f, .75f, .10f, .80f, .70f, .50f } },
        { "Cathedral Grain",  "Anything that needs to be huge",          { .85f, .80f, .60f, .50f, .00f, .60f, .85f, .65f }, -2.5f },
        { "Star Field",       "Glittering octave clouds",                { .70f, .75f, .60f, .75f, .00f, .90f, .90f, .60f }, -5.0f },
        { "Fifth Heaven",     "Adds a fifth above",                      { .60f, .65f, .40f, .646f, .00f, .85f, .80f, .50f } },
        { "Endless Hall",     "Long, deep tails",                        { .90f, .80f, .50f, .50f, .00f, .65f, 1.0f, .60f }, -5.0f },
        { "Glacier",          "Slow, icy smear",                         { .95f, .85f, .70f, .50f, .00f, .55f, .90f, .70f }, -4.0f },
        { "Choir Lift",       "Lifts vocals into a choir",               { .60f, .70f, .35f, .75f, .00f, .80f, .75f, .45f } },
        { "Nebula",           "Scattered, drifting space",               { .85f, .90f, .90f, .50f, .00f, .70f, .85f, .75f }, -1.5f },
    });

    add (lib, "Rhythmic Stutter", {
        { "Stutter Engine",   "Loops, synths and vocals",                { .25f, .70f, .10f, .50f, .75f, .85f, .20f, .70f }, 2.0f },
        { "Sixteenth Gate",   "Tight 16th repeats",                      { .15f, .70f, .05f, .50f, .60f, .90f, .10f, .60f } },
        { "Half-Time Repeat", "Relaxed repeats",                         { .30f, .60f, .05f, .50f, .45f, .85f, .15f, .50f }, 1.5f },
        { "Beat Roll",        "Drum-roll style repeats",                 { .20f, .80f, .05f, .50f, .85f, .90f, .10f, .70f } },
        { "Tape Stop Feel",   "Repeats that sink an octave",             { .40f, .60f, .10f, .25f, .50f, .60f, .20f, .55f } },
        { "Swing Chop",       "Loose, swung chops",                      { .25f, .65f, .15f, .50f, .55f, .85f, .20f, .55f } },
        { "Stutter Verb",     "Repeats into a wash",                     { .30f, .60f, .10f, .50f, .60f, .80f, .60f, .55f } },
        { "Micro Loops",      "Tiny buzzing loops",                      { .05f, .90f, .02f, .50f, .70f, .90f, .05f, .65f } },
    });

    add (lib, "Glitch & Chaos", {
        { "Data Rot",         "Digital decay",                           { .05f, 1.0f, .80f, .50f, .40f, .85f, .20f, .60f } },
        { "Shatter",          "Blown-apart transients",                  { .10f, .90f, .90f, .50f, .30f, .90f, .35f, .70f } },
        { "Broken Radio",     "Crackly, lost signal",                    { .15f, .80f, .60f, .50f, .35f, .45f, .30f, .60f } },
        { "Scatter Brain",    "Grains thrown everywhere",                { .20f, .85f, 1.0f, .50f, .20f, .80f, .40f, .70f } },
        { "Bit Storm",        "Buzzy, pitched-up chaos",                 { .03f, 1.0f, .50f, .75f, .50f, .95f, .10f, .65f } },
        { "Chaos Garden",     "Wild, spacious chaos",                    { .40f, .90f, .95f, .50f, .30f, .75f, .70f, .75f } },
        { "Skip Disc",        "A CD skipping",                           { .10f, .70f, .30f, .50f, .90f, .80f, .15f, .70f } },
        { "Splinter",         "Sharp shards, a fourth up",               { .08f, .95f, .70f, .604f, .25f, .90f, .25f, .60f } },
    });

    add (lib, "Pitch & Harmony", {
        { "Octave Down",      "Adds weight an octave below",             { .40f, .70f, .10f, .25f, .00f, .70f, .30f, .50f } },
        { "Octave Up",        "Adds sparkle an octave above",            { .35f, .70f, .10f, .75f, .00f, .85f, .30f, .50f } },
        { "Fifth Up",         "Power-chord harmony",                     { .40f, .70f, .10f, .646f, .00f, .80f, .30f, .45f }, 1.0f },
        { "Fourth Down",      "Dark harmony",                            { .40f, .70f, .10f, .396f, .00f, .75f, .30f, .45f } },
        { "Two Octaves Down", "Monstrous low layer",                     { .50f, .70f, .10f, .00f, .00f, .50f, .30f, .50f } },
        { "Harmonic Cloud",   "Octave cloud with space",                 { .60f, .85f, .50f, .75f, .00f, .80f, .60f, .55f } },
        { "Sub Ghost",        "A soft sub octave",                       { .50f, .60f, .10f, .25f, .00f, .40f, .20f, .35f } },
        { "Grain Chorus",     "Chorus-like thickening",                  { .20f, .75f, .35f, .50f, .00f, .85f, .25f, .40f } },
    });

    add (lib, "Freeze & Drone", {
        { "Drone Maker",      "Any sound into a drone",                  { 1.0f, .90f, .60f, .50f, .00f, .60f, .80f, .85f }, -1.5f },
        { "Endless Note",     "Holds notes forever",                     { .90f, .95f, .30f, .50f, .00f, .70f, .70f, .80f } },
        { "Dark Drone",       "Low, brooding drone",                     { 1.0f, .90f, .50f, .25f, .00f, .35f, .80f, .85f }, -4.5f },
        { "Frozen Air",       "Bright, airy drone",                      { .85f, .85f, .40f, .75f, .00f, .85f, .90f, .80f }, -5.5f },
        { "Sustain Pedal",    "Piano-pedal style sustain",               { .80f, .80f, .25f, .50f, .00f, .75f, .60f, .60f }, 1.0f },
        { "Deep Hum",         "Sub-heavy hum",                           { 1.0f, .80f, .40f, .00f, .00f, .30f, .60f, .80f }, -2.0f },
    });

    add (lib, "Lo-fi & Dark", {
        { "Tape Halo",        "Warm, washed-out tape feel",              { .70f, .60f, .15f, .50f, .00f, .55f, .55f, .45f } },
        { "Dusty Tape",       "Dusty and worn",                          { .40f, .55f, .15f, .50f, .05f, .45f, .30f, .40f } },
        { "Midnight Filter",  "Dark and muffled",                        { .50f, .60f, .20f, .50f, .00f, .30f, .45f, .50f }, 3.0f },
        { "Old Radio",        "Thin, crackly radio",                     { .20f, .70f, .30f, .50f, .10f, .50f, .15f, .55f } },
        { "Cassette Warble",  "Wobbly cassette feel",                    { .35f, .60f, .25f, .50f, .00f, .55f, .25f, .45f } },
        { "Smoke Room",       "Dark, smoky space",                       { .60f, .60f, .30f, .50f, .00f, .38f, .60f, .50f }, 1.5f },
        { "Underwater",       "Submerged and detuned",                   { .70f, .70f, .40f, .458f, .00f, .28f, .50f, .60f }, 3.5f },
    });

    add (lib, "Vocal FX", {
        { "Vox Halo",         "Octave halo around the voice",            { .50f, .65f, .30f, .75f, .00f, .85f, .70f, .35f } },
        { "Vox Chop",         "Rhythmic vocal chops",                    { .12f, .75f, .10f, .50f, .65f, .88f, .15f, .50f } },
        { "Vox Choir",        "One voice into many",                     { .45f, .75f, .40f, .50f, .00f, .82f, .60f, .50f } },
        { "Vox Demon",        "An octave-down shadow",                   { .40f, .70f, .15f, .25f, .00f, .60f, .30f, .55f } },
        { "Vox Chipmunk",     "An octave-up double",                     { .30f, .70f, .10f, .75f, .00f, .90f, .20f, .50f } },
        { "Vox Throw",        "Spacious throws on phrase ends",          { .60f, .60f, .30f, .50f, .20f, .80f, .85f, .40f }, -1.5f },
        { "Vox Glitch",       "Glitched-out vocals",                     { .08f, .85f, .40f, .50f, .50f, .90f, .20f, .55f } },
        { "Vox Air",          "Breathy top end",                         { .40f, .50f, .25f, .50f, .00f, 1.0f, .40f, .20f } },
    });

    add (lib, "Drum Bus", {
        { "Drum Smear",       "Blurs loops into texture",                { .25f, .70f, .30f, .50f, .10f, .75f, .40f, .35f } },
        { "Break Chopper",    "Chops breaks in time",                    { .12f, .75f, .10f, .50f, .70f, .85f, .10f, .55f } },
        { "Room Crusher",     "Roomy, crunchy drums",                    { .30f, .60f, .20f, .50f, .00f, .60f, .55f, .30f } },
        { "Hat Sparkle",      "Shimmer on hats and tops",                { .08f, .80f, .40f, .75f, .10f, .95f, .30f, .30f } },
        { "Tom Bloom",        "Deep, blooming toms",                     { .50f, .60f, .20f, .25f, .00f, .50f, .60f, .35f } },
        { "Glitch Groove",    "Glitchy fills in time",                   { .06f, .85f, .30f, .50f, .80f, .85f, .15f, .55f } },
    });

    add (lib, "Bass Tools", {
        { "Sub Thickener",    "Adds a sub octave under bass",            { .50f, .60f, .05f, .25f, .00f, .30f, .05f, .30f } },
        { "Bass Grit",        "Gritty grain texture on bass",            { .15f, .80f, .05f, .50f, .00f, .50f, .05f, .35f } },
        { "Wobble Chop",      "Tempo-synced bass chops",                 { .20f, .70f, .05f, .50f, .60f, .45f, .05f, .50f } },
        { "Growl Smear",      "Growly movement on mid-bass",             { .30f, .75f, .15f, .50f, .10f, .55f, .10f, .40f } },
        { "Reese Widen",      "Widens bass; keep the mix low",           { .40f, .60f, .20f, .50f, .00f, .45f, .05f, .30f } },
        { "Bass Stutter",     "Stuttered bass fills",                    { .25f, .70f, .03f, .50f, .70f, .40f, .05f, .55f } },
    });

    return lib;
}
} // namespace spark

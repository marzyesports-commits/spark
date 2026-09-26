#include "Common/Presets.h"

namespace spark
{
namespace
{
    constexpr bool G = false; // Grain engine
    constexpr bool T = true;  // Table engine

    struct Row
    {
        const char* name;
        const char* hint;
        bool table;
        FacetValues f; // Pitch, Position, Grain, Morph, Tone, Drive, Motion, Space
        float a, d, s, r;
        float levelDb = -3.0f;
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
            p.extras = { { "mode", r.table ? 1.0f : 0.0f },
                         { "attack", r.a }, { "decay", r.d }, { "sustain", r.s }, { "release", r.r },
                         { "level", (r.levelDb + 24.0f) / 30.0f } };
            lib.presets.push_back (std::move (p));
        }
    }
}

PresetLibrary makeInstrumentPresets()
{
    PresetLibrary lib;
    lib.categories = {
        { "Starters",       "Balanced starting points for any sound" },
        { "Bass",           "Built for bass one-shots, 808s, reese and bass loops" },
        { "Pads",           "For sustained sounds: synths, strings, vocals and drones" },
        { "Keys & Plucks",  "For piano, mallets, bells and plucked sounds" },
        { "Leads",          "Playable lead tones from single-note samples" },
        { "Vocal Chops",    "Drop a vocal phrase, acapella or one-shot" },
        { "Textures",       "Field recordings, noise, foley and found sound" },
        { "Drums & Perc",   "Drum loops, one-shots and percussion" },
        { "FX & Risers",    "Risers, impacts, sweeps and sound effects" },
        { "Wavetable",      "Table mode: your sound played as a wavetable" },
    };

    add (lib, "Starters", {
        { "Gilded Dust",  "Any sound: the Spark signature", G, { .50f, .32f, .45f, .58f, .68f, .30f, .40f, .42f }, .08f, .40f, .70f, .35f, -6.0f },
        { "Init Grain",   "Neutral grain player",           G, { .50f, .30f, .45f, .50f, .80f, .00f, .20f, .20f }, .02f, .40f, .90f, .25f, -4.5f },
        { "Init Table",   "Neutral wavetable player",       T, { .50f, .30f, .45f, .50f, .80f, .00f, .20f, .20f }, .02f, .40f, .90f, .25f, -4.5f },
    });

    add (lib, "Bass", {
        { "Sub Anchor",      "808s and sub one-shots",                T, { .50f, .30f, .60f, .20f, .34f, .08f, .04f, .02f }, .01f, .30f, .95f, .12f, 0.5f },
        { "Ember Bass",      "Any bass: dark and weighty",            T, { .25f, .20f, .20f, .35f, .42f, .55f, .15f, .10f }, .02f, .35f, .60f, .20f, -6.5f },
        { "Reese Engine",    "Reese and detuned bass loops",          T, { .50f, .40f, .55f, .55f, .55f, .45f, .55f, .08f }, .02f, .40f, .90f, .20f, -6.5f },
        { "Growl Chop",      "Mid-bass growls and wobbles",           G, { .50f, .35f, .18f, .50f, .60f, .70f, .35f, .05f }, .01f, .25f, .80f, .10f, -3.5f },
        { "Octave Rumble",   "Any bass, dropped an octave",           G, { .25f, .30f, .70f, .50f, .40f, .30f, .10f, .05f }, .02f, .40f, .90f, .20f, -4.5f },
        { "Foghorn",         "Brass-style bass stabs",                T, { .50f, .50f, .50f, .80f, .45f, .55f, .20f, .12f }, .05f, .50f, .85f, .25f, -2.5f },
        { "Rubber 808",      "Long 808 samples, played melodically",  G, { .50f, .15f, .80f, .50f, .40f, .35f, .02f, .03f }, .005f, .45f, .60f, .25f, -5.0f },
        { "Grit Sub",        "Distorted sub layers",                  T, { .50f, .30f, .60f, .35f, .42f, .80f, .08f, .04f }, .01f, .30f, .90f, .15f, -5.5f },
        { "Neuro Shred",     "Neuro and DnB bass loops",              G, { .50f, .55f, .08f, .50f, .70f, .85f, .60f, .06f }, .01f, .20f, .80f, .10f, -2.5f },
        { "Dub Pressure",    "Dub and steppers basslines",            T, { .50f, .40f, .60f, .25f, .30f, .20f, .30f, .25f }, .03f, .50f, .90f, .30f, -4.0f },
        { "Pluck Bass",      "Short, tight bass hits",                T, { .50f, .30f, .40f, .40f, .50f, .30f, .05f, .06f }, .005f, .22f, .20f, .12f, -2.0f },
        { "Wobble Scan",     "Wobble and LFO bass",                   G, { .50f, .30f, .35f, .50f, .52f, .50f, .80f, .06f }, .01f, .30f, .90f, .15f, -5.5f },
        { "Velvet Low",      "Warm, round basslines",                 T, { .50f, .30f, .50f, .15f, .33f, .05f, .20f, .18f }, .08f, .50f, .95f, .35f, 2.0f },
        { "Tape Bass",       "Warm, saturated bass loops",            G, { .50f, .25f, .65f, .50f, .45f, .40f, .12f, .10f }, .01f, .35f, .80f, .20f, -6.0f },
        { "Donk",            "Bouncy donk bass, an octave up",        T, { .75f, .30f, .30f, .70f, .62f, .30f, .05f, .06f }, .003f, .18f, .10f, .10f, 3.5f },
    });

    add (lib, "Pads", {
        { "Halo Pad",         "Any sustained synth or vocal",        T, { .50f, .40f, .50f, .60f, .62f, .12f, .70f, .70f }, .50f, .60f, .85f, .70f, -7.0f },
        { "Glass Choir",      "Vocals and choirs",                   G, { .50f, .55f, .75f, .30f, .80f, .10f, .55f, .65f }, .35f, .50f, .80f, .55f, -3.0f },
        { "Cloud Bank",       "Any sustained sound",                 G, { .50f, .50f, .90f, .50f, .58f, .05f, .70f, .80f }, .60f, .60f, .90f, .75f, -1.0f },
        { "Frozen Lake",      "Strings, synths, vocals",             G, { .50f, .35f, .95f, .50f, .70f, .02f, .25f, .85f }, .70f, .70f, .95f, .80f, 1.0f },
        { "Dusk Swell",       "Slow evolving swells",                T, { .50f, .40f, .50f, .30f, .50f, .15f, .60f, .60f }, .65f, .60f, .90f, .70f, -3.5f },
        { "Aurora",           "Bright airy pad, an octave up",       T, { .75f, .40f, .50f, .70f, .78f, .08f, .85f, .75f }, .50f, .60f, .85f, .75f, -6.5f },
        { "Warm Blanket",     "Soft analog-style pad",               T, { .50f, .40f, .50f, .20f, .42f, .20f, .35f, .55f }, .45f, .60f, .95f, .60f, -4.0f },
        { "Shimmer Veil",     "Octave shimmer from any sample",      G, { .75f, .60f, .80f, .50f, .85f, .02f, .75f, .90f }, .55f, .60f, .90f, .80f, -3.5f },
        { "Deep Space",       "Dark, huge drones",                   G, { .25f, .50f, .95f, .50f, .45f, .05f, .60f, .95f }, .70f, .70f, .95f, .85f, 3.0f },
        { "Breathing Choir",  "Vocal samples",                       G, { .50f, .45f, .60f, .50f, .66f, .08f, .90f, .70f }, .50f, .50f, .85f, .70f, -4.0f },
        { "Tidal",            "A pad that never sits still",         T, { .50f, .40f, .50f, .50f, .60f, .10f, 1.0f, .65f }, .50f, .60f, .90f, .70f, -6.5f },
        { "Glass Organ",      "Organ-like sustain",                  T, { .50f, .40f, .40f, .15f, .72f, .10f, .20f, .50f }, .25f, .50f, .95f, .50f, -7.5f },
        { "Solar Wind",       "Bright textures, a fourth up",        G, { .604f, .70f, .70f, .50f, .74f, .12f, .80f, .80f }, .60f, .60f, .90f, .80f, -4.0f },
        { "Midnight Strings", "String and orchestral samples",       G, { .50f, .40f, .85f, .50f, .55f, .06f, .45f, .70f }, .40f, .60f, .90f, .65f, -3.0f },
    });

    add (lib, "Keys & Plucks", {
        { "Bright Pluck",    "Any tonal sample",                     T, { .50f, .30f, .40f, .20f, .85f, .25f, .10f, .30f }, .01f, .30f, .15f, .30f, -4.5f },
        { "Glass Mallet",    "Bell and mallet tones",                T, { .75f, .30f, .40f, .80f, .80f, .05f, .10f, .45f }, .005f, .35f, .05f, .40f, -1.5f },
        { "Felt Keys",       "Piano and keys samples",               G, { .50f, .20f, .55f, .50f, .55f, .05f, .10f, .35f }, .02f, .45f, .30f, .35f, -4.0f },
        { "Kalimba Drop",    "Thumb-piano plinks",                   T, { .50f, .30f, .40f, .90f, .75f, .10f, .05f, .35f }, .003f, .30f, .02f, .30f, -2.5f },
        { "Toy Box",         "Music-box style",                      T, { .75f, .30f, .40f, .65f, .88f, .20f, .20f, .30f }, .003f, .20f, .05f, .20f, -1.0f },
        { "Harp Glint",      "Plucked strings",                      G, { .50f, .10f, .30f, .50f, .78f, .05f, .20f, .55f }, .005f, .40f, .05f, .50f, -2.0f },
        { "Electric Tine",   "Electric piano-style keys",            T, { .50f, .30f, .40f, .45f, .68f, .18f, .12f, .30f }, .005f, .45f, .25f, .35f, -6.0f },
        { "Chord Stab",      "House and garage chord stabs",         T, { .50f, .30f, .40f, .55f, .70f, .40f, .15f, .40f }, .003f, .22f, .10f, .25f, -1.0f },
        { "Marimba Grain",   "Wood and mallet hits",                 G, { .50f, .15f, .12f, .50f, .70f, .10f, .15f, .30f }, .003f, .28f, .02f, .25f, -2.5f },
        { "Pizz Spark",      "Short string plucks",                  G, { .50f, .10f, .10f, .50f, .72f, .15f, .30f, .25f }, .002f, .15f, .00f, .15f, -1.5f },
        { "Crystal Arp",     "Fast arpeggios",                       T, { .75f, .30f, .40f, .30f, .90f, .08f, .25f, .55f }, .003f, .25f, .05f, .40f, -3.0f },
        { "Lo-fi Keys",      "Dusty keys and samples",               G, { .50f, .30f, .50f, .50f, .45f, .35f, .20f, .30f }, .01f, .40f, .35f, .30f, -4.5f },
    });

    add (lib, "Leads", {
        { "Gold Lead",       "Classic bright lead",                  T, { .50f, .30f, .40f, .70f, .78f, .40f, .20f, .25f }, .02f, .35f, .85f, .20f, -5.5f },
        { "Screamer",        "Aggressive distorted lead",            T, { .50f, .30f, .40f, .85f, .82f, .75f, .30f, .20f }, .01f, .30f, .90f, .15f, -4.5f },
        { "Flute Ghost",     "Breathy wind samples",                 G, { .50f, .40f, .60f, .50f, .66f, .05f, .30f, .40f }, .12f, .40f, .90f, .30f, -4.0f },
        { "Chip Tune",       "8-bit style lead",                     T, { .75f, .30f, .40f, .95f, .90f, .35f, .02f, .10f }, .002f, .20f, .80f, .08f, -2.5f },
        { "Sine Whistle",    "Pure whistle tones",                   T, { .75f, .30f, .40f, .00f, .70f, .05f, .15f, .35f }, .05f, .30f, .90f, .25f, -6.5f },
        { "Vox Lead",        "Sung vocal notes",                     G, { .50f, .45f, .25f, .50f, .72f, .30f, .25f, .30f }, .02f, .30f, .90f, .20f, -5.0f },
        { "Hyper Wide",      "Wide, detuned supersaw feel",          T, { .50f, .30f, .40f, .75f, .85f, .35f, .65f, .35f }, .01f, .35f, .85f, .25f, -5.5f },
        { "Soft Square",     "Mellow lead",                          T, { .50f, .30f, .40f, .50f, .60f, .15f, .10f, .25f }, .03f, .35f, .85f, .20f, -7.0f },
        { "Acid Line",       "Squelchy mono lines",                  T, { .50f, .30f, .40f, .60f, .48f, .60f, .05f, .08f }, .003f, .20f, .60f, .08f, 2.5f },
        { "Glide Tone",      "Smooth sustained leads",               G, { .50f, .30f, .70f, .50f, .70f, .20f, .35f, .30f }, .05f, .40f, .90f, .30f, -6.0f },
    });

    add (lib, "Vocal Chops", {
        { "Chop Shop",        "Vocal phrases into rhythmic chops",   G, { .50f, .40f, .10f, .50f, .78f, .10f, .25f, .20f }, .003f, .18f, .30f, .12f, -0.5f },
        { "Choir of One",     "A single vocal into a choir",         G, { .50f, .45f, .55f, .50f, .72f, .05f, .70f, .65f }, .30f, .50f, .90f, .60f, -2.5f },
        { "Chipmunk Hook",    "Pitched-up vocal hooks",              G, { .75f, .40f, .15f, .50f, .82f, .10f, .20f, .20f }, .003f, .25f, .50f, .15f, -2.0f },
        { "Low Demon",        "Pitched-down vocals",                 G, { .25f, .40f, .25f, .50f, .55f, .40f, .20f, .30f }, .01f, .35f, .80f, .25f, -5.0f },
        { "Breath Pad",       "Breaths and airy vocals",             G, { .50f, .60f, .85f, .50f, .62f, .02f, .80f, .80f }, .50f, .60f, .90f, .70f, -1.5f },
        { "Stutter Syllable", "Single syllables",                    G, { .50f, .30f, .05f, .50f, .75f, .15f, .10f, .15f }, .002f, .12f, .20f, .08f, -1.0f },
        { "Formant Drift",    "Vowel sounds as wavetables",          T, { .50f, .40f, .50f, .50f, .68f, .12f, .70f, .45f }, .08f, .40f, .85f, .35f, -5.5f },
        { "Gospel Swell",     "Soulful vocal stacks",                G, { .50f, .50f, .70f, .50f, .70f, .08f, .50f, .70f }, .45f, .60f, .90f, .60f, -2.5f },
        { "Robot Choir",      "Vocals into robotic tones",           T, { .50f, .40f, .50f, .60f, .70f, .40f, .30f, .40f }, .02f, .35f, .90f, .30f, -5.5f },
        { "Whisper Cloud",    "Whispers and spoken word",            G, { .50f, .50f, .30f, .50f, .85f, .05f, .95f, .75f }, .20f, .50f, .80f, .60f, -4.5f },
    });

    add (lib, "Textures", {
        { "Night Static",    "Noise and crackle",                    G, { .50f, .80f, .12f, .50f, .55f, .45f, .85f, .35f }, .05f, .30f, .50f, .40f, -2.5f },
        { "Rain Glass",      "Rain, water, field recordings",        G, { .50f, .50f, .06f, .50f, .80f, .05f, .95f, .70f }, .20f, .50f, .80f, .60f, -4.0f },
        { "Machine Hum",     "Machinery and room tone",              G, { .25f, .40f, .90f, .50f, .40f, .35f, .30f, .50f }, .30f, .60f, .95f, .60f, -5.5f },
        { "Forest Floor",    "Nature recordings",                    G, { .50f, .60f, .20f, .50f, .65f, .05f, .80f, .60f }, .30f, .50f, .85f, .60f, -3.0f },
        { "Rust Bloom",      "Metallic scrapes and foley",           G, { .50f, .50f, .30f, .50f, .50f, .70f, .60f, .55f }, .35f, .50f, .85f, .50f, -2.5f },
        { "Radio Ghost",     "Radio, noise and static",              G, { .50f, .70f, .08f, .50f, .60f, .55f, .90f, .45f }, .10f, .40f, .70f, .40f, -3.0f },
        { "Tape Dust",       "Vinyl and tape noise",                 G, { .50f, .50f, .40f, .50f, .45f, .30f, .40f, .40f }, .20f, .50f, .85f, .50f, -3.0f },
        { "Ice Cavern",      "Any sound into icy ambience",          G, { .75f, .50f, .70f, .50f, .75f, .02f, .60f, .95f }, .60f, .70f, .95f, .85f, -1.0f },
        { "Swarm",           "Dense grain clouds",                   G, { .50f, .50f, .03f, .50f, .70f, .20f, 1.0f, .50f }, .20f, .50f, .85f, .50f, -5.0f },
        { "Underwater",      "Muffled and submerged",                G, { .396f, .50f, .60f, .50f, .30f, .10f, .55f, .70f }, .40f, .60f, .90f, .70f, 4.0f },
        { "Wind Tunnel",     "Air and wind",                         G, { .50f, .60f, .90f, .50f, .58f, .05f, .85f, .85f }, .60f, .70f, .95f, .80f, 0.0f },
        { "Circuit Bend",    "Harsh digital textures",               T, { .50f, .50f, .50f, .90f, .70f, .80f, .90f, .30f }, .05f, .35f, .80f, .30f, -4.0f },
    });

    add (lib, "Drums & Perc", {
        { "Kick Boom",       "Kick one-shots, played in tune",       G, { .50f, .02f, .35f, .50f, .50f, .40f, .00f, .05f }, .001f, .30f, .00f, .20f, -0.5f },
        { "Snare Crack",     "Snares and claps",                     G, { .50f, .02f, .25f, .50f, .85f, .50f, .05f, .25f }, .001f, .20f, .00f, .20f, 0.5f },
        { "Loop Slicer",     "Drum loops into new grooves",          G, { .50f, .30f, .06f, .50f, .85f, .20f, .40f, .15f }, .002f, .20f, .50f, .10f, -4.5f },
        { "Tuned Toms",      "Toms and percussion",                  G, { .50f, .03f, .30f, .50f, .65f, .20f, .02f, .20f }, .001f, .28f, .00f, .20f, -3.5f },
        { "Metal Perc",      "Metallic hits",                        T, { .75f, .30f, .40f, .90f, .85f, .30f, .10f, .30f }, .001f, .15f, .00f, .20f, 0.5f },
        { "Clap Cloud",      "Claps into textures",                  G, { .50f, .10f, .08f, .50f, .80f, .15f, .70f, .55f }, .005f, .30f, .10f, .35f, -3.5f },
        { "Hat Shimmer",     "Hi-hats and cymbals",                  G, { .75f, .20f, .05f, .50f, .95f, .10f, .60f, .35f }, .002f, .15f, .05f, .20f, -2.0f },
        { "Break Smear",     "Breakbeats",                           G, { .50f, .40f, .20f, .50f, .75f, .35f, .60f, .45f }, .01f, .30f, .70f, .30f, -5.0f },
        { "808 Knock",       "808 and kick layering",                G, { .25f, .02f, .50f, .50f, .40f, .60f, .00f, .03f }, .001f, .40f, .00f, .25f, -2.5f },
        { "Shaker Grain",    "Shakers and small percussion",         G, { .50f, .40f, .04f, .50f, .90f, .05f, .50f, .20f }, .002f, .12f, .10f, .10f, 0.0f },
    });

    add (lib, "FX & Risers", {
        { "Riser Swell",     "Hold a note to build tension",         G, { .50f, .50f, .40f, .50f, .70f, .20f, .90f, .70f }, .85f, .70f, .95f, .50f, 1.5f },
        { "Down Lifter",     "Falling impacts",                      G, { .25f, .50f, .50f, .50f, .45f, .30f, .70f, .80f }, .02f, .80f, .00f, .70f, -4.0f },
        { "Impact Boom",     "Hits and impacts",                     G, { .25f, .02f, .80f, .50f, .35f, .60f, .10f, .75f }, .001f, .70f, .00f, .70f, -4.0f },
        { "Laser Zap",       "Sci-fi zaps",                          T, { 1.0f, .30f, .40f, .90f, .90f, .50f, .40f, .20f }, .001f, .12f, .00f, .10f, 1.5f },
        { "White Wash",      "Noise sweeps",                         G, { .50f, .60f, .05f, .50f, 1.0f, .20f, 1.0f, .85f }, .70f, .70f, .95f, .80f, -2.5f },
        { "Reverse Bloom",   "Reverse-style swells",                 G, { .50f, .80f, .90f, .50f, .62f, .05f, .40f, .80f }, .90f, .50f, .90f, .15f, 5.5f },
        { "Siren",           "Siren-like tones",                     T, { .75f, .30f, .40f, .80f, .85f, .60f, .80f, .30f }, .01f, .30f, .90f, .20f, -5.0f },
        { "Glitch Burst",    "Short glitch bursts",                  G, { .50f, .50f, .02f, .50f, .80f, .50f, 1.0f, .20f }, .001f, .20f, .30f, .10f, -2.0f },
    });

    add (lib, "Wavetable", {
        { "Morph Sweep",     "Hear the whole table move",            T, { .50f, .30f, .40f, .50f, .75f, .20f, .90f, .40f }, .05f, .40f, .90f, .35f, -6.0f },
        { "Frame Zero",      "The first frame of your sound",        T, { .50f, .30f, .40f, .00f, .70f, .10f, .05f, .30f }, .02f, .40f, .90f, .30f, -6.0f },
        { "Last Frame",      "The final frame of your sound",        T, { .50f, .30f, .40f, 1.0f, .75f, .20f, .05f, .30f }, .02f, .40f, .90f, .30f, -7.0f },
        { "Folded Gold",     "Heavy waveshaping",                    T, { .50f, .30f, .40f, .55f, .78f, .85f, .30f, .30f }, .02f, .40f, .90f, .30f, -4.5f },
        { "Soft Table",      "Gentle, filtered tables",              T, { .50f, .30f, .40f, .40f, .45f, .05f, .30f, .40f }, .10f, .50f, .90f, .40f, -5.0f },
        { "Detune Wide",     "Maximum unison spread",                T, { .50f, .30f, .40f, .50f, .80f, .20f, 1.0f, .45f }, .03f, .40f, .90f, .35f, -6.0f },
        { "Octave Table",    "Tables an octave up",                  T, { .75f, .30f, .40f, .60f, .80f, .15f, .40f, .45f }, .03f, .40f, .90f, .35f, -7.0f },
        { "Hollow Morph",    "Mid-range movement",                   T, { .50f, .30f, .40f, .30f, .58f, .35f, .55f, .50f }, .08f, .45f, .85f, .40f, -6.0f },
        { "Digital Drift",   "Bright, shifting digital tones",       T, { .50f, .30f, .40f, .80f, .88f, .45f, .70f, .35f }, .02f, .35f, .90f, .30f, -6.0f },
    });

    return lib;
}
} // namespace spark

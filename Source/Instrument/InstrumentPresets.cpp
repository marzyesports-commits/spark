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
            p.extras = { { "mode", r.table ? 0.5f : 0.0f },   // Grain = 0, Table = 0.5, Sample = 1
                         { "attack", r.a }, { "decay", r.d }, { "sustain", r.s }, { "release", r.r },
                         { "level", (r.levelDb + 24.0f) / 30.0f } };
            lib.presets.push_back (std::move (p));
        }
    }
}

// Adds envelope detail to a preset. Values are real values: curves and toneAmount are -1..1,
// everything else 0..1 (times use the usual envelope mapping).
void shape (PresetLibrary& lib, const char* name, std::initializer_list<std::pair<const char*, float>> values)
{
    static const juce::StringArray bipolar { "attackCurve", "decayCurve", "releaseCurve", "toneAmount",
                                             "toneAttackCurve", "toneDecayCurve", "toneReleaseCurve",
                                             "sustainSlope", "toneSustainSlope" };
    for (auto& p : lib.presets)
        if (p.name == name)
            for (const auto& [id, v] : values)
                p.extras[id] = bipolar.contains (id) ? (v + 1.0f) * 0.5f : v;
}

// Helpers for presets that use Spark's newer features (values are real values unless noted)
namespace
{
    void setExtra (PresetLibrary& lib, const char* name, const juce::String& id, float normalised)
    {
        for (auto& p : lib.presets)
            if (p.name == name) p.extras[id] = normalised;
    }
    // a modulation routing: source and destination indices from mod::Source / mod::Dest, amount -1..1
    void route (PresetLibrary& lib, const char* name, int slot, int src, int dst, float amount)
    {
        setExtra (lib, name, "mod" + juce::String (slot + 1) + "Src", (float) src / 9.0f);
        setExtra (lib, name, "mod" + juce::String (slot + 1) + "Dst", (float) dst / 9.0f);
        setExtra (lib, name, "mod" + juce::String (slot + 1) + "Amt", (amount + 1.0f) * 0.5f);
    }
    // an LFO: shape index (0 sine .. 6 drift); rateHz > 0 = free, otherwise 'division' index (0 = 4 bars .. 7 = 1/32)
    void lfo (PresetLibrary& lib, const char* name, int which, int shape, float rateHz, int division = 4, bool retrigger = false)
    {
        const juce::String l = "lfo" + juce::String (which + 1);
        setExtra (lib, name, l + "Shape", (float) shape / 6.0f);
        setExtra (lib, name, l + "Sync", rateHz > 0.0f ? 0.0f : 1.0f);
        if (rateHz > 0.0f) setExtra (lib, name, l + "Rate", std::log (rateHz / 0.02f) / std::log (1000.0f));
        else setExtra (lib, name, l + "Div", (float) division / 12.0f);
        setExtra (lib, name, l + "Retrig", retrigger ? 1.0f : 0.0f);
    }
    void layers (PresetLibrary& lib, const char* name, float sub, int subSemis, float noise, float noiseColour = 0.6f)
    {
        setExtra (lib, name, "subLevel", sub);
        setExtra (lib, name, "subTune", (float) (subSemis + 36) / 36.0f);
        setExtra (lib, name, "noiseLevel", noise);
        setExtra (lib, name, "noiseColour", noiseColour);
    }
    void play (PresetLibrary& lib, const char* name, int voiceMode, float glide)   // 0 poly, 1 mono, 2 legato
    {
        setExtra (lib, name, "voiceMode", (float) voiceMode / 2.0f);
        setExtra (lib, name, "glide", glide);
    }
    void sound (PresetLibrary& lib, const char* name, const char* soundId)
    {
        for (auto& p : lib.presets)
            if (p.name == name) p.sound = soundId;
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
        { "Motion",         "Built around LFOs, macros, glide and layers" },
    };

    add (lib, "Starters", {
        { "Gilded Dust",  "Any sound: the Spark signature", G, { .50f, .32f, .45f, .58f, .68f, .30f, .40f, .42f }, .08f, .40f, .70f, .35f, -5.0f },
        { "Init Grain",   "Neutral grain player",           G, { .50f, .30f, .45f, .50f, .80f, .00f, .20f, .20f }, .02f, .40f, .90f, .25f, -0.5f },
        { "Init Table",   "Neutral wavetable player",       T, { .50f, .30f, .45f, .50f, .80f, .00f, .20f, .20f }, .02f, .40f, .90f, .25f, -10.5f },
    });

    add (lib, "Bass", {
        { "Sub Anchor",      "808s and sub one-shots",                T, { .50f, .30f, .60f, .20f, .34f, .08f, .04f, .02f }, .01f, .30f, .95f, .12f, -5.5f },
        { "Ember Bass",      "Any bass: dark and weighty",            T, { .25f, .20f, .20f, .35f, .42f, .55f, .15f, .10f }, .02f, .35f, .60f, .20f, -4.0f },
        { "Reese Engine",    "Reese and detuned bass loops",          T, { .50f, .40f, .55f, .55f, .55f, .45f, .55f, .08f }, .02f, .40f, .90f, .20f, -4.5f },
        { "Growl Chop",      "Mid-bass growls and wobbles",           G, { .50f, .35f, .18f, .50f, .60f, .70f, .35f, .05f }, .01f, .25f, .80f, .10f, -0.5f },
        { "Octave Rumble",   "Any bass, dropped an octave",           G, { .25f, .30f, .70f, .50f, .40f, .30f, .10f, .05f }, .02f, .40f, .90f, .20f, -2.0f },
        { "Foghorn",         "Brass-style bass stabs",                T, { .50f, .50f, .50f, .80f, .45f, .55f, .20f, .12f }, .05f, .50f, .85f, .25f, -5.0f },
        { "Rubber 808",      "Long 808 samples, played melodically",  G, { .50f, .15f, .80f, .50f, .40f, .35f, .02f, .03f }, .005f, .45f, .60f, .25f, 0.5f },
        { "Grit Sub",        "Distorted sub layers",                  T, { .50f, .30f, .60f, .35f, .42f, .80f, .08f, .04f }, .01f, .30f, .90f, .15f, -3.5f },
        { "Neuro Shred",     "Neuro and DnB bass loops",              G, { .50f, .55f, .08f, .50f, .70f, .85f, .60f, .06f }, .01f, .20f, .80f, .10f, 0.5f },
        { "Dub Pressure",    "Dub and steppers basslines",            T, { .50f, .40f, .60f, .25f, .30f, .20f, .30f, .25f }, .03f, .50f, .90f, .30f, -5.5f },
        { "Pluck Bass",      "Short, tight bass hits",                T, { .50f, .30f, .40f, .40f, .50f, .30f, .05f, .06f }, .005f, .22f, .20f, .12f, 3.5f },
        { "Wobble Scan",     "Wobble and LFO bass",                   G, { .50f, .30f, .35f, .50f, .52f, .50f, .80f, .06f }, .01f, .30f, .90f, .15f, -0.5f },
        { "Velvet Low",      "Warm, round basslines",                 T, { .50f, .30f, .50f, .15f, .33f, .05f, .20f, .18f }, .08f, .50f, .95f, .35f, -5.5f },
        { "Tape Bass",       "Warm, saturated bass loops",            G, { .50f, .25f, .65f, .50f, .45f, .40f, .12f, .10f }, .01f, .35f, .80f, .20f, -1.5f },
        { "Donk",            "Bouncy donk bass, an octave up",        T, { .75f, .30f, .30f, .70f, .62f, .30f, .05f, .06f }, .003f, .18f, .10f, .10f, 6.0f },
    });

    add (lib, "Pads", {
        { "Halo Pad",         "Any sustained synth or vocal",        T, { .50f, .40f, .50f, .60f, .62f, .12f, .70f, .70f }, .50f, .60f, .85f, .70f, -9.5f },
        { "Glass Choir",      "Vocals and choirs",                   G, { .50f, .55f, .75f, .30f, .80f, .10f, .55f, .65f }, .35f, .50f, .80f, .55f, -3.5f },
        { "Cloud Bank",       "Any sustained sound",                 G, { .50f, .50f, .90f, .50f, .58f, .05f, .70f, .80f }, .60f, .60f, .90f, .75f, 2.5f },
        { "Frozen Lake",      "Strings, synths, vocals",             G, { .50f, .35f, .95f, .50f, .70f, .02f, .25f, .85f }, .70f, .70f, .95f, .80f, 6.0f },
        { "Dusk Swell",       "Slow evolving swells",                T, { .50f, .40f, .50f, .30f, .50f, .15f, .60f, .60f }, .65f, .60f, .90f, .70f, 2.5f },
        { "Aurora",           "Bright airy pad, an octave up",       T, { .75f, .40f, .50f, .70f, .78f, .08f, .85f, .75f }, .50f, .60f, .85f, .75f, -10.5f },
        { "Warm Blanket",     "Soft analog-style pad",               T, { .50f, .40f, .50f, .20f, .42f, .20f, .35f, .55f }, .45f, .60f, .95f, .60f, -4.0f },
        { "Shimmer Veil",     "Octave shimmer from any sample",      G, { .75f, .60f, .80f, .50f, .85f, .02f, .75f, .90f }, .55f, .60f, .90f, .80f, 1.5f },
        { "Deep Space",       "Dark, huge drones",                   G, { .25f, .50f, .95f, .50f, .45f, .05f, .60f, .95f }, .70f, .70f, .95f, .85f, 4.5f },
        { "Breathing Choir",  "Vocal samples",                       G, { .50f, .45f, .60f, .50f, .66f, .08f, .90f, .70f }, .50f, .50f, .85f, .70f, -5.0f },
        { "Tidal",            "A pad that never sits still",         T, { .50f, .40f, .50f, .50f, .60f, .10f, 1.0f, .65f }, .50f, .60f, .90f, .70f, -11.0f },
        { "Glass Organ",      "Organ-like sustain",                  T, { .50f, .40f, .40f, .15f, .72f, .10f, .20f, .50f }, .25f, .50f, .95f, .50f, -11.0f },
        { "Solar Wind",       "Bright textures, a fourth up",        G, { .604f, .70f, .70f, .50f, .74f, .12f, .80f, .80f }, .60f, .60f, .90f, .80f, 0.0f },
        { "Midnight Strings", "String and orchestral samples",       G, { .50f, .40f, .85f, .50f, .55f, .06f, .45f, .70f }, .40f, .60f, .90f, .65f, 1.0f },
    });

    add (lib, "Keys & Plucks", {
        { "Bright Pluck",    "Any tonal sample",                     T, { .50f, .30f, .40f, .20f, .85f, .25f, .10f, .30f }, .01f, .30f, .15f, .30f, -12.0f },
        { "Glass Mallet",    "Bell and mallet tones",                T, { .75f, .30f, .40f, .80f, .80f, .05f, .10f, .45f }, .005f, .35f, .05f, .40f, -4.0f },
        { "Felt Keys",       "Piano and keys samples",               G, { .50f, .20f, .55f, .50f, .55f, .05f, .10f, .35f }, .02f, .45f, .30f, .35f, 3.0f },
        { "Kalimba Drop",    "Thumb-piano plinks",                   T, { .50f, .30f, .40f, .90f, .75f, .10f, .05f, .35f }, .003f, .30f, .02f, .30f, -12.0f },
        { "Toy Box",         "Music-box style",                      T, { .75f, .30f, .40f, .65f, .88f, .20f, .20f, .30f }, .003f, .20f, .05f, .20f, -7.0f },
        { "Harp Glint",      "Plucked strings",                      G, { .50f, .10f, .30f, .50f, .78f, .05f, .20f, .55f }, .005f, .40f, .05f, .50f, 2.0f },
        { "Electric Tine",   "Electric piano-style keys",            T, { .50f, .30f, .40f, .45f, .68f, .18f, .12f, .30f }, .005f, .45f, .25f, .35f, -11.5f },
        { "Chord Stab",      "House and garage chord stabs",         T, { .50f, .30f, .40f, .55f, .70f, .40f, .15f, .40f }, .003f, .22f, .10f, .25f, -2.5f },
        { "Marimba Grain",   "Wood and mallet hits",                 G, { .50f, .15f, .12f, .50f, .70f, .10f, .15f, .30f }, .003f, .28f, .02f, .25f, 3.0f },
        { "Pizz Spark",      "Short string plucks",                  G, { .50f, .10f, .10f, .50f, .72f, .15f, .30f, .25f }, .002f, .15f, .00f, .15f, 2.0f },
        { "Crystal Arp",     "Fast arpeggios",                       T, { .75f, .30f, .40f, .30f, .90f, .08f, .25f, .55f }, .003f, .25f, .05f, .40f, -12.0f },
        { "Lo-fi Keys",      "Dusty keys and samples",               G, { .50f, .30f, .50f, .50f, .45f, .35f, .20f, .30f }, .01f, .40f, .35f, .30f, 0.0f },
    });

    add (lib, "Leads", {
        { "Gold Lead",       "Classic bright lead",                  T, { .50f, .30f, .40f, .70f, .78f, .40f, .20f, .25f }, .02f, .35f, .85f, .20f, -12.0f },
        { "Screamer",        "Aggressive distorted lead",            T, { .50f, .30f, .40f, .85f, .82f, .75f, .30f, .20f }, .01f, .30f, .90f, .15f, -8.0f },
        { "Flute Ghost",     "Breathy wind samples",                 G, { .50f, .40f, .60f, .50f, .66f, .05f, .30f, .40f }, .12f, .40f, .90f, .30f, -2.5f },
        { "Chip Tune",       "8-bit style lead",                     T, { .75f, .30f, .40f, .95f, .90f, .35f, .02f, .10f }, .002f, .20f, .80f, .08f, -10.5f },
        { "Sine Whistle",    "Pure whistle tones",                   T, { .75f, .30f, .40f, .00f, .70f, .05f, .15f, .35f }, .05f, .30f, .90f, .25f, -12.0f },
        { "Vox Lead",        "Sung vocal notes",                     G, { .50f, .45f, .25f, .50f, .72f, .30f, .25f, .30f }, .02f, .30f, .90f, .20f, -6.0f },
        { "Hyper Wide",      "Wide, detuned supersaw feel",          T, { .50f, .30f, .40f, .75f, .85f, .35f, .65f, .35f }, .01f, .35f, .85f, .25f, -10.0f },
        { "Soft Square",     "Mellow lead",                          T, { .50f, .30f, .40f, .50f, .60f, .15f, .10f, .25f }, .03f, .35f, .85f, .20f, -9.0f },
        { "Acid Line",       "Squelchy mono lines",                  T, { .50f, .30f, .40f, .60f, .48f, .60f, .05f, .08f }, .003f, .20f, .60f, .08f, 0.0f },
        { "Glide Tone",      "Smooth sustained leads",               G, { .50f, .30f, .70f, .50f, .70f, .20f, .35f, .30f }, .05f, .40f, .90f, .30f, -7.0f },
    });

    add (lib, "Vocal Chops", {
        { "Chop Shop",        "Vocal phrases into rhythmic chops",   G, { .50f, .40f, .10f, .50f, .78f, .10f, .25f, .20f }, .003f, .18f, .30f, .12f, 5.5f },
        { "Choir of One",     "A single vocal into a choir",         G, { .50f, .45f, .55f, .50f, .72f, .05f, .70f, .65f }, .30f, .50f, .90f, .60f, -4.5f },
        { "Chipmunk Hook",    "Pitched-up vocal hooks",              G, { .75f, .40f, .15f, .50f, .82f, .10f, .20f, .20f }, .003f, .25f, .50f, .15f, -1.5f },
        { "Low Demon",        "Pitched-down vocals",                 G, { .25f, .40f, .25f, .50f, .55f, .40f, .20f, .30f }, .01f, .35f, .80f, .25f, -5.0f },
        { "Breath Pad",       "Breaths and airy vocals",             G, { .50f, .60f, .85f, .50f, .62f, .02f, .80f, .80f }, .50f, .60f, .90f, .70f, 1.0f },
        { "Stutter Syllable", "Single syllables",                    G, { .50f, .30f, .05f, .50f, .75f, .15f, .10f, .15f }, .002f, .12f, .20f, .08f, 2.0f },
        { "Formant Drift",    "Vowel sounds as wavetables",          T, { .50f, .40f, .50f, .50f, .68f, .12f, .70f, .45f }, .08f, .40f, .85f, .35f, -8.5f },
        { "Gospel Swell",     "Soulful vocal stacks",                G, { .50f, .50f, .70f, .50f, .70f, .08f, .50f, .70f }, .45f, .60f, .90f, .60f, -4.0f },
        { "Robot Choir",      "Vocals into robotic tones",           T, { .50f, .40f, .50f, .60f, .70f, .40f, .30f, .40f }, .02f, .35f, .90f, .30f, -10.0f },
        { "Whisper Cloud",    "Whispers and spoken word",            G, { .50f, .50f, .30f, .50f, .85f, .05f, .95f, .75f }, .20f, .50f, .80f, .60f, -3.5f },
    });

    add (lib, "Textures", {
        { "Night Static",    "Noise and crackle",                    G, { .50f, .80f, .12f, .50f, .55f, .45f, .85f, .35f }, .05f, .30f, .50f, .40f, 6.0f },
        { "Rain Glass",      "Rain, water, field recordings",        G, { .50f, .50f, .06f, .50f, .80f, .05f, .95f, .70f }, .20f, .50f, .80f, .60f, 1.0f },
        { "Machine Hum",     "Machinery and room tone",              G, { .25f, .40f, .90f, .50f, .40f, .35f, .30f, .50f }, .30f, .60f, .95f, .60f, -6.0f },
        { "Forest Floor",    "Nature recordings",                    G, { .50f, .60f, .20f, .50f, .65f, .05f, .80f, .60f }, .30f, .50f, .85f, .60f, 6.0f },
        { "Rust Bloom",      "Metallic scrapes and foley",           G, { .50f, .50f, .30f, .50f, .50f, .70f, .60f, .55f }, .35f, .50f, .85f, .50f, -5.0f },
        { "Radio Ghost",     "Radio, noise and static",              G, { .50f, .70f, .08f, .50f, .60f, .55f, .90f, .45f }, .10f, .40f, .70f, .40f, -1.5f },
        { "Tape Dust",       "Vinyl and tape noise",                 G, { .50f, .50f, .40f, .50f, .45f, .30f, .40f, .40f }, .20f, .50f, .85f, .50f, 6.0f },
        { "Ice Cavern",      "Any sound into icy ambience",          G, { .75f, .50f, .70f, .50f, .75f, .02f, .60f, .95f }, .60f, .70f, .95f, .85f, 6.0f },
        { "Swarm",           "Dense grain clouds",                   G, { .50f, .50f, .03f, .50f, .70f, .20f, 1.0f, .50f }, .20f, .50f, .85f, .50f, 0.0f },
        { "Underwater",      "Muffled and submerged",                G, { .396f, .50f, .60f, .50f, .30f, .10f, .55f, .70f }, .40f, .60f, .90f, .70f, 6.0f },
        { "Wind Tunnel",     "Air and wind",                         G, { .50f, .60f, .90f, .50f, .58f, .05f, .85f, .85f }, .60f, .70f, .95f, .80f, 6.0f },
        { "Circuit Bend",    "Harsh digital textures",               T, { .50f, .50f, .50f, .90f, .70f, .80f, .90f, .30f }, .05f, .35f, .80f, .30f, -10.0f },
    });

    add (lib, "Drums & Perc", {
        { "Kick Boom",       "Kick one-shots, played in tune",       G, { .50f, .02f, .35f, .50f, .50f, .40f, .00f, .05f }, .001f, .30f, .00f, .20f, 0.5f },
        { "Snare Crack",     "Snares and claps",                     G, { .50f, .02f, .25f, .50f, .85f, .50f, .05f, .25f }, .001f, .20f, .00f, .20f, 5.0f },
        { "Loop Slicer",     "Drum loops into new grooves",          G, { .50f, .30f, .06f, .50f, .85f, .20f, .40f, .15f }, .002f, .20f, .50f, .10f, -9.0f },
        { "Tuned Toms",      "Toms and percussion",                  G, { .50f, .03f, .30f, .50f, .65f, .20f, .02f, .20f }, .001f, .28f, .00f, .20f, -3.5f },
        { "Metal Perc",      "Metallic hits",                        T, { .75f, .30f, .40f, .90f, .85f, .30f, .10f, .30f }, .001f, .15f, .00f, .20f, -12.0f },
        { "Clap Cloud",      "Claps into textures",                  G, { .50f, .10f, .08f, .50f, .80f, .15f, .70f, .55f }, .005f, .30f, .10f, .35f, -3.0f },
        { "Hat Shimmer",     "Hi-hats and cymbals",                  G, { .75f, .20f, .05f, .50f, .95f, .10f, .60f, .35f }, .002f, .15f, .05f, .20f, 6.0f },
        { "Break Smear",     "Breakbeats",                           G, { .50f, .40f, .20f, .50f, .75f, .35f, .60f, .45f }, .01f, .30f, .70f, .30f, -5.0f },
        { "808 Knock",       "808 and kick layering",                G, { .25f, .02f, .50f, .50f, .40f, .60f, .00f, .03f }, .001f, .40f, .00f, .25f, -3.0f },
        { "Shaker Grain",    "Shakers and small percussion",         G, { .50f, .40f, .04f, .50f, .90f, .05f, .50f, .20f }, .002f, .12f, .10f, .10f, 6.0f },
    });

    add (lib, "FX & Risers", {
        { "Riser Swell",     "Hold a note to build tension",         G, { .50f, .50f, .40f, .50f, .70f, .20f, .90f, .70f }, .55f, .70f, .95f, .50f, 5.5f },
        { "Down Lifter",     "Falling impacts",                      G, { .25f, .50f, .50f, .50f, .45f, .30f, .70f, .80f }, .02f, .80f, .00f, .70f, 3.0f },
        { "Impact Boom",     "Hits and impacts",                     G, { .25f, .02f, .80f, .50f, .35f, .60f, .10f, .75f }, .001f, .70f, .00f, .70f, -6.5f },
        { "Laser Zap",       "Sci-fi zaps",                          T, { 1.0f, .30f, .40f, .90f, .90f, .50f, .40f, .20f }, .001f, .12f, .00f, .10f, -3.0f },
        { "White Wash",      "Noise sweeps",                         G, { .50f, .60f, .05f, .50f, 1.0f, .20f, 1.0f, .85f }, .70f, .70f, .95f, .80f, 0.5f },
        { "Reverse Bloom",   "Reverse-style swells",                 G, { .50f, .80f, .90f, .50f, .62f, .05f, .40f, .80f }, .90f, .50f, .90f, .15f, 6.0f },
        { "Siren",           "Siren-like tones",                     T, { .75f, .30f, .40f, .80f, .85f, .60f, .80f, .30f }, .01f, .30f, .90f, .20f, -11.0f },
        { "Glitch Burst",    "Short glitch bursts",                  G, { .50f, .50f, .02f, .50f, .80f, .50f, 1.0f, .20f }, .001f, .20f, .30f, .10f, -3.0f },
    });

    add (lib, "Wavetable", {
        { "Morph Sweep",     "Hear the whole table move",            T, { .50f, .30f, .40f, .50f, .75f, .20f, .90f, .40f }, .05f, .40f, .90f, .35f, -11.0f },
        { "Frame Zero",      "The first frame of your sound",        T, { .50f, .30f, .40f, .00f, .70f, .10f, .05f, .30f }, .02f, .40f, .90f, .30f, -11.5f },
        { "Last Frame",      "The final frame of your sound",        T, { .50f, .30f, .40f, 1.0f, .75f, .20f, .05f, .30f }, .02f, .40f, .90f, .30f, -10.5f },
        { "Folded Gold",     "Heavy waveshaping",                    T, { .50f, .30f, .40f, .55f, .78f, .85f, .30f, .30f }, .02f, .40f, .90f, .30f, -8.0f },
        { "Soft Table",      "Gentle, filtered tables",              T, { .50f, .30f, .40f, .40f, .45f, .05f, .30f, .40f }, .10f, .50f, .90f, .40f, -9.5f },
        { "Detune Wide",     "Maximum unison spread",                T, { .50f, .30f, .40f, .50f, .80f, .20f, 1.0f, .45f }, .03f, .40f, .90f, .35f, -6.5f },
        { "Octave Table",    "Tables an octave up",                  T, { .75f, .30f, .40f, .60f, .80f, .15f, .40f, .45f }, .03f, .40f, .90f, .35f, -10.5f },
        { "Hollow Morph",    "Mid-range movement",                   T, { .50f, .30f, .40f, .30f, .58f, .35f, .55f, .50f }, .08f, .45f, .85f, .40f, -7.5f },
        { "Digital Drift",   "Bright, shifting digital tones",       T, { .50f, .30f, .40f, .80f, .88f, .45f, .70f, .35f }, .02f, .35f, .90f, .30f, -9.0f },
    });

    add (lib, "Motion", {
        { "Wobble Machine",  "Tempo-synced bass wobble (Macro 1 = grit)", T, { .50f, .30f, .40f, .40f, .45f, .35f, .20f, .15f }, .003f, .40f, .90f, .15f, -5.0f },
        { "Breathing Pad",   "A pad that slowly breathes",            T, { .50f, .30f, .40f, .30f, .65f, .10f, .35f, .50f }, .35f, .60f, .85f, .60f, -9.5f },
        { "Glide 808",       "Mono 808 that slides between notes",    T, { .50f, .30f, .40f, .20f, .60f, .30f, .05f, .05f }, .002f, .60f, .70f, .25f, -6.0f },
        { "Legato Lead",     "Slides when you overlap keys (Macro 1 = bright)", T, { .50f, .30f, .40f, .40f, .66f, .25f, .35f, .30f }, .02f, .40f, .85f, .25f, -5.5f },
        { "Trance Gate",     "Sixteenth-note gated supersaw",         T, { .50f, .30f, .40f, .50f, .70f, .15f, .50f, .35f }, .01f, .30f, 1.0f, .20f, -12.0f },
        { "S&H Bleeps",      "Random filter steps on every 16th",     T, { .50f, .30f, .40f, .50f, .55f, .20f, .20f, .40f }, .002f, .30f, .30f, .20f, -9.0f },
        { "Macro Morph",     "Four macros: morph, tone, drive, motion", T, { .50f, .30f, .40f, .00f, .50f, .00f, .30f, .30f }, .01f, .40f, .90f, .30f, -11.5f },
        { "Tremolo Keys",    "Electric piano with tremolo",           T, { .50f, .30f, .40f, .30f, .62f, .10f, .10f, .30f }, .003f, .90f, .20f, .40f, -12.0f },
        { "Drift Strings",   "Strings with slow tuning drift",        T, { .50f, .30f, .40f, .50f, .60f, .05f, .30f, .50f }, .30f, .60f, .90f, .60f, -10.0f },
        { "Sub Pluck",       "Harp pluck with a sub underneath",      T, { .50f, .30f, .40f, .40f, .62f, .10f, .20f, .25f }, .002f, .50f, .00f, .30f, -8.0f },
        { "Breathy Flute",   "Legato flute with breath noise",        T, { .50f, .30f, .40f, .50f, .65f, .05f, .20f, .35f }, .06f, .40f, .85f, .30f, -5.0f },
        { "Velocity Bite",   "Play harder for a brighter bite",       T, { .50f, .30f, .40f, .40f, .35f, .40f, .10f, .10f }, .002f, .40f, .50f, .20f, -10.0f },
        { "Pressure Pad",    "Aftertouch opens it, mod wheel moves it", T, { .50f, .30f, .40f, .40f, .45f, .05f, .30f, .50f }, .30f, .60f, .90f, .60f, -6.5f },
        { "Fade Piano",      "Held notes fade away like a piano",     T, { .50f, .30f, .40f, .20f, .70f, .05f, .10f, .30f }, .002f, .60f, .60f, .35f, -9.5f },
        { "Stutter Stab",    "Chord stab through stutter and delay",  T, { .50f, .30f, .40f, .40f, .60f, .20f, .20f, .30f }, .002f, .35f, .00f, .25f, -4.5f },
    });

    // ---- the new features in the Motion presets (mod::Source: 1 LFO1, 2 LFO2, 3-6 macros, 7 wheel, 8 aftertouch, 9 velocity;
    //      mod::Dest: 0 pitch, 3 morph, 4 tone, 5 drive, 6 motion, 8 resonance, 9 volume)
    lfo (lib, "Wobble Machine", 0, 0, 0.0f, 5);   route (lib, "Wobble Machine", 0, 1, 4, .35f);  route (lib, "Wobble Machine", 1, 3, 5, .50f);
    setExtra (lib, "Wobble Machine", "resonance", .50f);  play (lib, "Wobble Machine", 1, .15f);  layers (lib, "Wobble Machine", .30f, -12, 0.0f);
    lfo (lib, "Breathing Pad", 0, 0, 0.2f);  lfo (lib, "Breathing Pad", 1, 6, 0.09f);
    route (lib, "Breathing Pad", 0, 1, 3, .35f);  route (lib, "Breathing Pad", 1, 2, 4, .12f);
    play (lib, "Glide 808", 1, .35f);  layers (lib, "Glide 808", .40f, -12, 0.0f);
    play (lib, "Legato Lead", 2, .30f);  lfo (lib, "Legato Lead", 0, 0, 5.5f, 4, true);
    route (lib, "Legato Lead", 0, 1, 0, .004f);  route (lib, "Legato Lead", 1, 3, 4, .30f);
    lfo (lib, "Trance Gate", 0, 4, 0.0f, 6);  route (lib, "Trance Gate", 0, 1, 9, -.90f);
    lfo (lib, "S&H Bleeps", 0, 5, 0.0f, 6);  route (lib, "S&H Bleeps", 0, 1, 4, .45f);  setExtra (lib, "S&H Bleeps", "resonance", .60f);
    route (lib, "Macro Morph", 0, 3, 3, 1.0f);  route (lib, "Macro Morph", 1, 4, 4, .50f);
    route (lib, "Macro Morph", 2, 5, 5, .60f);  route (lib, "Macro Morph", 3, 6, 6, .60f);
    lfo (lib, "Tremolo Keys", 0, 0, 5.0f);  route (lib, "Tremolo Keys", 0, 1, 9, -.45f);
    lfo (lib, "Drift Strings", 0, 6, 0.4f);  lfo (lib, "Drift Strings", 1, 0, 0.15f);
    route (lib, "Drift Strings", 0, 1, 0, .003f);  route (lib, "Drift Strings", 1, 2, 4, .12f);
    layers (lib, "Sub Pluck", .45f, -12, .06f);
    play (lib, "Breathy Flute", 2, .25f);  layers (lib, "Breathy Flute", 0.0f, -12, .20f, .70f);
    lfo (lib, "Breathy Flute", 0, 0, 5.0f, 4, true);  route (lib, "Breathy Flute", 0, 1, 0, .003f);
    route (lib, "Velocity Bite", 0, 9, 4, .45f);  setExtra (lib, "Velocity Bite", "resonance", .45f);
    route (lib, "Pressure Pad", 0, 8, 4, .35f);  route (lib, "Pressure Pad", 1, 7, 6, .50f);
    shape (lib, "Fade Piano", { { "sustainSlope", -.55f } });
    setExtra (lib, "Stutter Stab", "fxStutterOn", 1.0f);  setExtra (lib, "Stutter Stab", "fxDelayOn", 1.0f);

    // ---- layers on some classics
    layers (lib, "Reese Engine", .35f, -12, 0.0f);
    layers (lib, "Velvet Low", .30f, -12, 0.0f);
    layers (lib, "Ember Bass", .25f, -12, 0.0f);
    layers (lib, "Breath Pad", 0.0f, -12, .25f, .50f);
    layers (lib, "Flute Ghost", 0.0f, -12, .15f, .70f);
    layers (lib, "Cloud Bank", 0.0f, -12, .08f, .80f);
    layers (lib, "Whisper Cloud", 0.0f, -12, .20f, .60f);
    play (lib, "Acid Line", 1, .20f);

    // ---- every preset brings a sound from the library (unless you've locked your own)
    const std::pair<const char*, const char*> sounds[] = {
        { "Gilded Dust", "pad_glass" }, { "Init Grain", "pad_analog" }, { "Init Table", "wt_basic" },
        { "Sub Anchor", "bass_808_long" }, { "Ember Bass", "bass_analog" }, { "Reese Engine", "bass_reese" }, { "Growl Chop", "bass_growl" },
        { "Octave Rumble", "bass_tape" }, { "Foghorn", "bass_brass" }, { "Rubber 808", "bass_808_punch" }, { "Grit Sub", "bass_sub" },
        { "Neuro Shred", "bass_neuro" }, { "Dub Pressure", "bass_fm" }, { "Pluck Bass", "bass_pluck" }, { "Wobble Scan", "bass_wobble" },
        { "Velvet Low", "bass_analog" }, { "Tape Bass", "bass_tape" }, { "Donk", "bass_pluck" },
        { "Halo Pad", "pad_supersaw" }, { "Glass Choir", "pad_choir" }, { "Cloud Bank", "pad_air" }, { "Frozen Lake", "pad_strings" },
        { "Dusk Swell", "pad_analog" }, { "Aurora", "pad_shimmer" }, { "Warm Blanket", "pad_analog" }, { "Shimmer Veil", "pad_shimmer" },
        { "Deep Space", "pad_dark" }, { "Breathing Choir", "vox_ah" }, { "Tidal", "pad_supersaw" }, { "Glass Organ", "pad_organ" },
        { "Solar Wind", "pad_air" }, { "Midnight Strings", "pad_strings" },
        { "Bright Pluck", "key_harp" }, { "Glass Mallet", "key_bell" }, { "Felt Keys", "key_piano" }, { "Kalimba Drop", "key_kalimba" },
        { "Toy Box", "key_musicbox" }, { "Harp Glint", "key_harp" }, { "Electric Tine", "key_epiano" }, { "Chord Stab", "key_stab" },
        { "Marimba Grain", "key_marimba" }, { "Pizz Spark", "key_pizz" }, { "Crystal Arp", "key_musicbox" }, { "Lo-fi Keys", "key_epiano" },
        { "Gold Lead", "lead_saw" }, { "Screamer", "lead_scream" }, { "Flute Ghost", "lead_flute" }, { "Chip Tune", "lead_chip" },
        { "Sine Whistle", "lead_whistle" }, { "Vox Lead", "vox_oo" }, { "Hyper Wide", "pad_supersaw" }, { "Soft Square", "lead_square" },
        { "Acid Line", "lead_saw" }, { "Glide Tone", "lead_sync" },
        { "Chop Shop", "vox_phrase" }, { "Choir of One", "vox_ah" }, { "Chipmunk Hook", "vox_ee" }, { "Low Demon", "vox_oh" },
        { "Breath Pad", "vox_breath" }, { "Stutter Syllable", "vox_phrase" }, { "Formant Drift", "wt_formant" }, { "Gospel Swell", "pad_choir" },
        { "Robot Choir", "vox_oo" }, { "Whisper Cloud", "vox_breath" },
        { "Night Static", "tex_static" }, { "Rain Glass", "tex_rain" }, { "Machine Hum", "tex_hum" }, { "Forest Floor", "tex_insects" },
        { "Rust Bloom", "tex_metal" }, { "Radio Ghost", "tex_static" }, { "Tape Dust", "tex_vinyl" }, { "Ice Cavern", "key_bell" },
        { "Swarm", "tex_digital" }, { "Underwater", "tex_underwater" }, { "Wind Tunnel", "tex_wind" }, { "Circuit Bend", "tex_digital" },
        { "Kick Boom", "drm_kick" }, { "Snare Crack", "drm_snare" }, { "Loop Slicer", "drm_loop" }, { "Tuned Toms", "drm_tom" },
        { "Metal Perc", "drm_cowbell" }, { "Clap Cloud", "drm_clap" }, { "Hat Shimmer", "drm_hat_loop" }, { "Break Smear", "drm_loop" },
        { "808 Knock", "drm_kick_deep" }, { "Shaker Grain", "drm_shaker_loop" },
        { "Riser Swell", "fx_sweep" }, { "Down Lifter", "fx_downlifter" }, { "Impact Boom", "fx_impact" }, { "Laser Zap", "fx_laser" },
        { "White Wash", "fx_sweep" }, { "Reverse Bloom", "fx_reverse" }, { "Siren", "fx_siren" }, { "Glitch Burst", "tex_digital" },
        { "Morph Sweep", "wt_basic" }, { "Frame Zero", "wt_harmonic" }, { "Last Frame", "wt_fm" }, { "Folded Gold", "wt_fold" },
        { "Soft Table", "wt_soft" }, { "Detune Wide", "wt_pwm" }, { "Octave Table", "wt_harmonic" }, { "Hollow Morph", "wt_formant" },
        { "Digital Drift", "wt_digital" },
        { "Wobble Machine", "bass_reese" }, { "Breathing Pad", "pad_supersaw" }, { "Glide 808", "bass_808_long" }, { "Legato Lead", "lead_saw" },
        { "Trance Gate", "pad_supersaw" }, { "S&H Bleeps", "key_bell" }, { "Macro Morph", "wt_basic" }, { "Tremolo Keys", "key_epiano" },
        { "Drift Strings", "pad_strings" }, { "Sub Pluck", "key_harp" }, { "Breathy Flute", "lead_flute" }, { "Velocity Bite", "bass_pluck" },
        { "Pressure Pad", "pad_analog" }, { "Fade Piano", "key_piano" }, { "Stutter Stab", "key_stab" },
    };
    for (const auto& [presetName, soundId] : sounds)
        sound (lib, presetName, soundId);

    // ---- envelope character: tone sweeps, punchy decays, slow swells
    shape (lib, "Sub Anchor",    { { "releaseCurve", .5f } });
    shape (lib, "Rubber 808",    { { "decayCurve", .5f } });
    shape (lib, "Growl Chop",    { { "toneAmount", .35f }, { "toneDecay", .30f }, { "toneSustain", .30f } });
    shape (lib, "Neuro Shred",   { { "toneAmount", .30f }, { "toneDecay", .20f }, { "toneSustain", .40f } });
    shape (lib, "Foghorn",       { { "toneAmount", .30f }, { "toneAttack", .15f }, { "toneDecay", .50f }, { "toneSustain", .50f } });
    shape (lib, "Dub Pressure",  { { "toneAmount", .20f }, { "toneDecay", .35f }, { "toneSustain", .30f } });
    shape (lib, "Pluck Bass",    { { "toneAmount", .50f }, { "toneDecay", .25f }, { "toneSustain", 0.0f }, { "decayCurve", .60f } });
    shape (lib, "Donk",          { { "toneAmount", .50f }, { "toneDecay", .15f }, { "toneSustain", 0.0f }, { "decayCurve", .70f } });
    shape (lib, "Halo Pad",      { { "attackCurve", -.40f }, { "releaseCurve", .40f } });
    shape (lib, "Cloud Bank",    { { "attackCurve", -.40f }, { "releaseCurve", .40f } });
    shape (lib, "Dusk Swell",    { { "attackCurve", -.60f }, { "releaseCurve", .40f }, { "toneAmount", .25f }, { "toneAttack", .70f }, { "toneSustain", 1.0f } });
    shape (lib, "Warm Blanket",  { { "attackCurve", -.30f }, { "releaseCurve", .40f } });
    shape (lib, "Bright Pluck",  { { "toneAmount", .40f }, { "toneDecay", .30f }, { "toneSustain", 0.0f }, { "decayCurve", .50f } });
    shape (lib, "Glass Mallet",  { { "decayCurve", .60f }, { "releaseCurve", .50f } });
    shape (lib, "Felt Keys",     { { "ampVelocity", .90f }, { "toneVelocity", .50f }, { "toneAmount", .25f }, { "toneDecay", .50f }, { "toneSustain", .20f } });
    shape (lib, "Kalimba Drop",  { { "decayCurve", .60f }, { "toneAmount", .30f }, { "toneDecay", .25f }, { "toneSustain", 0.0f } });
    shape (lib, "Harp Glint",    { { "decayCurve", .60f } });
    shape (lib, "Electric Tine", { { "ampVelocity", .90f }, { "toneVelocity", .50f }, { "toneAmount", .30f }, { "toneDecay", .40f }, { "toneSustain", .10f } });
    shape (lib, "Chord Stab",    { { "toneAmount", .45f }, { "toneDecay", .20f }, { "toneSustain", 0.0f }, { "decayCurve", .50f } });
    shape (lib, "Marimba Grain", { { "decayCurve", .60f } });
    shape (lib, "Pizz Spark",    { { "decayCurve", .60f } });
    shape (lib, "Gold Lead",     { { "toneAmount", .20f }, { "toneDecay", .40f }, { "toneSustain", .50f } });
    shape (lib, "Screamer",      { { "toneAmount", .25f }, { "toneAttack", .10f }, { "toneDecay", .50f }, { "toneSustain", .60f } });
    shape (lib, "Acid Line",     { { "toneAmount", .70f }, { "toneDecay", .22f }, { "toneSustain", 0.0f }, { "toneVelocity", .60f } });
    shape (lib, "Kick Boom",     { { "decayCurve", .70f } });
    shape (lib, "Snare Crack",   { { "decayCurve", .60f } });
    shape (lib, "808 Knock",     { { "decayCurve", .50f } });
    shape (lib, "Riser Swell",   { { "attackCurve", -.70f }, { "toneAmount", .40f }, { "toneAttack", .85f }, { "toneSustain", 1.0f } });
    shape (lib, "Reverse Bloom", { { "attackCurve", -.45f }, { "releaseCurve", .80f } });
    shape (lib, "Laser Zap",     { { "toneAmount", .80f }, { "toneDecay", .12f }, { "toneSustain", 0.0f } });

    return lib;
}
} // namespace spark

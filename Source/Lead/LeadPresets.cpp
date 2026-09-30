#include "LeadProcessor.h"

namespace spark
{
namespace
{
    // Wave positions (the WAVE facet)
    constexpr float SIN = 0.0f, TRI = 1.0f / 7, SAW = 2.0f / 7, SQR = 3.0f / 7, PUL = 4.0f / 7, THIN = 5.0f / 7, SYNC = 6.0f / 7, REED = 1.0f;
    enum Mode { P = 0, M = 1, L = 2 };

    float envNorm (float seconds) { return std::sqrt (juce::jmax (0.0f, (seconds * 1000.0f - 1.0f) / 4999.0f)); }

    // Converts a real value to the parameter's normalised value (only the ones presets use)
    float norm (const juce::String& id, float v)
    {
        if (id == "unison")     return (v - 1.0f) / 6.0f;
        if (id == "oscBSemi")   return (v + 24.0f) / 48.0f;
        if (id == "oscBFine")   return (v + 50.0f) / 100.0f;
        if (id == "oscBWave")   return v;
        if (id == "voiceMode")  return v / 2.0f;
        if (id == "bendRange")  return (v - 1.0f) / 23.0f;
        if (id == "vibRate")    return (v - 2.0f) / 7.0f;
        if (id == "vibDelay")   return v / 1.5f;
        if (id == "level")      return (v + 36.0f) / 42.0f;
        if (id == "filterType") return v / 3.0f;
        if (id == "oscAMode")   return v / 3.0f;
        if (id == "fxEqLow" || id == "fxEqMid" || id == "fxEqHigh") return (v + 12.0f) / 24.0f;
        if (id == "fxDelayTime") return v / 5.0f;        // 0 = 1/16 .. 5 = 1/2
        if (id == "fxDistType") return v / 3.0f;
        if (id == "fxStutterRate") return v / 3.0f;
        if (id.endsWith ("On")) return v > 0.5f ? 1.0f : 0.0f;
        if (id == "ampA" || id == "ampD" || id == "ampR" || id == "fltA" || id == "fltD" || id == "fltR") return envNorm (v);
        return v;   // already 0..1
    }

    struct Row
    {
        const char* name;
        const char* hint;
        FacetValues f;      // Wave, Detune, Tone, Bite, Drive, Vibrato, Glide, Space
        int unison;
        float a, d, s, r;   // amp envelope: seconds, sustain 0..1
        int mode;
        float levelDb;
    };

    void add (PresetLibrary& lib, const char* category, const char* hint, std::initializer_list<Row> rows)
    {
        lib.categories.push_back ({ category, hint });
        for (const auto& r : rows)
        {
            Preset p;
            p.category = category;
            p.name = r.name;
            p.hint = r.hint;
            p.facets = r.f;
            for (const auto& [id, v] : std::initializer_list<std::pair<const char*, float>> {
                     { "unison", (float) r.unison }, { "ampA", r.a }, { "ampD", r.d }, { "ampS", r.s }, { "ampR", r.r },
                     { "voiceMode", (float) r.mode }, { "level", r.levelDb } })
                p.extras[id] = norm (id, v);
            lib.presets.push_back (std::move (p));
        }
    }

    // Extra settings for a preset, in real values
    void set (PresetLibrary& lib, const char* name, std::initializer_list<std::pair<const char*, float>> values)
    {
        for (auto& p : lib.presets)
            if (p.name == juce::String (name))
                for (const auto& [id, v] : values)
                    p.extras[id] = norm (id, v);
    }

    // a library sound played as oscillator A (mode: 1 table, 2 grain, 3 sample)
    void sound (PresetLibrary& lib, const char* name, const char* id, int mode)
    {
        for (auto& p : lib.presets)
            if (p.name == juce::String (name))
            {
                p.sound = id;
                p.extras["oscAMode"] = norm ("oscAMode", (float) mode);
            }
    }

    // common effect settings
    void delay (PresetLibrary& lib, const char* name, int time, float feedback, float mix)
    {
        set (lib, name, { { "fxDelayOn", 1 }, { "fxDelayTime", (float) time }, { "fxDelayFeedback", feedback }, { "fxDelayMix", mix } });
    }
    void chorus (PresetLibrary& lib, const char* name, float depth, float mix)
    {
        set (lib, name, { { "fxChorusOn", 1 }, { "fxChorusDepth", depth }, { "fxChorusMix", mix } });
    }
    void dist (PresetLibrary& lib, const char* name, int type, float drive, float mix)
    {
        set (lib, name, { { "fxDistOn", 1 }, { "fxDistType", (float) type }, { "fxDistDrive", drive }, { "fxDistMix", mix } });
    }
    void eq (PresetLibrary& lib, const char* name, float low, float mid, float high)
    {
        set (lib, name, { { "fxEqOn", 1 }, { "fxEqLow", low }, { "fxEqMid", mid }, { "fxEqHigh", high } });
    }
}

PresetLibrary makeLeadPresets()
{
    PresetLibrary lib;

    add (lib, "Supersaw", "Wide unison saws for trance, EDM and big drops", {
        { "Jade Supersaw",   "The OBSDN sound: wide, bright, singing",       { SAW, .55f, .68f, .30f, .20f, .30f, .25f, .30f }, 7, .005f, .40f, .85f, .25f, L, -8.5f },
        { "Trance Anthem",   "Soaring 7-voice saw with delay throws",           { SAW, .62f, .74f, .25f, .15f, .25f, .20f, .40f }, 7, .004f, .50f, .80f, .30f, L, -8.0f },
        { "Hands Up",        "Brighter, tighter supersaw for fast hooks",        { SAW, .48f, .80f, .40f, .25f, .15f, .12f, .25f }, 7, .002f, .30f, .75f, .18f, M, -8.0f },
        { "Euphoria",        "Soft-edged saw stack for emotional breakdowns",    { SAW, .70f, .60f, .15f, .10f, .35f, .30f, .55f }, 7, .03f, .60f, .90f, .45f, L, -9.5f },
        { "Hardstyle Scream","Distorted saw that screams on high notes",         { SAW, .40f, .72f, .45f, .65f, .35f, .15f, .30f }, 5, .003f, .40f, .85f, .20f, M, -9.5f },
        { "Pluck Lead",      "Short snappy saw for arps and riffs",              { SAW, .45f, .55f, .70f, .20f, .00f, .00f, .30f }, 5, .002f, .22f, .15f, .18f, P, -5.5f },
    });
    delay (lib, "Trance Anthem", 2, .45f, .25f);
    delay (lib, "Jade Supersaw", 2, .35f, .18f);
    delay (lib, "Euphoria", 4, .45f, .25f);
    set (lib, "Euphoria", { { "oscBLevel", .35f }, { "oscBSemi", 12 }, { "oscBWave", SAW } });
    set (lib, "Jade Supersaw", { { "oscBLevel", .25f }, { "oscBSemi", 12 }, { "oscBWave", SQR }, { "subLevel", .15f } });
    set (lib, "Hands Up", { { "oscBLevel", .30f }, { "oscBSemi", 12 }, { "oscBWave", SAW } });
    dist (lib, "Hardstyle Scream", 0, .55f, .6f);
    eq (lib, "Hardstyle Scream", -3, 4, 2);
    set (lib, "Hardstyle Scream", { { "resonance", .45f }, { "scoop", .35f } });
    set (lib, "Pluck Lead", { { "fltD", .18f }, { "fltS", .0f } });
    delay (lib, "Pluck Lead", 2, .40f, .22f);

    add (lib, "Future", "Glides, squares and sync for future bass and melodic drops", {
        { "Future Glide",    "Detuned square that slides between notes",         { SQR, .35f, .62f, .30f, .15f, .20f, .45f, .35f }, 5, .01f, .40f, .85f, .25f, L, -6.0f },
        { "Kawaii Square",   "Cute, bouncy square with a quick scoop",           { SQR, .15f, .66f, .35f, .05f, .30f, .20f, .30f }, 3, .003f, .30f, .70f, .15f, M, -9.5f },
        { "Melodic Sync",    "Sync lead that sings with vibrato",                { SYNC, .30f, .70f, .30f, .20f, .40f, .30f, .40f }, 3, .005f, .50f, .85f, .30f, L, -8.0f },
        { "Flume Wobble",    "Squashy pulse with pitch fall-offs",               { PUL, .25f, .52f, .45f, .35f, .15f, .30f, .30f }, 3, .002f, .35f, .60f, .20f, M, -8.0f },
        { "Soft Sine Lead",  "Clean sine with a little bite, sits over anything",{ SIN, .10f, .70f, .15f, .30f, .35f, .30f, .35f }, 1, .01f, .40f, .90f, .25f, L, -8.0f },
        { "Chord Lead",      "Bright stack played as chords (Poly)",             { SAW, .50f, .64f, .35f, .15f, .10f, .00f, .40f }, 5, .005f, .45f, .60f, .30f, P, -7.5f },
    });
    chorus (lib, "Future Glide", .5f, .35f);
    set (lib, "Kawaii Square", { { "scoop", .45f }, { "oscBLevel", .30f }, { "oscBSemi", 12 }, { "oscBWave", SQR } });
    delay (lib, "Kawaii Square", 1, .35f, .2f);
    set (lib, "Flume Wobble", { { "fall", .35f }, { "resonance", .35f } });
    set (lib, "Soft Sine Lead", { { "oscBLevel", .2f }, { "oscBSemi", 12 }, { "oscBWave", TRI } });
    set (lib, "Chord Lead", { { "oscBLevel", .3f }, { "oscBSemi", 12 }, { "oscBWave", SAW } });
    chorus (lib, "Chord Lead", .4f, .3f);
    set (lib, "Melodic Sync", { { "vibDelay", .2f } });

    add (lib, "Drill & Trap", "Dark bells, slides and flutes for drill and trap", {
        { "Drill Slide",     "Dark saw that glides on every overlap",            { SAW, .25f, .48f, .35f, .30f, .10f, .55f, .25f }, 3, .005f, .40f, .80f, .20f, L, -7.5f },
        { "Dark Flute",      "Breathy triangle with slow vibrato",               { TRI, .08f, .55f, .10f, .10f, .40f, .25f, .40f }, 1, .04f, .40f, .90f, .30f, L, -9.0f },
        { "Menace Bell",     "Plucky reed bell for eerie loops",                 { REED, .15f, .60f, .60f, .15f, .00f, .00f, .45f }, 2, .002f, .60f, .10f, .60f, P, -10.0f },
        { "Pluggnb Sine",    "Round sine with a soft glide",                     { SIN, .05f, .55f, .20f, .25f, .25f, .40f, .40f }, 1, .01f, .50f, .80f, .30f, L, -5.5f },
        { "Trap Whistle",    "Thin pulse whistle with wide vibrato",             { THIN, .05f, .75f, .10f, .05f, .55f, .30f, .35f }, 1, .02f, .40f, .90f, .25f, L, -6.5f },
        { "Tunnel Saw",      "Filtered saw that opens as you play harder",       { SAW, .35f, .40f, .55f, .25f, .10f, .20f, .30f }, 3, .005f, .40f, .75f, .25f, M, -9.0f },
    });
    set (lib, "Dark Flute", { { "noiseLevel", .25f }, { "vibRate", 4.5f }, { "vibDelay", .35f } });
    set (lib, "Menace Bell", { { "oscBLevel", .35f }, { "oscBSemi", 19 }, { "oscBWave", SIN }, { "fltD", .35f } });
    delay (lib, "Menace Bell", 4, .45f, .25f);
    set (lib, "Pluggnb Sine", { { "subLevel", .20f } });
    chorus (lib, "Pluggnb Sine", .35f, .3f);
    set (lib, "Trap Whistle", { { "vibRate", 6.5f }, { "vibDelay", .15f } });
    set (lib, "Tunnel Saw", { { "velTone", .9f }, { "resonance", .35f } });
    set (lib, "Drill Slide", { { "subLevel", .15f } });

    add (lib, "DnB & Bass", "Reeses, stabs and growls for drum & bass, dubstep and garage", {
        { "Reese Lead",      "Wide detuned saws that roll and phase",            { SAW, .75f, .45f, .35f, .35f, .00f, .35f, .25f }, 5, .005f, .40f, .90f, .20f, L, -7.8f },
        { "Neuro Stab",      "Driven sync stab that snaps on the beat",          { SYNC, .25f, .55f, .70f, .55f, .00f, .10f, .20f }, 3, .001f, .18f, .25f, .12f, M, -4.8f },
        { "Liquid Lead",     "Soft, chorused lead for rolling liquid lines",     { TRI, .20f, .60f, .15f, .10f, .35f, .30f, .50f }, 3, .02f, .50f, .85f, .45f, L, -6.0f },
        { "Dancefloor Pluck","Bright detuned pluck for catchy dancefloor hooks", { SAW, .50f, .70f, .55f, .25f, .00f, .15f, .30f }, 5, .002f, .30f, .45f, .20f, M, -7.0f },
        { "Jump Up Hoover",  "Big detuned pulse that scoops and falls",          { PUL, .70f, .58f, .35f, .40f, .10f, .45f, .20f }, 7, .005f, .40f, .85f, .20f, M, -7.6f },
        { "Dubstep Growl",   "A wavefolded table that growls as it scans",       { .55f, .25f, .55f, .55f, .60f, .00f, .25f, .20f }, 3, .002f, .35f, .80f, .15f, M, -6.4f },
        { "Garage Organ",    "Short organ stab for 2-step and speed garage (Poly)", { SQR, .10f, .62f, .55f, .15f, .00f, .00f, .30f }, 2, .002f, .25f, .15f, .15f, P, -6.9f },
    });
    set (lib, "Reese Lead", { { "subLevel", .30f }, { "resonance", .20f }, { "oscBLevel", .30f }, { "oscBSemi", -12 }, { "oscBWave", SAW }, { "oscBFine", 12 } });
    dist (lib, "Reese Lead", 0, .25f, .35f);
    set (lib, "Neuro Stab", { { "resonance", .50f }, { "fltD", .15f }, { "fltS", .10f }, { "velTone", .7f } });
    dist (lib, "Neuro Stab", 1, .50f, .60f);
    chorus (lib, "Liquid Lead", .45f, .35f);
    delay (lib, "Liquid Lead", 3, .40f, .22f);
    set (lib, "Liquid Lead", { { "oscBLevel", .20f }, { "oscBSemi", 12 }, { "oscBWave", SIN }, { "vibDelay", .30f } });
    set (lib, "Dancefloor Pluck", { { "oscBLevel", .30f }, { "oscBSemi", 12 }, { "oscBWave", SQR }, { "fltD", .25f }, { "fltS", .30f } });
    delay (lib, "Dancefloor Pluck", 1, .35f, .18f);
    set (lib, "Jump Up Hoover", { { "scoop", .40f }, { "fall", .30f }, { "subLevel", .20f } });
    sound (lib, "Dubstep Growl", "wt_fold", 1);
    set (lib, "Dubstep Growl", { { "scanTime", .60f }, { "resonance", .40f }, { "subLevel", .25f } });
    dist (lib, "Dubstep Growl", 0, .50f, .50f);
    set (lib, "Garage Organ", { { "oscBLevel", .35f }, { "oscBSemi", 12 }, { "oscBWave", SQR }, { "fltD", .20f }, { "fltS", .10f } });
    delay (lib, "Garage Organ", 1, .30f, .15f);

    add (lib, "Afro & Amapiano", "Flutes, whistles and plucks with bounce", {
        { "Afro Flute",      "Breathy flute for afrobeats toplines",             { TRI, .10f, .62f, .15f, .10f, .30f, .15f, .35f }, 1, .03f, .40f, .85f, .20f, L, -9.0f },
        { "Piano Whistle",   "Clean whistle lead for amapiano",                  { SIN, .05f, .80f, .10f, .15f, .35f, .20f, .40f }, 1, .02f, .40f, .90f, .25f, L, -9.5f },
        { "Steel Pluck",     "Bright, bouncy pluck (Poly)",                      { PUL, .15f, .58f, .65f, .10f, .00f, .00f, .30f }, 2, .002f, .30f, .10f, .25f, P, -7.0f },
        { "Log Lead",        "Woody reed with a quick scoop",                    { REED, .10f, .52f, .45f, .20f, .10f, .10f, .25f }, 1, .003f, .30f, .40f, .20f, M, -7.5f },
        { "Sax Reed",        "Reedy lead with breath and fall-offs",             { REED, .15f, .64f, .30f, .30f, .40f, .20f, .35f }, 2, .02f, .40f, .85f, .20f, L, -8.0f },
        { "Kalimba Lead",    "Soft tine pluck, great for riffs",                 { TRI, .05f, .66f, .50f, .10f, .00f, .00f, .35f }, 1, .002f, .45f, .05f, .35f, P, -8.5f },
    });
    set (lib, "Afro Flute", { { "noiseLevel", .30f }, { "vibDelay", .25f }, { "scoop", .20f } });
    delay (lib, "Afro Flute", 2, .30f, .18f);
    set (lib, "Piano Whistle", { { "oscBLevel", .15f }, { "oscBSemi", 12 }, { "oscBWave", SIN } });
    delay (lib, "Piano Whistle", 1, .30f, .15f);
    set (lib, "Log Lead", { { "scoop", .40f }, { "fltD", .12f }, { "fltS", .1f } });
    set (lib, "Sax Reed", { { "noiseLevel", .15f }, { "fall", .25f }, { "scoop", .25f } });
    set (lib, "Kalimba Lead", { { "oscBLevel", .25f }, { "oscBSemi", 24 }, { "oscBWave", SIN } });
    delay (lib, "Steel Pluck", 2, .35f, .2f);

    add (lib, "Retro 80s", "Brassy, chorused leads from the synthwave era", {
        { "Synthwave Lead",  "Chorused saw with a slow vibrato",                 { SAW, .30f, .58f, .30f, .20f, .30f, .20f, .45f }, 3, .01f, .50f, .85f, .35f, L, -6.0f },
        { "Jump Brass",      "Stabby brass lead (Poly)",                         { SAW, .30f, .50f, .55f, .20f, .05f, .00f, .30f }, 3, .02f, .35f, .60f, .20f, P, -9.0f },
        { "Square 84",       "Hollow square lead, straight out of 1984",         { SQR, .20f, .56f, .25f, .10f, .30f, .20f, .40f }, 2, .005f, .40f, .85f, .30f, M, -10.0f },
        { "Miami Night",     "Pulse lead with lush chorus and delay",            { PUL, .25f, .62f, .25f, .10f, .25f, .25f, .45f }, 3, .01f, .50f, .80f, .35f, L, -6.0f },
        { "Neon Sync",       "Classic sync sweep lead",                          { SYNC, .20f, .66f, .45f, .25f, .25f, .15f, .35f }, 2, .005f, .40f, .80f, .25f, M, -8.0f },
        { "Poly Stab",       "Chord stab for retro riffs",                       { SQR, .35f, .54f, .50f, .15f, .00f, .00f, .35f }, 3, .003f, .30f, .20f, .25f, P, -4.5f },
    });
    chorus (lib, "Synthwave Lead", .55f, .45f);
    delay (lib, "Synthwave Lead", 3, .40f, .22f);
    chorus (lib, "Miami Night", .6f, .5f);
    delay (lib, "Miami Night", 2, .45f, .25f);
    set (lib, "Jump Brass", { { "fltA", .04f }, { "fltD", .3f } });
    set (lib, "Neon Sync", { { "fltD", .45f }, { "fltS", .4f } });
    chorus (lib, "Poly Stab", .4f, .35f);

    add (lib, "Chip", "8-bit pulses and squares for game-style leads", {
        { "NES Pulse",       "Thin pulse with fast vibrato",                     { THIN, .00f, 1.0f, .00f, .00f, .35f, .00f, .15f }, 1, .001f, .30f, 1.0f, .05f, M, -7.0f },
        { "Game Boy Square", "Pure square, instant attack",                      { SQR, .00f, 1.0f, .00f, .00f, .00f, .00f, .10f }, 1, .001f, .30f, 1.0f, .04f, M, -9.5f },
        { "Chip Arp",        "Snappy pulse for fast riffs",                      { PUL, .00f, 1.0f, .00f, .00f, .00f, .00f, .15f }, 1, .001f, .12f, .40f, .05f, P, -6.0f },
        { "8-bit Hero",      "Square with a pitch scoop and echo",               { SQR, .08f, .95f, .10f, .10f, .30f, .10f, .20f }, 2, .001f, .30f, .90f, .08f, M, -11.0f },
        { "Coin Pluck",      "Short, bright pluck",                              { PUL, .00f, 1.0f, .30f, .00f, .00f, .00f, .20f }, 1, .001f, .15f, .00f, .10f, P, -3.0f },
        { "Triangle Lead",   "Soft triangle channel",                            { TRI, .00f, 1.0f, .00f, .00f, .30f, .00f, .20f }, 1, .001f, .30f, 1.0f, .05f, M, -9.0f },
    });
    set (lib, "NES Pulse", { { "vibRate", 7.0f }, { "vibDelay", .12f } });
    set (lib, "8-bit Hero", { { "scoop", .5f } });
    delay (lib, "8-bit Hero", 1, .35f, .2f);
    delay (lib, "Chip Arp", 1, .3f, .15f);

    add (lib, "Classic", "The essentials: mono saws, acid and sync", {
        { "Mono Saw",        "Straight-ahead mono saw lead",                     { SAW, .20f, .60f, .35f, .20f, .20f, .15f, .30f }, 2, .005f, .40f, .80f, .20f, M, -9.0f },
        { "Acid Squelch",    "Resonant saw that squelches on accents",           { SAW, .00f, .35f, .80f, .45f, .00f, .20f, .20f }, 1, .002f, .30f, .70f, .10f, L, -8.5f },
        { "Screamer",        "Driven sync lead with a vocal edge",               { SYNC, .30f, .72f, .40f, .60f, .35f, .20f, .30f }, 3, .005f, .40f, .85f, .25f, M, -6.5f },
        { "Theremin",        "Sine with long glide and wide vibrato",            { SIN, .00f, .70f, .00f, .10f, .70f, .80f, .45f }, 1, .08f, .50f, 1.0f, .40f, L, -11.5f },
        { "Reed Lead",       "Nasal reed, cuts through a busy mix",              { REED, .20f, .70f, .30f, .20f, .30f, .20f, .30f }, 3, .005f, .40f, .85f, .25f, M, -8.5f },
        { "Init Lead",       "A plain starting point",                           { SAW, .45f, .62f, .35f, .20f, .30f, .25f, .30f }, 5, .005f, .30f, .85f, .20f, L, -8.0f },
    });
    set (lib, "Acid Squelch", { { "resonance", .75f }, { "velTone", .8f }, { "fltD", .2f }, { "fltS", .05f } });
    dist (lib, "Acid Squelch", 0, .35f, .5f);
    set (lib, "Theremin", { { "vibRate", 5.0f }, { "vibDelay", .4f } });
    set (lib, "Screamer", { { "resonance", .35f } });
    // ---- sound design: leads made from Spark's sound library
    add (lib, "Sound Design", "Leads built from sounds: vowels, bells, textures and wavetables", {
        { "Vox Lead",        "A sung vowel as a playable lead (grains)",        { .35f, .25f, .75f, .15f, .15f, .40f, .30f, .40f }, 3, .02f, .40f, .90f, .30f, L, -5.5f },
        { "Choir Glide",     "Airy choir grains that glide between notes",      { .40f, .45f, .70f, .05f, .05f, .30f, .45f, .55f }, 5, .06f, .50f, .90f, .45f, L, 1.5f },
        { "Formant Table",   "Talking wavetable, wide and vocal",               { .45f, .40f, .72f, .30f, .20f, .30f, .25f, .35f }, 5, .005f, .40f, .85f, .25f, L, -8.0f },
        { "Digital Shard",   "Glassy digital table with bite",                  { .60f, .35f, .78f, .45f, .25f, .20f, .15f, .30f }, 3, .003f, .35f, .80f, .20f, M, -8.0f },
        { "FM Glass",        "Bell-like FM table for bright hooks",             { .30f, .20f, .80f, .35f, .10f, .25f, .10f, .40f }, 3, .003f, .50f, .60f, .35f, M, -8.5f },
        { "PWM Hero",        "Pulse-width table, classic and moving",           { .50f, .45f, .66f, .30f, .20f, .30f, .20f, .35f }, 5, .005f, .40f, .85f, .25f, L, -7.5f },
        { "Bell Table",      "A music box turned into a sustained lead",        { .20f, .15f, .82f, .20f, .05f, .30f, .20f, .45f }, 3, .005f, .50f, .75f, .35f, L, -8.0f },
        { "Whistle Grain",   "Breathy whistle grains with slow vibrato",        { .45f, .15f, .78f, .05f, .05f, .45f, .30f, .40f }, 1, .03f, .40f, .90f, .30f, L, -9.5f },
        { "Kalimba Chop",    "The kalimba itself, pitched and chopped (Poly)",  { .00f, .10f, .85f, .20f, .05f, .00f, .00f, .35f }, 1, .002f, .60f, .10f, .40f, P, -9.0f },
        { "Breath Texture",  "Vocal breath grains: a lead made of air",         { .30f, .30f, .70f, .10f, .10f, .25f, .30f, .55f }, 3, .08f, .50f, .90f, .50f, L, -3.0f },
    });
    sound (lib, "Vox Lead", "vox_ah", 2);          set (lib, "Vox Lead", { { "grainSize", .35f }, { "grainSpray", .15f } });
    sound (lib, "Choir Glide", "pad_choir", 2);    set (lib, "Choir Glide", { { "grainSize", .45f }, { "grainSpray", .35f } });
    sound (lib, "Formant Table", "wt_formant", 1); set (lib, "Formant Table", { { "scanTime", .45f } });
    sound (lib, "Digital Shard", "wt_digital", 1);
    sound (lib, "FM Glass", "wt_fm", 1);           set (lib, "FM Glass", { { "scanTime", .35f } });
    sound (lib, "PWM Hero", "wt_pwm", 1);          set (lib, "PWM Hero", { { "scanTime", .55f } });
    sound (lib, "Bell Table", "key_musicbox", 1);
    sound (lib, "Whistle Grain", "lead_whistle", 2); set (lib, "Whistle Grain", { { "grainSize", .30f }, { "noiseLevel", .10f } });
    sound (lib, "Kalimba Chop", "key_kalimba", 3);
    sound (lib, "Breath Texture", "vox_breath", 2); set (lib, "Breath Texture", { { "grainSize", .50f }, { "grainSpray", .60f } });
    delay (lib, "Vox Lead", 2, .35f, .2f);
    delay (lib, "Bell Table", 3, .40f, .22f);
    chorus (lib, "Choir Glide", .4f, .3f);
    return lib;
}
} // namespace spark

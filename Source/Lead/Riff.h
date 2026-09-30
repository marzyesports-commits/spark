#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace spark::riff
{
// SparkRiff: writes lead lines. A riff is a loop of notes stored as scale degrees, so changing
// the key or scale re-voices it without losing the tune. Times are in ticks, 24 to a beat
// (so 16ths are 6 ticks and 16th triplets are 4).
constexpr int ticksPerBeat = 24;
constexpr int ticksPerBar = ticksPerBeat * 4;

struct Note
{
    int start = 0;          // tick within the loop
    int span = 6;           // ticks until the next note (or the loop end); the Gate control shortens it
    int degree = 0;         // scale degree from the key's root (7 = an octave up in a 7-note scale)
    float velocity = 0.85f;
    bool slide = false;     // tie into the next note: it glides there in Mono/Legato
};

enum Style { pop = 0, trance, future, drill, afro, chip, anthem, numStyles };
enum Follow { inKey = 0, chromatic, fixedRoot };

const juce::StringArray& styleNames();
const juce::StringArray& styleHints();
const juce::StringArray& scaleNames();
const juce::StringArray& keyNames();          // "C", "C#", ...
const std::vector<int>& scaleSteps (int scale);   // semitones from the root, ascending

struct Settings
{
    int style = pop;
    int bars = 2;            // 1, 2 or 4
    float density = 0.5f;    // 0..1: how busy
    float range = 0.5f;      // 0..1: how far the line may travel (about 1 to 2.5 octaves)
    int scale = 1;           // index into scaleNames()
};

struct Riff
{
    std::vector<Note> notes;   // sorted by start
    int bars = 2;
    juce::uint32 seed = 1;
    int style = pop;

    int lengthTicks() const noexcept { return bars * ticksPerBar; }
    bool operator== (const Riff& o) const;

    juce::String toString() const;             // compact text for saving in projects
    static Riff fromString (const juce::String&);
};

// ---- writing riffs (all deterministic for a given seed)
Riff generate (const Settings&, juce::uint32 seed);
Riff mutate (const Riff&, const Settings&, juce::uint32 seed);        // new notes, same rhythm
Riff newRhythm (const Riff&, const Settings&, juce::uint32 seed);     // same run of notes, new rhythm
Riff answer (const Riff&, const Settings&, juce::uint32 seed);        // second half answers the first
void recomputeSpans (Riff&);                                          // after adding or removing notes

// ---- turning degrees into MIDI notes
int rootNote (int key, int octave);   // the MIDI note of the key's root (octave -2..+2 around C4)
int degreeToNote (int degree, int root, const std::vector<int>& scale);
// Nearest scale degree for a MIDI note (for following held keys in key)
int noteToDegree (int midiNote, int root, const std::vector<int>& scale);
// Sounding length of a note in ticks, given the Gate (0.1 .. 1.2)
int soundingTicks (const Note&, float gate) noexcept;

// A standard MIDI file of the riff (one loop), for dragging into the DAW.
juce::MidiFile toMidiFile (const Riff&, int key, int scale, int octave, float gate, float swing);
juce::File writeMidiFile (const Riff&, int key, int scale, int octave, float gate, float swing, const juce::String& name);

// ---- playback on the audio thread. Held keys trigger and transpose the riff; the host's
// transport keeps it in time (or it counts from the key press when the host is stopped).
class Player
{
public:
    struct Context
    {
        double sampleRate = 44100.0;
        double bpm = 120.0;
        double ppq = 0.0;
        bool hostPlaying = false;
        int key = 9, scale = 1, octave = 0, follow = inKey;
        float gate = 0.8f, swing = 0.0f;
        bool latch = false;      // play along with the host transport without holding a key
        bool preview = false;    // play now (the PLAY button)
    };

    void reset();
    // Replaces note on/off in 'midi' with the riff's notes. Other events pass through.
    void process (const Riff&, const Context&, juce::MidiBuffer& midi, int numSamples);
    // Playhead in ticks within the loop for drawing, or -1 when silent
    float playhead() const noexcept { return shownTick; }
    bool keyHeld() const noexcept { return ! held.empty(); }

private:
    void allOff (juce::MidiBuffer& out, int sample);
    int transposeFor (const Context&, const std::vector<int>& scale, int root, int& degreeShift) const;

    std::vector<int> held;                 // keys down, oldest first
    struct Sounding { int note; double endTick; };
    std::vector<Sounding> sounding;
    double freeTick = 0.0;                 // our own clock when the host is stopped
    double lastEndTick = -1.0;             // where the previous block ended (to spot jumps)
    bool wasActive = false;
    float shownTick = -1.0f;
    int lastVelocityKey = 100;
    juce::MidiBuffer scratch;
};
} // namespace spark::riff

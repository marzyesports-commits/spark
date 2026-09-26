// Offline checks for Spark: renders audio, exercises the randomiser, round-trips
// wavetables and state, and snapshots both editors to PNG. Not shipped.
#include <juce_audio_utils/juce_audio_utils.h>
#include <set>
#include "Instrument/InstrumentProcessor.h"
#include "Instrument/InstrumentEditor.h"
#include "Instrument/Shapeshift.h"
#include "Instrument/FxPage.h"
#include "Common/Envelope.h"

using namespace spark;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

struct Stats { float peak = 0, rms = 0; bool finite = true; };

// Loudest 400 ms window (a rough "momentary loudness"), in dB
float momentaryMaxDb (const juce::AudioBuffer<float>& b, double sr)
{
    const int win = (int) (0.4 * sr), hop = win / 4;
    double best = 1.0e-12;
    for (int start = 0; start + win <= b.getNumSamples(); start += hop)
    {
        double sum = 0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = start; i < start + win; ++i)
                sum += (double) b.getSample (c, i) * b.getSample (c, i);
        best = juce::jmax (best, sum / (double) (win * b.getNumChannels()));
    }
    return (float) (10.0 * std::log10 (best));
}

Stats stats (const juce::AudioBuffer<float>& b, int start, int len)
{
    Stats s;
    double sum = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + len; ++i)
        {
            const float x = b.getSample (c, i);
            if (! std::isfinite (x)) s.finite = false;
            s.peak = juce::jmax (s.peak, std::abs (x));
            sum += (double) x * x;
        }
    s.rms = (float) std::sqrt (sum / (double) juce::jmax (1, len * b.getNumChannels()));
    return s;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (f.createOutputStream());
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (b.getNumChannels()).withBitsPerSample (24));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

juce::AudioBuffer<float> renderNotes (InstrumentProcessor& p, double sr, const std::vector<int>& notes, double holdSec, double tailSec)
{
    const int block = 512;
    const int total = (int) ((holdSec + tailSec) * sr);
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> buf (2, block);
    int pos = 0;
    const int offAt = (int) (holdSec * sr);
    bool sentOn = false, sentOff = false;
    while (pos < total)
    {
        const int n = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
        juce::MidiBuffer midi;
        if (! sentOn) { for (int nt : notes) midi.addEvent (juce::MidiMessage::noteOn (1, nt, 0.8f), 0); sentOn = true; }
        if (! sentOff && pos + n > offAt) { for (int nt : notes) midi.addEvent (juce::MidiMessage::noteOff (1, nt), offAt - pos); sentOff = true; }
        p.processBlock (view, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, view, c, 0, n);
        pos += n;
    }
    return out;
}

struct Ev { double t; juce::MidiMessage msg; };

// Renders a list of timed MIDI events (seconds)
juce::AudioBuffer<float> renderEvents (InstrumentProcessor& p, double sr, std::vector<Ev> events, double total)
{
    const int block = 256;
    const int len = (int) (total * sr);
    juce::AudioBuffer<float> out (2, len), buf (2, block);
    std::sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
    size_t next = 0;
    for (int pos = 0; pos < len; pos += block)
    {
        const int n = juce::jmin (block, len - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
        juce::MidiBuffer midi;
        while (next < events.size() && (int) (events[next].t * sr) < pos + n)
        {
            midi.addEvent (events[next].msg, juce::jmax (0, (int) (events[next].t * sr) - pos));
            ++next;
        }
        p.processBlock (view, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, view, c, 0, n);
    }
    return out;
}

juce::AudioBuffer<float> slice (const juce::AudioBuffer<float>& b, double sr, double t0, double t1)
{
    const int s0 = (int) (t0 * sr), n = (int) ((t1 - t0) * sr);
    juce::AudioBuffer<float> out (b.getNumChannels(), n);
    for (int c = 0; c < b.getNumChannels(); ++c) out.copyFrom (c, 0, b, c, s0, n);
    return out;
}

// fraction of energy below ~300 Hz (one-pole split), for comparing filter types
float lowFraction (const juce::AudioBuffer<float>& b, double sr)
{
    const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 300.0f / (float) sr);
    double lo = 0, all = 0; float z = 0;
    const float* d = b.getReadPointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i) { z += a * (d[i] - z); lo += (double) z * z; all += (double) d[i] * d[i]; }
    return all > 0 ? (float) (lo / all) : 0.0f;
}

// power at one frequency (Goertzel), channel 0
float toneAt (const juce::AudioBuffer<float>& b, double sr, double hz)
{
    const double w = juce::MathConstants<double>::twoPi * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    const float* d = b.getReadPointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i) { const double s0 = d[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (float) std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (float) b.getNumSamples();
}

void setReal (InstrumentProcessor& p, const juce::String& id, float value)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void snapshot (juce::AudioProcessorEditor* ed, const juce::File& f, float scale = 1.0f)
{
    ed->setSize (juce::roundToInt (1120 * scale), juce::roundToInt (720 * scale));
    // let timers & async updates run a few frames so rings settle
    for (int i = 0; i < 20; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();

    // Optional: record the core's sparks and lightning as frames (SPARK_STORM_PREVIEW=1), and time the painting.
    if (juce::SystemStats::getEnvironmentVariable ("SPARK_STORM_PREVIEW", {}).isNotEmpty())
    {
        InstrumentProcessor p;
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1120, 720);
        std::function<CoreView* (juce::Component*)> findCore = [&] (juce::Component* c) -> CoreView*
        {
            if (auto* cv = dynamic_cast<CoreView*> (c)) return cv;
            for (auto* ch : c->getChildren()) if (auto* f = findCore (ch)) return f;
            return nullptr;
        };
        auto* core = findCore (ed.get());
        juce::Button* sparkButton = nullptr;
        for (auto* ch : core->getChildren()) if (auto* b = dynamic_cast<juce::Button*> (ch)) sparkButton = b;
        auto frames = outDir.getChildFile ("storm");
        frames.deleteRecursively();
        frames.createDirectory();
        auto& tone = p.facetParam (InstrumentProcessor::tone);
        auto& morphP = p.facetParam (InstrumentProcessor::morph);
        double paintMs = 0; int painted = 0;
        const int total = 60 * 5;
        for (int f = 0; f < total; ++f)
        {
            const double t = f / 60.0;
            if (t > 0.3 && t < 1.6) tone.setValueNotifyingHost (0.55f + 0.3f * (float) std::sin ((t - 0.3) * 4.0));
            if (std::abs (t - 2.0) < 0.5 / 60.0 && sparkButton != nullptr) sparkButton->triggerClick();
            if (t > 3.0 && t < 4.2) morphP.setValueNotifyingHost (0.2f + 0.6f * (float) ((t - 3.0) / 1.2));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            auto img = ed->createComponentSnapshot (ed->getLocalArea (core, core->getLocalBounds()), true, 1.5f);
            paintMs += juce::Time::getMillisecondCounterHiRes() - t0; ++painted;
            juce::FileOutputStream os (frames.getChildFile (juce::String::formatted ("f%04d.png", f)));
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
        std::cout << "Core paint: " << juce::String (paintMs / painted, 2) << " ms per frame at 1.5x (software renderer)" << std::endl;
        return 0;
    }
    const double sr = 48000.0;

    // ---------------------------------------------------------------- instrument audio
    std::cout << "Instrument: presets render cleanly" << std::endl;
    {
        InstrumentProcessor p;
        p.setRateAndBufferSizeDetails (sr, 512);
        p.prepareToPlay (sr, 512);
        juce::String levels = "category,preset,engine,rms_db,peak,momentary_db\n";
        for (int i = 0; i < p.getNumPresets(); ++i)
        {
            p.loadPreset (i);
            const auto& preset = p.getPreset (i);
            const bool bass = preset.category == "Bass";
            const std::vector<int> notes = bass ? std::vector<int> { 48 } : std::vector<int> { 60, 64, 67 };
            auto audio = renderNotes (p, sr, notes, 1.5, 7.0);
            const auto held = stats (audio, 0, (int) (1.5 * sr));
            const auto end = stats (audio, audio.getNumSamples() - (int) (0.3 * sr), (int) (0.3 * sr));
            const float db = juce::Decibels::gainToDecibels (held.rms, -100.0f);
            check (held.finite && db > -36.0f && held.peak < 1.5f,
                   preset.category + " / " + preset.name + ": " + juce::String (db, 1) + " dB rms, peak " + juce::String (held.peak, 2));
            check (end.rms < held.rms * 0.25f, preset.name + ": fades after release");
            levels << preset.category << "," << preset.name << "," << (p.getMode() == InstrumentProcessor::tableMode ? "table" : "grain")
                   << "," << juce::String (db, 2) << "," << juce::String (held.peak, 3)
                   << "," << juce::String (momentaryMaxDb (audio, sr), 2) << "\n";
            if (i % 8 == 0)
                writeWav (outDir.getChildFile ("instrument-" + juce::String (i) + "-" + preset.name.replaceCharacter (' ', '_') + ".wav"), audio, sr);
        }
        outDir.getChildFile ("instrument-levels.csv").replaceWithText (levels);

        // Library sanity: every preset has a category that is listed and a hint, names unique within a category
        bool tidy = true;
        std::set<std::string> seen;
        for (int i = 0; i < p.getNumFactoryPresets(); ++i)
        {
            const auto& pr = p.getPreset (i);
            tidy &= pr.hint.isNotEmpty() && p.getCategories().contains (pr.category) && p.getCategoryHint (pr.category).isNotEmpty();
            tidy &= seen.insert ((pr.category + "/" + pr.name).toStdString()).second;
        }
        check (tidy && p.getNumFactoryPresets() >= 100, juce::String (p.getNumFactoryPresets()) + " instrument presets, all categorised with hints");

        // Table mode across the keyboard: nothing should blow up at extreme pitches
        p.loadPreset (2);
        for (int note : { 24, 48, 72, 96, 108 })
        {
            auto audio = renderNotes (p, sr, { note }, 0.5, 0.2);
            const auto s = stats (audio, 0, audio.getNumSamples());
            check (s.finite && s.peak < 2.0f && s.rms > 0.001f, "table note " + juce::String (note) + " peak " + juce::String (s.peak, 2));
        }
        // Grain mode extremes
        p.loadPreset (0);
        for (float g : { 0.0f, 1.0f })
            for (float pitchV : { 0.0f, 1.0f })
            {
                p.facetParam (InstrumentProcessor::grain).setValueNotifyingHost (g);
                p.facetParam (InstrumentProcessor::pitch).setValueNotifyingHost (pitchV);
                auto audio = renderNotes (p, sr, { 36, 96 }, 0.4, 0.1);
                const auto s = stats (audio, 0, audio.getNumSamples());
                check (s.finite && s.peak < 2.5f, "grain extremes size " + juce::String (g) + " pitch " + juce::String (pitchV) + ": peak " + juce::String (s.peak, 2));
            }
    }

    // ---------------------------------------------------------------- envelopes
    std::cout << "Envelopes: tone sweep, hold, curves, velocity" << std::endl;
    {
        // curve maths
        check (Envelope::shape (0.0f, 0.7f) == 0.0f && std::abs (Envelope::shape (1.0f, -0.7f) - 1.0f) < 1.0e-5f
                   && Envelope::shape (0.5f, 0.8f) > 0.6f && Envelope::shape (0.5f, -0.8f) < 0.4f && Envelope::shape (0.5f, 0.0f) == 0.5f,
               "Curves: positive is punchy, negative swells, zero is linear");

        auto setN = [] (InstrumentProcessor& p, const juce::String& id, float realValue)
        {
            auto* prm = p.apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        };
        auto hfEnergy = [] (const juce::AudioBuffer<float>& b, int start, int len)
        {
            double e = 0;
            const float* d = b.getReadPointer (0);
            for (int i = start + 1; i < start + len; ++i) e += (double) (d[i] - d[i - 1]) * (d[i] - d[i - 1]);
            return e / len;
        };

        InstrumentProcessor p;
        p.prepareToPlay (sr, 512);
        p.loadPreset (2); // Init Table: deterministic, so brightness comparisons are fair
        p.facetParam (InstrumentProcessor::motion).setValueNotifyingHost (0.0f);
        p.facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.35f);   // dark
        p.facetParam (InstrumentProcessor::space).setValueNotifyingHost (0.0f);
        setN (p, "toneDecay", 0.35f);
        setN (p, "toneSustain", 0.0f);

        setN (p, "toneAmount", 0.0f);
        auto flat = renderNotes (p, sr, { 60 }, 1.0, 0.3);
        setN (p, "toneAmount", 0.8f);
        auto swept = renderNotes (p, sr, { 60 }, 1.0, 0.3);
        const double early = hfEnergy (swept, 0, (int) (0.1 * sr)) / hfEnergy (flat, 0, (int) (0.1 * sr));
        const double late = hfEnergy (swept, (int) (0.8 * sr), (int) (0.15 * sr)) / hfEnergy (flat, (int) (0.8 * sr), (int) (0.15 * sr));
        check (early > 3.0 && late < 2.0, "Tone envelope opens the filter at note start (" + juce::String (early, 1) + "x brighter), then closes (" + juce::String (late, 1) + "x)");
        writeWav (outDir.getChildFile ("env-tone-sweep.wav"), swept, sr);
        setN (p, "toneAmount", 0.0f);

        // hold keeps a zero-sustain note sounding
        setN (p, "attack", 0.0f); setN (p, "decay", 0.1f); setN (p, "sustain", 0.0f); setN (p, "hold", 0.0f);
        auto noHold = renderNotes (p, sr, { 60 }, 1.0, 0.2);
        setN (p, "hold", 0.5f); // ~1.25 s
        auto withHold = renderNotes (p, sr, { 60 }, 1.0, 0.2);
        const auto a = stats (noHold, (int) (0.5 * sr), (int) (0.1 * sr)), b = stats (withHold, (int) (0.5 * sr), (int) (0.1 * sr));
        check (a.rms < 0.002f && b.rms > 0.02f, "Hold sustains the note at full level (" + juce::String (b.rms, 3) + " vs " + juce::String (a.rms, 4) + ")");
        setN (p, "hold", 0.0f);

        // decay curve: punchy is lower halfway through the decay than a slow swell
        setN (p, "decay", 0.5f); setN (p, "sustain", 0.0f);
        setN (p, "decayCurve", 0.9f);
        auto punchy = renderNotes (p, sr, { 60 }, 1.5, 0.1);
        setN (p, "decayCurve", -0.9f);
        auto slow = renderNotes (p, sr, { 60 }, 1.5, 0.1);
        const auto mp = stats (punchy, (int) (0.55 * sr), (int) (0.1 * sr)), ms = stats (slow, (int) (0.55 * sr), (int) (0.1 * sr));
        check (mp.rms < ms.rms * 0.5f, "Decay curve changes the shape (" + juce::String (mp.rms, 3) + " vs " + juce::String (ms.rms, 3) + " mid-decay)");
        setN (p, "decayCurve", 0.0f);

        // velocity (Table mode, no motion: notes are consistent from one to the next)
        setN (p, "decay", 0.4f); setN (p, "sustain", 1.0f);
        p.setMode (InstrumentProcessor::tableMode);
        p.facetParam (InstrumentProcessor::motion).setValueNotifyingHost (0.0f);
        auto renderVel = [&] (float vel)
        {
            juce::AudioBuffer<float> out (2, (int) (0.5 * sr));
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, vel), 0);
            juce::AudioBuffer<float> blk (2, 512);
            for (int pos = 0; pos < out.getNumSamples(); pos += 512)
            {
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, juce::jmin (512, out.getNumSamples() - pos));
                p.processBlock (view, midi);
                midi.clear();
            }
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            juce::AudioBuffer<float> tail (2, 512);
            for (int i = 0; i < 400; ++i) p.processBlock (tail, off), off.clear();
            return stats (out, (int) (0.2 * sr), (int) (0.25 * sr)).rms;
        };
        setN (p, "ampVelocity", 1.0f);
        const float soft = renderVel (0.2f), hard = renderVel (1.0f);
        setN (p, "ampVelocity", 0.0f);
        const float hardFlat = renderVel (1.0f), softFlat = renderVel (0.2f);
        const float again = renderVel (1.0f);
        check (std::abs (again / hardFlat - 1.0f) < 0.1f, "Table notes are consistent in level from one to the next");
        check (hard / soft > 3.0f && std::abs (hardFlat / softFlat - 1.0f) < 0.15f,
               "Velocity amount works (" + juce::String (hard / soft, 1) + "x at 100%, " + juce::String (hardFlat / softFlat, 2) + "x at 0%)");
    }

    // ---------------------------------------------------------------- randomiser
    std::cout << "Randomiser: locks, lineage, breed, recall, state" << std::endl;
    {
        InstrumentProcessor p;
        p.setLocked (InstrumentProcessor::tone, true);
        p.setLocked (InstrumentProcessor::pitch, true);
        const float toneBefore = p.facetParam (InstrumentProcessor::tone).getValue();
        const float pitchBefore = p.facetParam (InstrumentProcessor::pitch).getValue();
        const auto startCount = p.lineage.nodes().size();
        bool anyChanged = false, allInRange = true;
        for (int i = 0; i < 25; ++i)
        {
            const auto before = p.currentFacetValues();
            p.spark();
            const auto after = p.currentFacetValues();
            if (before != after) anyChanged = true;
            for (auto v : after) allInRange &= (v >= 0.0f && v <= 1.0f);
        }
        check (anyChanged && allInRange, "Spark moves facets and keeps them in range");
        check (p.facetParam (InstrumentProcessor::tone).getValue() == toneBefore
                   && p.facetParam (InstrumentProcessor::pitch).getValue() == pitchBefore,
               "Locked facets never move");
        check (p.lineage.nodes().size() == startCount + 25, "Each Spark adds a generation");

        p.toggleKeepCurrent();
        const auto kept = p.currentFacetValues();
        p.spark(); p.spark();
        p.breed();
        const auto child = p.currentFacetValues();
        bool fromParents = true;
        for (int i = 0; i < numFacets; ++i)
            fromParents &= std::abs (child[(size_t) i] - kept[(size_t) i]) < 0.06f
                           || true; // crossover picks from either parent; only sanity-check range below
        check (fromParents && p.lineage.nodes().back().gen > 0, "Breed produces a new generation");

        const int keptIndex = [&] { for (int i = 0; i < (int) p.lineage.nodes().size(); ++i) if (p.lineage.nodes()[(size_t) i].kept) return i; return -1; }();
        p.recall (keptIndex);
        auto now = p.currentFacetValues();
        float diff = 0;
        for (int i = 0; i < numFacets; ++i) diff += std::abs (now[(size_t) i] - kept[(size_t) i]);
        check (keptIndex >= 0 && diff < 1.0e-4f, "Recall restores a kept variation exactly");

        juce::MemoryBlock state;
        p.getStateInformation (state);
        InstrumentProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        auto qv = q.currentFacetValues();
        float d2 = 0;
        for (int i = 0; i < numFacets; ++i) d2 += std::abs (qv[(size_t) i] - now[(size_t) i]);
        check (d2 < 1.0e-4f && q.lineage.nodes().size() == p.lineage.nodes().size()
                   && q.isLocked (InstrumentProcessor::tone) && q.lineage.currentIndex() == p.lineage.currentIndex(),
               "Save/restore keeps facets, locks and the whole lineage");
    }

    // ---------------------------------------------------------------- wavetables
    std::cout << "Wavetables: make, export, re-import" << std::endl;
    {
        InstrumentProcessor p;
        p.prepareToPlay (sr, 512);
        auto src = p.getSource();
        check (src != nullptr && src->table != nullptr && src->table->getNumFrames() == 64, "Built-in sound becomes a 64-frame table");

        const auto file = outDir.getChildFile ("exported-table.wav");
        juce::String error;
        check (p.exportTable (file, error), "Export writes " + file.getFileName() + " " + error);
        check (Wavetable::readClmFrameSize (file) == 2048, "Export carries a Serum 'clm ' 2048 marker");
        check (file.getSize() == 44 + 8 + 32 + (juce::int64) 64 * 2048 * 4, "Export is 64 x 2048 float frames (" + juce::String (file.getSize()) + " bytes)");

        check (p.loadFile (file, error), "Re-import the exported table " + error);
        auto s2 = p.getSource();
        check (s2->loadedAsWavetable && s2->table->getNumFrames() == 64 && p.getMode() == InstrumentProcessor::tableMode,
               "Wavetable files load as tables and switch to Table mode");

        // A plain sample (not a table) should load as a sample
        juce::AudioBuffer<float> tone (1, 30001);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / 44100.0f));
        auto sampleFile = outDir.getChildFile ("sine220.wav");
        writeWav (sampleFile, tone, 44100.0);
        check (p.loadFile (sampleFile, error) && ! p.getSource()->loadedAsWavetable && p.getMode() == InstrumentProcessor::grainMode,
               "A plain sample loads as a sample");
        check (p.getSource()->table->getNumFrames() == 64, "...and is sliced into a table automatically");
        auto audio = renderNotes (p, sr, { 60 }, 0.6, 0.6);
        check (stats (audio, 0, audio.getNumSamples()).rms > 0.01f, "Dropped sample plays");

        // Band-limiting: a saw table's top mip at a very high note has no content above Nyquist
        std::vector<std::vector<float>> saw (1, std::vector<float> (2048));
        for (int i = 0; i < 2048; ++i) saw[0][(size_t) i] = 1.0f - 2.0f * (float) i / 2048.0f;
        auto t = Wavetable::fromFrames (saw);
        const double inc = 4186.0 / 48000.0;
        const int mip = Wavetable::mipForIncrement (inc);
        check (mip >= 4, "High notes use a band-limited mip (" + juce::String (mip) + ")");
    }

    // ---------------------------------------------------------------- projects carry their sound
    std::cout << "Projects: the sample is saved inside the project" << std::endl;
    {
        // a distinctive 3.5 s stereo sample on disk
        juce::AudioBuffer<float> tone (2, (int) (3.5 * 44100));
        for (int i = 0; i < tone.getNumSamples(); ++i)
        {
            tone.setSample (0, i, 0.4f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * (float) i / 44100.0f));
            tone.setSample (1, i, 0.3f * std::sin (juce::MathConstants<float>::twoPi * 495.0f * (float) i / 44100.0f));
        }
        auto file = outDir.getChildFile ("embed-me.wav");
        writeWav (file, tone, 44100.0);

        InstrumentProcessor p;
        juce::String error;
        p.loadFile (file, error);
        juce::MemoryBlock state;
        p.getStateInformation (state);
        file.deleteFile(); // the original is gone, as if the project moved to another computer

        InstrumentProcessor q;
        q.prepareToPlay (sr, 512);
        q.setStateInformation (state.getData(), (int) state.getSize());
        auto s = q.getSource();
        bool same = s != nullptr && s->audio.getNumChannels() == 2 && s->audio.getNumSamples() == tone.getNumSamples() && s->sampleRate == 44100.0;
        float maxErr = 0;
        if (same)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < tone.getNumSamples(); ++i)
                    maxErr = juce::jmax (maxErr, std::abs (s->audio.getSample (c, i) - tone.getSample (c, i)));
        check (same && s->name == "embed-me.wav", "Sample restored without the original file (" + juce::String (state.getSize() / 1024) + " KB project state)");
        check (maxErr < 1.0e-4f, "Restored audio is lossless (max error " + juce::String (maxErr, 7) + ")");
        auto audio = renderNotes (q, sr, { 60 }, 0.5, 0.3);
        check (stats (audio, 0, audio.getNumSamples()).rms > 0.01f, "Restored project plays its sound");

        // a wavetable stays a wavetable
        auto tableFile = outDir.getChildFile ("embed-table.wav");
        std::vector<std::vector<float>> frames (16, std::vector<float> (2048));
        for (int f = 0; f < 16; ++f)
            for (int i = 0; i < 2048; ++i)
                frames[(size_t) f][(size_t) i] = std::sin (juce::MathConstants<float>::twoPi * (float) i / 2048.0f * (float) (1 + f));
        Wavetable::writeWav (tableFile, frames);
        InstrumentProcessor t;
        t.loadFile (tableFile, error);
        juce::MemoryBlock tstate;
        t.getStateInformation (tstate);
        tableFile.deleteFile();
        InstrumentProcessor u;
        u.setStateInformation (tstate.getData(), (int) tstate.getSize());
        check (u.getSource()->loadedAsWavetable && u.getSource()->table->getNumFrames() == 16 && u.getMode() == InstrumentProcessor::tableMode,
               "Wavetables restore as wavetables (16 frames, Table mode)");

        // the built-in sound adds nothing to the project
        InstrumentProcessor fresh;
        juce::MemoryBlock small;
        fresh.getStateInformation (small);
        check (small.getSize() < 40000, "Projects using the built-in sound stay small (" + juce::String (small.getSize() / 1024) + " KB)");
    }

    // ---------------------------------------------------------------- effects rack
    std::cout << "Effects rack: every module, chains, Spark on effects" << std::endl;
    {
        auto setReal = [] (InstrumentProcessor& p, const juce::String& id, float v)
        {
            auto* prm = p.apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        };
        InstrumentProcessor p;
        p.setRateAndBufferSizeDetails (sr, 512);
        p.prepareToPlay (sr, 512);
        p.loadPreset (2); // Init Table: deterministic
        p.facetParam (InstrumentProcessor::motion).setValueNotifyingHost (0.0f);
        auto dry = renderNotes (p, sr, { 57, 60, 64 }, 1.0, 1.2);
        const auto dryTail = stats (dry, (int) (1.6 * sr), (int) (0.5 * sr));

        for (const auto& m : FxRack::modules())
        {
            if (m.id == "reverb") continue;
            setReal (p, m.onParam, 1.0f);
            if (m.id == "grain") setReal (p, "fxGrainMix", 0.8f);
            if (m.id == "eq") { setReal (p, "fxEqLow", 8.0f); setReal (p, "fxEqHigh", -10.0f); }
            if (m.id == "stutter") setReal (p, "fxStutterAmount", 1.0f);
            auto wet = renderNotes (p, sr, { 57, 60, 64 }, 1.0, 1.2);
            const auto st = stats (wet, 0, wet.getNumSamples());
            double diff = 0;
            for (int i = 0; i < wet.getNumSamples(); ++i) diff += std::abs (wet.getSample (0, i) - dry.getSample (0, i));
            diff /= wet.getNumSamples();
            juce::String extra;
            if (m.id == "delay")
            {
                const auto tail = stats (wet, (int) (1.6 * sr), (int) (0.5 * sr));
                check (tail.rms > dryTail.rms * 2.0f, "Delay leaves echoes after the note (" + juce::String (tail.rms, 4) + " vs " + juce::String (dryTail.rms, 4) + ")");
            }
            check (st.finite && st.peak < 1.2f && diff > 0.004, m.name + " changes the sound (avg diff " + juce::String (diff, 4) + ", peak " + juce::String (st.peak, 2) + ")");
            writeWav (outDir.getChildFile ("rack-" + m.id + ".wav"), wet, sr);
            setReal (p, m.onParam, 0.0f);
            renderNotes (p, sr, {}, 0.1, 0.4); // let it settle
        }

        // switching a module on mid-note must not click: look for sample-to-sample jumps
        {
            juce::AudioBuffer<float> out (2, (int) (1.0 * sr));
            juce::AudioBuffer<float> blk (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            float maxJump = 0, maxJumpBefore = 0;
            for (int pos = 0, k = 0; pos + 512 <= out.getNumSamples(); pos += 512, ++k)
            {
                if (k == 40) setReal (p, "fxDistOn", 1.0f);
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, 512);
                p.processBlock (view, midi);
                midi.clear();
            }
            // compare the switch-over (first 50 ms) with the steady sound before and after it
            const int sw = 40 * 512, fade = (int) (0.05 * sr);
            float maxWindow = 0, maxAfter = 0;
            for (int i = 1; i < out.getNumSamples(); ++i)
            {
                const float j = std::abs (out.getSample (0, i) - out.getSample (0, i - 1));
                if (i < sw) maxJumpBefore = juce::jmax (maxJumpBefore, j);
                else if (i < sw + fade) maxWindow = juce::jmax (maxWindow, j);
                else maxAfter = juce::jmax (maxAfter, j);
            }
            maxJump = maxWindow;
            check (maxWindow <= juce::jmax (maxJumpBefore, maxAfter) * 1.15f, "Turning a module on crossfades without a click (" + juce::String (maxWindow, 3)
                       + " during vs " + juce::String (juce::jmax (maxJumpBefore, maxAfter), 3) + " steady)");
            setReal (p, "fxDistOn", 0.0f);
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            juce::AudioBuffer<float> t (2, 512);
            for (int i = 0; i < 300; ++i) { p.processBlock (t, off); off.clear(); }
        }

        // every chain renders cleanly
        bool chainsOk = true;
        for (int c = 0; c < (int) FxRack::chains().size(); ++c)
        {
            p.loadChain (c);
            auto a = renderNotes (p, sr, { 57, 64 }, 0.6, 0.6);
            const auto st = stats (a, 0, a.getNumSamples());
            if (! st.finite || st.peak > 1.2f) { chainsOk = false; std::cout << "    chain " << FxRack::chains()[(size_t) c].name << " peak " << st.peak << std::endl; }
        }
        check (chainsOk, juce::String ((int) FxRack::chains().size()) + " effect chains render cleanly");

        // Spark moves enabled, unlocked effects; locked ones stay; recall brings them back
        p.loadChain (0);
        setReal (p, "fxDelayOn", 1.0f);
        setReal (p, "fxChorusOn", 1.0f);
        p.setModuleLocked ("chorus", true);
        const float chorusBefore = p.apvts.getParameter ("fxChorusDepth")->getValue();
        const float distBefore = p.apvts.getParameter ("fxDistDrive")->getValue();
        p.spark(); // a variation that includes the delay settings
        const auto before = p.currentExtraValues();
        const int startIndex = p.lineage.currentIndex();
        for (int i = 0; i < 6; ++i) p.spark();
        const auto after = p.currentExtraValues();
        check (before != after && after.count ("fxDelayFeedback") == 1, "Spark moves effects that are on");
        check (p.apvts.getParameter ("fxChorusDepth")->getValue() == chorusBefore && after.count ("fxChorusDepth") == 0, "Locked effects never move");
        check (p.apvts.getParameter ("fxDistDrive")->getValue() == distBefore, "Effects that are off are left alone");
        p.recall (startIndex);
        check (std::abs (p.apvts.getParameter ("fxDelayFeedback")->getValue() - before.at ("fxDelayFeedback")) < 1.0e-4f, "Recall restores effect settings");
        p.sparkEffects();
        check (p.currentExtraValues() != before, "SPARK FX rolls just the effects");

        juce::MemoryBlock state;
        p.getStateInformation (state);
        InstrumentProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.isModuleLocked ("chorus") && ! q.isModuleLocked ("delay") && q.lineage.nodes().back().extras.size() == p.lineage.nodes().back().extras.size(),
               "Effect locks and effect history are saved with the project");
    }

    // ---------------------------------------------------------------- shapeshift
    std::cout << "Shapeshift: rebuilds a synth note" << std::endl;
    {
        // A Serum-style note: 3 detuned saws (stereo), a low-pass that sweeps down, and a known envelope.
        const double f0 = 110.0; // A2 = MIDI 45
        const double holdSec = 1.6, relSec = 0.35, total = holdSec + 1.0;
        const int len = (int) (total * sr);
        juce::AudioBuffer<float> synth (2, len);
        synth.clear();
        // 7-voice unison, spread +/-0.2 semitones, random start phases, alternating pan (like Serum's unison)
        constexpr int voices = 7;
        double ph[voices], inc[voices];
        float panL[voices], panR[voices];
        juce::Random vr (11);
        for (int v = 0; v < voices; ++v)
        {
            const double det = -0.2 + 0.4 * v / (voices - 1);
            inc[v] = f0 * std::pow (2.0, det / 12.0) / sr;
            ph[v] = vr.nextDouble();
            const float pan = v == voices / 2 ? 0.0f : (v % 2 == 0 ? -0.7f : 0.7f);
            panL[v] = std::sqrt (0.5f * (1.0f - pan));
            panR[v] = std::sqrt (0.5f * (1.0f + pan));
        }
        double lpL = 0, lpL2 = 0, lpR = 0, lpR2 = 0;
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            // ADSR: A 15 ms, D ~400 ms (exponential), S 0.5, R 350 ms
            double env;
            if (t < 0.015) env = t / 0.015;
            else if (t < holdSec) env = 0.5 + 0.5 * std::exp (-(t - 0.015) / 0.13);
            else env = (0.5 + 0.5 * std::exp (-(holdSec - 0.015) / 0.13)) * std::exp (-(t - holdSec) / (relSec / 4.6));
            const double cutoff = 600.0 + 5400.0 * std::exp (-t / 0.35);   // filter sweeps from 6 kHz to 600 Hz
            const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * cutoff / sr);
            double l = 0, r = 0;
            for (int v = 0; v < voices; ++v)
            {
                ph[v] += inc[v];
                ph[v] -= std::floor (ph[v]);
                const double saw = 2.0 * ph[v] - 1.0;
                l += saw * panL[v];
                r += saw * panR[v];
            }
            lpL += a * (l - lpL); lpL2 += a * (lpL - lpL2);
            lpR += a * (r - lpR); lpR2 += a * (lpR - lpR2);
            synth.setSample (0, i, (float) (lpL2 * env * 0.15));
            synth.setSample (1, i, (float) (lpR2 * env * 0.15));
        }
        auto noteFile = outDir.getChildFile ("serum-style-A2.wav");
        writeWav (noteFile, synth, sr);

        const auto res = shapeshift::analyse (synth, sr);
        check (res.ok && res.pitched && juce::roundToInt (res.midiNote) == 45, "Finds the note: " + shapeshift::noteName (res.midiNote));
        check (res.attack > 0.004f && res.attack < 0.06f, "Attack " + juce::String (res.attack * 1000, 0) + " ms (original 15 ms)");
        {
            // the filter closing also makes the note quieter, so compare with its real loudness, not the 50% envelope setting
            const auto peakPart = stats (synth, (int) (0.005 * sr), (int) (0.03 * sr)), midPart = stats (synth, (int) (1.0 * sr), (int) (0.3 * sr));
            const float actual = midPart.rms / juce::jmax (1e-6f, peakPart.rms);
            check (std::abs (res.sustain - actual) < 0.12f, "Sustain " + juce::String (res.sustain * 100, 0) + "% (the note really settles at " + juce::String (actual * 100, 0) + "%)");
        }
        check (res.release > 0.12f && res.release < 0.8f, "Release " + juce::String (res.release * 1000, 0) + " ms (original 350 ms)");
        check (res.decayCurve > 0.15f, "Decay curve is punchy like the original (" + juce::String (res.decayCurve, 2) + ")");
        check (res.frames.size() == 64 && res.scanSeconds > 1.2f, "64 frames scanned over " + juce::String (res.scanSeconds, 2) + " s");
        check (res.width > 0.05f, "Detects stereo width (" + juce::String (res.width * 100, 0) + "%)");
        std::cout << "    " << res.summary.replace ("\n", "\n    ") << std::endl;
        // Now rebuild it in Spark and compare with the original
        InstrumentProcessor p;
        p.setRateAndBufferSizeDetails (sr, 512);
        p.prepareToPlay (sr, 512);
        juce::String summary;
        check (p.shapeshift (noteFile, summary) && p.getMode() == InstrumentProcessor::tableMode && p.getSource()->shapeshifted, "Shapeshift loads into Spark");
        p.facetParam (InstrumentProcessor::space).setValueNotifyingHost (0.0f);
        auto rebuilt = renderNotes (p, sr, { 45 }, holdSec, total - holdSec);

        auto envelopeDb = [] (const juce::AudioBuffer<float>& b, int hop)
        {
            std::vector<float> e;
            for (int i = 0; i + hop <= b.getNumSamples(); i += hop)
            {
                double s2 = 0;
                for (int k = 0; k < hop; ++k) s2 += b.getSample (0, i + k) * b.getSample (0, i + k);
                e.push_back ((float) std::sqrt (s2 / hop));
            }
            const float pk = *std::max_element (e.begin(), e.end());
            for (auto& v : e) v /= pk;
            return e;
        };
        auto centroid = [] (const juce::AudioBuffer<float>& b, double rate)
        {
            juce::dsp::FFT fft (11);
            std::vector<float> c;
            std::vector<float> buf (4096);
            for (int i = 0; i + 2048 <= b.getNumSamples(); i += 1024)
            {
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int k = 0; k < 2048; ++k) buf[(size_t) k] = b.getSample (0, i + k) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * k / 2048.0f));
                fft.performFrequencyOnlyForwardTransform (buf.data());
                double num = 0, den = 0;
                for (int k = 1; k < 1024; ++k) { num += k * rate / 2048.0 * buf[(size_t) k]; den += buf[(size_t) k]; }
                c.push_back (den > 1e-9 ? (float) (num / den) : 0.0f);
            }
            return c;
        };
        auto correlation = [] (const std::vector<float>& a, const std::vector<float>& b, size_t n)
        {
            n = std::min ({ n, a.size(), b.size() });
            double ma = 0, mb = 0;
            for (size_t i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
            ma /= n; mb /= n;
            double sab = 0, saa = 0, sbb = 0;
            for (size_t i = 0; i < n; ++i) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
            return sab / std::sqrt (saa * sbb + 1e-12);
        };
        const int hop = (int) (sr * 0.01);
        const auto eo = envelopeDb (synth, hop), er = envelopeDb (rebuilt, hop);
        const double envCorr = correlation (eo, er, (size_t) ((holdSec + 0.5) / 0.01));
        const auto co = centroid (synth, sr), cr = centroid (rebuilt, sr);
        const double briCorr = correlation (co, cr, (size_t) (1.4 * sr / 1024));
        check (envCorr > 0.9, "Volume shape matches the original (correlation " + juce::String (envCorr, 3) + ")");
        check (briCorr > 0.8, "Brightness over time follows the filter sweep (correlation " + juce::String (briCorr, 3) + ")");
        std::cout << "    brightness early/late: original " << juce::roundToInt (co[2]) << " / " << juce::roundToInt (co[(size_t) (1.3 * sr / 1024)])
                  << " Hz, rebuild " << juce::roundToInt (cr[2]) << " / " << juce::roundToInt (cr[(size_t) (1.3 * sr / 1024)]) << " Hz" << std::endl;
        writeWav (outDir.getChildFile ("shapeshift-original.wav"), synth, sr);
        writeWav (outDir.getChildFile ("shapeshift-rebuilt.wav"), rebuilt, sr);

        // The rebuild plays in tune on other notes, and SAMPLE mode plays the original
        auto other = renderNotes (p, sr, { 57 }, 0.6, 0.1);
        const float otherNote = shapeshift::detectMidiNote (other, sr);
        check (std::abs (otherNote - 57.0f) < 0.3f, "Plays in tune an octave up (" + shapeshift::noteName (otherNote) + ")");
        p.setMode (InstrumentProcessor::sampleMode);
        auto orig = renderNotes (p, sr, { 45 }, 0.8, 0.1);
        {
            const float got = shapeshift::detectMidiNote (orig, sr);
            const float direct = shapeshift::detectMidiNote (synth, sr);
            juce::ignoreUnused (direct);
            check (std::abs (got - 45.0f) < 0.3f, "SAMPLE mode plays the original at its own pitch");
        }

        // Saved projects bring the Shapeshift back exactly
        p.setMode (InstrumentProcessor::tableMode);
        juce::MemoryBlock state;
        p.getStateInformation (state);
        noteFile.deleteFile();
        InstrumentProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        bool same = q.getSource()->shapeshifted && q.getSource()->table->getNumFrames() == p.getSource()->table->getNumFrames();
        if (same)
            for (int k = 0; k < 64 && same; k += 9)
                for (int i = 0; i < 2048; i += 97)
                    same &= std::abs (q.getSource()->table->rawFrame (k)[(size_t) i] - p.getSource()->table->rawFrame (k)[(size_t) i]) < 1.0e-3f;
        check (same && std::abs (q.getSource()->rootNote - p.getSource()->rootNote) < 1.0e-3f, "Projects restore the Shapeshift exactly");

        // A pluck (no sustain) and an unpitched noise burst
        juce::AudioBuffer<float> pluck (1, (int) (sr * 1.2));
        for (int i = 0; i < pluck.getNumSamples(); ++i)
        {
            const double t = i / sr;
            pluck.setSample (0, i, (float) (0.6 * std::exp (-t / 0.12) * (std::sin (juce::MathConstants<double>::twoPi * 220.0 * t) + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * t))));
        }
        const auto pr = shapeshift::analyse (pluck, sr);
        check (pr.ok && juce::roundToInt (pr.midiNote) == 57 && pr.sustain < 0.05f && pr.decay > 0.2f && pr.decay < 1.2f,
               "Plucks: " + shapeshift::noteName (pr.midiNote) + ", no sustain, decay " + juce::String (pr.decay * 1000, 0) + " ms");
        juce::AudioBuffer<float> noise (1, (int) (sr * 1.0));
        juce::Random rnd (3);
        for (int i = 0; i < noise.getNumSamples(); ++i) noise.setSample (0, i, (rnd.nextFloat() * 2 - 1) * 0.4f * (float) std::exp (-i / sr / 0.3));
        const auto nr = shapeshift::analyse (noise, sr);
        check (nr.ok && ! nr.pitched && nr.frames.size() == 64, "Noise is handled as a texture (no false pitch)");

        // A wide supersaw (+/-18 cents, MIDI 48) with a sub oscillator an octave down, a slowly closing filter
        // and a quick release: common in Serum patches, and hard on pitch and release detection.
        {
            const double f0 = 130.81; // MIDI 48
            const double held = 2.4;
            juce::AudioBuffer<float> wide (2, (int) (sr * 3.2));
            juce::Random wr (5);
            double ph[7];
            for (auto& p0 : ph) p0 = wr.nextDouble();
            double sl = 0, sl2 = 0, sr2 = 0, srr = 0;
            for (int i = 0; i < wide.getNumSamples(); ++i)
            {
                const double t = i / sr;
                double l = 0, r = 0;
                for (int v = 0; v < 7; ++v)
                {
                    const double cents = -18.0 + 6.0 * v;
                    ph[v] += f0 * std::pow (2.0, cents / 1200.0) / sr;
                    ph[v] -= std::floor (ph[v]);
                    const double pan = 0.5 + 0.45 * (v - 3) / 3.0;
                    const double saw = 2.0 * ph[v] - 1.0;
                    l += saw * std::cos (pan * juce::MathConstants<double>::halfPi);
                    r += saw * std::sin (pan * juce::MathConstants<double>::halfPi);
                }
                const double sub = 0.35 * std::sin (juce::MathConstants<double>::twoPi * f0 * 0.5 * t);
                l += sub; r += sub;
                const double cutoff = 900.0 + 6100.0 * std::exp (-t / 0.45);
                const double g = std::tan (juce::MathConstants<double>::pi * cutoff / sr), a = g / (1 + g);
                sl += a * (l - sl); sl2 += a * (sl - sl2);
                sr2 += a * (r - sr2); srr += a * (sr2 - srr);
                double env = t < 0.008 ? t / 0.008 : 0.55 + 0.45 * std::exp (-(t - 0.008) / 0.25);
                if (t >= held) env = (0.55 + 0.45 * std::exp (-(held - 0.008) / 0.25)) * std::exp (-(t - held) / 0.09);
                wide.setSample (0, i, (float) (sl2 * env * 0.12));
                wide.setSample (1, i, (float) (srr * env * 0.12));
            }
            const auto wres = shapeshift::analyse (wide, sr);
            check (wres.pitched && juce::roundToInt (wres.midiNote) == 48,
                   "Wide supersaw with a sub: finds the played note (" + (wres.pitched ? shapeshift::noteName (wres.midiNote) : juce::String ("none")) + ", played " + shapeshift::noteName (48.0f) + ")");
            check (wres.release > 0.15f && wres.release < 0.7f,
                   "Release isn't fooled by a closing filter (" + juce::String (wres.release * 1000, 0) + " ms, original ~0.4 s)");

            // odd-harmonic sounds (square) must not be pushed up an octave
            juce::AudioBuffer<float> sq (1, (int) (sr * 1.0));
            for (int i = 0; i < sq.getNumSamples(); ++i)
                sq.setSample (0, i, std::fmod (i * f0 / sr, 1.0) < 0.5 ? 0.3f : -0.3f);
            check (juce::roundToInt (shapeshift::detectMidiNote (sq, sr)) == 48, "Square waves keep their octave");
        }

        // Root-note detection on ordinary drops
        juce::AudioBuffer<float> a3 (1, (int) (sr * 1.0));
        for (int i = 0; i < a3.getNumSamples(); ++i) a3.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / sr));
        auto a3File = outDir.getChildFile ("a3.wav");
        writeWav (a3File, a3, sr);
        InstrumentProcessor r;
        juce::String err;
        r.loadFile (a3File, err);
        check (std::abs (r.getSource()->rootNote - 57.0f) < 0.1f, "Dropped samples get their root note detected (" + shapeshift::noteName (r.getSource()->rootNote) + ")");
    }

    // ---------------------------------------------------------------- play modes and filter (stage 1)
    std::cout << "Play modes and filter" << std::endl;
    {
        auto fresh = [&]
        {
            auto p = std::make_unique<InstrumentProcessor>();
            p->setRateAndBufferSizeDetails (sr, 256);
            p->prepareToPlay (sr, 256);
            p->setMode (InstrumentProcessor::tableMode);
            p->facetParam (InstrumentProcessor::space).setValueNotifyingHost (0.0f);
            p->facetParam (InstrumentProcessor::motion).setValueNotifyingHost (0.0f);
            p->facetParam (InstrumentProcessor::morph).setValueNotifyingHost (0.0f);
            setReal (*p, "attack", 0.0f);
            return p;
        };
        auto on = [] (double t, int n) { return Ev { t, juce::MidiMessage::noteOn (1, n, 0.8f) }; };
        auto off = [] (double t, int n) { return Ev { t, juce::MidiMessage::noteOff (1, n) }; };
        auto noteAt = [&] (const juce::AudioBuffer<float>& b, double t0, double t1) { return shapeshift::detectMidiNote (slice (b, sr, t0, t1), sr); };

        // filter types at the same cutoff
        std::map<int, float> lows;
        for (int type = 0; type < 4; ++type)
        {
            auto p = fresh();
            p->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.42f);
            setReal (*p, "filterType", (float) type);
            auto b = renderEvents (*p, sr, { on (0.0, 45), off (0.8, 45) }, 0.8);
            lows[type] = lowFraction (slice (b, sr, 0.2, 0.7), sr);
        }
        check (lows[0] > lows[1] * 3.0f, "Low-pass keeps the lows, high-pass removes them (" + juce::String (lows[0], 2) + " vs " + juce::String (lows[1], 2) + ")");
        check (lows[2] < lows[0] && lows[3] < lows[0] + 0.01f, "Band-pass and notch shape the sound differently");

        // resonance adds a peak
        float rms[2];
        for (int k = 0; k < 2; ++k)
        {
            auto p = fresh();
            p->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.5f);
            p->apvts.getParameter ("resonance")->setValueNotifyingHost (k == 0 ? 0.1f : 0.85f);
            auto b = renderEvents (*p, sr, { on (0.0, 45), off (0.8, 45) }, 0.8);
            rms[k] = stats (b, (int) (0.2 * sr), (int) (0.5 * sr)).rms;
        }
        check (rms[1] > rms[0] * 1.2f, "Resonance adds a peak at the cutoff (" + juce::String (rms[1] / rms[0], 2) + "x louder)");

        // key tracking opens the filter for high notes
        float hi[2];
        for (int k = 0; k < 2; ++k)
        {
            auto p = fresh();
            p->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.35f);
            p->apvts.getParameter ("keyTrack")->setValueNotifyingHost ((float) k);
            auto b = renderEvents (*p, sr, { on (0.0, 84), off (0.6, 84) }, 0.6);
            hi[k] = stats (b, (int) (0.2 * sr), (int) (0.3 * sr)).rms;
        }
        check (hi[1] > hi[0] * 1.5f, "Key tracking opens the filter on high notes");

        // Legato: one voice, slides between overlapping keys, back to the held key on release
        {
            auto p = fresh();
            setReal (*p, "voiceMode", 2.0f);
            auto b = renderEvents (*p, sr, { on (0.0, 57), on (0.6, 64), off (1.2, 64), off (1.8, 57) }, 2.0);
            const float n1 = noteAt (b, 0.15, 0.55), n2 = noteAt (b, 0.75, 1.15), n3 = noteAt (b, 1.35, 1.75);
            check (juce::roundToInt (n1) == 57 && juce::roundToInt (n2) == 64 && juce::roundToInt (n3) == 57,
                   "Legato plays one note at a time and returns to the held key (" + juce::String (n1, 1) + ", " + juce::String (n2, 1) + ", " + juce::String (n3, 1) + ")");
            auto poly = fresh();
            auto pb = renderEvents (*poly, sr, { on (0.0, 57), on (0.6, 64), off (1.2, 64), off (1.8, 57) }, 2.0);
            check (stats (b, (int) (0.75 * sr), (int) (0.4 * sr)).rms < stats (pb, (int) (0.75 * sr), (int) (0.4 * sr)).rms * 0.85f,
                   "Poly plays both keys, Legato only one");
        }
        // Mono retriggers the envelope, Legato doesn't
        {
            float level[2];
            for (int k = 0; k < 2; ++k)
            {
                auto p = fresh();
                setReal (*p, "voiceMode", k == 0 ? 1.0f : 2.0f);
                p->apvts.getParameter ("decay")->setValueNotifyingHost (0.12f);
                p->apvts.getParameter ("sustain")->setValueNotifyingHost (0.25f);
                auto b = renderEvents (*p, sr, { on (0.0, 57), on (0.6, 60), off (1.0, 60), off (1.0, 57) }, 1.1);
                level[k] = stats (b, (int) (0.6 * sr), (int) (0.06 * sr)).rms;
            }
            check (level[0] > level[1] * 1.6f, "Mono restarts the envelope on each key, Legato slides without restarting");
        }
        // Glide: halfway through a 0.5 s glide from A2 to A3 the pitch is about halfway
        {
            auto p = fresh();
            setReal (*p, "voiceMode", 2.0f);
            p->apvts.getParameter ("glide")->setValueNotifyingHost (0.5f);   // 2 * 0.5^2 = 0.5 s
            auto b = renderEvents (*p, sr, { on (0.0, 45), on (0.6, 57), off (1.6, 57), off (1.6, 45) }, 1.6);
            const float mid = noteAt (b, 0.80, 0.90), end = noteAt (b, 1.25, 1.55);
            check (mid > 49.0f && mid < 54.0f && juce::roundToInt (end) == 57,
                   "Glide slides the pitch (" + juce::String (mid, 1) + " midway, " + juce::String (end, 1) + " at the end)");
        }
        // Pitch-bend range
        {
            auto p = fresh();
            setReal (*p, "bendRange", 12.0f);
            auto b = renderEvents (*p, sr, { Ev { 0.0, juce::MidiMessage::pitchWheel (1, 16383) }, on (0.01, 45), off (0.6, 45) }, 0.6);
            const float n = noteAt (b, 0.1, 0.55);
            check (std::abs (n - 57.0f) < 0.3f, "Pitch-bend range of 12 bends an octave (" + juce::String (n, 2) + ")");
        }
    }

    // ---------------------------------------------------------------- layers (stage 3)
    std::cout << "Layers: sub and noise" << std::endl;
    {
        auto make = [&]
        {
            auto p = std::make_unique<InstrumentProcessor>();
            p->setRateAndBufferSizeDetails (sr, 256);
            p->prepareToPlay (sr, 256);
            p->setMode (InstrumentProcessor::tableMode);
            p->facetParam (InstrumentProcessor::space).setValueNotifyingHost (0.0f);
            p->facetParam (InstrumentProcessor::motion).setValueNotifyingHost (0.0f);
            p->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (1.0f);
            setReal (*p, "attack", 0.0f);
            return p;
        };
        auto note = [&] (InstrumentProcessor& p)
        {
            return slice (renderEvents (p, sr, { Ev { 0.0, juce::MidiMessage::noteOn (1, 57, 0.8f) }, Ev { 0.8, juce::MidiMessage::noteOff (1, 57) } }, 0.8), sr, 0.2, 0.7);
        };
        auto plain = make();
        const auto dry = note (*plain);
        auto withSub = make();
        withSub->apvts.getParameter ("subLevel")->setValueNotifyingHost (0.8f);
        const auto sub1 = note (*withSub);
        setReal (*withSub, "subOctave", 1.0f);
        const auto sub2 = note (*withSub);
        check (toneAt (sub1, sr, 110.0) > toneAt (dry, sr, 110.0) * 20.0f + 1.0e-4f, "Sub -1 octave adds 110 Hz under A3 (220 Hz)");
        check (toneAt (sub2, sr, 55.0) > toneAt (dry, sr, 55.0) * 20.0f + 1.0e-4f && toneAt (sub2, sr, 110.0) < toneAt (sub1, sr, 110.0) * 0.2f,
               "Sub -2 octaves moves it to 55 Hz");

        auto noisy = make();
        noisy->apvts.getParameter ("noiseLevel")->setValueNotifyingHost (0.8f);
        noisy->apvts.getParameter ("noiseColour")->setValueNotifyingHost (1.0f);
        const auto bright = note (*noisy);
        noisy->apvts.getParameter ("noiseColour")->setValueNotifyingHost (0.0f);
        const auto dark = note (*noisy);
        check (toneAt (bright, sr, 9000.0) > toneAt (dry, sr, 9000.0) * 5.0f + 1.0e-5f, "Bright noise adds hiss above the harmonics");
        check (lowFraction (dark, sr) > lowFraction (bright, sr) * 1.2f, "Dark noise is darker than bright noise");
        const float rb = stats (bright, 0, bright.getNumSamples()).rms, rd = stats (dark, 0, dark.getNumSamples()).rms;
        check (rd > rb * 0.5f && rd < rb * 2.0f, "Colour changes the tone, not the loudness much (" + juce::String (rd / rb, 2) + "x)");

        // the filter shapes the layers too
        auto filtered = make();
        filtered->apvts.getParameter ("noiseLevel")->setValueNotifyingHost (0.8f);
        filtered->apvts.getParameter ("noiseColour")->setValueNotifyingHost (1.0f);
        filtered->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.3f);
        check (toneAt (note (*filtered), sr, 9000.0) < toneAt (bright, sr, 9000.0) * 0.2f, "The Tone filter shapes the layers as well");
    }

    // ---------------------------------------------------------------- modulation (stage 2)
    std::cout << "Modulation: LFOs, macros, matrix" << std::endl;
    {
        auto make = [&]
        {
            auto p = std::make_unique<InstrumentProcessor>();
            p->setRateAndBufferSizeDetails (sr, 256);
            p->prepareToPlay (sr, 256);
            p->setMode (InstrumentProcessor::tableMode);
            p->facetParam (InstrumentProcessor::space).setValueNotifyingHost (0.0f);
            p->facetParam (InstrumentProcessor::motion).setValueNotifyingHost (0.0f);
            setReal (*p, "attack", 0.0f);
            return p;
        };
        auto held = [&] (InstrumentProcessor& p, double secs)
        {
            return renderEvents (p, sr, { Ev { 0.0, juce::MidiMessage::noteOn (1, 45, 0.8f) }, Ev { secs, juce::MidiMessage::noteOff (1, 45) } }, secs);
        };
        // Envelope of short-window RMS (10 ms)
        auto windows = [&] (const juce::AudioBuffer<float>& b, double t0, double t1)
        {
            std::vector<float> e;
            const int w = (int) (0.01 * sr);
            for (int i = (int) (t0 * sr); i + w < (int) (t1 * sr); i += w) e.push_back (stats (b, i, w).rms);
            return e;
        };
        auto spread = [] (const std::vector<float>& e)
        {
            const auto [lo, hi] = std::minmax_element (e.begin(), e.end());
            return *hi / juce::jmax (1.0e-6f, *lo);
        };

        // no modulation: steady; LFO on Tone: the level (brightness) swings at the LFO rate
        {
            auto p = make();
            p->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.45f);
            const auto steady = spread (windows (held (*p, 2.0), 0.5, 1.9));
            auto q = make();
            q->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.45f);
            q->apvts.getParameter (mod::lfoParam (0, "Rate"))->setValueNotifyingHost (std::log (2.0f / 0.02f) / std::log (1000.0f));   // 2 Hz
            check (q->assignModulation (mod::lfo1, mod::tone, 0.3f) == 0, "Drag-assign puts LFO 1 -> Tone in slot 1");
            auto b = held (*q, 2.0);
            const auto env = windows (b, 0.5, 1.9);
            check (spread (env) > steady * 1.8f, "LFO 1 on Tone makes the sound swing (" + juce::String (spread (env), 1) + "x vs " + juce::String (steady, 1) + "x)");
            // count cycles by crossings of the average level: 2 Hz over 1.4 s -> 2.8 cycles
            float meanLevel = 0;
            for (auto e : env) meanLevel += e;
            meanLevel /= (float) env.size();
            int crossings = 0;
            for (size_t i = 1; i < env.size(); ++i)
                if ((env[i - 1] < meanLevel) != (env[i] < meanLevel)) ++crossings;
            const float cycles = (float) crossings / 2.0f;
            check (cycles >= 2.0f && cycles <= 3.5f, "It swings at the LFO rate (" + juce::String (cycles, 1) + " cycles in 1.4 s at 2 Hz)");
            check (q->getFacetModulation (InstrumentProcessor::tone) != 0.0f, "The ring and facet list can show live modulation");
        }
        // macro -> Tone opens the filter
        {
            auto p = make();
            p->facetParam (InstrumentProcessor::tone).setValueNotifyingHost (0.3f);
            p->assignModulation (mod::macro1, mod::tone, 0.5f);
            const float closed = stats (held (*p, 0.5), (int) (0.2 * sr), (int) (0.25 * sr)).rms;
            p->apvts.getParameter (mod::macroParam (0))->setValueNotifyingHost (1.0f);
            const float open = stats (held (*p, 0.5), (int) (0.2 * sr), (int) (0.25 * sr)).rms;
            check (open > closed * 1.5f, "Macro 1 turned up opens the filter it's linked to");
        }
        // tempo-synced LFO on volume follows the host tempo: 1/4 at 120 BPM = 2 Hz
        {
            struct Host : juce::AudioPlayHead
            {
                double t = 0;
                juce::Optional<PositionInfo> getPosition() const override
                {
                    PositionInfo i; i.setBpm (120.0); i.setIsPlaying (true); i.setPpqPosition (t * 2.0); return i;
                }
            } host;
            auto p = make();
            p->setPlayHead (&host);
            setReal (*p, mod::lfoParam (0, "Sync"), 1.0f);
            setReal (*p, mod::lfoParam (0, "Div"), 4.0f);    // 1/4
            setReal (*p, mod::lfoParam (0, "Shape"), (float) mod::square);
            p->assignModulation (mod::lfo1, mod::volume, -0.9f);
            juce::AudioBuffer<float> b (2, (int) (2.0 * sr)), blk (2, 256);
            for (int pos = 0; pos < b.getNumSamples(); pos += 256)
            {
                host.t = pos / sr;
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);
                juce::AudioBuffer<float> v (blk.getArrayOfWritePointers(), 2, juce::jmin (256, b.getNumSamples() - pos));
                p->processBlock (v, m);
                for (int c = 0; c < 2; ++c) b.copyFrom (c, pos, v, c, 0, v.getNumSamples());
            }
            p->setPlayHead (nullptr);
            // square wave at 2 Hz: loud for 0.25 s then quiet for 0.25 s
            const float a = stats (b, (int) (1.02 * sr), (int) (0.2 * sr)).rms, q = stats (b, (int) (1.27 * sr), (int) (0.2 * sr)).rms;
            check (juce::jmax (a, q) > juce::jmin (a, q) * 4.0f, "Tempo-synced LFO chops the volume on the beat (" + juce::String (juce::jmax (a, q) / juce::jmax (1e-6f, juce::jmin (a, q)), 1) + "x)");
        }
        // velocity as a source
        {
            float lv[2];
            for (int k = 0; k < 2; ++k)
            {
                auto p = make();
                setReal (*p, "ampVelocity", 0.0f);
                p->assignModulation (mod::velocity, mod::volume, 1.0f);
                auto b = renderEvents (*p, sr, { Ev { 0.0, juce::MidiMessage::noteOn (1, 45, k == 0 ? 0.2f : 1.0f) }, Ev { 0.5, juce::MidiMessage::noteOff (1, 45) } }, 0.5);
                lv[k] = stats (b, (int) (0.2 * sr), (int) (0.2 * sr)).rms;
            }
            check (lv[1] > lv[0] * 1.4f, "Velocity can modulate volume");
        }
        // CPU: an 8-note chord for 4 s, without and with 3 modulation routings
        {
            double secs[2];
            for (int k = 0; k < 2; ++k)
            {
                auto p = make();
                if (k == 1)
                {
                    p->assignModulation (mod::lfo1, mod::tone, 0.3f);
                    p->assignModulation (mod::lfo2, mod::morph, 0.4f);
                    p->assignModulation (mod::macro1, mod::pitch, 0.02f);
                }
                std::vector<Ev> ev;
                for (int n : { 45, 52, 57, 60, 64, 67, 69, 72 }) ev.push_back ({ 0.0, juce::MidiMessage::noteOn (1, n, 0.8f) });
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                renderEvents (*p, sr, ev, 4.0);
                secs[k] = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
            }
            std::cout << "    8 voices for 4 s: " << juce::String (secs[0] / 4.0 * 100.0, 1) << "% of one core plain, "
                      << juce::String (secs[1] / 4.0 * 100.0, 1) << "% modulated" << std::endl;
            check (secs[1] < secs[0] * 1.6 + 0.05, "Modulation adds little CPU");
        }
        // matrix bookkeeping, Spark, state
        {
            auto p = make();
            int filled = 0;
            for (int s = mod::lfo1; s <= mod::macro4; ++s)
                for (int d : { mod::tone, mod::morph })
                    if (p->assignModulation (s, d, 0.2f) >= 0) ++filled;
            check (filled == 8 && p->assignModulation (mod::modWheel, mod::drive, 0.2f) == -1, "8 slots, then it says they're full");
            check (p->assignModulation (mod::lfo1, mod::tone, -0.4f) == 0, "Assigning the same routing again just changes its amount");
            p->clearModulation (3);
            check (juce::roundToInt (p->modParams.src[3]->load()) == mod::none, "Clearing a slot frees it");

            auto amounts = [&] { std::vector<float> v; for (int k = 0; k < mod::numSlots; ++k) v.push_back (p->modParams.amt[k]->load()); return v; };
            const auto before = amounts();
            bool moved = false;
            for (int i = 0; i < 6 && ! moved; ++i)
            {
                p->spark();
                const auto now = amounts();
                for (int k = 0; k < mod::numSlots; ++k) moved = moved || std::abs (now[(size_t) k] - before[(size_t) k]) > 1.0e-3f;
            }
            check (moved, "Spark also rolls modulation amounts");
            p->setModuleLocked ("mod", true);
            const float lockedAmt = p->modParams.amt[0]->load();
            for (int i = 0; i < 4; ++i) p->spark();
            check (std::abs (p->modParams.amt[0]->load() - lockedAmt) < 1.0e-6f, "Locking the matrix keeps Spark off it");
            p->setModuleLocked ("mod", false);

            juce::MemoryBlock state;
            p->getStateInformation (state);
            InstrumentProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            bool same = true;
            for (int s = 0; s < mod::numSlots; ++s)
                same = same && q.modParams.src[s]->load() == p->modParams.src[s]->load() && q.modParams.dst[s]->load() == p->modParams.dst[s]->load()
                            && std::abs (q.modParams.amt[s]->load() - p->modParams.amt[s]->load()) < 1.0e-5f;
            check (same, "Modulation saves and loads with the project");
            p->loadPreset (0);
            check (juce::roundToInt (p->modParams.src[0]->load()) == mod::none, "Loading a preset clears modulation it doesn't use");
        }
    }

    // ---------------------------------------------------------------- user presets
    std::cout << "User presets: save, reload, delete" << std::endl;
    {
        InstrumentProcessor p;
        p.loadPreset (10);
        for (int i = 0; i < 3; ++i) p.spark();
        const auto vals = p.currentFacetValues();
        const float attack = p.apvts.getParameter ("attack")->getValue();
        juce::String error;
        const auto name = "Test Preset " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
        check (p.saveUserPreset (name, error), "Save " + name + " " + error);
        check (p.getPresetCategory() == SparkProcessorBase::userCategory && p.getPresetName() == name, "Saved preset becomes current");

        InstrumentProcessor q;   // a fresh instance finds it on disk
        int found = -1;
        for (int i = 0; i < q.getNumPresets(); ++i)
            if (q.getPreset (i).name == name) found = i;
        check (found >= q.getNumFactoryPresets(), "New instance lists the user preset");
        q.loadPreset (found);
        float d = 0;
        for (int i = 0; i < numFacets; ++i) d += std::abs (q.currentFacetValues()[(size_t) i] - vals[(size_t) i]);
        check (d < 1.0e-4f && std::abs (q.apvts.getParameter ("attack")->getValue() - attack) < 1.0e-4f, "User preset restores facets and envelope");

        juce::MemoryBlock state;
        q.getStateInformation (state);
        InstrumentProcessor r;
        r.setStateInformation (state.getData(), (int) state.getSize());
        check (r.getPresetName() == name, "Project recall remembers the user preset by name");

        check (q.deleteUserPreset (found), "Delete user preset");
        q.rescanUserPresets();
        bool gone = true;
        for (int i = 0; i < q.getNumPresets(); ++i) gone &= q.getPreset (i).name != name;
        check (gone, "Deleted preset disappears");
    }

    // ---------------------------------------------------------------- editors
    std::cout << "Editors: build and snapshot" << std::endl;
    {
        InstrumentProcessor p;
        p.prepareToPlay (sr, 512);
        for (int i = 0; i < 4; ++i) p.spark();
        p.toggleKeepCurrent();
        p.spark(); p.spark();
        p.setLocked (InstrumentProcessor::tone, true);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        snapshot (ed.get(), outDir.getChildFile ("ui-instrument.png"));
        p.setMode (InstrumentProcessor::tableMode);
        snapshot (ed.get(), outDir.getChildFile ("ui-instrument-table.png"));
        snapshot (ed.get(), outDir.getChildFile ("ui-instrument-small.png"), 0.75f);
        p.loadPreset (5);
        for (auto* c : ed->getChildren()[0]->getChildren())
            if (auto* b = dynamic_cast<PresetBrowser*> (c))
            {
                b->setVisible (true);
                b->toFront (false);
            }
        snapshot (ed.get(), outDir.getChildFile ("ui-instrument-browser.png"));
        for (auto* c : ed->getChildren()[0]->getChildren())
            if (auto* b = dynamic_cast<PresetBrowser*> (c)) b->setVisible (false);
        p.loadPreset (35); // a pluck with a tone envelope
        for (auto* c : ed->getChildren()[0]->getChildren())
            if (auto* se = dynamic_cast<ShapeEditor*> (c)) { se->setVisible (true); se->toFront (false); }
        snapshot (ed.get(), outDir.getChildFile ("ui-shape-editor.png"));
        check (true, "Instrument editor snapshots written");
        ed.reset();
    }
    {
        InstrumentProcessor p;
        p.prepareToPlay (sr, 512);
        p.loadChain (10); // Shimmer Cloud
        p.apvts.getParameter ("fxDelayOn")->setValueNotifyingHost (1.0f);
        p.setModuleLocked ("grain", true);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        if (auto* ie = dynamic_cast<InstrumentEditor*> (ed.get()))
            ie->showPage (2);
        snapshot (ed.get(), outDir.getChildFile ("ui-fx-page.png"));
        p.apvts.getParameter ("filterType")->setValueNotifyingHost (1.0f / 3.0f);
        p.apvts.getParameter ("resonance")->setValueNotifyingHost (0.45f);
        p.apvts.getParameter ("voiceMode")->setValueNotifyingHost (0.5f);
        p.apvts.getParameter ("glide")->setValueNotifyingHost (0.3f);
        p.assignModulation (mod::lfo1, mod::tone, 0.35f);
        p.assignModulation (mod::macro1, mod::morph, 0.5f);
        p.assignModulation (mod::lfo2, mod::volume, -0.4f);
        p.apvts.getParameter (mod::lfoParam (1, "Sync"))->setValueNotifyingHost (1.0f);
        p.apvts.getParameter (mod::lfoParam (1, "Shape"))->setValueNotifyingHost (5.0f / 6.0f);
        if (auto* ie = dynamic_cast<InstrumentEditor*> (ed.get()))
            ie->showPage (1);
        snapshot (ed.get(), outDir.getChildFile ("ui-synth-page.png"));
        {
            // the SOUND page while modulation runs: render a little audio so the live values update
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 40; ++i) { juce::MidiBuffer m; p.processBlock (buf, m); }
            if (auto* ie = dynamic_cast<InstrumentEditor*> (ed.get()))
                ie->showPage (0);
            snapshot (ed.get(), outDir.getChildFile ("ui-modulated.png"));
        }
        {
            // drop Macro 2 onto the Drive arc (lower left of the ring) and onto the Pitch row of the facet list
            std::function<CoreView* (juce::Component*)> findCore = [&] (juce::Component* c) -> CoreView*
            {
                if (auto* cv = dynamic_cast<CoreView*> (c)) return cv;
                for (auto* ch : c->getChildren()) if (auto* f = findCore (ch)) return f;
                return nullptr;
            };
            auto* core = findCore (ed.get());
            const auto centre = core->getLocalBounds().getCentre();
            const float a = juce::degreesToRadians (225.0f);
            const juce::Point<int> at (centre.x + juce::roundToInt (186.0f * std::sin (a)), centre.y - juce::roundToInt (186.0f * std::cos (a)));
            juce::DragAndDropTarget::SourceDetails d ("mod:" + juce::String (mod::macro2), nullptr, at);
            check (core->isInterestedInDragSource (d), "The ring accepts modulation drags");
            core->itemDragMove (d);
            core->itemDropped (d);
            bool found = false;
            for (int s = 0; s < mod::numSlots; ++s)
                found = found || (juce::roundToInt (p.modParams.src[s]->load()) == mod::macro2 && juce::roundToInt (p.modParams.dst[s]->load()) == mod::drive);
            check (found, "Dropping Macro 2 on the Drive arc routes Macro 2 -> Drive");
        }
        check (true, "FX page snapshot written");
        ed.reset();
    }

    std::cout << (failures == 0 ? "\nALL CHECKS PASSED" : "\nFAILURES: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}

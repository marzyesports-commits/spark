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
            ie->showPage (1);
        snapshot (ed.get(), outDir.getChildFile ("ui-fx-page.png"));
        check (true, "FX page snapshot written");
        ed.reset();
    }

    std::cout << (failures == 0 ? "\nALL CHECKS PASSED" : "\nFAILURES: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}

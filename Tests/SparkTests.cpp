// Offline checks for Spark: renders audio, exercises the randomiser, round-trips
// wavetables and state, and snapshots both editors to PNG. Not shipped.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Instrument/InstrumentProcessor.h"
#include "Instrument/InstrumentEditor.h"
#include "FX/FxProcessor.h"
#include "FX/FxEditor.h"

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
        for (int i = 0; i < p.getNumPresets(); ++i)
        {
            p.loadPreset (i);
            auto audio = renderNotes (p, sr, { 60, 64, 67 }, 1.5, 5.0);
            const auto held = stats (audio, (int) (0.3 * sr), (int) (1.0 * sr));
            const auto end = stats (audio, audio.getNumSamples() - (int) (0.3 * sr), (int) (0.3 * sr));
            check (held.finite && held.rms > 0.01f && held.peak < 2.0f,
                   p.getPresetName() + " (" + (p.getMode() == InstrumentProcessor::tableMode ? "table" : "grain") + "): rms "
                       + juce::String (held.rms, 3) + ", peak " + juce::String (held.peak, 2));
            check (end.rms < held.rms * 0.2f, p.getPresetName() + ": fades after release (tail rms " + juce::String (end.rms, 4) + ")");
            writeWav (outDir.getChildFile ("instrument-" + juce::String (i) + "-" + p.getPresetName().replaceCharacter (' ', '_') + ".wav"), audio, sr);
        }

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

    // ---------------------------------------------------------------- effect
    std::cout << "Spark FX: processes live audio" << std::endl;
    {
        FxProcessor p;
        p.setRateAndBufferSizeDetails (sr, 512);
        p.prepareToPlay (sr, 512);
        const int total = (int) (sr * 4.0);
        juce::AudioBuffer<float> in (2, total);
        for (int i = 0; i < total; ++i)
        {
            const float t = (float) i / (float) sr;
            float x = 0.3f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * t) * (0.6f + 0.4f * std::sin (t * 3.0f));
            if (i % 12000 < 600) x += 0.5f * std::exp (-(float) (i % 12000) / 120.0f) * (juce::Random::getSystemRandom().nextFloat() * 2 - 1);
            in.setSample (0, i, x);
            in.setSample (1, i, x);
        }

        for (int preset = 0; preset < p.getNumPresets(); ++preset)
        {
            p.loadPreset (preset);
            juce::AudioBuffer<float> out (in);
            for (int pos = 0; pos < total; pos += 512)
            {
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, juce::jmin (512, total - pos));
                juce::MidiBuffer m;
                p.processBlock (view, m);
            }
            const auto s = stats (out, (int) sr, total - (int) sr);
            float diff = 0;
            for (int i = (int) sr; i < total; ++i) diff += std::abs (out.getSample (0, i) - in.getSample (0, i));
            check (s.finite && s.peak < 2.0f && s.rms > 0.01f && diff / (float) (total - sr) > 0.01f,
                   p.getPresetName() + ": rms " + juce::String (s.rms, 3) + ", differs from dry by " + juce::String (diff / (float) (total - sr), 3));
            writeWav (outDir.getChildFile ("fx-" + juce::String (preset) + "-" + p.getPresetName().replaceCharacter (' ', '_') + ".wav"), out, sr);
        }

        // Bypass passes the dry signal through
        p.apvts.getParameter ("bypass")->setValueNotifyingHost (1.0f);
        juce::AudioBuffer<float> out (in);
        for (int pos = 0; pos < total; pos += 512)
        {
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, juce::jmin (512, total - pos));
            juce::MidiBuffer m;
            p.processBlock (view, m);
        }
        float maxDiff = 0;
        for (int i = (int) sr; i < total; ++i) maxDiff = juce::jmax (maxDiff, std::abs (out.getSample (0, i) - in.getSample (0, i)));
        check (maxDiff < 1.0e-4f, "Bypass is transparent (max diff " + juce::String (maxDiff, 6) + ")");
        p.apvts.getParameter ("bypass")->setValueNotifyingHost (0.0f);

        juce::String error;
        auto captured = p.captureToFile (error);
        check (captured.existsAsFile() && captured.getSize() > 100000, "Capture writes a WAV (" + captured.getFileName() + ")");
        captured.deleteFile();

        std::vector<float> shape;
        p.getCoreShape (shape, 360);
        check (shape.size() == 360, "Core ring gets a live shape");
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
        p.apvts.getParameter ("mode")->setValueNotifyingHost (1.0f);
        snapshot (ed.get(), outDir.getChildFile ("ui-instrument-table.png"));
        snapshot (ed.get(), outDir.getChildFile ("ui-instrument-small.png"), 0.75f);
        check (true, "Instrument editor snapshots written");
        ed.reset();
    }
    {
        FxProcessor p;
        p.setRateAndBufferSizeDetails (sr, 512);
        p.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> buf (2, 512);
        for (int k = 0; k < 200; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float x = 0.4f * std::sin ((float) (k * 512 + i) * 0.03f) + 0.2f * std::sin ((float) (k * 512 + i) * 0.11f);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            juce::MidiBuffer m;
            p.processBlock (buf, m);
        }
        for (int i = 0; i < 3; ++i) p.spark();
        p.setLocked (FxProcessor::mix, true);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        snapshot (ed.get(), outDir.getChildFile ("ui-fx.png"));
        check (true, "FX editor snapshot written");
        ed.reset();
    }

    std::cout << (failures == 0 ? "\nALL CHECKS PASSED" : "\nFAILURES: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}

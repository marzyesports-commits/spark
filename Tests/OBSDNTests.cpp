// Offline checks for OBSDN: the riff writer, the riff player, rendering every
// preset, state, and snapshots of every page. Not shipped.
#include <juce_audio_utils/juce_audio_utils.h>
#include "Lead/LeadProcessor.h"
#include "Lead/LeadEditor.h"
#include "Instrument/FactorySounds.h"

using namespace spark;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

struct HostPlayHead : public juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        return p;
    }
};

struct Ev { double t; juce::MidiMessage msg; };

// Renders timed MIDI (seconds). Collects the MIDI the synth actually received (after the riff writer) in 'played'.
juce::AudioBuffer<float> render (LeadProcessor& p, double sr, std::vector<Ev> events, double total,
                                 HostPlayHead* host = nullptr, std::vector<Ev>* played = nullptr)
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
        if (host != nullptr)
            host->ppq = (double) pos / sr * host->bpm / 60.0 + (host->playing ? 0.0 : 0.0);
        p.processBlock (view, midi);
        if (played != nullptr)
            for (const auto m : midi)
                if (m.getMessage().isNoteOnOrOff())
                    played->push_back ({ (pos + m.samplePosition) / sr, m.getMessage() });
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, view, c, 0, n);
    }
    return out;
}

float peakOf (const juce::AudioBuffer<float>& b)
{
    return juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
}

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

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

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (f.createOutputStream());
    auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (b.getNumChannels()).withBitsPerSample (24));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

void setReal (LeadProcessor& p, const juce::String& id, float value)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void pump (int ms)
{
    for (int i = 0; i < ms / 20; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

void snapshot (juce::AudioProcessorEditor* ed, const juce::File& f)
{
    ed->setSize (1120, 720);
    pump (400);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}

int countOns (const std::vector<Ev>& evs) { int n = 0; for (auto& e : evs) if (e.msg.isNoteOn()) ++n; return n; }
int countOffs (const std::vector<Ev>& evs) { int n = 0; for (auto& e : evs) if (e.msg.isNoteOff()) ++n; return n; }
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto outDir = juce::File (argc > 1 ? argv[1] : "/tmp/sparklead-tests");
    outDir.createDirectory();
    const double sr = 48000.0;

    // Gallery: every page at 2x, in a state that shows what it does
    if (argc > 2 && juce::String (argv[2]) == "gallery")
    {
        LeadProcessor p;
        p.setRateAndBufferSizeDetails (sr, 256);
        p.prepareToPlay (sr, 256);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* le = dynamic_cast<LeadEditor*> (ed.get());
        ed->setSize (1120, 720);
        auto shoot = [&] (const juce::String& name, juce::Rectangle<int> area = {})
        {
            pump (300);
            if (area.isEmpty()) area = ed->getLocalBounds();
            auto img = ed->createComponentSnapshot (area, true, 2.0f);
            auto f = outDir.getChildFile (name + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        juce::AudioBuffer<float> buf (2, 256);
        auto run = [&] (int blocks, bool note)
        {
            for (int i = 0; i < blocks; ++i)
            {
                juce::MidiBuffer m;
                if (note && i == 0) m.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0);
                p.processBlock (buf, m);
            }
        };
        // a lived-in state: a few variations in the lineage, routings, a riff playing
        for (int k = 0; k < 5; ++k) p.spark();
        for (int i = 0; i < p.getNumFactoryPresets(); ++i) if (p.getPreset (i).name == "Jade Supersaw") p.loadPreset (i);
        for (int k = 0; k < 3; ++k) p.spark();
        p.assignModulation (mod::lfo1, mod::tone, 0.3f);
        p.assignModulation (mod::lfo2, mod::leadWave, 0.25f);
        p.assignModulation (mod::macro1, mod::drive, 0.4f);
        p.assignModulation (mod::modWheel, mod::leadVibrato, 0.5f);
        setReal (p, mod::lfoParam (1, "Shape"), (float) mod::triangle);
        setReal (p, mod::lfoParam (0, "Sync"), 1.0f);
        setReal (p, mod::lfoParam (0, "Div"), 5.0f);
        setReal (p, mod::macroParam (0), 0.55f);
        setReal (p, mod::macroParam (1), 0.3f);
        setReal (p, "riffOn", 1.0f);
        p.riffPreview = true;
        run (120, false);

        le->showPage (0);                               shoot ("01-lead");
        le->showPage (1);                               shoot ("02-synth");
        le->showPage (2);                               shoot ("03-mod");
        le->showPage (LeadEditor::riffPage_); run (60, false); shoot ("04-riff");
        {
            std::function<juce::Button* (juce::Component*)> findMore = [&] (juce::Component* c) -> juce::Button*
            {
                if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText() == "MORE") return b;
                for (auto* child : c->getChildren()) if (auto* f = findMore (child)) return f;
                return nullptr;
            };
            if (auto* more = findMore (ed.get()))
            {
                more->triggerClick(); pump (100); shoot ("04b-riff-more");
                more->triggerClick(); pump (100);
            }
            setReal (p, "riffKey", 2.0f); setReal (p, "riffScale", 6.0f);   // D minor pentatonic: fewer lit rows
            run (20, false); shoot ("04c-riff-pentatonic");
            p.riffFoldToScale = true;   // FOLD: only the scale's notes
            run (20, false); shoot ("04d-riff-folded");
            p.riffFoldToScale = false;
            setReal (p, "riffKey", 9.0f); setReal (p, "riffScale", 1.0f);
        }
        le->showPage (LeadEditor::fxPage_);
        setReal (p, "fxChorusOn", 1.0f); setReal (p, "fxDelayOn", 1.0f);
        shoot ("05-fx");
        // sound design: oscillator A playing a sound, three ways
        le->showPage (0);
        p.loadFactorySound ("vox_ah"); p.setOscMode (LeadProcessor::grain); shoot ("06-sound-grain");
        p.loadFactorySound ("wt_formant"); p.setOscMode (LeadProcessor::table); shoot ("07-sound-table");
        p.loadFactorySound ("key_kalimba"); p.setOscMode (LeadProcessor::sample); shoot ("08-sound-sample");
        p.clearSource();
        // the preset browser
        for (int i = 0; i < p.getNumFactoryPresets(); ++i) if (p.getPreset (i).name == "Vox Lead") p.loadPreset (i);
        if (auto* browser = [&] { std::function<PresetBrowser* (juce::Component*)> f = [&] (juce::Component* c) -> PresetBrowser*
                                  { if (auto* b = dynamic_cast<PresetBrowser*> (c)) return b; for (auto* ch : c->getChildren()) if (auto* r = f (ch)) return r; return nullptr; };
                                  return f (ed.get()); }())
        {
            browser->setVisible (true); browser->toFront (false); browser->refresh (true);
            shoot ("09-presets");
            browser->setVisible (false);
        }
        // strike: the ring throwing lightning
        for (int i = 0; i < p.getNumFactoryPresets(); ++i) if (p.getPreset (i).name == "Jade Supersaw") p.loadPreset (i);
        std::function<juce::Button* (juce::Component*)> findStrike = [&] (juce::Component* c) -> juce::Button*
        {
            if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getName() == "Spark") return b;
            for (auto* child : c->getChildren()) if (auto* found = findStrike (child)) return found;
            return nullptr;
        };
        pump (600);
        if (auto* strike = findStrike (ed.get()))
        {
            strike->triggerClick();
            for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (15);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            auto f = outDir.getChildFile ("10-strike.png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
        p.riffPreview = false;
        return 0;
    }

    // App icon: the gem on black, from the same drawing code as the plugin
    if (argc > 2 && juce::String (argv[2]) == "icon")
    {
        juce::Image img (juce::Image::ARGB, 1024, 1024, true);
        {
            juce::Graphics g (img);
            g.setColour (colours::bg);
            g.fillRoundedRectangle (0.0f, 0.0f, 1024.0f, 1024.0f, 220.0f);
            g.setColour (colours::line2);
            g.drawRoundedRectangle (6.0f, 6.0f, 1012.0f, 1012.0f, 214.0f, 6.0f);
            g.addTransform (juce::AffineTransform::scale (5.6f, 5.6f, 512.0f, 512.0f));
            drawGem (g, juce::Rectangle<float> (120.0f, 120.0f).withCentre ({ 512.0f, 512.0f }), 0.45f);
        }
        auto f = outDir.getChildFile ("Icon.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        return 0;
    }

    // Demo: the riff writer following a chord progression (A minor: Am F C G) in several styles, at 124 bpm
    if (argc > 2 && juce::String (argv[2]) == "demo")
    {
        struct Section { const char* preset; int style; juce::uint32 seed; float density; };
        const Section sections[] { { "Jade Supersaw", riff::trance, 11, 0.45f }, { "Future Glide", riff::future, 5, 0.55f },
                                   { "Drill Slide", riff::drill, 21, 0.45f }, { "Afro Flute", riff::afro, 8, 0.55f },
                                   { "8-bit Hero", riff::chip, 3, 0.5f } };
        const double bpm = 124.0, bar = 240.0 / bpm;
        const int roots[] { 57, 53, 60, 55 };   // A3 F3 C4 G3
        int index = 0;
        for (const auto& sec : sections)
        {
            LeadProcessor p;
            for (int i = 0; i < p.getNumFactoryPresets(); ++i)
                if (p.getPreset (i).name == sec.preset) p.loadPreset (i);
            setReal (p, "riffOn", 1.0f);
            setReal (p, "riffKey", 9.0f);
            setReal (p, "riffScale", 1.0f);
            setReal (p, "riffStyle", (float) sec.style);
            setReal (p, "riffBars", 0.0f);
            setReal (p, "riffDensity", sec.density);
            pump (300);
            riff::Settings st = p.riffSettings();
            p.setRiff (riff::generate (st, sec.seed));
            HostPlayHead host;
            host.bpm = bpm;
            host.playing = true;
            p.setPlayHead (&host);
            p.prepareToPlay (sr, 256);
            std::vector<Ev> ev;
            for (int b = 0; b < 8; ++b)
            {
                const int k = roots[b % 4];
                ev.push_back ({ juce::jmax (0.0, b * bar - 0.02), juce::MidiMessage::noteOn (1, k, 0.9f) });
                ev.push_back ({ (b + 1) * bar + (b == 7 ? -0.03 : 0.02), juce::MidiMessage::noteOff (1, k) });
            }
            auto out = render (p, sr, ev, 8 * bar + 2.0, &host);
            writeWav (outDir.getChildFile ("demo-" + juce::String (index++) + ".wav"), out, sr);
            std::cout << sec.preset << ": " << p.getRiff().notes.size() << " notes" << std::endl;
        }
        return 0;
    }

    // ------------------------------------------------------------------ riff writer
    std::cout << "Riff: writing riffs" << std::endl;
    {
        bool shapesOk = true, endsOk = true, deterministic = true, spansOk = true;
        int chordHits = 0, strongNotes = 0, distinct = 0, total = 0;
        for (int style = 0; style < riff::numStyles; ++style)
            for (int bars : { 1, 2, 4 })
                for (int scale : { 0, 1, 6 })
                    for (juce::uint32 seed : { 1u, 77u, 4242u, 99991u })
                    {
                        riff::Settings s;
                        s.style = style; s.bars = bars; s.scale = scale; s.density = 0.5f; s.range = 0.5f;
                        const auto r = riff::generate (s, seed);
                        const auto again = riff::generate (s, seed);
                        const auto other = riff::generate (s, seed + 1);
                        ++total;
                        if (! (r == again)) deterministic = false;
                        if (! (r == other)) ++distinct;
                        const int size = (int) riff::scaleSteps (scale).size();
                        if (r.notes.size() < 3 || r.bars != bars) shapesOk = false;
                        int last = -1;
                        for (const auto& n : r.notes)
                        {
                            if (n.start <= last || n.start >= r.lengthTicks() || n.degree < -4 || n.degree > size * 3) shapesOk = false;
                            if (n.span < 1) spansOk = false;
                            last = n.start;
                            if (n.start % riff::ticksPerBeat == 0)
                            {
                                ++strongNotes;
                                const int st = riff::scaleSteps (scale)[(size_t) (((n.degree % size) + size) % size)];
                                if (st == 0 || st == 3 || st == 4 || st == 7) ++chordHits;
                            }
                        }
                        if (! r.notes.empty() && ((r.notes.back().degree % size) + size) % size != 0) endsOk = false;
                        if (! r.notes.empty() && r.notes.back().slide) spansOk = false;
                    }
        check (shapesOk, "every style, length and scale writes a riff that fits its length and range");
        check (deterministic, "the same seed always writes the same riff");
        check (distinct > total * 9 / 10, "different seeds write different riffs (" + juce::String (distinct) + "/" + juce::String (total) + ")");
        check (endsOk, "every riff comes home to the root on its last note");
        check (spansOk, "note lengths are positive and the last note never slides");
        const float ratio = (float) chordHits / (float) juce::jmax (1, strongNotes);
        check (ratio > 0.6f, "notes on the beat are mostly chord tones (" + juce::String (juce::roundToInt (ratio * 100)) + "%)");

        riff::Settings s;
        s.style = riff::future; s.bars = 2;
        const auto base = riff::generate (s, 123);
        const auto m = riff::mutate (base, s, 5);
        bool sameRhythm = m.notes.size() == base.notes.size();
        int changed = 0;
        for (size_t i = 0; sameRhythm && i < base.notes.size(); ++i)
        {
            sameRhythm = m.notes[i].start == base.notes[i].start;
            if (m.notes[i].degree != base.notes[i].degree) ++changed;
        }
        check (sameRhythm && changed > 0, "Mutate keeps the rhythm and changes some notes (" + juce::String (changed) + " changed)");
        const auto nr = riff::newRhythm (base, s, 9);
        check (nr.bars == base.bars && ! nr.notes.empty() && nr.notes.back().degree == base.notes.back().degree && ! (nr == base),
               "Rhythm keeps the tune's ending and writes a new rhythm");
        const auto ans = riff::answer (base, s, 3);
        bool echoes = true;
        const int half = ans.lengthTicks() / 2;
        std::vector<int> first, second;
        for (auto& n : ans.notes) (n.start < half ? first : second).push_back (n.start % half);
        echoes = first == second && ((ans.notes.back().degree % 7) + 7) % 7 == 0;
        check (echoes, "Answer repeats the call's rhythm in the second half and resolves home");

        const auto text = base.toString();
        check (riff::Riff::fromString (text) == base, "a riff survives saving as text");
        check (riff::Riff::fromString ("garbage").notes.empty(), "bad riff text loads as an empty riff, not a crash");

        const auto file = riff::toMidiFile (base, 9, 1, 0, 0.8f, 0.0f);
        int ons = 0; double lastTime = 0;
        for (auto* e : *file.getTrack (0))
        {
            if (e->message.isNoteOn()) ++ons;
            lastTime = juce::jmax (lastTime, e->message.getTimeStamp());
        }
        check (ons == (int) base.notes.size() && lastTime <= base.lengthTicks() * 4.0 + 0.1, "the MIDI export has every note and ends at the loop length");
        const auto written = riff::writeMidiFile (base, 9, 1, 0, 0.8f, 0.2f, "OBSDN riff test");
        check (written.existsAsFile() && written.getSize() > 30, "the MIDI file is written for dragging");

        const auto& minor = riff::scaleSteps (1);
        check (riff::noteToDegree (60, 57, minor) == 2 && riff::degreeToNote (2, 57, minor) == 60 && riff::degreeToNote (-1, 57, minor) == 55,
               "scale degrees and notes convert both ways (C is the 3rd of A minor)");
    }

    // ------------------------------------------------------------------ riff player
    std::cout << "Riff: playing riffs" << std::endl;
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        setReal (p, "riffOn", 1.0f);
        setReal (p, "riffKey", 9.0f);       // A minor
        setReal (p, "riffScale", 1.0f);
        const auto r = p.getRiff();
        const int root = riff::rootNote (9, 0);
        check (root == 57, "the riff's root for A is A3");

        // host stopped: holding the root plays the riff from the start at 120 bpm
        std::vector<Ev> played;
        render (p, sr, { { 0.1, juce::MidiMessage::noteOn (1, root, 0.9f) }, { 0.1 + r.bars * 2.0 - 0.01, juce::MidiMessage::noteOff (1, root) } },
                r.bars * 2.0 + 1.0, nullptr, &played);
        check (countOns (played) == (int) r.notes.size(), "holding a key for one loop plays every note once (" + juce::String (countOns (played)) + "/" + juce::String ((int) r.notes.size()) + ")");
        check (countOns (played) == countOffs (played), "every riff note is let go");
        bool pitchesOk = ! played.empty(), timingOk = true;
        int k = 0;
        for (auto& e : played)
            if (e.msg.isNoteOn() && k < (int) r.notes.size())
            {
                const auto& n = r.notes[(size_t) k++];
                if (e.msg.getNoteNumber() != riff::degreeToNote (n.degree, root, riff::scaleSteps (1))) pitchesOk = false;
                const double expect = 0.1 + n.start / (double) riff::ticksPerBeat * 0.5;
                if (std::abs (e.t - expect) > 0.004) timingOk = false;
            }
        check (pitchesOk, "the riff plays the right notes in A minor");
        check (timingOk, "each note starts on time (within 4 ms) from the key press");

        // In key: holding C4 moves the riff two scale steps up
        p.prepareToPlay (sr, 256);
        played.clear();
        render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 60, 0.9f) }, { 0.9, juce::MidiMessage::noteOff (1, 60) } }, 1.2, nullptr, &played);
        const int firstOn = played.empty() ? -1 : played.front().msg.getNoteNumber();
        check (firstOn == riff::degreeToNote (r.notes.front().degree + 2, root, riff::scaleSteps (1)), "In key: holding C4 moves the riff along A minor");

        setReal (p, "riffFollow", 1.0f);   // chromatic
        p.prepareToPlay (sr, 256);
        played.clear();
        render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 59, 0.9f) }, { 0.9, juce::MidiMessage::noteOff (1, 59) } }, 1.2, nullptr, &played);
        check (! played.empty() && played.front().msg.getNoteNumber() == riff::degreeToNote (r.notes.front().degree, root, riff::scaleSteps (1)) + 2,
               "Chromatic: holding B3 transposes the riff up 2 semitones");
        setReal (p, "riffFollow", 0.0f);

        // Latch with the host playing: plays in time with no key held
        setReal (p, "riffLatch", 1.0f);
        HostPlayHead host;
        host.playing = true;
        p.setPlayHead (&host);
        p.prepareToPlay (sr, 256);
        played.clear();
        render (p, sr, {}, r.bars * 2.0, &host, &played);
        check (countOns (played) == (int) r.notes.size(), "Latch: the riff plays along with the DAW without a key");
        bool onGrid = true;
        for (auto& e : played)
            if (e.msg.isNoteOn())
            {
                const double tick = e.t * 2.0 * riff::ticksPerBeat;   // 120 bpm: 2 beats a second
                if (std::abs (tick - std::round (tick)) > 0.15) onGrid = false;
            }
        check (onGrid, "Latch notes land on the song's grid");
        host.playing = false;
        setReal (p, "riffLatch", 0.0f);
        p.setPlayHead (nullptr);

        // Riff off: keys play normally
        setReal (p, "riffOn", 0.0f);
        p.prepareToPlay (sr, 256);
        played.clear();
        render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 64, 0.9f) }, { 0.5, juce::MidiMessage::noteOff (1, 64) } }, 0.8, nullptr, &played);
        check (countOns (played) == 1 && played.front().msg.getNoteNumber() == 64, "with the riff writer off, a key plays just that note");

        // turning the riff off mid-phrase leaves nothing hanging (effects off so their tails don't count)
        setReal (p, "fxReverbOn", 0.0f);
        setReal (p, "fxDelayOn", 0.0f);
        setReal (p, "fxChorusOn", 0.0f);
        setReal (p, "riffOn", 1.0f);
        p.prepareToPlay (sr, 256);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.9f), 0);
        for (int i = 0; i < 100; ++i) { p.processBlock (buf, midi); midi.clear(); }
        setReal (p, "riffOn", 0.0f);
        for (int i = 0; i < 400; ++i) { p.processBlock (buf, midi); midi.clear(); }
        check (buf.getMagnitude (0, 0, 256) < 1.0e-4f, "switching the riff writer off mid-note lets the note go");
    }

    // ------------------------------------------------------------------ riff editing, history, state
    std::cout << "Riff: editing and saving" << std::endl;
    {
        LeadProcessor p;
        const auto start = p.getRiff();
        p.generateRiff();
        check (! (p.getRiff() == start) && p.getRiffHistory().size() == 2, "GENERATE writes a new riff and keeps the last one in Recent");
        p.recallRiff (0);
        check (p.getRiff() == start, "clicking a recent riff brings it back");
        const auto before = p.getRiff();
        const int t = 3 * 6;
        p.toggleRiffNote (t, 11);
        bool added = false;
        for (auto& n : p.getRiff().notes) if (n.start == t && n.degree == 11) added = true;
        p.toggleRiffNote (t, 11);
        bool removed = true;
        for (auto& n : p.getRiff().notes) if (n.start == t && n.degree == 11) removed = false;
        check (added && removed, "clicking the roll adds a note and clicking it again removes it");
        (void) before;

        // saving and loading keeps the exact riff, even though the riff settings arrive afterwards
        setReal (p, "riffStyle", (float) riff::drill);
        pump (300);
        p.mutateRiff();
        p.riffFoldToScale = true;
        const auto saved = p.getRiff();
        juce::MemoryBlock state;
        p.getStateInformation (state);
        LeadProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        pump (400);
        check (q.getRiff() == saved, "a project reopens with its exact riff");
        check (juce::roundToInt (q.params.riffStyle->load()) == riff::drill, "a project reopens with its riff settings");
        check (q.riffFoldToScale.load(), "a project reopens with the piano roll folded to the scale");

        // changing a riff setting rewrites the riff from its seed
        const auto beforeStyle = q.getRiff();
        setReal (q, "riffStyle", (float) riff::chip);
        pump (400);
        check (! (q.getRiff() == beforeStyle) && q.getRiff().style == riff::chip, "changing Style rewrites the riff in the new style");

        // presets change the sound, never the riff
        setReal (q, "riffKey", 2.0f);
        const auto riffNow = q.getRiff();
        q.loadPreset (7);
        pump (300);
        check (juce::roundToInt (q.params.riffKey->load()) == 2 && q.getRiff() == riffNow, "loading a preset keeps the riff and its key");
    }

    // ------------------------------------------------------------------ sound
    std::cout << "OBSDN: sound library" << std::endl;
    {
        const auto& lib = factory::sounds();
        bool allDecode = true;
        for (const auto& snd : lib)
        {
            juce::AudioBuffer<float> audio; double rate = 0;
            if (! factory::decode (snd.id, audio, rate) || audio.getNumSamples() < 1000) { allDecode = false; std::cout << "    can't decode " << snd.id << std::endl; }
        }
        check (lib.size() >= 20 && lib.size() <= 25, "the library is a focused set of lead sounds (" + juce::String ((int) lib.size()) + ")");
        check (allDecode, "every library sound decodes");
        bool presetsFound = true;
        for (const auto& pr : makeLeadPresets().presets)
            if (pr.sound.isNotEmpty() && factory::find (pr.sound) == nullptr) { presetsFound = false; std::cout << "    missing " << pr.sound << std::endl; }
        check (presetsFound, "every preset's sound is in the library");
    }

    std::cout << "OBSDN: presets" << std::endl;
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        bool allOk = true;
        juce::StringArray levels;
        std::vector<float> dbs;
        const juce::File levelsCsv = outDir.getChildFile ("levels.csv");
        juce::String csv = "preset,category,db,peak\n";
        for (int i = 0; i < p.getNumFactoryPresets(); ++i)
        {
            p.loadPreset (i);
            p.prepareToPlay (sr, 256);
            // a short phrase: legato line then a held note
            std::vector<Ev> ev;
            const int notes[] { 69, 72, 74, 76, 74, 72, 69 };
            double t = 0.05;
            for (int k = 0; k < 7; ++k)
            {
                ev.push_back ({ t, juce::MidiMessage::noteOn (1, notes[k], 0.85f) });
                ev.push_back ({ t + (k == 6 ? 1.2 : 0.27), juce::MidiMessage::noteOff (1, notes[k]) });
                t += 0.25;
            }
            auto out = render (p, sr, ev, 3.2);
            const float peak = peakOf (out);
            const float db = momentaryMaxDb (out, sr);
            dbs.push_back (db);
            const bool ok = allFinite (out) && peak > 0.02f && peak < 1.0f;
            if (! ok) { allOk = false; std::cout << "    problem: " << p.getPreset (i).name << " peak " << peak << std::endl; }
            csv << p.getPreset (i).name << "," << p.getPreset (i).category << "," << juce::String (db, 2) << "," << juce::String (peak, 3) << "\n";
            if (argc > 2 && juce::String (argv[2]) == "wavs")
                writeWav (outDir.getChildFile (p.getPreset (i).name + ".wav"), out, sr);
        }
        levelsCsv.replaceWithText (csv);
        check (allOk, "all " + juce::String (p.getNumFactoryPresets()) + " presets play cleanly (no silence, no clipping, no NaN)");
        const auto [mn, mx] = std::minmax_element (dbs.begin(), dbs.end());
        check (*mx - *mn < 6.0f, "presets are level-matched within 6 dB (" + juce::String (*mn, 1) + " to " + juce::String (*mx, 1) + " dB)");
    }

    std::cout << "OBSDN: engine" << std::endl;
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        // Legato glide: two overlapping notes move one voice
        setReal (p, "voiceMode", 2.0f);
        auto out = render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 60, 0.8f) }, { 0.4, juce::MidiMessage::noteOn (1, 67, 0.8f) },
                                    { 0.6, juce::MidiMessage::noteOff (1, 60) }, { 1.0, juce::MidiMessage::noteOff (1, 67) } }, 1.6);
        check (allFinite (out) && peakOf (out) > 0.05f, "Legato plays overlapping notes as one gliding line");

        // CPU: a poly chord with the full 7-voice unison stack
        setReal (p, "voiceMode", 0.0f);
        setReal (p, "unison", 7.0f);
        setReal (p, "oscBLevel", 0.5f);
        std::vector<Ev> chord;
        for (int n : { 60, 64, 67, 71 }) { chord.push_back ({ 0.0, juce::MidiMessage::noteOn (1, n, 0.8f) }); chord.push_back ({ 9.0, juce::MidiMessage::noteOff (1, n) }); }
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto cpu = render (p, sr, chord, 10.0);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const double load = ms / 10000.0 * 100.0;
        check (load < 25.0, "a 4-note chord of 7-voice supersaws uses " + juce::String (load, 1) + "% of one core");

        // Mono: one note at a time
        setReal (p, "unison", 5.0f);
        setReal (p, "voiceMode", 1.0f);
        setReal (p, "scoop", 0.6f);
        setReal (p, "fall", 0.5f);
        out = render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 72, 0.8f) }, { 0.5, juce::MidiMessage::noteOff (1, 72) } }, 1.2);
        check (allFinite (out) && peakOf (out) > 0.05f, "scoop and fall render cleanly");

        // wave sweep: no level jumps between shapes
        setReal (p, "scoop", 0.0f);
        setReal (p, "fall", 0.0f);
        setReal (p, "unison", 1.0f);
        std::vector<float> lv;
        for (int w = 0; w < 8; ++w)
        {
            p.facetParam (LeadProcessor::wave).setValueNotifyingHost ((float) w / 7.0f);
            p.facetParam (LeadProcessor::tone).setValueNotifyingHost (1.0f);
            p.prepareToPlay (sr, 256);
            lv.push_back (momentaryMaxDb (render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 60, 0.8f) }, { 0.8, juce::MidiMessage::noteOff (1, 60) } }, 1.0), sr));
        }
        const auto [a, b] = std::minmax_element (lv.begin(), lv.end());
        check (*b - *a < 4.0f, "every wave shape plays at about the same level (spread " + juce::String (*b - *a, 1) + " dB)");
    }

    // ------------------------------------------------------------------ riff demo audio
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        p.loadPreset (0);
        setReal (p, "riffOn", 1.0f);
        const auto r = p.getRiff();
        auto out = render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 57, 0.9f) }, { r.bars * 4.0, juce::MidiMessage::noteOff (1, 57) } }, r.bars * 4.0 + 1.5);
        writeWav (outDir.getChildFile ("riff-demo.wav"), out, sr);
        check (peakOf (out) > 0.05f && allFinite (out), "holding one key plays the riff through Jade Supersaw");
    }

    // ------------------------------------------------------------------ sound design
    std::cout << "OBSDN: sound design" << std::endl;
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        auto phrase = [&]
        {
            p.prepareToPlay (sr, 256);
            return render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 69, 0.85f) }, { 0.9, juce::MidiMessage::noteOff (1, 69) } }, 1.3);
        };
        setReal (p, "unison", 3.0f);
        const float wavesDb = momentaryMaxDb (phrase(), sr);
        struct Case { const char* id; LeadProcessor::OscMode mode; const char* what; };
        const Case cases[] { { "wt_formant", LeadProcessor::table, "a library wavetable (TABLE)" },
                             { "vox_ah", LeadProcessor::grain, "a sung vowel as grains (GRAIN)" },
                             { "key_kalimba", LeadProcessor::sample, "a kalimba played as itself (SAMPLE)" },
                             { "pad_choir", LeadProcessor::table, "a choir cut into a wavetable (TABLE)" } };
        for (const auto& c : cases)
        {
            const bool loaded = p.loadFactorySound (c.id);
            p.setOscMode (c.mode);
            auto out = phrase();
            const float db = momentaryMaxDb (out, sr);
            check (loaded && allFinite (out) && peakOf (out) > 0.02f && std::abs (db - wavesDb) < 7.0f,
                   juce::String ("oscillator A plays ") + c.what + " (" + juce::String (db - wavesDb, 1) + " dB from the built-in waves)");
            if (argc > 2 && juce::String (argv[2]) == "wavs") writeWav (outDir.getChildFile (juce::String ("mode-") + c.id + ".wav"), out, sr);
        }

        // drop your own file: a pitched sound lands in GRAIN and plays in tune
        juce::AudioBuffer<float> tone (1, (int) (sr * 1.5));
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * (float) i / (float) sr)
                                   + 0.2f * std::sin (juce::MathConstants<float>::twoPi * 660.0f * (float) i / (float) sr));
        const auto wav = outDir.getChildFile ("dropped-E4.wav");
        writeWav (wav, tone, sr);
        juce::String error;
        const bool ok = p.loadFile (wav, error);
        auto src = p.getSource();
        check (ok && p.getOscMode() == LeadProcessor::grain && src != nullptr && std::abs (src->rootNote - 64.0f) < 0.3f,
               "a dropped file becomes oscillator A in GRAIN, with its pitch found (E4)");

        // Shapeshift a bounced synth note
        juce::AudioBuffer<float> saw (2, (int) (sr * 1.2));
        double ph = 0.0;
        for (int i = 0; i < saw.getNumSamples(); ++i)
        {
            float v = 0.0f;
            for (int h = 1; h < 40; ++h) v += std::sin ((float) (ph * h)) / (float) h;
            const float env = juce::jmin (1.0f, (float) i / 400.0f) * std::exp (-(float) i / (float) (sr * 1.5));
            saw.setSample (0, i, v * 0.3f * env);
            saw.setSample (1, i, v * 0.3f * env);
            ph += juce::MathConstants<double>::twoPi * 110.0 / sr;
        }
        const auto bounced = outDir.getChildFile ("bounced-A2.wav");
        writeWav (bounced, saw, sr);
        juce::String summary;
        const bool shifted = p.shapeshift (bounced, summary);
        check (shifted && p.getOscMode() == LeadProcessor::table && p.getSource()->shapeshifted,
               "Shapeshift rebuilds a bounced synth note as a playable table: " + summary.upToFirstOccurrenceOf ("\n", false, false));
        auto out = phrase();
        check (allFinite (out) && peakOf (out) > 0.02f, "the Shapeshifted table plays");

        // projects keep their sound: a library sound by name, your own sound inside the project
        p.loadFactorySound ("vox_ah");
        juce::MemoryBlock st;
        p.getStateInformation (st);
        LeadProcessor q;
        q.setStateInformation (st.getData(), (int) st.getSize());
        pump (300);
        check (q.getSource() != nullptr && q.getSource()->factoryId == "vox_ah" && q.getOscMode() == LeadProcessor::grain,
               "a project reopens with its library sound and mode");
        p.loadFile (wav, error);
        p.getStateInformation (st);
        LeadProcessor r2;
        r2.setStateInformation (st.getData(), (int) st.getSize());
        pump (300);
        check (r2.getSource() != nullptr && r2.getSource()->name == "dropped-E4.wav" && r2.getSource()->audio.getNumSamples() == tone.getNumSamples(),
               "a project reopens with your own dropped sound inside it");

        // a Sound Design preset brings its sound
        for (int i = 0; i < p.getNumFactoryPresets(); ++i)
            if (p.getPreset (i).name == "Vox Lead") p.loadPreset (i);
        check (p.getSource() != nullptr && p.getSource()->factoryId == "vox_ah" && p.getOscMode() == LeadProcessor::grain,
               "the Vox Lead preset brings its vowel and plays it as grains");
        for (int i = 0; i < p.getNumFactoryPresets(); ++i)
            if (p.getPreset (i).name == "Jade Supersaw") p.loadPreset (i);
        check (p.getOscMode() == LeadProcessor::waves, "a Waves preset goes back to the built-in shapes");
    }

    // ------------------------------------------------------------------ modulation
    std::cout << "OBSDN: modulation" << std::endl;
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        setReal (p, "fxReverbOn", 0.0f);
        setReal (p, "fxDelayOn", 0.0f);
        // an LFO on volume: tremolo you can measure
        setReal (p, mod::lfoParam (0, "Rate"), 0.0f);
        p.apvts.getParameter (mod::lfoParam (0, "Rate"))->setValueNotifyingHost (0.55f);   // ~9 Hz
        const int slot = p.assignModulation (mod::lfo1, mod::volume, -0.8f);
        auto out = render (p, sr, { { 0.0, juce::MidiMessage::noteOn (1, 69, 0.85f) }, { 1.4, juce::MidiMessage::noteOff (1, 69) } }, 1.5);
        float lo = 1e9f, hi = 0.0f;
        for (int w = (int) (0.3 * sr); w + 1200 < (int) (1.3 * sr); w += 600)
        {
            const float rms = out.getRMSLevel (0, w, 1200);
            lo = juce::jmin (lo, rms); hi = juce::jmax (hi, rms);
        }
        check (slot >= 0 && hi > lo * 2.5f, "an LFO on Volume makes a tremolo (" + juce::String (juce::Decibels::gainToDecibels (hi / juce::jmax (1e-6f, lo)), 1) + " dB swing)");
        p.clearModulation (slot);

        // the ring shows it: LFO 1 on Tone moves the Tone facet's marker
        p.assignModulation (mod::lfo1, mod::tone, 0.3f);
        float seen = 0.0f;
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer none;
        for (int i = 0; i < 200; ++i) { p.processBlock (buf, none); seen = juce::jmax (seen, std::abs (p.getFacetModulation (LeadProcessor::tone))); }
        check (seen > 0.1f, "the Tone facet shows its live modulation on the ring");
        check (LeadProcessor::destForFacet (LeadProcessor::glide) < 0 && LeadProcessor::destForFacet (LeadProcessor::wave) == mod::leadWave,
               "every facet but Glide can be a modulation target");

        // a macro on Wave sweeps the oscillator
        p.assignModulation (mod::macro1, mod::leadWave, 0.8f);
        check (mod::destNames()[mod::leadWave] == "Wave" && mod::destNames()[mod::leadVibrato] == "Vibrato", "the matrix names OBSDN's own destinations");

        // routings save with the project, and Strike rolls their amounts
        juce::MemoryBlock st;
        p.getStateInformation (st);
        LeadProcessor q;
        q.setStateInformation (st.getData(), (int) st.getSize());
        pump (200);
        int routed = 0;
        for (int s2 = 0; s2 < mod::numSlots; ++s2) if (juce::roundToInt (q.modParams.src[s2]->load()) != mod::none) ++routed;
        check (routed == 2, "modulation routings reopen with the project");
        bool rollsMod = false;
        for (auto* prm : p.getRandomisableExtras()) if (prm->getParameterID().startsWith ("mod")) rollsMod = true;
        check (rollsMod, "Strike also rolls modulation amounts (lock the matrix to keep them)");

        // CPU with everything moving
        setReal (p, "unison", 7.0f);
        setReal (p, "voiceMode", 0.0f);
        std::vector<Ev> chord;
        for (int n2 : { 60, 64, 67, 71 }) { chord.push_back ({ 0.0, juce::MidiMessage::noteOn (1, n2, 0.8f) }); chord.push_back ({ 4.5, juce::MidiMessage::noteOff (1, n2) }); }
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        render (p, sr, chord, 5.0);
        const double load = (juce::Time::getMillisecondCounterHiRes() - t0) / 5000.0 * 100.0;
        check (load < 30.0, "a 4-note 7-voice chord with 3 routings uses " + juce::String (load, 1) + "% of one core");
    }

    // ------------------------------------------------------------------ UI
    std::cout << "OBSDN: pages" << std::endl;
    {
        LeadProcessor p;
        p.prepareToPlay (sr, 256);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* le = dynamic_cast<LeadEditor*> (ed.get());
        // a few routings so the MOD page and the ring have something to show
        p.assignModulation (mod::lfo1, mod::tone, 0.3f);
        p.assignModulation (mod::lfo2, mod::leadWave, 0.25f);
        p.assignModulation (mod::macro1, mod::drive, 0.4f);
        juce::AudioBuffer<float> warm (2, 256);
        juce::MidiBuffer none;
        for (int i = 0; i < 40; ++i) p.processBlock (warm, none);
        const char* names[] { "lead", "synth", "mod", "riff", "fx" };
        for (int page = 0; page < 5; ++page)
        {
            le->showPage (page);
            snapshot (ed.get(), outDir.getChildFile (juce::String ("page-") + names[page] + ".png"));
        }
        check (true, "every page draws");
        // oscillator A in each sound mode
        const std::pair<const char*, LeadProcessor::OscMode> modes[] { { "wt_formant", LeadProcessor::table }, { "vox_ah", LeadProcessor::grain },
                                                                       { "key_kalimba", LeadProcessor::sample } };
        le->showPage (0);
        for (auto [id, m] : modes)
        {
            p.loadFactorySound (id);
            p.setOscMode (m);
            snapshot (ed.get(), outDir.getChildFile (juce::String ("osc-") + id + ".png"));
        }
        p.clearSource();

        // strike the stone and catch the lightning mid-flight
        le->showPage (0);
        std::function<juce::Button* (juce::Component*)> findStrike = [&] (juce::Component* c) -> juce::Button*
        {
            if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getName() == "Spark") return b;
            for (auto* child : c->getChildren())
                if (auto* found = findStrike (child)) return found;
            return nullptr;
        };
        if (auto* strike = findStrike (ed.get()))
        {
            strike->triggerClick();
            for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (15);
            auto* core = strike->getParentComponent();
            auto img = ed->createComponentSnapshot (ed->getLocalArea (core, core->getLocalBounds()), true, 1.5f);
            auto f = outDir.getChildFile ("strike.png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            check (true, "striking the stone throws lightning");
        }
        setReal (p, "riffOn", 1.0f);
        p.riffPreview = true;
        le->showPage (LeadEditor::riffPage_);
        // run audio so the playhead moves
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        for (int i = 0; i < 300; ++i) p.processBlock (buf, midi);
        snapshot (ed.get(), outDir.getChildFile ("page-riff-playing.png"));
        p.riffPreview = false;
    }

    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}

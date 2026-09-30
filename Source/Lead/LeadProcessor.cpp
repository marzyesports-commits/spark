#include "LeadProcessor.h"
#include "LeadVoice.h"
#include "LeadEditor.h"
#include "Instrument/FactorySounds.h"
#include "Instrument/Shapeshift.h"

namespace spark
{
namespace leadfmt
{
    float cutoffHz (float v)     { return 300.0f * std::pow (60.0f, juce::jlimit (0.0f, 1.0f, v)); }
    float glideSeconds (float v) { return 0.6f * v * v; }
    float vibratoSemis (float v) { return 0.6f * v * v; }

    juce::String cutoff (float v)
    {
        const float hz = cutoffHz (v);
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

namespace
{
    // The Wave facet's text depends on oscillator A's mode; the processor keeps this up to date.
    thread_local std::shared_ptr<std::atomic<int>> pendingModeRef;

    std::vector<FacetSpec> leadFacets()
    {
        auto mode = std::make_shared<std::atomic<int>> (0);
        pendingModeRef = mode;
        return {
            { "wave",    "WAVE",    0.29f, [mode] (float v)
              {
                  switch (mode->load())
                  {
                      case LeadProcessor::table:  return "Frame " + juce::String (juce::roundToInt (v * 100.0f)) + "%";
                      case LeadProcessor::grain:
                      case LeadProcessor::sample: return "Position " + juce::String (juce::roundToInt (v * 100.0f)) + "%";
                      default:                    return LeadProcessor::describeWave (v);
                  }
              }, "Oscillator A: the shape (Waves), the frame (Table) or where in the sound it plays (Grain, Sample)" },
            { "detune",  "DETUNE",  0.45f, fmt::percent, "Spread of the unison stack: from one clean oscillator to a wide supersaw" },
            { "tone",    "TONE",    0.62f, leadfmt::cutoff, "Filter cutoff" },
            { "bite",    "BITE",    0.35f, fmt::percent, "Filter envelope and resonance: the pluck and snap at the start of each note" },
            { "drive",   "DRIVE",   0.2f,  fmt::driveDb, "Saturation before the filter" },
            { "vibrato", "VIBRATO", 0.3f,  [] (float v) { return juce::String (juce::roundToInt (leadfmt::vibratoSemis (v) * 100.0f)) + " ct"; },
              "Vibrato depth. It waits (Vib Delay), then fades in on held notes. The mod wheel adds more" },
            { "glide",   "GLIDE",   0.25f, [] (float v)
              {
                  const float s = leadfmt::glideSeconds (v);
                  return s < 0.001f ? juce::String ("Off") : juce::String (juce::roundToInt (s * 1000.0f)) + " ms";
              }, "Glide time between notes (Legato glides when notes overlap, Mono always)" },
            { "space",   "SPACE",   0.3f,  fmt::percent, "Reverb size and amount" },
        };
    }

    void addLeadParameters (SparkProcessorBase::Layout& layout)
    {
        juce::NormalisableRange<float> unit (0.0f, 1.0f);
        auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::percent (v); });
        auto env = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::envTime (v); });
        auto add = [&] (std::unique_ptr<juce::RangedAudioParameter> p) { layout.add (std::move (p)); };
        auto flt = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::AudioParameterFloatAttributes a)
        {
            add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, a));
        };
        auto choice = [&] (const char* id, const char* name, juce::StringArray items, int def)
        {
            add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
        };

        FxRack::addParameters (layout);
        mod::addParameters (layout);

        // oscillators
        add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "unison", 1 }, "Unison", 1, LeadVoice::maxUnison, 5,
             juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 1 ? juce::String ("1 voice") : juce::String (v) + " voices"; })));
        flt ("width", "Width", unit, 0.7f, pct);
        choice ("oscAMode", "Osc A Source", { "Waves", "Table", "Grain", "Sample" }, LeadProcessor::waves);
        flt ("scanTime", "Table Scan", unit, 0.0f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 0.001f ? juce::String ("Off") : fmt::envTime (v); }));
        flt ("grainSize", "Grain Size", unit, 0.3f, juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::grainMs (v); }));
        flt ("grainSpray", "Grain Spray", unit, 0.25f, pct);
        flt ("oscBWave", "Osc B Wave", unit, 2.0f / 7.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return LeadProcessor::describeWave (v); }));
        add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "oscBSemi", 1 }, "Osc B Pitch", -24, 24, 0,
             juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; })));
        flt ("oscBFine", "Osc B Fine", { -50.0f, 50.0f }, 7.0f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; }));
        flt ("oscBLevel", "Osc B Level", unit, 0.0f, pct);
        flt ("subLevel", "Sub", unit, 0.0f, pct);
        flt ("noiseLevel", "Breath", unit, 0.0f, pct);

        // playing
        choice ("voiceMode", "Voice Mode", { "Poly", "Mono", "Legato" }, LeadProcessor::legato);
        add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "bendRange", 1 }, "Bend Range", 1, 24, 2,
             juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return juce::String (v) + " st"; })));
        flt ("vibRate", "Vibrato Rate", { 2.0f, 9.0f }, 5.5f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " Hz"; }));
        flt ("vibDelay", "Vibrato Delay", { 0.0f, 1.5f }, 0.3f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? juce::String ("None") : juce::String (juce::roundToInt (v * 1000.0f)) + " ms"; }));
        flt ("scoop", "Scoop", unit, 0.0f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? juce::String ("Off") : juce::String (v * 2.0f, 1) + " st"; }));
        flt ("fall", "Fall", unit, 0.0f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.005f ? juce::String ("Off") : "-" + juce::String (v * 12.0f, 1) + " st"; }));

        // filter
        choice ("filterType", "Filter Type", { "Low-pass", "High-pass", "Band-pass", "Notch" }, 0);
        flt ("resonance", "Resonance", unit, 0.15f, pct);
        flt ("keyTrack", "Key Track", unit, 0.5f, pct);
        flt ("velTone", "Velocity to Tone", unit, 0.35f, pct);
        flt ("ampVel", "Velocity to Amp", unit, 0.3f, pct);

        // envelopes (same time scale as Spark: 1 ms .. 5 s)
        flt ("ampA", "Amp Attack", unit, 0.03f, env);
        flt ("ampD", "Amp Decay", unit, 0.3f, env);
        flt ("ampS", "Amp Sustain", unit, 0.85f, pct);
        flt ("ampR", "Amp Release", unit, 0.2f, env);
        flt ("fltA", "Filter Attack", unit, 0.02f, env);
        flt ("fltD", "Filter Decay", unit, 0.22f, env);
        flt ("fltS", "Filter Sustain", unit, 0.25f, pct);
        flt ("fltR", "Filter Release", unit, 0.2f, env);

        flt ("level", "Level", { -36.0f, 6.0f }, -3.0f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; }));

        // the riff writer
        choice ("riffOn", "Riff", { "Off", "On" }, 0);
        choice ("riffKey", "Riff Key", riff::keyNames(), 9);
        choice ("riffScale", "Riff Scale", riff::scaleNames(), 1);
        choice ("riffStyle", "Riff Style", riff::styleNames(), riff::pop);
        choice ("riffBars", "Riff Length", { "1 bar", "2 bars", "4 bars" }, 1);
        flt ("riffDensity", "Riff Density", unit, 0.5f, pct);
        flt ("riffRange", "Riff Range", unit, 0.45f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (1.0f + v * 1.5f, 1) + " oct"; }));
        flt ("riffGate", "Riff Gate", { 0.1f, 1.2f }, 0.8f,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }));
        flt ("riffSwing", "Riff Swing", unit, 0.0f, pct);
        add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "riffOctave", 1 }, "Riff Octave", -2, 2, 0,
             juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v); })));
        choice ("riffFollow", "Riff Follow", { "In key", "Chromatic", "Fixed" }, riff::inKey);
        choice ("riffLatch", "Riff Latch", { "Off", "On" }, 0);
    }

    std::vector<float> makeFrame (int shape)
    {
        constexpr int n = Wavetable::frameSize;
        std::vector<float> f ((size_t) n);
        const float twoPi = juce::MathConstants<float>::twoPi;
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) n;
            float v = 0.0f;
            switch (shape)
            {
                case 0: v = std::sin (twoPi * x); break;
                case 1: v = x < 0.25f ? 4.0f * x : (x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f); break;
                case 2: v = 1.0f - 2.0f * x; break;                                  // saw
                case 3: v = x < 0.5f ? 1.0f : -1.0f; break;                          // square
                case 4: v = x < 0.25f ? 1.0f : 0.0f; break;                          // pulse 25%
                case 5: v = x < 0.12f ? 1.0f : 0.0f; break;                          // pulse 12%
                case 6: { const float y = x * 2.6f; v = 1.0f - 2.0f * (y - std::floor (y)); v *= 1.0f - x * 0.35f; break; }   // hard sync
                default:
                    // reed: harmonics with a formant bump around the 6th-8th
                    for (int h = 1; h <= 48; ++h)
                    {
                        const float bump = 1.0f + 3.0f * std::exp (-std::pow (((float) h - 7.0f) / 2.5f, 2.0f));
                        v += std::sin (twoPi * x * (float) h) / (float) h * bump * (h % 2 == 1 ? 1.0f : 0.55f);
                    }
                    break;
            }
            f[(size_t) i] = v;
        }
        // remove DC, then match loudness so sweeping Wave doesn't jump in level
        double mean = 0.0;
        for (auto v : f) mean += v;
        mean /= n;
        double power = 0.0;
        for (auto& v : f) { v -= (float) mean; power += (double) v * v; }
        const float rms = (float) std::sqrt (power / n);
        const float g = rms > 1.0e-6f ? 0.5f / rms : 1.0f;
        for (auto& v : f) v *= g;
        return f;
    }
}

// =====================================================================================
const Wavetable& LeadProcessor::waveTable()
{
    static const Wavetable::Ptr table = []
    {
        std::vector<std::vector<float>> frames;
        for (int s = 0; s < 8; ++s)
            frames.push_back (makeFrame (s));
        return Wavetable::fromFrames (std::move (frames));
    }();
    return *table;
}

const juce::StringArray& LeadProcessor::waveNames()
{
    static const juce::StringArray n { "Sine", "Triangle", "Saw", "Square", "Pulse", "Thin Pulse", "Sync", "Reed" };
    return n;
}

juce::String LeadProcessor::describeWave (float v)
{
    const float pos = juce::jlimit (0.0f, 1.0f, v) * 7.0f;
    const int i = juce::jlimit (0, 7, juce::roundToInt (pos));
    if (std::abs (pos - (float) i) < 0.12f)
        return waveNames()[i];
    const int a = (int) std::floor (pos);
    return waveNames()[a] + juce::String::fromUTF8 (" \xe2\x86\x92 ") + waveNames()[juce::jmin (7, a + 1)];
}

// =====================================================================================
LeadProcessor::LeadProcessor()
    : SparkProcessorBase (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          leadFacets(), addLeadParameters, makeLeadPresets(), "lead"),
      FxHost (static_cast<SparkProcessorBase&> (*this)),
      ModHost (static_cast<SparkProcessorBase&> (*this), static_cast<FxHost&> (*this)),
      synth (*this)
{
    for (int i = 0; i < numFacets; ++i)
        params.facet[i] = apvts.getRawParameterValue (getFacets()[(size_t) i].id);
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    params.unison = raw ("unison");        params.width = raw ("width");
    params.oscAMode = raw ("oscAMode");    params.scanTime = raw ("scanTime");
    params.grainSize = raw ("grainSize");  params.grainSpray = raw ("grainSpray");
    modeForDisplay = pendingModeRef;
    pendingModeRef.reset();
    formats.registerBasicFormats();
    params.oscBWave = raw ("oscBWave");    params.oscBSemi = raw ("oscBSemi");
    params.oscBFine = raw ("oscBFine");    params.oscBLevel = raw ("oscBLevel");
    params.subLevel = raw ("subLevel");    params.noiseLevel = raw ("noiseLevel");
    params.voiceMode = raw ("voiceMode");  params.bendRange = raw ("bendRange");
    params.filterType = raw ("filterType"); params.resonance = raw ("resonance");
    params.keyTrack = raw ("keyTrack");    params.velTone = raw ("velTone");  params.ampVel = raw ("ampVel");
    params.ampA = raw ("ampA"); params.ampD = raw ("ampD"); params.ampS = raw ("ampS"); params.ampR = raw ("ampR");
    params.fltA = raw ("fltA"); params.fltD = raw ("fltD"); params.fltS = raw ("fltS"); params.fltR = raw ("fltR");
    params.vibRate = raw ("vibRate");      params.vibDelay = raw ("vibDelay");
    params.scoop = raw ("scoop");          params.fall = raw ("fall");
    params.level = raw ("level");
    params.riffOn = raw ("riffOn");        params.riffKey = raw ("riffKey");
    params.riffScale = raw ("riffScale");  params.riffStyle = raw ("riffStyle");
    params.riffBars = raw ("riffBars");    params.riffDensity = raw ("riffDensity");
    params.riffRange = raw ("riffRange");  params.riffGate = raw ("riffGate");
    params.riffSwing = raw ("riffSwing");  params.riffOctave = raw ("riffOctave");
    params.riffFollow = raw ("riffFollow"); params.riffLatch = raw ("riffLatch");
    rack.attach (apvts);
    attachModulation (apvts);

    for (int i = 0; i < 8; ++i)
        synth.addVoice (new LeadVoice (*this));
    synth.addSound (new LeadSound());
    synth.setMinimumRenderingSubdivisionSize (32, false);

    // a first riff to play with
    const auto s = riffSettings();
    lastGenSettings = { (float) s.style, (float) s.bars, s.density, s.range };
    setRiff (riff::generate (s, 2026));
    player.reset();
    startTimerHz (10);
}

LeadProcessor::~LeadProcessor()
{
    stopTimer();
}

Envelope::Settings LeadProcessor::ampSettings() const
{
    Envelope::Settings s;
    s.attack = fmt::envSeconds (params.ampA->load());
    s.decay = fmt::envSeconds (params.ampD->load());
    s.sustain = params.ampS->load();
    s.release = fmt::envSeconds (params.ampR->load());
    return s;
}

Envelope::Settings LeadProcessor::filterSettings() const
{
    Envelope::Settings s;
    s.attack = fmt::envSeconds (params.fltA->load());
    s.decay = fmt::envSeconds (params.fltD->load());
    s.sustain = params.fltS->load();
    s.release = fmt::envSeconds (params.fltR->load());
    s.decayCurve = 0.4f;   // snappy
    return s;
}

// =====================================================================================
riff::Settings LeadProcessor::riffSettings() const
{
    riff::Settings s;
    s.style = juce::roundToInt (params.riffStyle->load());
    const int barsIndex = juce::roundToInt (params.riffBars->load());
    s.bars = barsIndex == 0 ? 1 : (barsIndex == 1 ? 2 : 4);
    s.density = params.riffDensity->load();
    s.range = params.riffRange->load();
    s.scale = juce::roundToInt (params.riffScale->load());
    return s;
}

riff::Riff LeadProcessor::getRiff() const
{
    return currentRiff;
}

void LeadProcessor::pushRiffToAudio()
{
    riff::Riff copy = currentRiff;          // allocate here, on the message thread
    const juce::SpinLock::ScopedLockType sl (riffLock);
    std::swap (audioRiff, copy);
}

void LeadProcessor::setRiff (const riff::Riff& r, bool addToHistory)
{
    currentRiff = r;
    if (addToHistory)
    {
        riffHistory.push_back (r);
        if (riffHistory.size() > 24)
            riffHistory.erase (riffHistory.begin());
        riffHistoryIndex = (int) riffHistory.size() - 1;
    }
    else if (juce::isPositiveAndBelow (riffHistoryIndex, (int) riffHistory.size()))
    {
        riffHistory[(size_t) riffHistoryIndex] = r;
    }
    pushRiffToAudio();
    sendChangeMessage();
}

void LeadProcessor::generateRiff()
{
    setRiff (riff::generate (riffSettings(), (juce::uint32) riffRandom.nextInt (1 << 30) + 1));
}

void LeadProcessor::mutateRiff()
{
    setRiff (riff::mutate (currentRiff, riffSettings(), (juce::uint32) riffRandom.nextInt (1 << 30) + 1));
}

void LeadProcessor::newRiffRhythm()
{
    setRiff (riff::newRhythm (currentRiff, riffSettings(), (juce::uint32) riffRandom.nextInt (1 << 30) + 1));
}

void LeadProcessor::answerRiff()
{
    setRiff (riff::answer (currentRiff, riffSettings(), (juce::uint32) riffRandom.nextInt (1 << 30) + 1));
}

void LeadProcessor::regenerateRiff()
{
    setRiff (riff::generate (riffSettings(), currentRiff.seed));
}

void LeadProcessor::toggleRiffNote (int tick, int degree)
{
    auto r = currentRiff;
    tick = juce::jlimit (0, r.lengthTicks() - 1, tick);
    auto it = std::find_if (r.notes.begin(), r.notes.end(), [tick] (const riff::Note& n) { return n.start == tick; });
    if (it != r.notes.end())
    {
        if (it->degree == degree) r.notes.erase (it);
        else it->degree = degree;
    }
    else
    {
        r.notes.push_back ({ tick, 6, degree, tick % riff::ticksPerBeat == 0 ? 0.9f : 0.8f, false });
    }
    riff::recomputeSpans (r);
    setRiff (r, false);
}

void LeadProcessor::recallRiff (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) riffHistory.size()))
        return;
    riffHistoryIndex = index;
    currentRiff = riffHistory[(size_t) index];
    pushRiffToAudio();
    sendChangeMessage();
}

juce::String LeadProcessor::riffName() const
{
    const auto key = riff::keyNames()[juce::roundToInt (params.riffKey->load())];
    const auto scale = riff::scaleNames()[juce::roundToInt (params.riffScale->load())];
    const auto style = riff::styleNames()[currentRiff.style];
    return "OBSDN Riff " + key + " " + scale + " " + style + " " + juce::String (currentRiff.seed % 1000).paddedLeft ('0', 3);
}

juce::File LeadProcessor::exportRiffMidi() const
{
    return riff::writeMidiFile (currentRiff, juce::roundToInt (params.riffKey->load()), juce::roundToInt (params.riffScale->load()),
                                juce::roundToInt (params.riffOctave->load()), params.riffGate->load(), params.riffSwing->load(), riffName());
}

void LeadProcessor::timerCallback()
{
    if (modeForDisplay != nullptr)
        modeForDisplay->store (getOscMode());
    for (int i = retired.size(); --i >= 0;)
        if (retired.getObjectPointerUnchecked (i)->getReferenceCount() <= 1)
            retired.remove (i);

    // Changing the style, length, density or range rewrites the riff from its seed, so you hear the change
    const auto s = riffSettings();
    const std::array<float, 4> now { (float) s.style, (float) s.bars, s.density, s.range };
    if (syncRiffSettings.exchange (false))
        lastGenSettings = now;   // a project just loaded: its riff already matches its settings
    if (now != lastGenSettings)
    {
        lastGenSettings = now;
        setRiff (riff::generate (s, currentRiff.seed));
    }
}

// =====================================================================================
std::vector<juce::RangedAudioParameter*> LeadProcessor::getRandomisableExtras() const
{
    std::vector<juce::RangedAudioParameter*> out;
    addRackExtras (out);
    out.push_back (apvts.getParameter ("resonance"));
    out.push_back (apvts.getParameter ("width"));
    if (params.oscBLevel->load() > 0.01f)
    {
        out.push_back (apvts.getParameter ("oscBWave"));
        out.push_back (apvts.getParameter ("oscBLevel"));
    }
    if (getOscMode() == grain)
    {
        out.push_back (apvts.getParameter ("grainSize"));
        out.push_back (apvts.getParameter ("grainSpray"));
    }
    if (params.subLevel->load() > 0.01f) out.push_back (apvts.getParameter ("subLevel"));
    addModExtras (out);
    if (params.noiseLevel->load() > 0.01f) out.push_back (apvts.getParameter ("noiseLevel"));
    return out;
}

void LeadProcessor::getCoreShape (std::vector<float>& out, int n)
{
    out.assign ((size_t) n, 0.0f);
    const auto& table = waveTable();
    const float last = (float) (table.getNumFrames() - 1);
    std::vector<float> a, b;
    const auto src = getSource();
    const int mode = getOscMode();
    if (src != nullptr && mode == LeadProcessor::table && src->table != nullptr)
        src->table->getFrameShape (params.facet[wave]->load() * (float) (src->table->getNumFrames() - 1), a, n);
    else if (src != nullptr && (mode == grain || mode == sample) && src->audio.getNumSamples() > 64)
    {
        // the stretch of sound under the play position, wrapped into a ring
        const int len = src->audio.getNumSamples();
        const int span = juce::jlimit (n, len, (int) (0.02 * src->sampleRate));
        const int start = juce::jlimit (0, juce::jmax (0, len - span), (int) (params.facet[wave]->load() * (float) len) - span / 2);
        const float* d = src->audio.getReadPointer (0);
        a.resize ((size_t) n);
        for (int i = 0; i < n; ++i) a[(size_t) i] = d[start + (int) ((juce::int64) i * span / n)];
    }
    else
        table.getFrameShape (params.facet[wave]->load() * last, a, n);
    const float levelB = params.oscBLevel->load();
    if (levelB > 0.01f)
        table.getFrameShape (params.oscBWave->load() * last, b, n);
    // the unison stack smears the shape: show copies slid apart by the detune
    const float detuneAmt = params.facet[detune]->load();
    const int shift = juce::roundToInt (detuneAmt * (float) n * 0.04f);
    const int voices = juce::roundToInt (params.unison->load());
    float peak = 1.0e-6f;
    for (int i = 0; i < n; ++i)
    {
        float v = a[(size_t) i];
        if (voices > 1 && shift > 0)
            v = (v + 0.6f * a[(size_t) ((i + shift) % n)] + 0.6f * a[(size_t) ((i + n - shift) % n)]) / 2.2f;
        if (! b.empty()) v += b[(size_t) i] * levelB;
        out[(size_t) i] = v;
        peak = juce::jmax (peak, std::abs (v));
    }
    for (auto& v : out) v /= peak;
}

// =====================================================================================
void LeadProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    rack.prepare (sampleRate, samplesPerBlock);
    levelSmooth.reset (sampleRate, 0.03);
    levelSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.level->load()));
    player.reset();
    riffWasOn = false;
}

bool LeadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void LeadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
            if (auto q = pos->getPpqPosition()) { ppq = *q; playing = pos->getIsPlaying(); }
        }

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isController() && msg.getControllerNumber() == 1) modWheel = (float) msg.getControllerValue() / 127.0f;
        else if (msg.isChannelPressure()) pressure = (float) msg.getChannelPressureValue() / 127.0f;
        else if (msg.isAftertouch()) pressure = (float) msg.getAfterTouchValue() / 127.0f;
    }

    // the riff writer turns held keys into the riff
    const bool riffOn = params.riffOn->load() > 0.5f;
    if (riffOn)
    {
        riff::Player::Context ctx;
        ctx.sampleRate = currentSampleRate;
        ctx.bpm = bpm;
        ctx.ppq = ppq;
        ctx.hostPlaying = playing;
        ctx.key = juce::roundToInt (params.riffKey->load());
        ctx.scale = juce::roundToInt (params.riffScale->load());
        ctx.octave = juce::roundToInt (params.riffOctave->load());
        ctx.follow = juce::roundToInt (params.riffFollow->load());
        ctx.gate = params.riffGate->load();
        ctx.swing = params.riffSwing->load();
        ctx.latch = params.riffLatch->load() > 0.5f;
        ctx.preview = riffPreview.load();
        const juce::SpinLock::ScopedTryLockType sl (riffLock);
        player.process (sl.isLocked() ? audioRiff : emptyRiff, ctx, midi, n);
        riffPlayhead = player.playhead();
    }
    else if (riffWasOn)
    {
        player.reset();
        riffPlayhead = -1.0f;
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    }
    riffWasOn = riffOn;

    updateModulation (midi, bpm, ppq, playing, currentSampleRate);
    synth.renderNextBlock (buffer, midi, 0, n);

    const int numCh = buffer.getNumChannels();
    if (numCh == 2)
        rack.process (buffer, bpm, ppq, playing, juce::jlimit (0.0f, 1.0f, params.facet[space]->load() + liveMod[mod::space].load()));
    advanceLfos (n);

    levelSmooth.setTargetValue (juce::Decibels::decibelsToGain (params.level->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = levelSmooth.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& x = buffer.getWritePointer (ch)[i];
            x *= g;
            const float ax = std::abs (x);
            if (ax > 0.8f)
                x = std::copysign (0.8f + 0.2f * std::tanh ((ax - 0.8f) / 0.2f), x);
        }
    }
}

juce::AudioProcessorEditor* LeadProcessor::createEditor()
{
    return new LeadEditor (*this);
}

// =====================================================================================
void LeadProcessor::writeExtraState (juce::ValueTree& extra)
{
    writeRackState (extra);
    if (auto s = getSource())
    {
        if (s->factoryId.isNotEmpty() && ! s->shapeshifted)
            extra.setProperty ("factorySound", s->factoryId, nullptr);   // library sounds are saved by name
        else
        {
            extra.setProperty ("sourceName", s->name, nullptr);
            extra.setProperty ("tableFrame", s->tableFrameLength, nullptr);
            extra.setProperty ("rootNote", (double) s->rootNote, nullptr);
            extra.setProperty ("shapeshift", s->shapeshifted, nullptr);
            extra.setProperty ("sourceAudio", s->getEmbeddedAudio(), nullptr);   // travels with the project
        }
    }
    extra.setProperty ("riff", currentRiff.toString(), nullptr);
    extra.setProperty ("riffFold", riffFoldToScale.load(), nullptr);
}

void LeadProcessor::readExtraState (const juce::ValueTree& extra)
{
    readRackState (extra);
    if (const auto id = extra.getProperty ("factorySound").toString(); id.isNotEmpty())
        loadFactorySound (id, true);
    else if (const auto embedded = extra.getProperty ("sourceAudio").toString(); embedded.isNotEmpty())
    {
        juce::MemoryBlock block;
        if (block.fromBase64Encoding (embedded))
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (block, false), true));
            if (reader != nullptr && reader->lengthInSamples > 64)
            {
                juce::AudioBuffer<float> audio ((int) juce::jlimit (1u, 2u, reader->numChannels), (int) reader->lengthInSamples);
                reader->read (&audio, 0, audio.getNumSamples(), 0, true, audio.getNumChannels() > 1);
                const bool shifted = extra.getProperty ("shapeshift", false);
                SourceData::Ptr s;
                if (shifted)
                {
                    const auto result = shapeshift::analyse (audio, reader->sampleRate);
                    s = new SourceData();
                    s->audio = std::move (audio);
                    s->sampleRate = reader->sampleRate;
                    s->table = Wavetable::fromFrames (result.ok ? result.frames : std::vector<std::vector<float>> {});
                    s->shapeshifted = true;
                    s->computePeaks();
                    s->computeGains();
                }
                else
                    s = SourceData::make (std::move (audio), reader->sampleRate, {}, {}, (int) extra.getProperty ("tableFrame", 0));
                s->name = extra.getProperty ("sourceName").toString();
                s->rootNote = (float) (double) extra.getProperty ("rootNote", 60.0);
                installSource (s);
            }
        }
    }
    else
        installSource (nullptr);
    riffFoldToScale = (bool) extra.getProperty ("riffFold", false);
    const auto text = extra.getProperty ("riff").toString();
    syncRiffSettings = true;   // the riff settings arrive after this; don't rewrite the saved riff when they do
    if (text.isNotEmpty())
    {
        auto r = riff::Riff::fromString (text);
        if (! r.notes.empty())
        {
            riffHistory.clear();
            setRiff (r);
        }
    }
}
int LeadProcessor::destForFacet (int facet)
{
    static const int map[] { mod::leadWave, mod::leadDetune, mod::tone, mod::leadBite, mod::drive, mod::leadVibrato, -1, mod::space };
    return juce::isPositiveAndBelow (facet, numFacets) ? map[facet] : -1;
}

float LeadProcessor::getFacetModulation (int facet) const
{
    const int d = destForFacet (facet);
    return d >= 0 ? liveMod[(size_t) d].load() : 0.0f;
}

// =====================================================================================
SourceData::Ptr LeadProcessor::getSource() const
{
    const juce::SpinLock::ScopedLockType lock (sourceLock);
    return source;
}

void LeadProcessor::installSource (SourceData::Ptr s)
{
    SourceData::Ptr old;
    {
        const juce::SpinLock::ScopedLockType lock (sourceLock);
        old = source;
        source = s;
    }
    if (old != nullptr)
        retired.add (old);
    sendChangeMessage();
}

void LeadProcessor::setOscMode (OscMode m)
{
    setParam ("oscAMode", (float) m / 3.0f);
    if (modeForDisplay != nullptr)
        modeForDisplay->store (m);
}

juce::String LeadProcessor::sourceName() const
{
    auto s = getSource();
    return s != nullptr ? s->name : juce::String();
}

void LeadProcessor::clearSource()
{
    installSource (nullptr);
    setOscMode (waves);
}

bool LeadProcessor::loadFile (const juce::File& file, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "OBSDN can't read " + file.getFileName() + ". Try a WAV, AIFF or FLAC file.";
        return false;
    }
    const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    if (len < 64)
    {
        error = file.getFileName() + " is too short to use.";
        return false;
    }
    juce::AudioBuffer<float> audio ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&audio, 0, len, 0, true, audio.getNumChannels() > 1);
    if (audio.getMagnitude (0, len) < 1.0e-5f)
    {
        error = file.getFileName() + " is silent.";
        return false;
    }
    const int clmFrame = Wavetable::readClmFrameSize (file);
    const bool looksLikeTable = clmFrame > 0
        || (len % Wavetable::frameSize == 0 && len / Wavetable::frameSize >= 2 && len / Wavetable::frameSize <= Wavetable::maxFrames);
    auto s = SourceData::make (std::move (audio), reader->sampleRate > 0 ? reader->sampleRate : 44100.0, file.getFileName(), file,
                               looksLikeTable ? (clmFrame > 0 ? clmFrame : Wavetable::frameSize) : 0);
    if (! looksLikeTable)
        if (const float midi = shapeshift::detectMidiNote (s->audio, s->sampleRate); midi > 0.0f)
            s->rootNote = midi;
    installSource (s);
    setOscMode (looksLikeTable ? table : grain);
    return true;
}

bool LeadProcessor::loadFactorySound (const juce::String& id, bool fromPreset)
{
    const auto* info = factory::find (id);
    juce::AudioBuffer<float> audio;
    double sr = 44100.0;
    if (info == nullptr)
        return false;
    if (auto current = getSource(); current != nullptr && current->factoryId == id && ! current->shapeshifted)
    {
        if (! fromPreset) setOscMode (info->tableFrame > 0 ? table : grain);
        return true;
    }
    if (! factory::decode (id, audio, sr))
        return false;
    auto s = SourceData::make (std::move (audio), sr, info->name, {}, info->tableFrame);
    s->rootNote = info->root;
    s->factoryId = id;
    installSource (s);
    if (! fromPreset)
        setOscMode (info->tableFrame > 0 ? table : (info->category == "Drums & Perc" ? sample : grain));
    return true;
}

void LeadProcessor::applyPresetSound (const Preset& p)
{
    if (p.sound.isNotEmpty())
        loadFactorySound (p.sound, true);
}

juce::String LeadProcessor::currentSoundId() const
{
    auto s = getSource();
    return s != nullptr && ! s->shapeshifted && getOscMode() != waves ? s->factoryId : juce::String();
}

bool LeadProcessor::shapeshift (const juce::File& file, juce::String& summary)
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::String name;
    if (file != juce::File())
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            summary = "OBSDN can't read " + file.getFileName() + ". Try a WAV, AIFF or FLAC file.";
            return false;
        }
        const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 20.0));
        audio.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
        reader->read (&audio, 0, len, 0, true, audio.getNumChannels() > 1);
        sampleRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        name = file.getFileName();
    }
    else if (auto current = getSource())
    {
        audio = current->audio;
        sampleRate = current->sampleRate;
        name = current->name;
    }
    else
    {
        summary = "Choose a sound first: bounce one note from Serum, Serum 2 or Vital as a WAV.";
        return false;
    }

    const auto result = shapeshift::analyse (audio, sampleRate);
    if (! result.ok)
    {
        summary = result.error;
        return false;
    }
    SourceData::Ptr src (new SourceData());
    src->audio = std::move (audio);
    src->sampleRate = sampleRate;
    src->name = name;
    src->rootNote = result.midiNote;
    src->shapeshifted = true;
    src->table = Wavetable::fromFrames (result.frames);
    src->computePeaks();
    src->computeGains();
    installSource (src);

    // Rebuild it with OBSDN's controls: the table plays through the note's evolution, the filter opens
    // (the timbre is in the frames) and the amp envelope matches the original.
    setOscMode (table);
    setParam ("scanTime", shapeshift::timeToParam (result.scanSeconds * 0.5f));
    setParam ("ampA", shapeshift::timeToParam (result.attack));
    setParam ("ampD", shapeshift::timeToParam (result.decay));
    setParam ("ampS", result.sustain);
    setParam ("ampR", shapeshift::timeToParam (result.release));
    setParam ("unison", 0.0f);
    FacetValues f = currentFacetValues();
    f[wave] = 0.0f;
    f[detune] = 0.0f;
    f[tone] = 1.0f;
    f[bite] = 0.0f;
    f[drive] = 0.0f;
    applyFacetValues (f);
    lineage.push (currentFacetValues(), (juce::uint32) juce::Random::getSystemRandom().nextInt(), currentExtraValues());
    lastShapeshift = result.summary;
    summary = result.summary;
    sendChangeMessage();
    return true;
}
} // namespace spark

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new spark::LeadProcessor();
}

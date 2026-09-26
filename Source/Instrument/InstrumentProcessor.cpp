#include "InstrumentProcessor.h"
#include "InstrumentEditor.h"
#include "SparkVoice.h"
#include "Common/Presets.h"
#include "Shapeshift.h"

namespace spark
{
namespace
{
    std::vector<FacetSpec> instrumentFacets()
    {
        return {
            { "pitch",    "PITCH",    0.5f,  fmt::semitones, "Transpose the whole sound, in semitones" },
            { "position", "POSITION", 0.32f, fmt::percent,   "Where in the sample the grains are taken from" },
            { "grain",    "GRAIN",    0.45f, fmt::grainMs,   "Length of each grain" },
            { "morph",    "MORPH",    0.58f, fmt::percent,   "Position in the wavetable" },
            { "tone",     "TONE",     0.68f, fmt::cutoff,    "Low-pass filter cutoff" },
            { "drive",    "DRIVE",    0.3f,  fmt::driveDb,   "Saturation" },
            { "motion",   "MOTION",   0.4f,  fmt::percent,   "Movement: grain spray, stereo spread, table sweep and detune" },
            { "space",    "SPACE",    0.42f, fmt::percent,   "Reverb size and amount" },
        };
    }

    void addInstrumentParameters (SparkProcessorBase::Layout& layout)
    {
        auto envAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::envTime (v); });
        auto pctAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::percent (v); });
        juce::NormalisableRange<float> unit (0.0f, 1.0f);

        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Engine", juce::StringArray { "Grain", "Table", "Sample" }, 0));
        // Morph scan: sweeps the wavetable from Morph to the last frame over this time on every note (0 = off).
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "scanTime", 1 }, "Morph Scan", unit, 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 0.001f ? juce::String ("Off") : fmt::envTime (v); })));
        FxRack::addParameters (layout);
        juce::NormalisableRange<float> bipolar (-1.0f, 1.0f);
        auto curveAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::curve (v); });
        auto octAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::octaves (v); });
        auto addEnv = [&] (const juce::String& prefix, const juce::String& name, float a, float d, float s, float r)
        {
            auto id = [&] (const juce::String& stage) { return prefix.isEmpty() ? stage.substring (0, 1).toLowerCase() + stage.substring (1) : prefix + stage; };
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Attack"), 1 }, name + "Attack", unit, a, envAttr));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Hold"), 1 }, name + "Hold", unit, 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 0.0f ? juce::String ("0 ms") : fmt::envTime (v); })));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Decay"), 1 }, name + "Decay", unit, d, envAttr));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Sustain"), 1 }, name + "Sustain", unit, s, pctAttr));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Release"), 1 }, name + "Release", unit, r, envAttr));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("AttackCurve"), 1 }, name + "Attack Curve", bipolar, 0.0f, curveAttr));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("DecayCurve"), 1 }, name + "Decay Curve", bipolar, 0.0f, curveAttr));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("ReleaseCurve"), 1 }, name + "Release Curve", bipolar, 0.0f, curveAttr));
        };
        // Amp envelope keeps its original ids (attack, decay, sustain, release) so old projects still load.
        addEnv ({}, {}, 0.08f, 0.4f, 0.7f, 0.35f);
        addEnv ("tone", "Tone ", 0.01f, 0.35f, 0.0f, 0.3f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "ampVelocity", 1 }, "Velocity to Amp", unit, 0.75f, pctAttr));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "toneAmount", 1 }, "Tone Envelope Amount", bipolar, 0.0f, octAttr));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "toneVelocity", 1 }, "Velocity to Tone Envelope", unit, 0.0f, pctAttr));
        // ---- Play: voice mode, glide, pitch-bend range
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "voiceMode", 1 }, "Voice Mode",
                                                                  juce::StringArray { "Poly", "Mono", "Legato" }, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "glide", 1 }, "Glide", unit, 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return fmt::glideTime (v); })));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "bendRange", 1 }, "Pitch Bend Range", 0, 24, 2,
            juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return juce::String (v) + " st"; })));
        // ---- Filter: type, resonance, key tracking (cutoff is the Tone facet)
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "filterType", 1 }, "Filter Type",
                                                                  juce::StringArray { "Low-pass", "High-pass", "Band-pass", "Notch" }, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "resonance", 1 }, "Resonance", unit, 0.1f, pctAttr));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "keyTrack", 1 }, "Filter Key Tracking", unit, 0.0f, pctAttr));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "level", 1 }, "Level",
                                                                 juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -3.0f,
                                                                 juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }
}

void SourceData::computePeaks()
{
    const int bins = 256;
    peaks.assign ((size_t) bins, 0.0f);
    const int len = audio.getNumSamples();
    if (len == 0)
        return;
    float maxPeak = 1.0e-6f;
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((juce::int64) b * len / bins);
        const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (b + 1) * len / bins));
        float pk = 0.0f;
        for (int c = 0; c < audio.getNumChannels(); ++c)
            pk = juce::jmax (pk, audio.getMagnitude (c, s0, s1 - s0));
        peaks[(size_t) b] = pk;
        maxPeak = juce::jmax (maxPeak, pk);
    }
    for (auto& p : peaks) p /= maxPeak;
}

InstrumentProcessor::InstrumentProcessor()
    : SparkProcessorBase (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                          instrumentFacets(), addInstrumentParameters, makeInstrumentPresets(), "instrument"),
      synth (*this)
{
    formats.registerBasicFormats();

    for (int i = 0; i < numFacets; ++i)
        params.facet[i] = apvts.getRawParameterValue (getFacets()[(size_t) i].id);
    params.mode = modeParam = apvts.getRawParameterValue ("mode");
    auto wireEnv = [this] (EnvParams& e, const juce::String& prefix)
    {
        auto id = [&] (const juce::String& stage) { return prefix.isEmpty() ? stage.substring (0, 1).toLowerCase() + stage.substring (1) : prefix + stage; };
        e.attack = apvts.getRawParameterValue (id ("Attack"));
        e.hold = apvts.getRawParameterValue (id ("Hold"));
        e.decay = apvts.getRawParameterValue (id ("Decay"));
        e.sustain = apvts.getRawParameterValue (id ("Sustain"));
        e.release = apvts.getRawParameterValue (id ("Release"));
        e.attackCurve = apvts.getRawParameterValue (id ("AttackCurve"));
        e.decayCurve = apvts.getRawParameterValue (id ("DecayCurve"));
        e.releaseCurve = apvts.getRawParameterValue (id ("ReleaseCurve"));
    };
    wireEnv (params.amp, {});
    wireEnv (params.toneEnv, "tone");
    params.ampVelocity = apvts.getRawParameterValue ("ampVelocity");
    params.toneAmount = apvts.getRawParameterValue ("toneAmount");
    params.toneVelocity = apvts.getRawParameterValue ("toneVelocity");
    params.level = apvts.getRawParameterValue ("level");
    params.scanTime = apvts.getRawParameterValue ("scanTime");
    params.voiceMode = apvts.getRawParameterValue ("voiceMode");
    params.glide = apvts.getRawParameterValue ("glide");
    params.bendRange = apvts.getRawParameterValue ("bendRange");
    params.filterType = apvts.getRawParameterValue ("filterType");
    params.resonance = apvts.getRawParameterValue ("resonance");
    params.keyTrack = apvts.getRawParameterValue ("keyTrack");
    rack.attach (apvts);

    installSource (makeBuiltInSource());

    synth.addSound (new SparkSound());
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new SparkVoice (*this));

    startTimer (2000);
}

InstrumentProcessor::~InstrumentProcessor() { stopTimer(); }

SourceData::Ptr InstrumentProcessor::makeBuiltInSource()
{
    // A slowly evolving, harmonically rich tone at middle C, so Spark makes sound out of the box.
    SourceData::Ptr s (new SourceData());
    s->sampleRate = 48000.0;
    s->name = "Spark Init";
    const int len = (int) (s->sampleRate * 3.0);
    s->audio.setSize (2, len);
    s->audio.clear();

    const double f0 = 261.6256;
    juce::Random r (1234);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = s->audio.getWritePointer (ch);
        const double detune = ch == 0 ? 0.9985 : 1.0015;
        for (int h = 1; h <= 40; ++h)
        {
            const double hz = f0 * h * detune;
            if (hz > s->sampleRate * 0.45) break;
            const float amp = 1.0f / (float) h;
            const float wobble = (float) h * 0.7f;
            const double rate = 0.25 + 0.05 * h;
            for (int i = 0; i < len; ++i)
            {
                const double t = (double) i / s->sampleRate;
                const float e = 0.35f + 0.65f * (0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * rate * t + wobble));
                d[i] += amp * e * (float) std::sin (juce::MathConstants<double>::twoPi * hz * t + h * 0.3);
            }
        }
        // a little air
        for (int i = 0; i < len; ++i)
            d[i] += (r.nextFloat() * 2.0f - 1.0f) * 0.015f;
    }
    const float mag = s->audio.getMagnitude (0, len);
    if (mag > 0) s->audio.applyGain (0.8f / mag);

    // short fades so grains taken near the edges don't click
    s->audio.applyGainRamp (0, 480, 0.0f, 1.0f);
    s->audio.applyGainRamp (len - 480, 480, 1.0f, 0.0f);

    s->table = Wavetable::fromSample (s->audio);
    s->computePeaks();
    return s;
}

SourceData::Ptr InstrumentProcessor::getSource() const
{
    const juce::SpinLock::ScopedLockType lock (sourceLock);
    return source;
}

void InstrumentProcessor::installSource (SourceData::Ptr s)
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

void InstrumentProcessor::timerCallback()
{
    for (int i = retired.size(); --i >= 0;)
        if (retired.getObjectPointerUnchecked (i)->getReferenceCount() <= 1)
            retired.remove (i);
}

juce::String InstrumentProcessor::getSupportedExtensions() const
{
    return formats.getWildcardForAllFormats();
}

SourceData::Ptr InstrumentProcessor::makeSource (juce::AudioBuffer<float> audio, double sampleRate, const juce::String& name,
                                                const juce::File& file, int tableFrameLength)
{
    SourceData::Ptr s (new SourceData());
    s->audio = std::move (audio);
    s->sampleRate = sampleRate;
    s->name = name;
    s->file = file;
    s->tableFrameLength = tableFrameLength;
    s->loadedAsWavetable = tableFrameLength > 0;
    s->table = tableFrameLength > 0 ? Wavetable::fromTableAudio (s->audio, tableFrameLength)
                                    : Wavetable::fromSample (s->audio);
    s->computePeaks();
    return s;
}

juce::String SourceData::getEmbeddedAudio() const
{
    const juce::ScopedLock sl (embedLock);
    if (embedded.isNotEmpty() || audio.getNumSamples() == 0)
        return embedded;

    juce::FlacAudioFormat flac;
    juce::MemoryBlock block;
    {
        std::unique_ptr<juce::OutputStream> os (new juce::MemoryOutputStream (block, false));
        auto writer = flac.createWriterFor (os, juce::AudioFormatWriterOptions()
                                                    .withSampleRate (juce::jlimit (8000.0, 192000.0, sampleRate))
                                                    .withNumChannels (audio.getNumChannels())
                                                    .withBitsPerSample (24));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
            return {};
    } // writer flushes and closes here
    embedded = block.toBase64Encoding();
    return embedded;
}

bool InstrumentProcessor::loadFile (const juce::File& file, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Spark can't read " + file.getFileName() + ". Try a WAV, AIFF or FLAC file.";
        return false;
    }

    const auto maxSamples = (juce::int64) (reader->sampleRate * 60.0);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxSamples);
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

    // Wavetable files: Serum's 'clm ' marker, or a length that is an exact number of 2048-sample frames.
    const int clmFrame = Wavetable::readClmFrameSize (file);
    const bool looksLikeTable = clmFrame > 0
        || (len % Wavetable::frameSize == 0 && len / Wavetable::frameSize >= 2 && len / Wavetable::frameSize <= Wavetable::maxFrames);

    auto s = makeSource (std::move (audio), reader->sampleRate > 0 ? reader->sampleRate : 44100.0, file.getFileName(), file,
                         looksLikeTable ? (clmFrame > 0 ? clmFrame : Wavetable::frameSize) : 0);
    if (! looksLikeTable)
        if (const float midi = shapeshift::detectMidiNote (s->audio, s->sampleRate); midi > 0.0f)
            s->rootNote = midi; // pitched samples play in tune across the keyboard
    installSource (s);
    setMode (looksLikeTable ? tableMode : grainMode);
    return true;
}

bool InstrumentProcessor::shapeshift (const juce::File& file, juce::String& summary)
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::String name;
    juce::File origin = file;

    if (file != juce::File())
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            summary = "Spark can't read " + file.getFileName() + ". Try a WAV, AIFF or FLAC file.";
            return false;
        }
        const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 20.0));
        audio.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
        reader->read (&audio, 0, len, 0, true, audio.getNumChannels() > 1);
        sampleRate = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        name = file.getFileName();
    }
    else
    {
        auto current = getSource();
        if (current == nullptr || current->name == "Spark Init")
        {
            summary = "Drop in or choose a sound first: bounce one note from Serum, Serum 2 or Vital as a WAV.";
            return false;
        }
        audio = current->audio;
        sampleRate = current->sampleRate;
        name = current->name;
        origin = current->file;
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
    src->file = origin;
    src->rootNote = result.midiNote;
    src->shapeshifted = true;
    src->table = Wavetable::fromFrames (result.frames);
    src->computePeaks();
    installSource (src);

    // Rebuild the sound with Spark's controls: the table plays through the note's evolution,
    // the filter is open (the timbre is in the frames), and the envelope matches the original.
    auto bipolar = [] (float c) { return (juce::jlimit (-1.0f, 1.0f, c) + 1.0f) * 0.5f; };
    setMode (tableMode);
    setParam ("scanTime", shapeshift::timeToParam (result.scanSeconds * 0.5f));
    setParam ("attack", shapeshift::timeToParam (result.attack));
    setParam ("hold", result.hold > 0.0f ? shapeshift::timeToParam (result.hold) : 0.0f);
    setParam ("decay", shapeshift::timeToParam (result.decay));
    setParam ("sustain", result.sustain);
    setParam ("release", shapeshift::timeToParam (result.release));
    setParam ("attackCurve", bipolar (result.attackCurve));
    setParam ("decayCurve", bipolar (result.decayCurve));
    setParam ("releaseCurve", bipolar (result.releaseCurve));
    setParam ("toneAmount", 0.5f);

    FacetValues f = currentFacetValues();
    f[pitch] = 0.5f;
    f[morph] = 0.0f;
    f[tone] = 1.0f;
    f[drive] = 0.0f;
    f[motion] = juce::jlimit (0.0f, 0.6f, result.width * 0.5f);
    f[space] = 0.12f;
    applyFacetValues (f);
    lineage.push (currentFacetValues(), (juce::uint32) juce::Random::getSystemRandom().nextInt(), currentExtraValues());
    lastShapeshift = result.summary;
    summary = result.summary;
    sendChangeMessage();
    return true;
}

void InstrumentProcessor::makeTableFromSample()
{
    auto current = getSource();
    if (current == nullptr)
        return;

    SourceData::Ptr s (new SourceData());
    s->audio = current->audio;
    s->sampleRate = current->sampleRate;
    s->name = current->name;
    s->file = current->file;
    s->peaks = current->peaks;
    s->rootNote = current->rootNote;
    s->table = Wavetable::fromSample (s->audio);
    installSource (s);
    setMode (tableMode);
}

bool InstrumentProcessor::exportTable (const juce::File& file, juce::String& error) const
{
    auto s = getSource();
    if (s == nullptr || s->table == nullptr)
    {
        error = "There's no wavetable to export yet.";
        return false;
    }
    // The export carries Spark's character: current Drive and Tone are baked into every frame.
    const auto frames = s->table->bake (params.facet[drive]->load(), params.facet[tone]->load());
    if (! Wavetable::writeWav (file, frames))
    {
        error = "Couldn't write " + file.getFullPathName();
        return false;
    }
    return true;
}

void InstrumentProcessor::getCoreShape (std::vector<float>& out, int n)
{
    out.assign ((size_t) n, 0.0f);
    auto s = getSource();
    if (s == nullptr)
        return;

    const float motionAmt = params.facet[motion]->load();
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (getMode() == tableMode && s->table != nullptr)
    {
        const float lfo = (float) std::sin (t * juce::MathConstants<double>::twoPi * (0.1 + motionAmt * 0.9)) * motionAmt * 0.35f;
        const float m = juce::jlimit (0.0f, 1.0f, params.facet[morph]->load() + lfo);
        s->table->getFrameShape (m * (float) (s->table->getNumFrames() - 1), out, n);
        return;
    }

    // Grain mode: the stretch of audio under the grain window, wrapped into a ring.
    const int len = s->audio.getNumSamples();
    const float scan = (float) std::sin (t * juce::MathConstants<double>::twoPi * (0.15 + motionAmt * 1.5)) * motionAmt * 0.15f;
    const float pos = std::fmod (params.facet[position]->load() + scan + 1.0f, 1.0f);
    // a short window reads as a clear shape; longer grains show a little more of the sound
    const int span = juce::jlimit (n, juce::jmax (n, len), (int) (juce::jmin (0.012f + fmt::grainSeconds (params.facet[grain]->load()) * 0.05f, 0.04f) * s->sampleRate));
    const int start = juce::jlimit (0, juce::jmax (0, len - span), (int) (pos * (float) len) - span / 2);
    const float* d = s->audio.getReadPointer (0);
    float peak = 1.0e-6f;
    for (int i = 0; i < n; ++i)
    {
        const int idx = start + (int) ((juce::int64) i * span / n);
        out[(size_t) i] = d[juce::jlimit (0, len - 1, idx)];
        peak = juce::jmax (peak, std::abs (out[(size_t) i]));
    }
    for (auto& x : out) x /= peak;
}

void InstrumentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    rack.prepare (sampleRate, samplesPerBlock);
    levelSmooth.reset (sampleRate, 0.03);
    levelSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.level->load()));
}

bool InstrumentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void InstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, n);

    const int numCh = buffer.getNumChannels();
    levelSmooth.setTargetValue (juce::Decibels::decibelsToGain (params.level->load()));

    // Drive and Tone run per note inside each voice; then the effects rack (its reverb is the Space facet).
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
            if (auto q = pos->getPpqPosition()) { ppq = *q; playing = pos->getIsPlaying(); }
        }
    if (numCh == 2)
        rack.process (buffer, bpm, ppq, playing, params.facet[space]->load());

    // Level, then a transparent safety clipper: untouched below 0.8, rounds off smoothly above
    // so stacked chords or hot samples never hard-clip the DAW's input.
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

juce::AudioProcessorEditor* InstrumentProcessor::createEditor()
{
    return new InstrumentEditor (*this);
}

void InstrumentProcessor::setMode (Mode m)
{
    setParam ("mode", (float) m / 2.0f);
}

bool InstrumentProcessor::isModuleLocked (const juce::String& id) const
{
    const juce::ScopedLock sl (lockLock);
    return lockedModules.contains (id);
}

void InstrumentProcessor::setModuleLocked (const juce::String& id, bool locked)
{
    {
        const juce::ScopedLock sl (lockLock);
        if (locked) lockedModules.addIfNotAlreadyThere (id); else lockedModules.removeString (id);
    }
    sendChangeMessage();
}

std::vector<juce::RangedAudioParameter*> InstrumentProcessor::getRandomisableExtras() const
{
    // Effects that are switched on and not locked. The Space facet is already a facet, so it's skipped here.
    std::vector<juce::RangedAudioParameter*> out;
    for (const auto& m : FxRack::modules())
    {
        if (apvts.getRawParameterValue (m.onParam)->load() < 0.5f || isModuleLocked (m.id))
            continue;
        for (const auto& id : m.params)
            if (id != "space")
                out.push_back (apvts.getParameter (id));
    }
    return out;
}

void InstrumentProcessor::sparkEffects()
{
    const auto seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
    const auto extras = Lineage::rollExtras (currentExtraValues(), seed, 1.0f, chaosParam().getValue());
    lineage.push (currentFacetValues(), seed, extras);
    applyExtraValues (extras);
    sendChangeMessage();
}

void InstrumentProcessor::loadChain (int index)
{
    const auto& chains = FxRack::chains();
    if (! juce::isPositiveAndBelow (index, (int) chains.size()))
        return;
    const auto& chain = chains[(size_t) index];
    for (const auto& m : FxRack::modules())
    {
        juce::StringArray ids (m.params);
        ids.add (m.onParam);
        for (const auto& id : ids)
        {
            if (id == "space") continue;
            auto* prm = apvts.getParameter (id);
            const auto it = chain.values.find (id);
            setParam (id, it != chain.values.end() ? prm->convertTo0to1 (it->second) : prm->getDefaultValue());
        }
    }
    chainName = chain.name;
    sendChangeMessage();
}

void InstrumentProcessor::writeExtraState (juce::ValueTree& extra)
{
    {
        const juce::ScopedLock sl (lockLock);
        extra.setProperty ("fxLocks", lockedModules.joinIntoString (","), nullptr);
    }
    extra.setProperty ("chain", chainName, nullptr);

    auto s = getSource();
    if (s == nullptr || s->name == "Spark Init")
        return;

    extra.setProperty ("sourceName", s->name, nullptr);
    extra.setProperty ("sourceFile", s->file.getFullPathName(), nullptr);
    extra.setProperty ("tableFrame", s->tableFrameLength, nullptr);
    extra.setProperty ("rootNote", (double) s->rootNote, nullptr);
    extra.setProperty ("shapeshift", s->shapeshifted, nullptr);
    // The sound itself travels with the project (lossless FLAC), so sessions open on any computer.
    extra.setProperty ("sourceAudio", s->getEmbeddedAudio(), nullptr);
}

void InstrumentProcessor::readExtraState (const juce::ValueTree& extra)
{
    {
        const juce::ScopedLock sl (lockLock);
        lockedModules = juce::StringArray::fromTokens (extra.getProperty ("fxLocks").toString(), ",", "");
        lockedModules.removeEmptyStrings();
    }
    chainName = extra.getProperty ("chain", "Clean").toString();

    const auto embedded = extra.getProperty ("sourceAudio").toString();
    const juce::File file (extra.getProperty ("sourceFile").toString());

    if (embedded.isNotEmpty())
    {
        juce::MemoryBlock block;
        if (block.fromBase64Encoding (embedded))
        {
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::AudioFormatReader> reader (
                flac.createReaderFor (new juce::MemoryInputStream (block, false), true));
            if (reader != nullptr && reader->lengthInSamples > 0)
            {
                juce::AudioBuffer<float> audio ((int) juce::jlimit (1u, 2u, reader->numChannels), (int) reader->lengthInSamples);
                reader->read (&audio, 0, audio.getNumSamples(), 0, true, audio.getNumChannels() > 1);
                auto s = makeSource (std::move (audio), reader->sampleRate, extra.getProperty ("sourceName").toString(),
                                     file, (int) extra.getProperty ("tableFrame", 0));
                s->rootNote = (float) (double) extra.getProperty ("rootNote", 60.0);
                if ((bool) extra.getProperty ("shapeshift", false))
                {
                    // Shapeshift is deterministic, so the same frames come back from the saved audio
                    const auto result = shapeshift::analyse (s->audio, s->sampleRate);
                    if (result.ok)
                    {
                        s->table = Wavetable::fromFrames (result.frames);
                        s->shapeshifted = true;
                    }
                }
                installSource (s);
                return;
            }
        }
    }

    // Older projects only stored the file path
    if (file.existsAsFile())
    {
        juce::String error;
        loadFile (file, error); // the saved Engine choice is restored with the parameters afterwards
    }
}
} // namespace spark

#if ! SPARK_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new spark::InstrumentProcessor();
}
#endif

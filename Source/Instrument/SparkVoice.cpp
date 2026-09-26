#include "SparkVoice.h"

namespace spark
{
namespace
{
    constexpr int chunkSize = 512;

    struct HannTable
    {
        static constexpr int size = 1024;
        float data[size + 1];
        HannTable()
        {
            for (int i = 0; i <= size; ++i)
                data[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        }
        float at (float x) const noexcept // x in 0..1
        {
            const float p = juce::jlimit (0.0f, 1.0f, x) * size;
            const int i = juce::jmin ((int) p, size - 1);
            return data[i] + (data[i + 1] - data[i]) * (p - (float) i);
        }
    };

    const HannTable& hann()
    {
        static HannTable t;
        return t;
    }

    inline float readInterp (const float* d, int len, double pos) noexcept
    {
        const int i0 = (int) pos;
        const float frac = (float) (pos - i0);
        const float a = d[i0 % len];
        const float b = d[(i0 + 1) % len];
        return a + (b - a) * frac;
    }
}

SparkVoice::SparkVoice (InstrumentProcessor& p) : processor (p)
{
    scratch.setSize (2, chunkSize);
}

void SparkVoice::setCurrentPlaybackSampleRate (double newRate)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    if (newRate > 0)
    {
        ampEnv.setSampleRate (newRate);
        toneEnv.setSampleRate (newRate);
        filter.reset();
    }
}

void SparkVoice::startNote (int midiNote, float vel, juce::SynthesiserSound*, int pitchWheel)
{
    source = processor.getSource();
    note = targetNote = midiNote;
    velocity = vel;
    pitchWheelMoved (pitchWheel);

    // Mono/Legato hand us the previous key to glide from
    pitchNote = (float) midiNote;
    glideLeft = 0;
    if (processor.glideFromNote >= 0 && processor.glideFromNote != midiNote)
    {
        pitchNote = (float) processor.glideFromNote;
        startGlide();
    }

    for (auto& g : grains) g.active = false;
    samplesToNextGrain = 0.0;
    samplePos = 0.0;
    noteSamples = 0.0;
    // Unison oscillators start together so every note has the same level and punch;
    // the detune then drifts them apart naturally.
    const double startPhase = random.nextDouble();
    for (auto& ph : phases) ph = startPhase;
    lfoPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;
    for (int l = 0; l < mod::numLfos; ++l)
    {
        lfoVoicePhase[l] = 0.0;
        lfoHeld[l] = random.nextFloat() * 2.0f - 1.0f;
        lfoNext[l] = random.nextFloat() * 2.0f - 1.0f;
    }
    volumeNow = -1.0f;   // set on the first chunk
    subPhase = 0.0;
    noiseLp[0] = noiseLp[1] = 0.0f;

    filter.reset();
    driveAmount = processor.params.facet[InstrumentProcessor::drive]->load();
    baseCutoff = fmt::cutoffHz (processor.params.facet[InstrumentProcessor::tone]->load());
    ampEnv.reset();
    toneEnv.reset();
    ampEnv.noteOn();
    toneEnv.noteOn();
}

void SparkVoice::startGlide()
{
    const float seconds = fmt::glideSeconds (processor.params.glide->load());
    glideLeft = (int) (seconds * getSampleRate());
    if (glideLeft <= 0)
    {
        pitchNote = (float) targetNote;
        glideLeft = 0;
        return;
    }
    glideStep = ((float) targetNote - pitchNote) / (float) glideLeft;
}

void SparkVoice::changeNote (int midiNote, float vel, bool retrigger)
{
    targetNote = midiNote;
    startGlide();
    if (retrigger)
    {
        velocity = vel;
        samplePos = 0.0;
        noteSamples = 0.0;
        ampEnv.noteOn();   // restarts the attack from the current level, so no click
        toneEnv.noteOn();
    }
}

void SparkVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        toneEnv.noteOff();
    }
    else
    {
        ampEnv.reset();
        toneEnv.reset();
        clearCurrentNote();
    }
}

// Per note: drive -> filter (swept by the tone envelope) -> amp envelope.
void SparkVoice::processChain (float* l, float* r, int n, float toneNorm, float driveNorm, float resNorm, float volStart, float volEnd)
{
    auto& p = processor.params;
    const auto ampSettings = p.amp.settings();
    const auto toneSettings = p.toneEnv.settings();

    const float velAmp = p.ampVelocity->load();
    const float ampGain = 1.0f - velAmp * (1.0f - velocity);
    const float velTone = p.toneVelocity->load();
    const float octaves = p.toneAmount->load() * 5.0f * (1.0f - velTone * (1.0f - velocity));

    const float targetDrive = driveNorm;
    const float keyTrack = p.keyTrack->load();
    const float targetCutoff = fmt::cutoffHz (toneNorm) * std::exp2 ((pitchNote - 60.0f) / 12.0f * keyTrack);
    const float nyquistSafe = (float) getSampleRate() * 0.45f;
    const float q = fmt::filterQ (resNorm);
    const float volStep = (volEnd - volStart) / (float) juce::jmax (1, n);
    const int type = juce::roundToInt (p.filterType->load());
    const double sr = getSampleRate();

    for (int i = 0; i < n; ++i)
    {
        // gentle per-sample smoothing of the facet values
        driveAmount += (targetDrive - driveAmount) * 0.002f;
        baseCutoff += (targetCutoff - baseCutoff) * 0.002f;

        const float te = toneEnv.next (toneSettings);
        if ((i & 15) == 0)
        {
            const float hz = baseCutoff * std::exp2 (octaves * te);
            filter.set (juce::jlimit (20.0f, nyquistSafe, hz), q, sr);
        }

        const float g = 1.0f + driveAmount * 12.0f;
        const float makeup = 1.0f / std::sqrt (g);
        const float env = ampEnv.next (ampSettings) * ampGain * (volStart + volStep * (float) i);
        l[i] = filter.process (0, std::tanh (g * l[i]) * makeup, type) * env;
        r[i] = filter.process (1, std::tanh (g * r[i]) * makeup, type) * env;
    }
}

void SparkVoice::computeModulation (int blockOffset, int n, float (&offsets)[mod::numDests])
{
    const auto& ms = processor.modState;
    float src[mod::numSources] {};
    for (int l = 0; l < mod::numLfos; ++l)
    {
        if (! ms.usesLfo[l]) continue;
        if (ms.retrigger[l])
        {
            src[mod::lfo1 + l] = mod::shapeValue (ms.shape[l], (float) lfoVoicePhase[l], lfoHeld[l], lfoNext[l]);
            lfoVoicePhase[l] += ms.lfoInc[l] * n;
            if (lfoVoicePhase[l] >= 1.0)
            {
                lfoVoicePhase[l] -= std::floor (lfoVoicePhase[l]);
                lfoHeld[l] = lfoNext[l];
                lfoNext[l] = random.nextFloat() * 2.0f - 1.0f;
            }
        }
        else
        {
            double ph = ms.lfoPhase[l] + ms.lfoInc[l] * blockOffset;
            ph -= std::floor (ph);
            src[mod::lfo1 + l] = mod::shapeValue (ms.shape[l], (float) ph, ms.held[l], ms.next[l]);
        }
    }
    for (int m = 0; m < mod::numMacros; ++m) src[mod::macro1 + m] = ms.macro[m];
    src[mod::modWheel] = ms.modWheel;
    src[mod::aftertouch] = ms.aftertouch;
    src[mod::velocity] = velocity;
    ms.route (src, offsets);
}

void SparkVoice::addLayers (float* l, float* r, int n, double baseHz)
{
    auto& p = processor.params;
    const float sub = p.subLevel->load(), noise = p.noiseLevel->load();
    if (sub > 1.0e-4f)
    {
        const double inc = baseHz / (p.subOctave->load() > 0.5f ? 4.0 : 2.0) / getSampleRate();
        const float g = sub * 0.55f;
        for (int i = 0; i < n; ++i)
        {
            const float v = g * std::sin ((float) (subPhase * juce::MathConstants<double>::twoPi));
            l[i] += v;
            r[i] += v;
            subPhase += inc;
            if (subPhase >= 1.0) subPhase -= 1.0;
        }
    }
    if (noise > 1.0e-4f)
    {
        // colour: a one-pole low-pass from ~200 Hz (dark) up to fully white (bright), level-matched
        const float colour = p.noiseColour->load();
        const float hz = 200.0f * std::exp2 (colour * 7.5f);
        const float a = colour > 0.97f ? 1.0f : 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (hz, 20000.0f) / (float) getSampleRate());
        const float makeup = juce::jmin (6.0f, std::sqrt ((2.0f - a) / a));   // a one-pole passes a/(2-a) of white noise's power
        const float g = noise * 0.3f * makeup;
        for (int i = 0; i < n; ++i)
        {
            noiseLp[0] += a * ((random.nextFloat() * 2.0f - 1.0f) - noiseLp[0]);
            noiseLp[1] += a * ((random.nextFloat() * 2.0f - 1.0f) - noiseLp[1]);
            l[i] += noiseLp[0] * g;
            r[i] += noiseLp[1] * g;
        }
    }
}

void SparkVoice::pitchWheelMoved (int value)
{
    wheelValue = value;
}

void SparkVoice::spawnGrain (const SourceData& src, double ratio, float position, float grainSec, float motion, float scan)
{
    auto it = std::find_if (grains.begin(), grains.end(), [] (const Grain& g) { return ! g.active; });
    if (it == grains.end())
        return;

    const int len = src.audio.getNumSamples();
    const float spray = (random.nextFloat() - 0.5f) * motion * 0.3f;
    float start = std::fmod (position + scan + spray + 2.0f, 1.0f);

    auto& g = *it;
    g.active = true;
    g.pos = (double) start * (double) (len - 1);
    g.rate = ratio;
    g.age = 0;
    g.length = juce::jmax (64, (int) (grainSec * getSampleRate()));

    const float pan = (random.nextFloat() * 2.0f - 1.0f) * (0.2f + motion * 0.6f);
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g.gainL = std::cos (angle);
    g.gainR = std::sin (angle);
}

void SparkVoice::renderGrains (float* left, float* right, int n, const SourceData& src, double ratio,
                               float position, float grainSec, float motion, float scan)
{
    const int len = src.audio.getNumSamples();
    if (len < 2)
        return;

    const float* chL = src.audio.getReadPointer (0);
    const float* chR = src.audio.getReadPointer (src.audio.getNumChannels() > 1 ? 1 : 0);
    const double grainLength = juce::jmax (64.0, (double) grainSec * getSampleRate());
    const auto& window = hann();

    for (int i = 0; i < n; ++i)
    {
        if (samplesToNextGrain <= 0.0)
        {
            spawnGrain (src, ratio, position, grainSec, motion, scan);
            // four overlapping grains, with timing jitter from Motion
            samplesToNextGrain += grainLength * 0.25 * (1.0 + (random.nextDouble() - 0.5) * motion * 0.6);
        }
        samplesToNextGrain -= 1.0;

        float l = 0.0f, r = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active)
                continue;
            g.rate = ratio;   // follow glides and bends
            const float w = window.at ((float) g.age / (float) g.length);
            l += readInterp (chL, len, g.pos) * w * g.gainL;
            r += readInterp (chR, len, g.pos) * w * g.gainR;
            g.pos += g.rate;
            while (g.pos >= (double) len) g.pos -= (double) len;
            if (++g.age >= g.length)
                g.active = false;
        }
        left[i] = l * 0.6f;
        right[i] = r * 0.6f;
    }
}

void SparkVoice::renderSample (float* left, float* right, int n, const SourceData& src, double ratio)
{
    // Straight playback of the recording from the start, pitched from its root note.
    const int len = src.audio.getNumSamples();
    const float* chL = src.audio.getReadPointer (0);
    const float* chR = src.audio.getReadPointer (src.audio.getNumChannels() > 1 ? 1 : 0);
    for (int i = 0; i < n; ++i)
    {
        if (samplePos >= len - 1)
        {
            left[i] = right[i] = 0.0f;
            continue;
        }
        const int i0 = (int) samplePos;
        const float fr = (float) (samplePos - i0);
        left[i] = (chL[i0] + (chL[i0 + 1] - chL[i0]) * fr) * 0.8f;
        right[i] = (chR[i0] + (chR[i0 + 1] - chR[i0]) * fr) * 0.8f;
        samplePos += ratio;
    }
}

void SparkVoice::renderTable (float* left, float* right, int n, const Wavetable& table, double baseHz, float morph, float motion, float scanSeconds)
{
    const double sr = getSampleRate();
    // Unison as stereo width: the centre oscillator carries the sound (full and bright in mono),
    // the detuned pair only adds side signal, so nothing cancels when summed to mono.
    const float detuneCents = motion * 12.0f;
    const float sideAmount = juce::jmin (1.0f, motion * 2.5f) * 0.5f;
    const double ratios[3] = { 1.0, std::pow (2.0, detuneCents / 1200.0), std::pow (2.0, -detuneCents / 1200.0) };
    const float lastFrame = (float) (table.getNumFrames() - 1);
    const float lfoInc = (0.1f + motion * 0.9f) * juce::MathConstants<float>::twoPi / (float) sr;

    int mips[3];
    for (int k = 0; k < 3; ++k)
        mips[k] = Wavetable::mipForIncrement (baseHz * ratios[k] / sr);

    for (int i = 0; i < n; ++i)
    {
        // Morph scan sweeps from Morph to the last frame over scanSeconds (Shapeshift uses this to replay a sound's evolution)
        const float scanned = scanSeconds > 0.0f ? morph + (1.0f - morph) * (float) juce::jmin (1.0, noteSamples / (scanSeconds * sr)) : morph;
        noteSamples += 1.0;
        const float m = juce::jlimit (0.0f, 1.0f, scanned + std::sin (lfoPhase) * motion * (scanSeconds > 0.0f ? 0.05f : 0.35f));
        const float framePos = m * lastFrame;
        lfoPhase += lfoInc;
        if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

        float s[3];
        for (int k = 0; k < 3; ++k)
        {
            s[k] = table.sample (framePos, phases[k], mips[k]);
            phases[k] += baseHz * ratios[k] / sr;
            phases[k] -= std::floor (phases[k]);
        }
        const float side = (s[1] - s[2]) * sideAmount;
        left[i] = (s[0] + side) * 0.6f;
        right[i] = (s[0] - side) * 0.6f;
    }
}

void SparkVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive() || source == nullptr)
        return;

    auto& p = processor.params;

    const auto& ms = processor.modState;
    const bool modulated = ms.active();
    float base[mod::numDests];
    for (int d = 0; d < InstrumentProcessor::numFacetsInstrument; ++d)
        base[d] = p.facet[d]->load();
    base[mod::resonance] = p.resonance->load();
    base[mod::volume] = 1.0f;
    const int mode = juce::roundToInt (p.mode->load());
    const bool tableMode = mode == InstrumentProcessor::tableMode;
    const bool sampleMode = mode == InstrumentProcessor::sampleMode;
    const float scanSeconds = p.scanTime->load() > 0.001f ? fmt::envSeconds (p.scanTime->load()) * 2.0f : 0.0f;

    const float bendSemitones = (float) (wheelValue - 8192) / 8192.0f * p.bendRange->load();

    while (numSamples > 0)
    {
        // short chunks while gliding or modulating, so pitch and modulation move smoothly
        const int n = juce::jmin (numSamples, (glideLeft > 0 || modulated) ? 32 : chunkSize);
        float offs[mod::numDests] {};
        if (modulated)
            computeModulation (startSample, n, offs);
        auto value = [&] (int d) { return juce::jlimit (0.0f, 1.0f, base[d] + offs[d]); };
        const float pitchSt = fmt::semitoneValue (value (mod::pitch));
        const float position = value (mod::position);
        const float grainSec = fmt::grainSeconds (value (mod::grain));
        const float morph = value (mod::morph);
        const float motion = value (mod::motion);
        const float volume = juce::jmax (0.0f, 1.0f + offs[mod::volume]);
        if (volumeNow < 0.0f) volumeNow = volume;
        const float semis = pitchNote - source->rootNote + pitchSt + bendSemitones;
        const double ratio = std::pow (2.0, semis / 12.0) * (source->sampleRate / getSampleRate());
        const double baseHz = 440.0 * std::pow (2.0, ((double) pitchNote - 69.0 + pitchSt + bendSemitones) / 12.0);
        float* l = scratch.getWritePointer (0);
        float* r = scratch.getWritePointer (1);

        if (sampleMode)
        {
            renderSample (l, r, n, *source, ratio);
        }
        else if (tableMode && source->table != nullptr)
        {
            renderTable (l, r, n, *source->table, baseHz, morph, motion, scanSeconds);
        }
        else
        {
            // Motion also scans the grain position slowly through the sound
            const float scan = std::sin (lfoPhase) * motion * 0.15f;
            lfoPhase += (0.15f + motion * 1.5f) * juce::MathConstants<float>::twoPi * (float) n / (float) getSampleRate();
            if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;
            renderGrains (l, r, n, *source, ratio, position, grainSec, motion, scan);
        }

        addLayers (l, r, n, baseHz);
        processChain (l, r, n, value (mod::tone), value (mod::drive), value (mod::resonance), volumeNow, volume);
        volumeNow = volume;

        if (glideLeft > 0)
        {
            const int step = juce::jmin (n, glideLeft);
            pitchNote += glideStep * (float) step;
            glideLeft -= step;
            if (glideLeft <= 0) pitchNote = (float) targetNote;
        }

        out.addFrom (0, startSample, l, n);
        if (out.getNumChannels() > 1)
            out.addFrom (1, startSample, r, n);

        startSample += n;
        numSamples -= n;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
} // namespace spark

namespace spark
{
// ================================================================================ SparkSynth
SparkSynth::SparkSynth (InstrumentProcessor& p) : processor (p)
{
    held.reserve (128);
    heldVelocity.reserve (128);
}

SparkVoice* SparkSynth::findMonoVoice() const
{
    for (auto* v : voices)
        if (v->isVoiceActive() && v->getCurrentlyPlayingNote() == soundingNote && v->isKeyDown())
            return dynamic_cast<SparkVoice*> (v);
    return nullptr;
}

void SparkSynth::noteOn (int channel, int midiNote, float velocity)
{
    const int mode = juce::roundToInt (processor.params.voiceMode->load());
    if (mode != lastMode)
    {
        held.clear();
        heldVelocity.clear();
        lastMode = mode;
    }

    if (mode == InstrumentProcessor::poly)
    {
        processor.glideFromNote = -1;
        Synthesiser::noteOn (channel, midiNote, velocity);
        lastNote = midiNote;
        return;
    }

    for (size_t i = 0; i < held.size(); ++i)
        if (held[i] == midiNote) { held.erase (held.begin() + (long) i); heldVelocity.erase (heldVelocity.begin() + (long) i); break; }
    const bool overlapping = ! held.empty();
    held.push_back (midiNote);
    heldVelocity.push_back (velocity);

    if (overlapping)
        if (auto* v = findMonoVoice())
        {
            v->changeNote (midiNote, velocity, mode == InstrumentProcessor::mono);
            lastNote = midiNote;
            return;
        }

    // A fresh note. Mono glides from the last key even when detached; Legato only glides between overlapping keys.
    processor.glideFromNote = (mode == InstrumentProcessor::mono && lastNote >= 0) ? lastNote : -1;
    // Only one note sounds in the mono modes: let any other held voice go.
    for (auto* v : voices)
        if (v->isVoiceActive() && v->isKeyDown())
            stopVoice (v, 0.0f, true);
    Synthesiser::noteOn (channel, midiNote, velocity);
    processor.glideFromNote = -1;
    soundingNote = midiNote;
    lastNote = midiNote;
}

void SparkSynth::noteOff (int channel, int midiNote, float velocity, bool allowTailOff)
{
    const int mode = juce::roundToInt (processor.params.voiceMode->load());
    if (mode == InstrumentProcessor::poly || mode != lastMode)
    {
        Synthesiser::noteOff (channel, midiNote, velocity, allowTailOff);
        return;
    }

    for (size_t i = 0; i < held.size(); ++i)
        if (held[i] == midiNote) { held.erase (held.begin() + (long) i); heldVelocity.erase (heldVelocity.begin() + (long) i); break; }

    auto* v = findMonoVoice();
    if (held.empty())
    {
        Synthesiser::noteOff (channel, soundingNote, velocity, allowTailOff);
        return;
    }
    // Still holding other keys: go back to the most recent one if we released the note that was playing
    if (v != nullptr && v->getTargetNote() == midiNote)
    {
        v->changeNote (held.back(), heldVelocity.back(), mode == InstrumentProcessor::mono);
        lastNote = held.back();
    }
}

void SparkSynth::allNotesOff (int channel, bool allowTailOff)
{
    held.clear();
    heldVelocity.clear();
    Synthesiser::allNotesOff (channel, allowTailOff);
}
} // namespace spark

#include "LeadVoice.h"

namespace spark
{
namespace
{
    constexpr int chunk = 32;   // control rate: pitch, vibrato and unison spread update every 32 samples
}

LeadVoice::LeadVoice (LeadProcessor& p) : processor (p)
{
    scratch.setSize (2, 512);
}

void LeadVoice::setCurrentPlaybackSampleRate (double sr)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate (sr);
    if (sr > 0)
    {
        ampEnv.setSampleRate (sr);
        fltEnv.setSampleRate (sr);
        filter.reset();
    }
}

void LeadVoice::startNote (int midiNote, float vel, juce::SynthesiserSound*, int pitchWheel)
{
    targetNote = midiNote;
    velocity = vel;
    wheelValue = pitchWheel;
    pitchNote = (float) midiNote;
    glideLeft = 0;
    if (processor.glideFromNote >= 0 && processor.glideFromNote != midiNote)
    {
        pitchNote = (float) processor.glideFromNote;
        startGlide();
    }

    // unison copies start at random points so the stack sounds wide; the centre one starts at zero for punch
    for (int u = 0; u < maxUnison; ++u)
        phaseA[u] = u == maxUnison / 2 ? 0.0 : random.nextDouble();
    phaseB = subPhase = 0.0;
    vibPhase = 0.0;
    sinceAttack = 0.0;
    sinceRelease = -1.0;
    scoopNow = -processor.params.scoop->load() * 2.0f;
    noiseLp = 0.0f;

    filter.reset();
    driveNow = processor.params.facet[LeadProcessor::drive]->load();
    cutoffNow = leadfmt::cutoffHz (processor.params.facet[LeadProcessor::tone]->load());
    ampEnv.reset();
    fltEnv.reset();
    ampEnv.noteOn();
    fltEnv.noteOn();
}

void LeadVoice::startGlide()
{
    const float seconds = leadfmt::glideSeconds (processor.params.facet[LeadProcessor::glide]->load());
    glideLeft = (int) (seconds * getSampleRate());
    if (glideLeft <= 0)
    {
        pitchNote = (float) targetNote;
        glideLeft = 0;
        return;
    }
    glideStep = ((float) targetNote - pitchNote) / (float) glideLeft;
}

void LeadVoice::changeNote (int midiNote, float vel, bool retrigger)
{
    targetNote = midiNote;
    startGlide();
    sinceAttack = 0.0;   // vibrato waits again on the new note, like a singer
    sinceRelease = -1.0;
    if (retrigger)
    {
        velocity = vel;
        scoopNow = -processor.params.scoop->load() * 2.0f;
        ampEnv.noteOn();
        fltEnv.noteOn();
    }
}

void LeadVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        fltEnv.noteOff();
        sinceRelease = 0.0;
    }
    else
    {
        ampEnv.reset();
        fltEnv.reset();
        clearCurrentNote();
    }
}

void LeadVoice::renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;
    if (scratch.getNumSamples() < numSamples)
        scratch.setSize (2, numSamples, false, false, true);   // only if the host sends a bigger block than promised

    auto* l = scratch.getWritePointer (0);
    auto* r = scratch.getWritePointer (1);
    for (int done = 0; done < numSamples;)
    {
        const int n = juce::jmin (chunk, numSamples - done);
        render (l + done, r + done, n);
        done += n;
    }
    if (out.getNumChannels() > 1)
    {
        out.addFrom (0, startSample, l, numSamples);
        out.addFrom (1, startSample, r, numSamples);
    }
    else
    {
        out.addFrom (0, startSample, l, numSamples, 0.5f);
        out.addFrom (0, startSample, r, numSamples, 0.5f);
    }
    if (! ampEnv.isActive())
        clearCurrentNote();
}

void LeadVoice::render (float* l, float* r, int n)
{
    auto& p = processor.params;
    const double sr = getSampleRate();
    const float dt = (float) n / (float) sr;
    const auto& table = LeadProcessor::waveTable();
    const float lastFrame = (float) (table.getNumFrames() - 1);

    // ---- pitch: glide, bend, scoop, vibrato, fall-off
    const float bend = ((float) wheelValue - 8192.0f) / 8192.0f * p.bendRange->load();
    const float vibDepth = leadfmt::vibratoSemis (p.facet[LeadProcessor::vibrato]->load()) + processor.modWheel * 0.5f;
    const float vibWait = p.vibDelay->load();
    const float vibIn = juce::jlimit (0.0f, 1.0f, (float) (sinceAttack - vibWait) / 0.35f);
    const float vib = (float) std::sin (vibPhase * juce::MathConstants<double>::twoPi) * vibDepth * vibIn;
    vibPhase += p.vibRate->load() * dt;
    vibPhase -= std::floor (vibPhase);
    float fallNow = 0.0f;
    if (sinceRelease >= 0.0 && p.fall->load() > 0.001f)
    {
        const float len = juce::jmax (0.08f, juce::jmin (0.6f, processor.ampSettings().release));
        const float t = juce::jmin (1.0f, (float) sinceRelease / len);
        fallNow = -p.fall->load() * 12.0f * t * t;
        sinceRelease += dt;
    }
    const float scoopTau = 0.025f + std::abs (scoopNow) * 0.025f;
    scoopNow *= std::exp (-dt / scoopTau);

    const float pitch = pitchNote + bend + vib + scoopNow + fallNow;
    const double baseHz = 440.0 * std::exp2 ((pitch - 69.0f) / 12.0f);
    if (glideLeft > 0)
    {
        const int step = juce::jmin (n, glideLeft);
        pitchNote += glideStep * (float) step;
        glideLeft -= step;
        if (glideLeft <= 0) pitchNote = (float) targetNote;
    }
    sinceAttack += dt;

    // ---- oscillator A: unison stack
    const float detune = p.facet[LeadProcessor::detune]->load();
    const int voices = juce::jlimit (1, maxUnison, juce::roundToInt (p.unison->load()));
    const float spread = std::pow (detune, 1.5f) * 0.5f;   // semitones at the outer copies
    const float width = p.width->load();
    const float framesA = p.facet[LeadProcessor::wave]->load() * lastFrame;
    double incA[maxUnison];
    int mipA[maxUnison];
    float gainL[maxUnison], gainR[maxUnison];
    const float norm = 1.0f / std::sqrt ((float) voices);
    for (int u = 0; u < voices; ++u)
    {
        const float pos = voices == 1 ? 0.0f : -1.0f + 2.0f * (float) u / (float) (voices - 1);
        // slightly uneven spacing keeps the beating from lining up
        const float off = pos * spread * (1.0f - 0.08f * (float) (u % 2));
        incA[u] = baseHz * std::exp2 (off / 12.0f) / sr;
        mipA[u] = Wavetable::mipForIncrement (incA[u]);
        const float pan = pos * width;
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL[u] = std::cos (angle) * norm * juce::MathConstants<float>::sqrt2;
        gainR[u] = std::sin (angle) * norm * juce::MathConstants<float>::sqrt2;
    }
    // map each copy onto the phase slot of the same index (centre copy is slot 3)
    const int firstSlot = maxUnison / 2 - voices / 2;

    // ---- oscillator B, sub, noise
    const float levelB = p.oscBLevel->load();
    const double incB = baseHz * std::exp2 ((p.oscBSemi->load() + p.oscBFine->load() / 100.0f + detune * 0.12f) / 12.0f) / sr;
    const int mipB = Wavetable::mipForIncrement (incB);
    const float framesB = p.oscBWave->load() * lastFrame;
    const float sub = p.subLevel->load() * 0.8f;
    const double incSub = baseHz * 0.5 / sr;
    const float noise = p.noiseLevel->load() * 0.25f;

    for (int i = 0; i < n; ++i)
    {
        float sl = 0.0f, sr_ = 0.0f;
        for (int u = 0; u < voices; ++u)
        {
            auto& ph = phaseA[firstSlot + u];
            const float s = table.sample (framesA, ph, mipA[u]);
            ph += incA[u];
            if (ph >= 1.0) ph -= 1.0;
            sl += s * gainL[u];
            sr_ += s * gainR[u];
        }
        float mono = 0.0f;
        if (levelB > 0.001f)
        {
            mono += table.sample (framesB, phaseB, mipB) * levelB;
            phaseB += incB;
            if (phaseB >= 1.0) phaseB -= 1.0;
        }
        if (sub > 0.001f)
        {
            mono += (float) std::sin (subPhase * juce::MathConstants<double>::twoPi) * sub;
            subPhase += incSub;
            if (subPhase >= 1.0) subPhase -= 1.0;
        }
        if (noise > 0.001f)
        {
            // breath: noise with a gentle high-pass tilt
            const float w = random.nextFloat() * 2.0f - 1.0f;
            noiseLp += 0.2f * (w - noiseLp);
            mono += (w - noiseLp) * noise;
        }
        l[i] = sl + mono;
        r[i] = sr_ + mono;
    }

    // ---- drive, filter, amp
    const auto ampS = processor.ampSettings();
    const auto fltS = processor.filterSettings();
    const float bite = p.facet[LeadProcessor::bite]->load();
    const float velTone = p.velTone->load();
    const float keyTrack = p.keyTrack->load();
    const float targetCutoff = leadfmt::cutoffHz (p.facet[LeadProcessor::tone]->load())
                               * std::exp2 ((pitchNote - 60.0f) / 12.0f * keyTrack)
                               * std::exp2 (velTone * 2.0f * (velocity - 1.0f))
                               * std::exp2 (processor.pressure * 1.5f);
    const float envOctaves = bite * 5.0f;
    const float q = fmt::filterQ (juce::jlimit (0.0f, 1.0f, p.resonance->load() + bite * 0.25f));
    const int type = juce::roundToInt (p.filterType->load());
    const float nyquistSafe = (float) sr * 0.45f;
    const float targetDrive = p.facet[LeadProcessor::drive]->load();
    const float ampGain = 1.0f - p.ampVel->load() * (1.0f - velocity);

    for (int i = 0; i < n; ++i)
    {
        driveNow += (targetDrive - driveNow) * 0.002f;
        cutoffNow += (targetCutoff - cutoffNow) * 0.004f;
        const float fe = fltEnv.next (fltS);
        if ((i & 15) == 0)
            filter.set (juce::jlimit (20.0f, nyquistSafe, cutoffNow * std::exp2 (envOctaves * fe)), q, sr);
        const float g = 1.0f + driveNow * 10.0f;
        const float makeup = 1.0f / std::sqrt (g);
        const float env = ampEnv.next (ampS) * ampGain;
        l[i] = filter.process (0, std::tanh (g * l[i]) * makeup, type) * env;
        r[i] = filter.process (1, std::tanh (g * r[i]) * makeup, type) * env;
    }
}

// =====================================================================================
LeadSynth::LeadSynth (LeadProcessor& p) : processor (p)
{
    held.reserve (128);
    heldVelocity.reserve (128);
}

LeadVoice* LeadSynth::findMonoVoice() const
{
    for (auto* v : voices)
        if (v->isVoiceActive() && v->getCurrentlyPlayingNote() == soundingNote && v->isKeyDown())
            return dynamic_cast<LeadVoice*> (v);
    return nullptr;
}

void LeadSynth::noteOn (int channel, int midiNote, float velocity)
{
    const int mode = juce::roundToInt (processor.params.voiceMode->load());
    if (mode != lastMode)
    {
        held.clear();
        heldVelocity.clear();
        lastMode = mode;
    }
    if (mode == LeadProcessor::poly)
    {
        processor.glideFromNote = -1;
        Synthesiser::noteOn (channel, midiNote, velocity);
        lastNote = midiNote;
        return;
    }

    if (const auto it = std::find (held.begin(), held.end(), midiNote); it != held.end())
    {
        heldVelocity.erase (heldVelocity.begin() + (it - held.begin()));
        held.erase (it);
    }
    const bool overlapping = ! held.empty();
    held.push_back (midiNote);
    heldVelocity.push_back (velocity);

    if (overlapping)
        if (auto* v = findMonoVoice())
        {
            v->changeNote (midiNote, velocity, mode == LeadProcessor::mono);
            lastNote = midiNote;
            return;
        }

    processor.glideFromNote = (mode == LeadProcessor::mono && lastNote >= 0) ? lastNote : -1;
    for (auto* v : voices)
        if (v->isVoiceActive() && v->isKeyDown())
            stopVoice (v, 0.0f, true);
    Synthesiser::noteOn (channel, midiNote, velocity);
    processor.glideFromNote = -1;
    soundingNote = midiNote;
    lastNote = midiNote;
}

void LeadSynth::noteOff (int channel, int midiNote, float velocity, bool allowTailOff)
{
    const int mode = juce::roundToInt (processor.params.voiceMode->load());
    if (mode == LeadProcessor::poly || mode != lastMode)
    {
        Synthesiser::noteOff (channel, midiNote, velocity, allowTailOff);
        return;
    }
    const auto it = std::find (held.begin(), held.end(), midiNote);
    if (it != held.end())
    {
        heldVelocity.erase (heldVelocity.begin() + (it - held.begin()));
        held.erase (it);
    }
    auto* v = findMonoVoice();
    if (held.empty())
    {
        Synthesiser::noteOff (channel, soundingNote, velocity, allowTailOff);
        return;
    }
    if (v != nullptr && v->getTargetNote() == midiNote)
    {
        v->changeNote (held.back(), heldVelocity.back(), mode == LeadProcessor::mono);
        lastNote = held.back();
    }
}

void LeadSynth::allNotesOff (int channel, bool allowTailOff)
{
    held.clear();
    heldVelocity.clear();
    Synthesiser::allNotesOff (channel, allowTailOff);
}
} // namespace spark

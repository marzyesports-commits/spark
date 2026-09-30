#include "SourceData.h"

namespace spark
{
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

SourceData::Ptr SourceData::make (juce::AudioBuffer<float> audio, double sampleRate, const juce::String& name,
                                  const juce::File& file, int tableFrameLength)
{
    Ptr s (new SourceData());
    s->audio = std::move (audio);
    s->sampleRate = sampleRate;
    s->name = name;
    s->file = file;
    s->tableFrameLength = tableFrameLength;
    s->loadedAsWavetable = tableFrameLength > 0;
    s->table = tableFrameLength > 0 ? Wavetable::fromTableAudio (s->audio, tableFrameLength)
                                    : Wavetable::fromSample (s->audio);
    s->computePeaks();
    s->computeGains();
    return s;
}

void SourceData::computeGains()
{
    // table: the average RMS of its frames; audio: the RMS of its loudest half-second
    if (table != nullptr)
    {
        double sum = 0.0;
        int count = 0;
        for (int f = 0; f < table->getNumFrames(); f += juce::jmax (1, table->getNumFrames() / 16))
        {
            const auto& fr = table->rawFrame (f);
            double p = 0.0;
            for (auto v : fr) p += (double) v * v;
            sum += std::sqrt (p / (double) juce::jmax ((size_t) 1, fr.size()));
            ++count;
        }
        const float rms = (float) (sum / juce::jmax (1, count));
        tableGain = rms > 1.0e-4f ? juce::jlimit (0.1f, 8.0f, 0.5f / rms) : 1.0f;
    }
    const int len = audio.getNumSamples();
    const int win = juce::jmax (64, juce::jmin (len, (int) (sampleRate * 0.5)));
    float best = 0.0f;
    for (int s0 = 0; s0 + win <= len; s0 += win / 2)
    {
        double p = 0.0;
        for (int c = 0; c < audio.getNumChannels(); ++c)
        {
            const float* d = audio.getReadPointer (c, s0);
            for (int i = 0; i < win; ++i) p += (double) d[i] * d[i];
        }
        best = juce::jmax (best, (float) std::sqrt (p / (double) (win * audio.getNumChannels())));
    }
    audioGain = best > 1.0e-4f ? juce::jlimit (0.1f, 16.0f, 0.55f / best) : 1.0f;
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

} // namespace spark

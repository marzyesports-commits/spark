#include "FactorySounds.h"
#include "SparkSoundData.h"

namespace spark::factory
{
namespace
{
    const char* resourceFor (const juce::String& originalFile, int& size)
    {
        for (int i = 0; i < SparkSoundData::namedResourceListSize; ++i)
            if (originalFile == SparkSoundData::originalFilenames[i])
                return SparkSoundData::getNamedResource (SparkSoundData::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }
}

const std::vector<Sound>& sounds()
{
    static const std::vector<Sound> list = []
    {
        std::vector<Sound> out;
        int size = 0;
        if (auto* data = resourceFor ("manifest.json", size))
        {
            const auto json = juce::JSON::parse (juce::String::fromUTF8 (data, size));
            if (auto* arr = json.getProperty ("sounds", {}).getArray())
                for (const auto& v : *arr)
                    out.push_back ({ v["id"].toString(), v["name"].toString(), v["category"].toString(),
                                     (float) (double) v["root"], (int) v["table"] });
        }
        return out;
    }();
    return list;
}

juce::StringArray categories()
{
    juce::StringArray c;
    for (const auto& s : sounds()) c.addIfNotAlreadyThere (s.category);
    return c;
}

const Sound* find (const juce::String& id)
{
    for (const auto& s : sounds())
        if (s.id == id) return &s;
    return nullptr;
}

bool decode (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRate)
{
    int size = 0;
    auto* data = resourceFor (id + ".flac", size);
    if (data == nullptr)
        return false;
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;
    out.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&out, 0, out.getNumSamples(), 0, true, out.getNumChannels() > 1);
    sampleRate = reader->sampleRate;
    return true;
}
} // namespace spark::factory

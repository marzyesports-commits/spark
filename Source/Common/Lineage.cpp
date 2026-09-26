#include "Lineage.h"

namespace spark
{
const LineageNode* Lineage::currentNode() const noexcept
{
    return juce::isPositiveAndBelow (current, (int) list.size()) ? &list[(size_t) current] : nullptr;
}

const LineageNode* Lineage::previousNode() const noexcept
{
    return juce::isPositiveAndBelow (current - 1, (int) list.size()) ? &list[(size_t) current - 1] : nullptr;
}

const LineageNode& Lineage::push (const FacetValues& vals, juce::uint32 seed, std::map<juce::String, float> extras)
{
    LineageNode n;
    n.vals = vals;
    n.extras = std::move (extras);
    n.seed = seed;
    n.gen = nextGen++;
    list.push_back (n);

    // Trim the oldest un-kept variations first, then the oldest overall.
    while ((int) list.size() > maxNodes)
    {
        auto it = std::find_if (list.begin(), list.end() - 1, [] (const LineageNode& x) { return ! x.kept; });
        list.erase (it != list.end() - 1 ? it : list.begin());
    }

    current = (int) list.size() - 1;
    return list.back();
}

void Lineage::setCurrent (int index)
{
    if (juce::isPositiveAndBelow (index, (int) list.size()))
        current = index;
}

void Lineage::toggleKeep (int index)
{
    if (juce::isPositiveAndBelow (index, (int) list.size()))
        list[(size_t) index].kept = ! list[(size_t) index].kept;
}

void Lineage::updateValues (int index, const FacetValues& vals, const std::map<juce::String, float>& extras)
{
    if (juce::isPositiveAndBelow (index, (int) list.size()))
    {
        list[(size_t) index].vals = vals;
        list[(size_t) index].extras = extras;
    }
}

std::map<juce::String, float> Lineage::rollExtras (const std::map<juce::String, float>& from, juce::uint32 seed, float mutate, float chaos)
{
    juce::Random r ((juce::int64) seed * 69069LL + 5);
    const float amount = 0.06f + 0.45f * chaos;
    const float probability = 0.25f + 0.6f * mutate;
    auto out = from;
    for (auto& [id, v] : out)
    {
        const float a = r.nextFloat(), b = r.nextFloat() * 2.0f - 1.0f;
        if (a < probability)
            v = juce::jlimit (0.0f, 1.0f, v + b * amount);
    }
    return out;
}

std::map<juce::String, float> Lineage::breedExtras (const std::map<juce::String, float>& a, const std::map<juce::String, float>& b, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 7919LL + 3);
    auto out = a;
    for (auto& [id, v] : out)
        if (auto it = b.find (id); it != b.end() && r.nextBool())
            v = it->second;
    return out;
}

void Lineage::clear()
{
    list.clear();
    current = -1;
    nextGen = 1;
}

FacetValues Lineage::roll (const FacetValues& from, juce::uint32 seed, float mutate, float chaos, const FacetLocks& locks)
{
    juce::Random r ((juce::int64) seed * 2654435761LL + 17);
    const float amount = 0.08f + 0.62f * chaos;
    const float probability = 0.3f + 0.7f * mutate;

    FacetValues out = from;
    bool anyMoved = false;
    for (int i = 0; i < numFacets; ++i)
    {
        const float a = r.nextFloat();
        const float b = r.nextFloat() * 2.0f - 1.0f;
        if (! locks[(size_t) i] && a < probability)
        {
            out[(size_t) i] = juce::jlimit (0.0f, 1.0f, from[(size_t) i] + b * amount);
            anyMoved = true;
        }
    }

    // Always move at least one free facet so every Spark is audibly new.
    if (! anyMoved)
    {
        std::vector<int> free;
        for (int i = 0; i < numFacets; ++i)
            if (! locks[(size_t) i]) free.push_back (i);
        if (! free.empty())
        {
            const int i = free[(size_t) r.nextInt ((int) free.size())];
            out[(size_t) i] = juce::jlimit (0.0f, 1.0f, from[(size_t) i] + (r.nextFloat() * 2.0f - 1.0f) * amount);
        }
    }
    return out;
}

FacetValues Lineage::breed (const FacetValues& a, const FacetValues& b, juce::uint32 seed, const FacetLocks& locks)
{
    juce::Random r ((juce::int64) seed * 40503LL + 99);
    FacetValues out = a;
    for (int i = 0; i < numFacets; ++i)
    {
        const bool fromA = r.nextBool();
        const float jitter = (r.nextFloat() * 2.0f - 1.0f) * 0.05f;
        if (! locks[(size_t) i])
            out[(size_t) i] = juce::jlimit (0.0f, 1.0f, (fromA ? a[(size_t) i] : b[(size_t) i]) + jitter);
    }
    return out;
}

const LineageNode* Lineage::breedPartner() const noexcept
{
    for (int i = (int) list.size() - 1; i >= 0; --i)
        if (list[(size_t) i].kept && i != current)
            return &list[(size_t) i];
    return previousNode();
}

juce::ValueTree Lineage::toValueTree() const
{
    juce::ValueTree t ("LINEAGE");
    t.setProperty ("current", current, nullptr);
    t.setProperty ("nextGen", nextGen, nullptr);
    for (const auto& n : list)
    {
        juce::ValueTree c ("NODE");
        juce::StringArray v;
        for (auto x : n.vals) v.add (juce::String (x, 5));
        c.setProperty ("vals", v.joinIntoString (","), nullptr);
        c.setProperty ("seed", (juce::int64) n.seed, nullptr);
        c.setProperty ("kept", n.kept, nullptr);
        c.setProperty ("gen", n.gen, nullptr);
        if (! n.extras.empty())
        {
            juce::StringArray e;
            for (const auto& [id, v] : n.extras) e.add (id + "=" + juce::String (v, 5));
            c.setProperty ("extras", e.joinIntoString (";"), nullptr);
        }
        t.appendChild (c, nullptr);
    }
    return t;
}

void Lineage::fromValueTree (const juce::ValueTree& t)
{
    if (! t.hasType ("LINEAGE"))
        return;

    std::vector<LineageNode> loaded;
    for (auto c : t)
    {
        LineageNode n;
        auto parts = juce::StringArray::fromTokens (c.getProperty ("vals").toString(), ",", "");
        for (int i = 0; i < numFacets && i < parts.size(); ++i)
            n.vals[(size_t) i] = juce::jlimit (0.0f, 1.0f, parts[i].getFloatValue());
        n.seed = (juce::uint32) (juce::int64) c.getProperty ("seed");
        n.kept = (bool) c.getProperty ("kept");
        n.gen = (int) c.getProperty ("gen", 1);
        for (const auto& kv : juce::StringArray::fromTokens (c.getProperty ("extras").toString(), ";", ""))
            if (kv.contains ("="))
                n.extras[kv.upToFirstOccurrenceOf ("=", false, false)] = juce::jlimit (0.0f, 1.0f, kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
        loaded.push_back (n);
    }
    if (loaded.empty())
        return;

    list = std::move (loaded);
    current = juce::jlimit (0, (int) list.size() - 1, (int) t.getProperty ("current", (int) list.size() - 1));
    nextGen = juce::jmax ((int) t.getProperty ("nextGen", 1), list.back().gen + 1);
}

std::vector<float> makeGlyph (const LineageNode& node, int n)
{
    juce::Random r ((juce::int64) node.seed + 7);
    const int k1 = 2 + r.nextInt (5), k2 = 5 + r.nextInt (9), k3 = 11 + r.nextInt (14);
    const float p1 = r.nextFloat() * 6.2832f, p2 = r.nextFloat() * 6.2832f, p3 = r.nextFloat() * 6.2832f;
    const float a2 = 0.2f + node.vals[2] * 0.7f, a3 = node.vals[3] * 0.6f, drive = 1.0f + node.vals[5] * 5.0f;

    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const float th = (float) i / (float) n * juce::MathConstants<float>::twoPi;
        out[(size_t) i] = std::tanh (drive * (std::sin (k1 * th + p1) + a2 * std::sin (k2 * th + p2) + a3 * std::sin (k3 * th + p3)) * 0.5f);
    }
    return out;
}
} // namespace spark

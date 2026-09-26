#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <vector>
#include <map>

namespace spark
{
static constexpr int numFacets = 8;
using FacetValues = std::array<float, numFacets>;
using FacetLocks  = std::array<bool, numFacets>;

// One variation in the lineage: the eight facet values plus the seed that produced it.
struct LineageNode
{
    FacetValues vals {};
    juce::uint32 seed = 0;
    bool kept = false;
    int gen = 1;
    std::map<juce::String, float> extras; // other randomised parameters (effects), normalised
};

// The randomiser's family tree. Message thread only.
class Lineage
{
public:
    static constexpr int maxNodes = 48;

    const std::vector<LineageNode>& nodes() const noexcept { return list; }
    int currentIndex() const noexcept { return current; }
    const LineageNode* currentNode() const noexcept;
    const LineageNode* previousNode() const noexcept;

    const LineageNode& push (const FacetValues& vals, juce::uint32 seed, std::map<juce::String, float> extras = {});
    void setCurrent (int index);
    void toggleKeep (int index);
    void updateValues (int index, const FacetValues& vals, const std::map<juce::String, float>& extras);
    void clear();

    // Mutate: probability each unlocked facet moves. Chaos: how far it moves.
    static FacetValues roll (const FacetValues& from, juce::uint32 seed, float mutate, float chaos, const FacetLocks& locks);
    // Crossover of two parents with a light mutation.
    static FacetValues breed (const FacetValues& a, const FacetValues& b, juce::uint32 seed, const FacetLocks& locks);
    // Same moves for the extra parameters (all unlocked).
    static std::map<juce::String, float> rollExtras (const std::map<juce::String, float>& from, juce::uint32 seed, float mutate, float chaos);
    static std::map<juce::String, float> breedExtras (const std::map<juce::String, float>& a, const std::map<juce::String, float>& b, juce::uint32 seed);

    // Picks the partner for Breed: the most recently kept variation that is not current, else the previous one.
    const LineageNode* breedPartner() const noexcept;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

private:
    std::vector<LineageNode> list;
    int current = -1;
    int nextGen = 1;
};

// Deterministic glyph for a variation: 'n' radii offsets in -1..1 around a ring.
std::vector<float> makeGlyph (const LineageNode&, int n);
} // namespace spark

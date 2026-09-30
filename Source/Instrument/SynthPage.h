#pragma once

#include "Common/Components.h"
#include "Common/Cards.h"
#include "InstrumentProcessor.h"
#include "ModCards.h"

namespace spark
{
class FilterCard : public SynthCard
{
public:
    FilterCard (InstrumentProcessor&, ModDropHandler);

private:
    ChoiceSegments type;
};

class LayersCard : public SynthCard
{
public:
    explicit LayersCard (InstrumentProcessor&);
};

class PlayCard : public SynthCard
{
public:
    explicit PlayCard (InstrumentProcessor&);

private:
    InstrumentProcessor& processor;
    ChoiceSegments mode;
};

// The SYNTH page: filter, voice mode, and modulation.
class SynthPage : public juce::Component
{
public:
    SynthPage (InstrumentProcessor&, ModDropHandler onModDrop);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FilterCard filter;
    LayersCard layers;
    PlayCard play;
    MacroCard macros;
    LfoCard lfo1, lfo2;
    MatrixCard matrix;
};
} // namespace spark

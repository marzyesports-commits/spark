#pragma once

#include "SparkProcessorBase.h"

namespace spark
{
// Factory preset libraries. Facet order:
//   Spark:    Pitch, Position, Grain, Morph, Tone, Drive, Motion, Space
// All values are normalised 0..1. Useful reference points:
//   Pitch     0.5 = original, 0.25 = -12 st, 0.75 = +12 st (1/48 per semitone)
//   Tone      0.3 = 160 Hz, 0.4 = 320 Hz, 0.5 = 630 Hz, 0.6 = 1.3 kHz, 0.7 = 2.5 kHz, 0.8 = 5 kHz, 0.9 = 10 kHz
//   Grain     0.1 = 55 ms, 0.2 = 104 ms, 0.4 = 203 ms, 0.6 = 302 ms, 1.0 = 500 ms
//   Env time  0.01 = 1.5 ms, 0.05 = 14 ms, 0.1 = 51 ms, 0.2 = 0.2 s, 0.3 = 0.45 s, 0.5 = 1.25 s, 0.7 = 2.45 s
PresetLibrary makeInstrumentPresets();
} // namespace spark

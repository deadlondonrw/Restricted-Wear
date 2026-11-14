#pragma once

#include "Wavetable.h"
#include <functional>

namespace dw::wavegen
{
using Frame = std::vector<float>;
using Frames = std::vector<Frame>;

/** Evaluates fn(phase 0..1) at 8x resolution and band-limits it to 1024 harmonics. */
Frame bandLimited (const std::function<double (double)>& fn, int oversample = 8);

/** Additive: magnitudes[h-1] / phases[h-1] (radians, sine phase) for harmonics 1..N. */
Frame fromHarmonics (const std::vector<float>& magnitudes, const std::vector<float>& phases = {});

Frame sine();
Frame saw();
Frame square();
Frame triangle();
Frame pulse (double width);

void normalise (Frame& f, float peak = 1.0f);
void removeDc (Frame& f);
/** Peak-normalises every frame by the single loudest frame (keeps relative dynamics). */
void normaliseTable (Frames& frames, float peak = 1.0f);

// ---------------------------------------------------------------------------------- factory tables
const juce::StringArray& factoryTableNames();
Frames factoryTable (int index);
int factoryTableIndex (const juce::String& name);

// ---------------------------------------------------------------------------------- random waves
enum class RandomStyle { Spectral, Harmonic, Segments, Stepped, Chaotic, NumStyles };
const juce::StringArray& randomStyleNames();
Frame randomCycle (RandomStyle style, uint32_t seed, float complexity /* 0..1 */);
/** Random multi-frame table: random keyframes morphed spectrally. */
Frames randomTable (RandomStyle style, uint32_t seed, float complexity, int numFrames);

// ---------------------------------------------------------------------------------- morphing
/** Builds in-between frames by spectral interpolation (magnitude + unwrapped phase). */
Frame spectralMorph (const Frame& a, const Frame& b, float t);
} // namespace dw::wavegen

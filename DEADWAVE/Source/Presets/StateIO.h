#pragma once

#include "../DSP/Oscillator.h"
#include "../DSP/Modulators.h"
#include "../Wave/WaveDocument.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace dw::stateio
{
juce::String encodeFloats (const float* data, size_t count);
std::vector<float> decodeFloats (const juce::String& base64);

juce::ValueTree sampleToTree (const SampleAsset& s);
std::unique_ptr<SampleAsset> sampleFromTree (const juce::ValueTree& v);

/** LFO custom shape: control points (x 0..1, y 0..1 unipolar, curve). */
using LfoPoints = std::vector<WaveDocument::ControlPoint>;
LfoPoints defaultLfoPoints();
std::unique_ptr<LfoShapeTable> renderLfo (const LfoPoints& pts);
juce::String lfoPointsToString (const LfoPoints& pts);
LfoPoints lfoPointsFromString (const juce::String& s);

/** Writes a wavetable as a 32-bit float WAV with a Serum/Vital-compatible "clm " chunk. */
bool writeWavetableWav (const juce::File& file, const wavegen::Frames& frames);
/** Reads a wavetable WAV (any frame size given by a clm chunk, otherwise 2048). */
bool readWavetableWav (const juce::File& file, wavegen::Frames& framesOut, juce::String& error);
} // namespace dw::stateio

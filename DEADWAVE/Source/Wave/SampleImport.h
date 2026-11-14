#pragma once

#include "../DSP/Oscillator.h"
#include "../DSP/WaveGen.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace dw::sampleimport
{
struct Audio
{
    juce::AudioBuffer<float> buffer; // 1 or 2 channels
    double rate = 44100.0;
    juce::String name;
    int length() const noexcept { return buffer.getNumSamples(); }
};

/** Loads WAV/AIFF/FLAC/OGG (and MP3 where the platform supports it), limited to maxSeconds. */
bool load (const juce::File& file, Audio& out, juce::String& error, double maxSeconds = 120.0);

/** Fundamental period (in samples) of a region using a YIN difference function. 0 if unpitched. */
float detectPeriod (const Audio& a, int start, int length);
/** MIDI note for a period. */
float periodToNote (float period, double rate);

/**
    Extracts single cycles across [start, end): numFrames cycles evenly spaced through the region,
    each aligned to an upward zero crossing and resampled to 2048 samples. period <= 0 = auto-detect.
*/
wavegen::Frames extractCycles (const Audio& a, int start, int end, int numFrames, float period);

/** Splits [start, end) into numFrames equal windows, each resampled to one 2048-sample frame. */
wavegen::Frames sliceFixed (const Audio& a, int start, int end, int numFrames);

/** Builds a playable sample asset (for the oscillator SAMPLE mode). */
std::unique_ptr<SampleAsset> toSample (const Audio& a, int start, int end, float rootNote, bool loop);
} // namespace dw::sampleimport

// DEADNOISE - learned background noise profile.
#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace deadnoise
{

// A learned noise profile is stored as a normalised power spectral density:
//   psd[k] = E{|X_k|^2} / sum(w^2)
// measured on the raw plugin input (before input gain), where X is the
// Hann-windowed STFT at fftSize. For white noise of variance s^2 this equals
// s^2 in every bin independent of FFT size, so profiles can be re-mapped to a
// different FFT size or sample rate.
struct NoiseProfile
{
    double sampleRate = 0.0;
    int fftSize = 0;
    std::vector<float> psd;           // fftSize/2 + 1 values
    float acceptedFraction = 0.0f;    // fraction of captured frames judged noise-only
    float levelDb = -200.0f;          // broadband noise level (dBFS RMS) for display

    bool isValid() const noexcept
    {
        return sampleRate > 0.0 && fftSize >= 64 && psd.size() == (size_t) (fftSize / 2 + 1);
    }

    // compact binary serialisation (for plugin state)
    std::vector<uint8_t> serialise() const;
    static bool deserialise (const uint8_t* data, size_t size, NoiseProfile& out);
};

// Map a profile onto another bin grid. dst must hold dstFftSize/2+1 values.
// Values are in normalised-PSD units (multiply by sum(w^2) to get bin power).
// Does not allocate.
void mapProfileToGrid (const NoiseProfile& src, double dstSampleRate, int dstFftSize, float* dst) noexcept;

enum class LearnOutcome
{
    ok,
    tooMuchActivity,   // speech / sounds during most of the capture
    noSignal           // digital silence: microphone muted or not routed
};

struct LearnAnalysis
{
    LearnOutcome outcome = LearnOutcome::noSignal;
    float acceptedFraction = 0.0f;
    int acceptedFrames = 0;
};

// Robust estimation of the noise PSD from captured frames.
// frames: numFrames x numBins, row-major, values in normalised-PSD units.
// Frames containing speech, breaths, clicks etc. are detected per sub-band
// against a low-percentile reference and rejected (with temporal dilation so
// onsets and decays are rejected too). Runs on a background thread (allocates).
LearnAnalysis analyseNoiseCapture (const float* frames, int numFrames, int numBins,
                                   double sampleRate, int fftSize, std::vector<float>& psdOut);

} // namespace deadnoise

#pragma once

#include <juce_core/juce_core.h>
#include <complex>
#include <vector>

namespace dw
{
/** Length of one source cycle (the editable waveform resolution). */
constexpr int kWaveSize = 2048;
constexpr int kMaxHarmonics = kWaveSize / 2;
constexpr int kMaxFrames = 256;

/** In-place radix-2 complex FFT (double precision, offline use only). Size must be a power of two. */
void fft (std::vector<std::complex<double>>& data, bool inverse);

/** Magnitude/phase spectrum of one 2048-sample cycle: bins 0..1024. */
struct Spectrum
{
    std::vector<std::complex<double>> bins; // size kMaxHarmonics + 1
};

Spectrum analyse (const float* cycle);
void synthesise (const Spectrum& s, float* cycleOut); // writes kWaveSize samples

/**
    Immutable, band-limited, mipmapped wavetable used by the audio thread.

    Each frame is stored at 20 levels spaced half an octave apart. Level k keeps harmonics
    1..harmonics(k) and is stored at a power-of-two length of at least 4 samples per period of its top
    harmonic, so 4-point Hermite interpolation stays accurate. Every level carries guard samples
    (one before, two after) so the oscillator can interpolate without wrapping.

    The DC component of each frame is preserved in the data and also reported separately via
    frameDc(), so the oscillator can remove a waveform's embedded DC in real time while leaving the
    explicit Vertical Bias untouched.
*/
class Wavetable
{
public:
    static constexpr int kNumLevels = 20;

    Wavetable() = default;

    /** Builds from raw cycles (each kWaveSize samples). */
    static std::unique_ptr<Wavetable> fromFrames (const std::vector<std::vector<float>>& frames, const juce::String& name);

    int getNumFrames() const noexcept { return numFrames; }
    const juce::String& getName() const noexcept { return name; }

    static int levelHarmonics (int level) noexcept;
    static int levelSize (int level) noexcept;

    /** Chooses the level whose harmonic count does not exceed maxHarmonics. */
    static int levelForMaxHarmonics (float maxHarmonics) noexcept
    {
        int k = 0;
        while (k < kNumLevels - 1 && (float) levelHarmonics (k) > maxHarmonics)
            ++k;
        return k;
    }

    /** Pointer to sample 0 of a frame at a level. Indices -1 .. size+1 are valid. */
    const float* frameData (int level, int frame) const noexcept
    {
        return data.data() + offsets[level] + (size_t) frame * (size_t) strides[level] + 1;
    }

    float frameDc (int frame) const noexcept { return dc[(size_t) frame]; }

private:
    juce::String name;
    int numFrames = 0;
    std::vector<float> data;
    std::vector<float> dc;
    size_t offsets[kNumLevels] {};
    int strides[kNumLevels] {};
};
} // namespace dw

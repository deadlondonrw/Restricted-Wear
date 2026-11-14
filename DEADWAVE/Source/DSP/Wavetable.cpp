#include "Wavetable.h"
#include <cmath>

namespace dw
{
void fft (std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();
    jassert (juce::isPowerOfTwo ((int) n));

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * juce::MathConstants<double>::pi / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wlen (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
        for (auto& x : a)
            x /= (double) n;
}

Spectrum analyse (const float* cycle)
{
    std::vector<std::complex<double>> buf ((size_t) kWaveSize);
    for (int i = 0; i < kWaveSize; ++i)
        buf[(size_t) i] = { (double) cycle[i], 0.0 };
    fft (buf, false);

    Spectrum s;
    s.bins.assign (buf.begin(), buf.begin() + kMaxHarmonics + 1);
    return s;
}

void synthesise (const Spectrum& s, float* out)
{
    std::vector<std::complex<double>> buf ((size_t) kWaveSize, { 0.0, 0.0 });
    const int nb = juce::jmin ((int) s.bins.size(), kMaxHarmonics + 1);
    for (int h = 0; h < nb; ++h)
    {
        buf[(size_t) h] = s.bins[(size_t) h];
        if (h > 0 && h < kMaxHarmonics)
            buf[(size_t) (kWaveSize - h)] = std::conj (s.bins[(size_t) h]);
    }
    fft (buf, true);
    for (int i = 0; i < kWaveSize; ++i)
        out[i] = (float) buf[(size_t) i].real();
}

int Wavetable::levelHarmonics (int level) noexcept
{
    static const int h[kNumLevels] = { 1024, 724, 512, 362, 256, 181, 128, 90, 64, 45, 32, 22, 16, 11, 8, 6, 4, 3, 2, 1 };
    return h[juce::jlimit (0, kNumLevels - 1, level)];
}

int Wavetable::levelSize (int level) noexcept
{
    // >= 4 samples per period of the top harmonic for the large levels, >= 8 for the smaller ones
    // (cheap in memory, and keeps Hermite interpolation images below -90 dB).
    const int h = levelHarmonics (level);
    return h > 256 ? juce::nextPowerOfTwo (4 * h) : juce::jmax (256, juce::nextPowerOfTwo (8 * h));
}

std::unique_ptr<Wavetable> Wavetable::fromFrames (const std::vector<std::vector<float>>& frames, const juce::String& tableName)
{
    auto wt = std::make_unique<Wavetable>();
    wt->name = tableName;
    wt->numFrames = juce::jlimit (1, kMaxFrames, (int) frames.size());

    size_t total = 0;
    for (int k = 0; k < kNumLevels; ++k)
    {
        wt->offsets[k] = total;
        wt->strides[k] = levelSize (k) + 4; // 1 guard before, 2 after, 1 pad
        total += (size_t) wt->strides[k] * (size_t) wt->numFrames;
    }
    wt->data.assign (total, 0.0f);
    wt->dc.assign ((size_t) wt->numFrames, 0.0f);

    std::vector<std::complex<double>> work;

    for (int f = 0; f < wt->numFrames; ++f)
    {
        std::vector<float> cycle ((size_t) kWaveSize, 0.0f);
        if (f < (int) frames.size())
        {
            const auto& src = frames[(size_t) f];
            for (int i = 0; i < kWaveSize && i < (int) src.size(); ++i)
                cycle[(size_t) i] = std::isfinite (src[(size_t) i]) ? juce::jlimit (-4.0f, 4.0f, src[(size_t) i]) : 0.0f;
        }

        const auto spec = analyse (cycle.data());
        wt->dc[(size_t) f] = (float) (spec.bins[0].real() / (double) kWaveSize);

        for (int k = 0; k < kNumLevels; ++k)
        {
            const int n = levelSize (k);
            const int hmax = juce::jmin (levelHarmonics (k), n / 2 - 1);
            work.assign ((size_t) n, { 0.0, 0.0 });
            // Rescale from the 2048-point spectrum to an n-point spectrum.
            const double scale = (double) n / (double) kWaveSize;
            work[0] = spec.bins[0] * scale;
            for (int h = 1; h <= hmax; ++h)
            {
                const auto b = spec.bins[(size_t) h] * scale;
                work[(size_t) h] = b;
                work[(size_t) (n - h)] = std::conj (b);
            }
            fft (work, true);

            float* dst = wt->data.data() + wt->offsets[k] + (size_t) f * (size_t) wt->strides[k] + 1;
            for (int i = 0; i < n; ++i)
                dst[i] = (float) work[(size_t) i].real();
            dst[-1] = dst[n - 1];
            dst[n] = dst[0];
            dst[n + 1] = dst[1];
            dst[n + 2] = dst[2];
        }
    }

    return wt;
}
} // namespace dw

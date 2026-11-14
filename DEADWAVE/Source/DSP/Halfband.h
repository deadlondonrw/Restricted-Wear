#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace dw
{
/** Kaiser-windowed half-band FIR coefficients (odd taps around the centre; centre tap is 0.5). */
struct HalfbandKernel
{
    static constexpr int kTaps = 79;              // total length (4k + 3)
    static constexpr int kHalf = (kTaps - 1) / 2; // 39
    static constexpr int kOdd = (kHalf + 1) / 2;  // number of non-zero odd-offset taps per side = 20
    std::array<float, kOdd> c {};                 // c[j] = h[centre +/- (2j+1)]

    HalfbandKernel()
    {
        const double beta = 8.6;
        auto bessel0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 50; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        };
        const double denom = bessel0 (beta);
        double total = 0.5;
        for (int j = 0; j < kOdd; ++j)
        {
            const int m = 2 * j + 1;
            const double sinc = std::sin (juce::MathConstants<double>::pi * 0.5 * m) / (juce::MathConstants<double>::pi * m);
            const double r = (double) m / (double) kHalf;
            const double w = bessel0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / denom;
            c[(size_t) j] = (float) (sinc * w);
            total += 2.0 * sinc * w;
        }
        // Normalise DC gain to exactly 1.
        for (auto& v : c) v = (float) (v / total);
        centre = (float) (0.5 / total);
    }

    float centre = 0.5f;

    static const HalfbandKernel& get()
    {
        static const HalfbandKernel k;
        return k;
    }
};

/** 2:1 decimator. Latency: kHalf / 2 output samples (~19.5). */
class HalfbandDecimator
{
public:
    void reset() noexcept { std::fill (hist.begin(), hist.end(), 0.0f); pos = 0; }

    /** in has 2 * nOut samples. */
    void process (const float* in, float* out, int nOut) noexcept
    {
        const auto& k = HalfbandKernel::get();
        for (int i = 0; i < nOut; ++i)
        {
            push (in[2 * i]);
            push (in[2 * i + 1]);
            // newest sample is at pos-1; centre tap sits kHalf samples back.
            const float* h = hist.data() + pos; // h[-1] newest (mirrored buffer)
            float acc = k.centre * h[N - 1 - HalfbandKernel::kHalf];
            for (int j = 0; j < HalfbandKernel::kOdd; ++j)
            {
                const int m = 2 * j + 1;
                acc += k.c[(size_t) j] * (h[N - 1 - HalfbandKernel::kHalf - m] + h[N - 1 - HalfbandKernel::kHalf + m]);
            }
            out[i] = acc;
        }
    }

private:
    static constexpr int N = HalfbandKernel::kTaps;
    void push (float x) noexcept
    {
        hist[(size_t) pos] = x;
        hist[(size_t) (pos + N)] = x;
        pos = (pos + 1) % N;
    }
    std::array<float, 2 * N> hist {};
    int pos = 0;
};

/** 1:2 interpolator (zero-stuff + half-band). */
class HalfbandInterpolator
{
public:
    void reset() noexcept { std::fill (hist.begin(), hist.end(), 0.0f); pos = 0; }

    void process (const float* in, float* out, int nIn) noexcept
    {
        const auto& k = HalfbandKernel::get();
        for (int i = 0; i < nIn; ++i)
        {
            push (in[i]);
            const float* h = hist.data() + pos; // h[M-1] = newest input
            constexpr int K = HalfbandKernel::kOdd;
            // Even output: the odd-offset taps align with real input samples.
            float even = 0.0f;
            for (int j = 0; j < K; ++j)
                even += k.c[(size_t) j] * (h[M - 1 - (K - 1 - j)] + h[M - 1 - (K + j)]);
            out[2 * i] = 2.0f * even;
            // Odd output: only the centre tap aligns with a real input sample.
            out[2 * i + 1] = 2.0f * k.centre * h[M - 1 - (K - 1)];
        }
    }

private:
    static constexpr int M = HalfbandKernel::kOdd * 2 + 1;
    void push (float x) noexcept
    {
        hist[(size_t) pos] = x;
        hist[(size_t) (pos + M)] = x;
        pos = (pos + 1) % M;
    }
    std::array<float, 2 * M> hist {};
    int pos = 0;
};
} // namespace dw

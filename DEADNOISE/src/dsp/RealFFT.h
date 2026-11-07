// DEADNOISE - real-valued FFT (radix-2, via half-size complex FFT).
// Self-contained so the DSP core has no framework dependency and can be
// unit-tested natively. All memory is allocated in init(); forward() and
// inverse() never allocate.
#pragma once

#include <complex>
#include <vector>
#include <cmath>
#include <cassert>
#include "DspMath.h"

namespace deadnoise
{

class RealFFT
{
public:
    using cfloat = std::complex<float>;

    void init (int fftSize)
    {
        assert (fftSize >= 8 && (fftSize & (fftSize - 1)) == 0);
        n = fftSize;
        half = n / 2;

        // bit reversal for the half-size complex transform
        bitrev.assign ((size_t) half, 0);
        int bits = 0;
        while ((1 << bits) < half) ++bits;
        for (int i = 0; i < half; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            bitrev[(size_t) i] = r;
        }

        // twiddles for the half-size complex FFT: exp(-2*pi*i*k/half)
        twiddle.resize ((size_t) half / 2 + 1);
        for (int k = 0; k <= half / 2; ++k)
        {
            const double a = -2.0 * kPi * k / half;
            twiddle[(size_t) k] = cfloat ((float) std::cos (a), (float) std::sin (a));
        }

        // post-processing twiddles: exp(-2*pi*i*k/n), k = 0..half
        post.resize ((size_t) half + 1);
        for (int k = 0; k <= half; ++k)
        {
            const double a = -2.0 * kPi * k / n;
            post[(size_t) k] = cfloat ((float) std::cos (a), (float) std::sin (a));
        }

        work.assign ((size_t) half, cfloat());
    }

    int size() const noexcept { return n; }
    int numBins() const noexcept { return half + 1; }

    // in: n real samples. out: n/2+1 complex bins (unnormalised DFT).
    void forward (const float* in, cfloat* out) noexcept
    {
        for (int m = 0; m < half; ++m)
            work[(size_t) bitrev[(size_t) m]] = cfloat (in[2 * m], in[2 * m + 1]);

        complexFFT (work.data());

        const cfloat z0 = work[0];
        out[0]    = cfloat (z0.real() + z0.imag(), 0.0f);
        out[half] = cfloat (z0.real() - z0.imag(), 0.0f);

        for (int k = 1; k < half; ++k)
        {
            const cfloat zk  = work[(size_t) k];
            const cfloat znk = std::conj (work[(size_t) (half - k)]);
            const cfloat fe  = 0.5f * (zk + znk);                 // even part
            const cfloat fo  = cfloat (0.0f, -0.5f) * (zk - znk);                 // odd part
            out[k] = fe + post[(size_t) k] * fo;
        }
    }

    // in: n/2+1 complex bins. out: n real samples, normalised so that
    // inverse(forward(x)) == x.
    void inverse (const cfloat* in, float* out) noexcept
    {
        // Rebuild the half-size complex spectrum Z[k] = Fe[k] + i*Fo[k]
        for (int k = 0; k < half; ++k)
        {
            const cfloat xk  = in[k];
            const cfloat xnk = std::conj (in[half - k]);
            const cfloat fe  = 0.5f * (xk + xnk);
            const cfloat fo  = 0.5f * (xk - xnk) * std::conj (post[(size_t) k]);
            const cfloat z   = fe + cfloat (0.0f, 1.0f) * fo;
            // inverse via conjugation trick: ifft(Z) = conj(fft(conj(Z))) / half
            work[(size_t) bitrev[(size_t) k]] = std::conj (z);
        }

        complexFFT (work.data());

        const float scale = 1.0f / (float) half;
        for (int m = 0; m < half; ++m)
        {
            const cfloat z = std::conj (work[(size_t) m]) * scale;
            out[2 * m]     = z.real();
            out[2 * m + 1] = z.imag();
        }
    }

private:
    // In-place iterative radix-2 DIT FFT. Input must already be in
    // bit-reversed order.
    void complexFFT (cfloat* data) noexcept
    {
        for (int len = 2; len <= half; len <<= 1)
        {
            const int halfLen = len >> 1;
            const int step = half / len;
            for (int i = 0; i < half; i += len)
            {
                for (int j = 0; j < halfLen; ++j)
                {
                    // j * step < half / 2 always, so the table covers every index
                    const cfloat w = twiddle[(size_t) (j * step)];
                    const cfloat u = data[i + j];
                    const cfloat v = data[i + j + halfLen] * w;
                    data[i + j]           = u + v;
                    data[i + j + halfLen] = u - v;
                }
            }
        }
    }

    int n = 0, half = 0;
    std::vector<int> bitrev;
    std::vector<cfloat> twiddle, post, work;
};

} // namespace deadnoise

// DEADNOISE - small numeric helpers used by the DSP core.
#pragma once

#include <cmath>
#include <algorithm>

namespace deadnoise
{

constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept      { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept       { return 20.0f * std::log10 (std::max (g, 1.0e-12f)); }
inline float powerToDb (float p) noexcept      { return 10.0f * std::log10 (std::max (p, 1.0e-30f)); }
inline float dbToPower (float db) noexcept     { return std::pow (10.0f, db * 0.1f); }

// One-pole smoothing coefficient for a time constant (seconds) at a given update rate (Hz).
inline float timeConstantCoef (double seconds, double updateRate) noexcept
{
    if (seconds <= 0.0) return 0.0f;
    return (float) std::exp (-1.0 / (seconds * updateRate));
}

// Exponential integral E1(x) for x > 0.
// x < 1 : power series. x >= 1 : rational approximation (Abramowitz & Stegun 5.1.56),
// relative error < 5e-5, which is far below what is audible in a gain function.
inline float expint1 (float xf) noexcept
{
    const double x = std::max ((double) xf, 1.0e-10);
    if (x < 1.0)
    {
        // E1(x) = -gamma - ln(x) - sum_{k>=1} (-x)^k / (k * k!)
        double sum = 0.0, term = 1.0;
        for (int k = 1; k <= 14; ++k)
        {
            term *= -x / k;
            sum += term / k;
        }
        return (float) (-0.57721566490153286 - std::log (x) - sum);
    }
    const double a1 = 2.334733, a2 = 0.250621, b1 = 3.330657, b2 = 1.681534;
    const double num = x * x + a1 * x + a2;
    const double den = x * x + b1 * x + b2;
    return (float) (std::exp (-x) / x * (num / den));
}

// Linear-in-dB ramp mapping: 0 below lo, 1 above hi.
inline float rampDb (float valueLinearPower, float loDb, float hiDb) noexcept
{
    const float d = powerToDb (valueLinearPower);
    if (d <= loDb) return 0.0f;
    if (d >= hiDb) return 1.0f;
    return (d - loDb) / (hiDb - loDb);
}

template <typename T> inline T clampv (T v, T lo, T hi) noexcept { return std::min (hi, std::max (lo, v)); }

inline bool isFiniteFloat (float x) noexcept { return std::isfinite (x); }

} // namespace deadnoise

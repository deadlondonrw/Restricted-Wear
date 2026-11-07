// DEADNOISE - secondary noise gate.
// Optional and deliberately gentle: the spectral suppressor does the real
// work; the gate only lowers what is left in pauses. Detection runs on the
// *denoised* signal, so hiss does not hold it open.
#pragma once

#include "DspMath.h"
#include <cmath>
#include <algorithm>

namespace deadnoise
{

class NoiseGate
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        envRelease  = timeConstantCoef (0.030, sr);
        gainAttack  = timeConstantCoef (0.0015, sr);   // opening
        gainRelease = timeConstantCoef (0.080, sr);    // closing
        toggleCoef  = timeConstantCoef (0.010, sr);
        holdSamples = (int) std::round (0.150 * sr);
        reset();
    }

    void reset()
    {
        env = 0.0f;
        open = false;
        holdCounter = 0;
        gainDb = 0.0f;
    }

    void setParams (bool isEnabled, float thresholdDbIn, float rangeDbIn) noexcept
    {
        enabled = isEnabled;
        thresholdDb = thresholdDbIn;
        rangeDb = std::max (0.0f, rangeDbIn);
    }

    bool isOpen() const noexcept { return ! enabled || open; }

    // detector: up to 2 channels of denoised audio. Writes a linear gain per sample.
    void computeGains (const float* const* detect, int numCh, int numSamples, float* gainOut) noexcept
    {
        const float hystDb = 4.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c) peak = std::max (peak, std::abs (detect[c][n]));
            env = std::max (peak, env * envRelease);
            const float envDb = gainToDb (env);

            if (envDb > thresholdDb)
            {
                open = true;
                holdCounter = holdSamples;
            }
            else if (open)
            {
                if (holdCounter > 0) --holdCounter;
                else if (envDb < thresholdDb - hystDb) open = false;
            }

            const float target = (! enabled || open) ? 0.0f : -rangeDb;
            const float coef = target > gainDb ? gainAttack : (enabled ? gainRelease : toggleCoef);
            gainDb = target + (gainDb - target) * coef;
            gainOut[n] = gainDb > -0.001f ? 1.0f : dbToGain (gainDb);
        }
    }

private:
    double sr = 48000.0;
    bool enabled = false, open = false;
    float thresholdDb = -50.0f, rangeDb = 40.0f;
    float env = 0.0f, gainDb = 0.0f;
    float envRelease = 0.0f, gainAttack = 0.0f, gainRelease = 0.0f, toggleCoef = 0.0f;
    int holdSamples = 0, holdCounter = 0;
};

} // namespace deadnoise

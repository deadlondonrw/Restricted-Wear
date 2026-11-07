// DEADNOISE - complete processing chain.
#include "DeadnoiseEngine.h"
#include "DspMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace deadnoise
{

void DeadnoiseEngine::prepare (double sr, int maxBlockSize, int nch)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    maxBlock = std::max (16, maxBlockSize);
    numChannels = clampv (nch, 1, kMaxChannels);

    denoiser.prepare (sampleRate, numChannels);
    gate.prepare (sampleRate);

    for (int c = 0; c < kMaxChannels; ++c)
    {
        dry[c].assign ((size_t) maxBlock, 0.0f);
        wet[c].assign ((size_t) maxBlock, 0.0f);
    }
    delayLen = denoiser.getLatencySamples();
    for (int c = 0; c < kMaxChannels; ++c)
        delayBuf[c].assign ((size_t) delayLen + 1, 0.0f);

    gateGain.assign ((size_t) maxBlock, 1.0f);
    inRamp.assign ((size_t) maxBlock, 1.0f);
    outRamp.assign ((size_t) maxBlock, 1.0f);
    mixRamp.assign ((size_t) maxBlock, 1.0f);

    paramCoef = timeConstantCoef (0.015, sampleRate);
    monCoef = timeConstantCoef (0.008, sampleRate);
    reset();
}

void DeadnoiseEngine::reset()
{
    denoiser.reset();
    gate.reset();
    for (int c = 0; c < kMaxChannels; ++c)
        std::fill (delayBuf[c].begin(), delayBuf[c].end(), 0.0f);
    delayWrite = 0;
    inGainS = dbToGain (params.inputGainDb);
    outGainS = dbToGain (params.outputGainDb);
    mixS = clampv (params.mix, 0.0f, 1.0f);
    for (int i = 0; i < 3; ++i) monW[i] = (i == (int) params.monitor) ? 1.0f : 0.0f;
}

void DeadnoiseEngine::process (float* const* channels, int numCh, int numSamples) noexcept
{
    const int nch = std::min (numCh, numChannels);
    if (nch <= 0 || numSamples <= 0) return;

    float* chunkPtrs[kMaxChannels] = {};
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        for (int c = 0; c < nch; ++c) chunkPtrs[c] = channels[c] + done;
        processChunk (chunkPtrs, nch, n);
        done += n;
    }
}

void DeadnoiseEngine::processChunk (float* const* io, int nch, int n) noexcept
{
    // ---- parameter ramps (one-pole, per sample)
    const float inTarget = dbToGain (params.inputGainDb);
    const float outTarget = dbToGain (params.outputGainDb);
    const float mixTarget = clampv (params.mix, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        inGainS  = inTarget  + (inGainS  - inTarget)  * paramCoef;
        outGainS = outTarget + (outGainS - outTarget) * paramCoef;
        mixS     = mixTarget + (mixS     - mixTarget) * paramCoef;
        inRamp[(size_t) i] = inGainS;
        outRamp[(size_t) i] = outGainS;
        mixRamp[(size_t) i] = mixS;
    }

    DenoiserSettings ds;
    ds.amount = clampv (params.reduction, 0.0f, 1.0f);
    ds.maximumMode = params.maximumMode;
    ds.voicePreservation = clampv (params.voicePreservation, 0.0f, 1.0f);
    ds.inputGain = inGainS;
    denoiser.setSettings (ds);
    gate.setParams (params.gateEnabled, params.gateThresholdDb, params.gateRangeDb);

    // ---- sanitise + input gain
    float pkIn = 0.0f;
    for (int c = 0; c < nch; ++c)
    {
        const float* src = io[c];
        float* d = dry[c].data();
        float* w = wet[c].data();
        for (int i = 0; i < n; ++i)
        {
            float x = src[i];
            if (! std::isfinite (x)) x = 0.0f;
            x *= inRamp[(size_t) i];
            d[i] = x;
            w[i] = x;
            pkIn = std::max (pkIn, std::abs (x));
        }
    }

    // ---- spectral denoiser (wet path, latency = delayLen)
    float* wetPtrs[kMaxChannels] = {};
    const float* wetConst[kMaxChannels] = {};
    for (int c = 0; c < nch; ++c) { wetPtrs[c] = wet[c].data(); wetConst[c] = wet[c].data(); }
    denoiser.process (wetPtrs, nch, n);

    // ---- latency-aligned dry
    {
        const int size = delayLen + 1;
        int w = delayWrite;
        for (int i = 0; i < n; ++i)
        {
            int r = w - delayLen;
            if (r < 0) r += size;
            for (int c = 0; c < nch; ++c)
            {
                float* buf = delayBuf[c].data();
                buf[w] = dry[c][(size_t) i];
                dry[c][(size_t) i] = buf[r];
            }
            if (++w >= size) w = 0;
        }
        delayWrite = w;
    }

    // ---- gate (detector on the denoised signal)
    gate.computeGains (wetConst, nch, n, gateGain.data());
    gateOpen.store (gate.isOpen(), std::memory_order_relaxed);

    // ---- output: monitor crossfade, mix, gain
    const int mon = clampv ((int) params.monitor, 0, 2);
    float pkOut = 0.0f;
    float w0 = monW[0], w1 = monW[1], w2 = monW[2];
    for (int i = 0; i < n; ++i)
    {
        w0 = (mon == 0 ? 1.0f : 0.0f) + (w0 - (mon == 0 ? 1.0f : 0.0f)) * monCoef;
        w1 = (mon == 1 ? 1.0f : 0.0f) + (w1 - (mon == 1 ? 1.0f : 0.0f)) * monCoef;
        w2 = (mon == 2 ? 1.0f : 0.0f) + (w2 - (mon == 2 ? 1.0f : 0.0f)) * monCoef;
        const float m = mixRamp[(size_t) i];
        const float g = gateGain[(size_t) i];
        const float og = outRamp[(size_t) i];
        for (int c = 0; c < nch; ++c)
        {
            const float dr = dry[c][(size_t) i];
            const float wt = wet[c][(size_t) i];
            const float processed = (m * wt + (1.0f - m) * dr) * g;
            float y = (w0 * processed + w1 * dr + w2 * (dr - wt)) * og;
            if (! std::isfinite (y)) y = 0.0f;
            io[c][i] = y;
            pkOut = std::max (pkOut, std::abs (y));
        }
    }
    // snap tiny residual weights so the crossfade settles exactly
    monW[0] = std::abs (w0 - (mon == 0)) < 1.0e-6f ? (float) (mon == 0) : w0;
    monW[1] = std::abs (w1 - (mon == 1)) < 1.0e-6f ? (float) (mon == 1) : w1;
    monW[2] = std::abs (w2 - (mon == 2)) < 1.0e-6f ? (float) (mon == 2) : w2;

    // ---- meters (max since last UI read)
    float prev = inPeak.load (std::memory_order_relaxed);
    if (pkIn > prev) inPeak.store (pkIn, std::memory_order_relaxed);
    prev = outPeak.load (std::memory_order_relaxed);
    if (pkOut > prev) outPeak.store (pkOut, std::memory_order_relaxed);
}

} // namespace deadnoise

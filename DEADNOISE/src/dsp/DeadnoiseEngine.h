// DEADNOISE - complete processing chain (framework independent).
//
//   input -> sanitise -> input gain -> [spectral denoiser] -> mix with latency-
//   aligned dry -> optional gate -> monitor select -> output gain -> output
#pragma once

#include "SpectralDenoiser.h"
#include "NoiseGate.h"

#include <atomic>
#include <vector>

namespace deadnoise
{

enum class MonitorMode : int { processed = 0, original = 1, removedNoise = 2 };

struct EngineParams
{
    float reduction = 0.8f;          // 0..1
    bool maximumMode = false;
    float voicePreservation = 0.5f;  // 0..1
    float mix = 1.0f;                // 0 = dry, 1 = wet
    bool gateEnabled = false;
    float gateThresholdDb = -55.0f;
    float gateRangeDb = 40.0f;
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    MonitorMode monitor = MonitorMode::processed;
};

class DeadnoiseEngine
{
public:
    static constexpr int kMaxChannels = SpectralDenoiser::kMaxChannels;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    int getLatencySamples() const noexcept { return denoiser.getLatencySamples(); }

    void setParams (const EngineParams& p) noexcept { params = p; }
    const EngineParams& getParams() const noexcept { return params; }

    // In-place. numChannels <= prepared channel count is processed; never allocates.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    SpectralDenoiser& getDenoiser() noexcept { return denoiser; }
    const SpectralDenoiser& getDenoiser() const noexcept { return denoiser; }

    // peak since the last call (UI thread)
    float takeInputPeak() noexcept  { return inPeak.exchange (0.0f, std::memory_order_relaxed); }
    float takeOutputPeak() noexcept { return outPeak.exchange (0.0f, std::memory_order_relaxed); }
    bool isGateOpen() const noexcept { return gateOpen.load (std::memory_order_relaxed); }

private:
    void processChunk (float* const* channels, int numCh, int n) noexcept;

    SpectralDenoiser denoiser;
    NoiseGate gate;
    EngineParams params;

    double sampleRate = 48000.0;
    int maxBlock = 512, numChannels = 1;

    std::vector<float> dry[kMaxChannels], wet[kMaxChannels];
    std::vector<float> delayBuf[kMaxChannels];
    int delayWrite = 0, delayLen = 0;
    std::vector<float> gateGain, inRamp, outRamp, mixRamp;

    // smoothed parameters
    float inGainS = 1.0f, outGainS = 1.0f, mixS = 1.0f;
    float monW[3] = { 1.0f, 0.0f, 0.0f };
    float paramCoef = 0.0f, monCoef = 0.0f;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<bool> gateOpen { true };
};

} // namespace deadnoise

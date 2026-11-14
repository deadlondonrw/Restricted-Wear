#pragma once

#include "../Core/Params.h"
#include "Filter.h"
#include "Modulators.h"
#include "Oscillator.h"

namespace dw
{
struct ModSlot
{
    int src = 0;    // ModSrc
    int dst = -1;   // parameter index
    int amtIdx = 0; // parameter index of the amount
};

/** Read-only engine state handed to voices for one control tick. */
struct EngineContext
{
    const float* base = nullptr;      // real parameter values [P::Count]
    const float* baseNorm = nullptr;  // normalised values [P::Count]
    const uint8_t* modActive = nullptr; // [P::Count] dest has voice-level modulation
    const juce::NormalisableRange<float>* ranges = nullptr;
    const ModSlot* slots = nullptr;
    int numSlots = 0;
    const int* voiceDests = nullptr;  // unique per-voice dests with modulation
    int numVoiceDests = 0;

    const Wavetable* tables[kNumOsc] {};
    const SampleAsset* samples[kNumOsc] {};
    LfoSettings lfo[kNumLfos];
    float globalLfo[kNumLfos] {};     // free-running LFO values
    EnvSettings envBase[kNumEnvs];    // unmodulated envelope settings

    float modWheel = 0.0f, channelPressure = 0.0f, pitchBendSemis = 0.0f, pitchBendNorm = 0.0f;
    float macro[kNumMacros] {};

    double processRate = 48000.0;
    int oversample = 1;
    bool ultra = false;
};

class Voice
{
public:
    void prepare (double baseRate, int maxOversample);

    /** Starts a note. glideFrom < 0 disables glide. */
    void start (int note, float velocity, float glideFromPitch, uint32_t age, uint32_t seed, const EngineContext& ctx);
    /** Mono/legato: moves to a new pitch without restarting (retrigger optional). */
    void legatoTo (int note, float velocity, bool glide, bool retrigger, bool hardRetrigger, uint32_t age);
    void release() noexcept;
    void steal() noexcept { fadeOut = true; }
    void kill() noexcept;

    bool isActive() const noexcept { return active; }
    bool isReleasing() const noexcept { return released || fadeOut; }
    bool isFading() const noexcept { return fadeOut; }

    /** Renders n samples at the process rate into the buses (added). laneUsed is set for lanes written. */
    void render (const EngineContext& ctx, float* mainL, float* mainR, float* const* laneL, float* const* laneR,
                 bool* laneUsed, int n) noexcept;

    float getModSourceValue (ModSrc s) const noexcept { return srcVal[(int) s]; }
    float getCurrentPitch() const noexcept { return pitch; }
    float getAmpLevel() const noexcept { return env[0].getValue(); }

    int note = -1;
    float velocity = 0.0f;
    uint32_t age = 0;
    bool keyHeld = false;
    bool sustained = false;
    float polyPressure = 0.0f;

private:
    float param (const EngineContext& ctx, int idx) const noexcept
    {
        return ctx.modActive[idx] ? modVal[idx] : ctx.base[idx];
    }
    void computeModulation (const EngineContext& ctx) noexcept;
    void buildTicks (const EngineContext& ctx, float dt) noexcept;

    bool active = false, released = false, fadeOut = false, firstTick = true;
    float fadeGain = 1.0f;

    Oscillator osc[kNumOsc];
    VoiceFilter filter[kNumFilters];
    VoiceFilter laneFilter[kNumOsc][kNumFilters];
    Envelope env[kNumEnvs];
    Lfo lfo[kNumLfos];
    DcBlocker dcMain[2], dcLane[kNumOsc][2];

    OscTick prevOsc[kNumOsc], curOsc[kNumOsc];
    FilterTick prevFilt[kNumFilters], curFilt[kNumFilters];
    float prevAmp = 0.0f, curAmp = 0.0f;

    // pitch / glide
    float pitch = 60.0f, glideFrom = 60.0f, glideTo = 60.0f, glidePos = 1.0f;
    bool gliding = false;

    // analog
    float drift[kNumOsc] {}, driftTarget[kNumOsc] {}, instab[kNumOsc] {};
    float driftTimer = 0.0f;

    float srcVal[(int) ModSrc::NumSources] {};
    float randomValue = 0.0f;
    float modVal[P::Count] {};
    float modAccum[P::Count] {};

    Rng rng;
    double processRate = 48000.0;

    static constexpr int kBuf = Oscillator::kMaxBlock;
    float oscL[kNumOsc][kBuf] {}, oscR[kNumOsc][kBuf] {}, oscMono[kNumOsc][kBuf] {};
    float bufAL[kBuf] {}, bufAR[kBuf] {}, bufBL[kBuf] {}, bufBR[kBuf] {}, bufDL[kBuf] {}, bufDR[kBuf] {};
};
} // namespace dw

#pragma once

#include "../Core/DspUtil.h"
#include <array>

namespace dw
{
// ======================================================================================== envelope
struct EnvSettings
{
    float delay = 0.0f, attack = 0.002f, hold = 0.0f, decay = 0.4f, sustain = 0.8f, release = 0.2f;
    float aCurve = 0.25f, dCurve = 0.6f, rCurve = 0.6f;
};

/** DAHDSR envelope with curved segments, evaluated at control rate. */
class Envelope
{
public:
    enum class Stage { Idle, Delay, Attack, Hold, Decay, Sustain, Release };

    void trigger (bool fromZero) noexcept
    {
        startLevel = fromZero ? 0.0f : value;
        if (fromZero) value = 0.0f;
        stage = Stage::Delay;
        pos = 0.0f;
    }

    void release() noexcept
    {
        if (stage == Stage::Idle || stage == Stage::Release) return;
        startLevel = value;
        stage = Stage::Release;
        pos = 0.0f;
    }

    void kill() noexcept { stage = Stage::Idle; value = 0.0f; }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    float getValue() const noexcept { return value; }
    Stage getStage() const noexcept { return stage; }

    static float curve (float u, float c) noexcept
    {
        u = juce::jlimit (0.0f, 1.0f, u);
        if (std::abs (c) < 0.01f) return u;
        const float k = c * 7.0f;
        return (1.0f - std::exp (-k * u)) / (1.0f - std::exp (-k));
    }

    /** Advances by dt seconds and returns the new value. */
    float advance (const EnvSettings& s, float dt) noexcept
    {
        float remaining = dt;
        for (int guard = 0; guard < 8 && remaining > 0.0f; ++guard)
        {
            switch (stage)
            {
                case Stage::Idle:
                    value = 0.0f;
                    return value;
                case Stage::Delay:
                    if (! step (s.delay, remaining)) return value;
                    stage = Stage::Attack; pos = 0.0f;
                    break;
                case Stage::Attack:
                {
                    const bool done = step (s.attack, remaining);
                    value = startLevel + (1.0f - startLevel) * curve (s.attack <= 0.0f ? 1.0f : pos / s.attack, s.aCurve);
                    if (! done) return value;
                    value = 1.0f; stage = Stage::Hold; pos = 0.0f;
                    break;
                }
                case Stage::Hold:
                    value = 1.0f;
                    if (! step (s.hold, remaining)) return value;
                    stage = Stage::Decay; pos = 0.0f;
                    break;
                case Stage::Decay:
                {
                    const bool done = step (s.decay, remaining);
                    value = 1.0f + (s.sustain - 1.0f) * curve (s.decay <= 0.0f ? 1.0f : pos / s.decay, s.dCurve);
                    if (! done) return value;
                    value = s.sustain; stage = Stage::Sustain; pos = 0.0f;
                    break;
                }
                case Stage::Sustain:
                    // Follow sustain changes smoothly (modulated sustain)
                    value += (s.sustain - value) * juce::jmin (1.0f, remaining * 60.0f);
                    return value;
                case Stage::Release:
                {
                    const bool done = step (s.release, remaining);
                    value = startLevel * (1.0f - curve (s.release <= 0.0f ? 1.0f : pos / s.release, s.rCurve));
                    if (! done) return value;
                    value = 0.0f; stage = Stage::Idle;
                    return value;
                }
            }
        }
        return value;
    }

private:
    /** Advances pos within a segment of the given length; returns true if the segment finished. */
    bool step (float length, float& remaining) noexcept
    {
        const float left = length - pos;
        if (left <= remaining)
        {
            remaining -= juce::jmax (0.0f, left);
            pos = length;
            return true;
        }
        pos += remaining;
        remaining = 0.0f;
        return false;
    }

    Stage stage = Stage::Idle;
    float value = 0.0f, startLevel = 0.0f, pos = 0.0f;
};

// ============================================================================================ LFO
struct LfoShapeTable
{
    static constexpr int kSize = 1024;
    std::array<float, kSize + 1> v {}; // unipolar 0..1, last = wrap
};

struct LfoSettings
{
    int shape = 0;
    int mode = 1;          // 0 free, 1 retrigger, 2 one-shot
    float rateHz = 2.0f;
    float phase = 0.0f;
    float smooth = 0.0f;
    float rise = 0.0f;
    bool bipolar = true;
    const LfoShapeTable* custom = nullptr;
};

class Lfo
{
public:
    void reset (const LfoSettings& s, uint32_t seed) noexcept
    {
        phase = 0.0;
        rng.seed (seed);
        held = rng.bi(); prevRand = held; nextRand = rng.bi();
        riseTime = 0.0f;
        done = false;
        smoothed = shapeValue (s, wrap01 ((double) s.phase));
        juce::ignoreUnused (s);
    }

    /** Sets absolute phase (host-synced free LFO). */
    void setPhase (double p) noexcept
    {
        const double np = p - std::floor (p);
        if (np < phase - 0.5) newCycle();
        phase = np;
    }

    float advance (const LfoSettings& s, float dt, bool externalPhase = false) noexcept
    {
        if (! externalPhase && ! done)
        {
            phase += (double) s.rateHz * (double) dt;
            if (phase >= 1.0)
            {
                if (s.mode == 2) { phase = 1.0; done = true; }
                else { phase -= std::floor (phase); newCycle(); }
            }
        }
        riseTime += dt;

        const double p = s.mode == 2 && done ? 0.99999 : wrap01 (phase + (double) s.phase);
        float v = shapeValue (s, p);

        if (s.smooth > 0.001f)
        {
            const float tau = s.smooth * s.smooth * 0.5f / juce::jmax (0.05f, s.rateHz) + 0.0005f;
            const float a = 1.0f - std::exp (-dt / tau);
            smoothed += (v - smoothed) * a;
            v = smoothed;
        }
        else
            smoothed = v;

        float out = s.bipolar ? v * 2.0f - 1.0f : v;
        if (s.rise > 0.001f)
            out *= juce::jmin (1.0f, riseTime / s.rise);
        value = out;
        return out;
    }

    float getValue() const noexcept { return value; }
    double getPhase() const noexcept { return phase; }

    /** Unipolar shape value 0..1 at phase p. */
    float shapeValue (const LfoSettings& s, double p) noexcept
    {
        const float x = (float) p;
        switch (s.shape)
        {
            case 0: return 0.5f - 0.5f * std::cos (kTwoPi * x);
            case 1: return x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x;
            case 2: return x;
            case 3: return 1.0f - x;
            case 4: return x < 0.5f ? 1.0f : 0.0f;
            case 5: return held * 0.5f + 0.5f;
            case 6:
            {
                const float u = 0.5f - 0.5f * std::cos (kPi * x);
                return (prevRand + (nextRand - prevRand) * u) * 0.5f + 0.5f;
            }
            case 7:
            default:
                if (s.custom != nullptr)
                {
                    const float idx = x * (float) LfoShapeTable::kSize;
                    const int i = juce::jlimit (0, LfoShapeTable::kSize - 1, (int) idx);
                    const float t = idx - (float) i;
                    return s.custom->v[(size_t) i] + (s.custom->v[(size_t) i + 1] - s.custom->v[(size_t) i]) * t;
                }
                return 0.5f - 0.5f * std::cos (kTwoPi * x);
        }
    }

private:
    void newCycle() noexcept
    {
        held = rng.bi();
        prevRand = nextRand;
        nextRand = rng.bi();
    }

    double phase = 0.0;
    float value = 0.0f, smoothed = 0.0f, riseTime = 0.0f;
    float held = 0.0f, prevRand = 0.0f, nextRand = 0.0f;
    bool done = false;
    Rng rng;
};
} // namespace dw

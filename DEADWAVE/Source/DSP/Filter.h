#pragma once

#include "../Core/DspUtil.h"
#include <vector>

namespace dw
{
struct FilterTick
{
    int type = 1;
    float cutoff = 4000.0f;  // Hz
    float reso = 0.1f;       // 0..1
    float drive = 0.0f;      // 0..1
    float mix = 1.0f;
    float spread = 0.0f;     // stereo cutoff offset 0..1
    float nonlin = 0.0f;     // analog nonlinearity 0..1
};

/**
    Stereo voice filter. Types: SVF LP/HP/BP 12+24, notch, peak, all-pass, nonlinear ZDF ladder LP/HP,
    keytracked comb (+/-) and a three-formant vowel filter. Coefficients are ramped per sample between
    control ticks so modulation is zipper-free.
*/
class VoiceFilter
{
public:
    enum Type { LP12, LP24, HP12, HP24, BP12, BP24, Notch, Peak, AllPass, LadderLP, LadderHP, CombPos, CombNeg, Formant, NumTypes };

    /** Allocates (message thread). combBufferSize must cover the highest process rate / lowest comb freq. */
    void prepare (double sampleRate, int combBufferSize)
    {
        fs = sampleRate;
        for (auto& c : comb)
            c.assign ((size_t) juce::nextPowerOfTwo (juce::jmax (64, combBufferSize)), 0.0f);
        combMask = (int) comb[0].size() - 1;
        reset();
    }

    /** Real-time safe rate change (no allocation). */
    void setSampleRate (double sampleRate) noexcept { fs = sampleRate; }

    void reset() noexcept
    {
        for (auto& c : ch) c = {};
        for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
        combPos = 0;
        lastType = -1;
    }

    void process (const FilterTick& prev, const FilterTick& cur, float* l, float* r, int n) noexcept;

private:
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        inline void tick (float v0, float g, float k, float nl, float& lp, float& bp, float& hp) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            if (nl > 0.0f)
                ic1 = fastTanh (ic1 * nl) / nl;
            ic1 = undenormalise (ic1);
            ic2 = undenormalise (ic2);
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
    };

    struct Channel
    {
        Svf a, b, f1, f2, f3;
        float s[4] {};          // ladder states
    };

    float tanG (float hz) const noexcept
    {
        const float w = juce::jlimit (5.0f, (float) fs * 0.49f, hz);
        return std::tan (kPi * w / (float) fs);
    }

    void processChannel (Channel& c, int chIdx, float* x, int n, const FilterTick& prev, const FilterTick& cur, float spreadSign) noexcept;

    double fs = 48000.0;
    Channel ch[2];
    std::vector<float> comb[2];
    int combMask = 0, combPos = 0;
    int lastType = -1;
};
} // namespace dw

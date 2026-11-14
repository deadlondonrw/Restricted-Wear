#include "Filter.h"

namespace dw
{
namespace
{
const float kVowels[5][3] = { { 800.0f, 1150.0f, 2900.0f }, { 400.0f, 1600.0f, 2700.0f }, { 350.0f, 2300.0f, 3000.0f },
                              { 450.0f, 800.0f, 2830.0f }, { 325.0f, 700.0f, 2700.0f } };
const float kVowelGain[3] = { 1.0f, 0.63f, 0.4f };

inline void vowelAt (float cutoff, float out[3]) noexcept
{
    // 20 Hz .. 20 kHz mapped to A .. U
    const float pos = juce::jlimit (0.0f, 3.999f, std::log2 (juce::jmax (20.0f, cutoff) / 20.0f) / std::log2 (1000.0f) * 4.0f);
    const int a = (int) pos;
    const float t = pos - (float) a;
    for (int i = 0; i < 3; ++i)
        out[i] = kVowels[a][i] + (kVowels[a + 1][i] - kVowels[a][i]) * t;
}
} // namespace

void VoiceFilter::process (const FilterTick& prev, const FilterTick& cur, float* l, float* r, int n) noexcept
{
    if (cur.type != lastType)
    {
        for (auto& c : ch) c = {};
        if (cur.type == CombPos || cur.type == CombNeg)
            for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
        lastType = cur.type;
    }

    const int startPos = combPos;
    processChannel (ch[0], 0, l, n, prev, cur, -1.0f);
    const int endPos = combPos;
    combPos = startPos; // both channels walk the same comb write position
    processChannel (ch[1], 1, r, n, prev, cur, 1.0f);
    combPos = endPos;
}

void VoiceFilter::processChannel (Channel& c, int chIdx, float* x, int n, const FilterTick& prev, const FilterTick& cur,
                                  float spreadSign) noexcept
{
    const float invN = 1.0f / (float) n;
    const float sp0 = std::exp2 (spreadSign * prev.spread * 0.5f);
    const float sp1 = std::exp2 (spreadSign * cur.spread * 0.5f);
    const float fc0 = prev.cutoff * sp0, fc1 = cur.cutoff * sp1;

    const float mix0 = prev.mix, dMix = (cur.mix - prev.mix) * invN;
    const float drv = cur.drive;
    const bool driven = drv > 1.0e-4f;
    const float dg = 1.0f + drv * drv * 15.0f;
    const float dgComp = 1.0f / std::sqrt (dg);
    const float nl = cur.nonlin > 0.0f ? 0.08f + cur.nonlin * 0.6f : 0.0f;

    const int type = cur.type;
    float mix = mix0;

    if (type == CombPos || type == CombNeg)
    {
        auto& buf = comb[chIdx];
        const float fb = (type == CombPos ? 1.0f : -1.0f) * (0.3f + 0.69f * juce::jlimit (0.0f, 1.0f, cur.reso));
        const float d0 = (float) fs / juce::jmax (20.0f, fc0), d1 = (float) fs / juce::jmax (20.0f, fc1);
        const float maxD = (float) (combMask - 4);
        float d = juce::jmin (d0, maxD);
        const float dd = (juce::jmin (d1, maxD) - d) * invN;
        float& damp = c.s[0];
        const float dampA = 0.35f + 0.6f * (1.0f - cur.drive * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            const float in = driven ? fastTanh (x[i] * dg) * dgComp : x[i];
            float rp = (float) combPos - d;
            while (rp < 0.0f) rp += (float) (combMask + 1);
            const int i0 = (int) rp;
            const float t = rp - (float) i0;
            const float delayed = buf[(size_t) (i0 & combMask)] + (buf[(size_t) ((i0 + 1) & combMask)] - buf[(size_t) (i0 & combMask)]) * t;
            damp += dampA * (delayed - damp);
            float y = in + fb * damp;
            y = nl > 0.0f ? fastTanh (y * nl) / nl : juce::jlimit (-8.0f, 8.0f, y);
            buf[(size_t) (combPos & combMask)] = undenormalise (y);
            combPos = (combPos + 1) & combMask;
            const float wet = y * (1.0f - std::abs (fb) * 0.5f);
            x[i] = x[i] + (wet - x[i]) * mix;
            d += dd;
            mix += dMix;
        }
        return;
    }

    if (type == Formant)
    {
        float f0[3], f1[3];
        vowelAt (fc0, f0);
        vowelAt (fc1, f1);
        float g0[3], g1[3];
        for (int k = 0; k < 3; ++k) { g0[k] = tanG (f0[k]); g1[k] = tanG (f1[k]); }
        const float kq = 0.5f - 0.45f * juce::jlimit (0.0f, 1.0f, cur.reso);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i * invN;
            const float in = driven ? fastTanh (x[i] * dg) * dgComp : x[i];
            float lp, bp, hp, acc = 0.0f;
            Svf* f[3] = { &c.f1, &c.f2, &c.f3 };
            for (int k = 0; k < 3; ++k)
            {
                f[k]->tick (in, g0[k] + (g1[k] - g0[k]) * t, kq, nl, lp, bp, hp);
                acc += bp * kVowelGain[k];
            }
            const float wet = acc * 1.8f;
            x[i] = x[i] + (wet - x[i]) * mix;
            mix += dMix;
        }
        return;
    }

    const float gA = tanG (fc0), gB = tanG (fc1);
    const float dG = (gB - gA) * invN;
    const float res0 = juce::jlimit (0.0f, 1.0f, prev.reso), res1 = juce::jlimit (0.0f, 1.0f, cur.reso);

    if (type == LadderLP || type == LadderHP)
    {
        float k = res0 * 3.96f;
        const float dk = (res1 - res0) * 3.96f * invN;
        float g = gA;
        for (int i = 0; i < n; ++i)
        {
            const float G = g / (1.0f + g);
            const float b = 1.0f / (1.0f + g);
            const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
            const float S = G3 * b * c.s[0] + G2 * b * c.s[1] + G * b * c.s[2] + b * c.s[3];
            float in = x[i] * (1.0f + 0.5f * k);
            if (driven) in = fastTanh (in * dg) * dgComp * 1.4f;
            float u = (in - k * S) / (1.0f + k * G4);
            if (nl > 0.0f || driven)
                u = fastTanh (u);
            float y[4];
            float v = u;
            for (int s = 0; s < 4; ++s)
            {
                const float vv = (v - c.s[s]) * G;
                y[s] = vv + c.s[s];
                c.s[s] = undenormalise (y[s] + vv);
                v = y[s];
            }
            const float wet = type == LadderLP ? y[3] : (u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3]);
            x[i] = x[i] + (wet - x[i]) * mix;
            g += dG; k += dk; mix += dMix;
        }
        return;
    }

    // ---- state-variable family -----------------------------------------------------------------
    float k = 2.0f - 1.97f * res0;
    const float dk = -1.97f * (res1 - res0) * invN;
    constexpr float kButter = 1.41421356f;
    float g = gA;
    for (int i = 0; i < n; ++i)
    {
        const float in = driven ? fastTanh (x[i] * dg) * dgComp : x[i];
        float lp, bp, hp, wet;
        c.a.tick (in, g, k, nl, lp, bp, hp);
        switch (type)
        {
            case LP12: wet = lp; break;
            case HP12: wet = hp; break;
            case BP12: wet = bp * k; break;
            case Notch: wet = in - k * bp; break;
            case Peak: wet = lp - hp; break;
            case AllPass: wet = in - 2.0f * k * bp; break;
            case LP24: { float l2, b2, h2; c.b.tick (lp, g, kButter, 0.0f, l2, b2, h2); wet = l2; break; }
            case HP24: { float l2, b2, h2; c.b.tick (hp, g, kButter, 0.0f, l2, b2, h2); wet = h2; break; }
            case BP24: { float l2, b2, h2; c.b.tick (bp * k, g, kButter, 0.0f, l2, b2, h2); wet = b2 * kButter; break; }
            default: wet = lp; break;
        }
        x[i] = x[i] + (wet - x[i]) * mix;
        g += dG; k += dk; mix += dMix;
    }
}
} // namespace dw

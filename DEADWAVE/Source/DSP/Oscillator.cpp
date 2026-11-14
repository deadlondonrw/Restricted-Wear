#include "Oscillator.h"

namespace dw
{
// ======================================================================================= samples
void SampleAsset::buildLevels()
{
    // Windowed-sinc half-band low-pass (63 taps) for each 2:1 decimation step.
    constexpr int taps = 63;
    float h[taps];
    for (int i = 0; i < taps; ++i)
    {
        const double m = i - (taps - 1) / 2.0;
        const double sinc = std::abs (m) < 1e-9 ? 0.5 : std::sin (juce::MathConstants<double>::pi * 0.5 * m) / (juce::MathConstants<double>::pi * m);
        const double win = 0.42 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (taps - 1))
                         + 0.08 * std::cos (4.0 * juce::MathConstants<double>::pi * i / (taps - 1));
        h[i] = (float) (sinc * win);
    }
    float sum = 0.0f;
    for (auto v : h) sum += v;
    for (auto& v : h) v /= sum;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& base = data[0][ch];
        if (base.size() < 4) continue;
        const int baseLen = (int) base.size() - 4;
        for (int k = 1; k < kLevels; ++k)
        {
            const auto& src = data[k - 1][ch];
            const int srcLen = (int) src.size() - 4;
            const int dstLen = juce::jmax (1, srcLen / 2);
            auto& dst = data[k][ch];
            dst.assign ((size_t) dstLen + 4, 0.0f);
            for (int i = 0; i < dstLen; ++i)
            {
                float acc = 0.0f;
                const int c = i * 2;
                for (int t = 0; t < taps; ++t)
                {
                    const int idx = c + t - (taps - 1) / 2;
                    if (idx >= 0 && idx < srcLen)
                        acc += src[(size_t) idx] * h[t];
                }
                dst[(size_t) i] = acc;
            }
        }
        juce::ignoreUnused (baseLen);
    }
}

// ==================================================================================== oscillator
void Oscillator::noteOn (const OscTick& t, float phaseRand, Rng& rng, bool resetPhase, double sampleStart) noexcept
{
    if (resetPhase)
    {
        for (auto& u : uni)
            u.phase = (double) (phaseRand * rng.uni());
        fmPhase = 0.0;
        noiseLpL = noiseLpR = noiseHpL = noiseHpR = shL = shR = 0.0f;
        shPhase = 0.0;
        crushHoldL = crushHoldR = crushCount = 0.0f;
        sampleDone = false;
        samplePos = juce::jmax (0.0, sampleStart);
        lastLevel = -1;
    }

    // Each note gets a fresh random stereo/analog offset per unison voice.
    for (auto& s : stereoOffsets)
        s = rng.bi();
    lastUnison = 0; // force unison recompute
    juce::ignoreUnused (t);
}

void Oscillator::updateUnison (const OscTick& t) noexcept
{
    const int U = juce::jlimit (1, kMaxUnison, t.unison);
    if (U == lastUnison && t.detune == lastDetune && t.spread == lastSpread && t.blend == lastBlend
        && t.pan == lastPan && t.stereoVar == lastStereoVar)
        return;

    lastUnison = U; lastDetune = t.detune; lastSpread = t.spread; lastBlend = t.blend; lastPan = t.pan;
    lastStereoVar = t.stereoVar;

    float weights[kMaxUnison];
    float sumSq = 0.0f;
    const float maxCents = t.detune * t.detune * 100.0f;

    for (int u = 0; u < U; ++u)
    {
        const float pos = U == 1 ? 0.0f : -1.0f + 2.0f * (float) u / (float) (U - 1);
        uni[u].detuneRatio = std::exp2 (maxCents * pos / 1200.0f);

        const bool inner = U <= 2 || (U % 2 == 1 ? u == U / 2 : (u == U / 2 || u == U / 2 - 1));
        weights[u] = inner ? 1.0f : t.blend;
        sumSq += weights[u] * weights[u];

        float side = 0.0f;
        if (U > 1)
            side = (u % 2 == 0 ? -1.0f : 1.0f) * std::abs (pos);
        const float pan = juce::jlimit (-1.0f, 1.0f, t.pan + t.spread * side + t.stereoVar * 0.35f * stereoOffsets[u]);
        const float theta = (pan + 1.0f) * kPi * 0.25f;
        uni[u].gainL = std::cos (theta) * juce::MathConstants<float>::sqrt2 * weights[u];
        uni[u].gainR = std::sin (theta) * juce::MathConstants<float>::sqrt2 * weights[u];
    }

    uniNorm = sumSq > 0.0f ? 1.0f / std::sqrt (sumSq) : 1.0f;
    for (int u = 0; u < U; ++u)
    {
        uni[u].gainL *= uniNorm;
        uni[u].gainR *= uniNorm;
    }
}

namespace
{
template <int Warp>
inline float applyWarp (float p, float s) noexcept
{
    if constexpr (Warp == 0) // bend (Mobius map, bounded slope)
    {
        const float k = std::exp2 (s * 3.0f);
        return p / (p + k * (1.0f - p));
    }
    else if constexpr (Warp == 1) // squeeze
    {
        const float d = 0.5f + 0.48f * s;
        return p < d ? 0.5f * p / d : 0.5f + 0.5f * (p - d) / (1.0f - d);
    }
    else if constexpr (Warp == 2) // pulse width
    {
        const float w = 1.0f - 0.95f * std::abs (s);
        if (s >= 0.0f)
            return p < w ? p / w : 0.0f;
        const float start = 1.0f - w;
        return p < start ? 0.0f : (p - start) / w;
    }
    else if constexpr (Warp == 3) // sync
    {
        const float r = 1.0f + std::abs (s) * 15.0f;
        return wrap01 (p * r);
    }
    else if constexpr (Warp == 4) // formant
    {
        const float r = 1.0f + std::abs (s) * 7.0f;
        return wrap01 (p * r);
    }
    else if constexpr (Warp == 6) // quantize
    {
        const float a = std::abs (s);
        if (a < 0.001f) return p;
        const float q = std::exp2 (11.0f - 10.0f * a);
        return std::floor (p * q) / q;
    }
    else if constexpr (Warp == 7) // phase FM (self)
    {
        return wrap01 (p + s * 0.25f * std::sin (kTwoPi * p));
    }
    else
    {
        return p;
    }
}

inline float readLevel (const float* a, const float* b, int size, float ph, float fmix) noexcept
{
    const float idx = ph * (float) size;
    int i = (int) idx;
    const float t = idx - (float) i;
    i = juce::jlimit (0, size - 1, i);
    const float va = hermite (a[i - 1], a[i], a[i + 1], a[i + 2], t);
    if (a == b) return va;
    const float vb = hermite (b[i - 1], b[i], b[i + 1], b[i + 2], t);
    return va + (vb - va) * fmix;
}
} // namespace

template <int Warp>
void Oscillator::renderWavetable (const Wavetable* wt, const OscTick& prev, const OscTick& cur, const float* fmIn,
                                  float* outL, float* outR, float* monoOut, int n) noexcept
{
    const int U = juce::jlimit (1, kMaxUnison, cur.unison);
    const int nf = wt->getNumFrames();

    // ---- mip level selection (band-limiting) --------------------------------------------------
    float maxRatio = 1.0f;
    for (int u = 0; u < U; ++u)
        maxRatio = juce::jmax (maxRatio, uni[u].detuneRatio);
    const float fmax = juce::jmax (prev.freq, cur.freq) * maxRatio;
    const float warpSlope = juce::jmax (warp::slope (Warp, prev.shape), warp::slope (Warp, cur.shape));
    float fmSpread = 0.0f;
    if (cur.fmSrc != 0 && cur.fmAmt > 0.0f)
    {
        const float beta = kTwoPi * 2.0f * cur.fmAmt * cur.fmAmt;
        const float modRatio = cur.fmSrc == 1 ? cur.fmRatio : 4.0f;
        fmSpread = modRatio * (beta + 1.0f);
    }
    const float maxH = harmonicLimit * (float) fs / juce::jmax (1.0f, fmax * warpSlope) - fmSpread;
    const int level = Wavetable::levelForMaxHarmonics (juce::jmax (1.0f, maxH));
    const int size = Wavetable::levelSize (level);
    const bool xfade = lastLevel >= 0 && lastLevel != level;
    const int oldLevel = xfade ? lastLevel : level;
    const int oldSize = Wavetable::levelSize (oldLevel);
    lastLevel = level;

    // ---- ramps --------------------------------------------------------------------------------
    const float invN = 1.0f / (float) n;
    const float fp0 = juce::jlimit (0.0f, 1.0f, prev.wtPos) * (float) (nf - 1);
    const float fp1 = juce::jlimit (0.0f, 1.0f, cur.wtPos) * (float) (nf - 1);
    const float dFp = (fp1 - fp0) * invN;
    const float dBias = (cur.bias - prev.bias) * invN;
    const float dAmp = (cur.amp - prev.amp) * invN;
    float dPh = cur.phase - prev.phase;
    if (dPh > 0.5f) dPh -= 1.0f;
    if (dPh < -0.5f) dPh += 1.0f;
    dPh *= invN;
    const float dShape = (cur.shape - prev.shape) * invN;
    const double inc0 = (double) prev.freq / fs, inc1 = (double) cur.freq / fs;
    const double dInc = (inc1 - inc0) / (double) n;
    const float fmD0 = 2.0f * prev.fmAmt * prev.fmAmt, fmD1 = 2.0f * cur.fmAmt * cur.fmAmt;
    const float dFm = (fmD1 - fmD0) * invN;
    const bool fmSine = cur.fmSrc == 1 && (fmD0 > 0.0f || fmD1 > 0.0f);
    const bool fmExt = cur.fmSrc >= 2 && fmIn != nullptr && (fmD0 > 0.0f || fmD1 > 0.0f);
    const double fmRatio = (double) cur.fmRatio;

    float fpos = fp0, bias = prev.bias, amp = prev.amp, phOff = prev.phase, shape = prev.shape, fmD = fmD0;
    double inc = inc0;

    for (int i = 0; i < n; ++i)
    {
        const int fa = juce::jlimit (0, nf - 1, (int) fpos);
        const int fb = juce::jmin (fa + 1, nf - 1);
        const float fmix = fpos - (float) fa;
        const float dcVal = cur.dcRemove ? wt->frameDc (fa) + (wt->frameDc (fb) - wt->frameDc (fa)) * fmix : 0.0f;

        const float* la = wt->frameData (level, fa);
        const float* lb = wt->frameData (level, fb);
        const float* oa = wt->frameData (oldLevel, fa);
        const float* ob = wt->frameData (oldLevel, fb);
        const float xf = xfade ? (float) i * invN : 1.0f;

        float pm = phOff;
        if (fmSine)
        {
            pm += fmD * std::sin (kTwoPi * (float) fmPhase);
            fmPhase += inc * fmRatio;
            fmPhase -= std::floor (fmPhase);
        }
        else if (fmExt)
        {
            pm += fmD * fmIn[i];
        }

        float sumL = 0.0f, sumR = 0.0f, mono = 0.0f;
        for (int u = 0; u < U; ++u)
        {
            auto& uv = uni[u];
            float p = (float) uv.phase + pm;
            p -= std::floor (p);

            float s;
            if constexpr (Warp == 5) // mirror: crossfade forward read with palindrome read
            {
                const float a = std::abs (shape);
                const float pmir = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
                float s1 = readLevel (la, lb, size, p, fmix);
                float s2 = readLevel (la, lb, size, juce::jmin (pmir, 0.99999f), fmix);
                if (xfade)
                {
                    s1 = readLevel (oa, ob, oldSize, p, fmix) + (s1 - readLevel (oa, ob, oldSize, p, fmix)) * xf;
                    s2 = readLevel (oa, ob, oldSize, juce::jmin (pmir, 0.99999f), fmix)
                       + (s2 - readLevel (oa, ob, oldSize, juce::jmin (pmir, 0.99999f), fmix)) * xf;
                }
                s = s1 + (s2 - s1) * a;
            }
            else
            {
                const float pw = juce::jlimit (0.0f, 0.99999f, applyWarp<Warp> (p, shape));
                s = readLevel (la, lb, size, pw, fmix);
                if (xfade)
                {
                    const float so = readLevel (oa, ob, oldSize, pw, fmix);
                    s = so + (s - so) * xf;
                }
                if constexpr (Warp == 4) // formant window removes the reset discontinuity
                    s *= 0.5f - 0.5f * std::cos (kTwoPi * p);
            }

            const float v = amp * (s - dcVal) + bias;
            sumL += v * uv.gainL;
            sumR += v * uv.gainR;
            mono += v * (uv.gainL + uv.gainR);

            uv.phase += inc * (double) uv.detuneRatio;
            if (uv.phase >= 1.0) uv.phase -= std::floor (uv.phase);
        }

        tmpL[i] = sumL;
        tmpR[i] = sumR;
        monoOut[i] = mono * 0.5f;

        fpos += dFp; bias += dBias; amp += dAmp; phOff += dPh; shape += dShape; inc += dInc; fmD += dFm;
    }

    juce::ignoreUnused (outL, outR);
}

void Oscillator::renderNoise (const OscTick& prev, const OscTick& cur, float* outL, float* outR, float* monoOut, int n, Rng& rng) noexcept
{
    juce::ignoreUnused (outL, outR);
    const float invN = 1.0f / (float) n;
    const float color = cur.shape;
    // Colour filter coefficients (per tick)
    const float lpHz = color < 0.0f ? 18000.0f * std::pow (1.0f - std::abs (color), 3.0f) + 40.0f : 20000.0f;
    const float hpHz = color > 0.0f ? 20.0f * std::pow (400.0f, color) : 0.0f;
    const float aLp = 1.0f - std::exp (-kTwoPi * juce::jmin (lpHz, (float) fs * 0.45f) / (float) fs);
    const float aHp = 1.0f - std::exp (-kTwoPi * hpHz / (float) fs);
    const float comp = color < 0.0f ? juce::jmin (6.0f, std::sqrt (1.0f / juce::jmax (aLp, 0.01f)) * 0.6f) : 1.0f;

    // Sample & hold rate: wtPos 0 = full rate, 1 = keytracked (pitched digital noise)
    const float shTarget = std::exp (std::log ((float) fs) + (std::log (juce::jmax (20.0f, cur.freq * 2.0f)) - std::log ((float) fs)) * cur.wtPos);
    const double shInc = (double) shTarget / fs;
    const bool sh = cur.wtPos > 0.001f;

    const float theta = (juce::jlimit (-1.0f, 1.0f, cur.pan) + 1.0f) * kPi * 0.25f;
    const float gL = std::cos (theta) * juce::MathConstants<float>::sqrt2, gR = std::sin (theta) * juce::MathConstants<float>::sqrt2;
    float amp = prev.amp, bias = prev.bias;
    const float dAmp = (cur.amp - prev.amp) * invN, dBias = (cur.bias - prev.bias) * invN;

    for (int i = 0; i < n; ++i)
    {
        float wl, wr;
        bool take = true;
        if (sh)
        {
            shPhase += shInc;
            take = shPhase >= 1.0;
            if (take) shPhase -= std::floor (shPhase);
        }
        if (take)
        {
            const float a = rng.bi();
            shL = a;
            shR = a + (rng.bi() - a) * cur.spread;
        }
        wl = shL; wr = shR;

        noiseLpL += aLp * (wl - noiseLpL);
        noiseLpR += aLp * (wr - noiseLpR);
        float l = noiseLpL * comp, r = noiseLpR * comp;
        if (hpHz > 0.0f)
        {
            noiseHpL += aHp * (l - noiseHpL);
            noiseHpR += aHp * (r - noiseHpR);
            l -= noiseHpL; r -= noiseHpR;
        }
        const float vl = amp * l * 0.8f + bias;
        const float vr = amp * r * 0.8f + bias;
        tmpL[i] = vl * gL;
        tmpR[i] = vr * gR;
        monoOut[i] = 0.5f * (vl + vr);
        amp += dAmp; bias += dBias;
    }
}

void Oscillator::renderSample (const SampleAsset* s, const OscTick& prev, const OscTick& cur, float* outL, float* outR,
                               float* monoOut, int n) noexcept
{
    juce::ignoreUnused (outL, outR);
    const float invN = 1.0f / (float) n;
    float amp = prev.amp, bias = prev.bias;
    const float dAmp = (cur.amp - prev.amp) * invN, dBias = (cur.bias - prev.bias) * invN;
    const float theta = (juce::jlimit (-1.0f, 1.0f, cur.pan) + 1.0f) * kPi * 0.25f;
    const float gL = std::cos (theta) * juce::MathConstants<float>::sqrt2, gR = std::sin (theta) * juce::MathConstants<float>::sqrt2;

    if (s == nullptr || s->length < 2)
    {
        for (int i = 0; i < n; ++i)
        {
            tmpL[i] = bias * gL; tmpR[i] = bias * gR; monoOut[i] = bias;
            bias += dBias;
        }
        return;
    }

    const double rootHz = (double) midiToHz (s->rootNote);
    const double r0 = (double) prev.freq / rootHz * s->sourceRate / fs;
    const double r1 = (double) cur.freq / rootHz * s->sourceRate / fs;
    const double dr = (r1 - r0) / (double) n;
    const double rmax = juce::jmax (r0, r1);
    const int level = juce::jlimit (0, SampleAsset::kLevels - 1, rmax <= 1.0 ? 0 : (int) std::ceil (std::log2 (rmax)));
    const double levelScale = 1.0 / (double) (1 << level);
    double rate = r0;
    const bool loop = s->loop && s->loopEnd > s->loopStart + 16;
    const double loopLen = (double) (s->loopEnd - s->loopStart);

    for (int i = 0; i < n; ++i)
    {
        float l = 0.0f, r = 0.0f;
        if (! sampleDone)
        {
            const double p = samplePos * levelScale;
            l = s->read (level, 0, p);
            r = s->numChannels > 1 ? s->read (level, 1, p) : l;
            samplePos += rate;
            if (loop && samplePos >= (double) s->loopEnd)
                samplePos -= loopLen;
            else if (samplePos >= (double) (s->length - 1))
                sampleDone = true;
        }
        const float vl = amp * l + bias, vr = amp * r + bias;
        tmpL[i] = vl * gL;
        tmpR[i] = vr * gR;
        monoOut[i] = 0.5f * (vl + vr);
        rate += dr; amp += dAmp; bias += dBias;
    }
}

void Oscillator::applyInserts (const OscTick& prev, const OscTick& cur, float* l, float* r, int n) noexcept
{
    // ---- drive --------------------------------------------------------------------------------
    if (prev.drive > 1.0e-4f || cur.drive > 1.0e-4f)
    {
        const float invN = 1.0f / (float) n;
        float d = prev.drive;
        const float dd = (cur.drive - prev.drive) * invN;
        const int mode = cur.driveMode;
        for (int i = 0; i < n; ++i)
        {
            const float g = 1.0f + d * d * 31.0f;
            const float wet = juce::jmin (1.0f, d * 8.0f);
            for (float* ch : { l, r })
            {
                const float x = ch[i];
                float y;
                switch (mode)
                {
                    case 1: y = juce::jlimit (-1.0f, 1.0f, g * x); break;
                    case 2: y = std::sin (g * x * kPi * 0.5f); break;
                    case 3: y = (fastTanh (g * x + 0.6f) - 0.53704957f) * 1.25f; break;
                    case 4: y = std::abs (fastTanh (g * x)) * 1.5f; break;
                    case 0:
                    default: y = fastTanh (g * x) / fastTanh (juce::jmax (1.0f, g * 0.5f)); break;
                }
                ch[i] = x + (y - x) * wet;
            }
            d += dd;
        }
    }

    // ---- crush --------------------------------------------------------------------------------
    if (cur.crush > 1.0e-4f)
    {
        const float hold = 1.0f + cur.crush * cur.crush * 47.0f;
        const float bits = 24.0f - cur.crush * 20.0f;
        const float q = std::exp2 (bits - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            crushCount += 1.0f;
            if (crushCount >= hold)
            {
                crushCount -= hold;
                crushHoldL = std::round (l[i] * q) / q;
                crushHoldR = std::round (r[i] * q) / q;
            }
            l[i] = crushHoldL;
            r[i] = crushHoldR;
        }
    }
}

void Oscillator::render (const Wavetable* wt, const SampleAsset* sample, const OscTick& prev, const OscTick& cur,
                         const float* fmIn, float* outL, float* outR, float* monoOut, int n, Rng& rng) noexcept
{
    jassert (n <= kMaxBlock);
    updateUnison (cur);

    if (cur.type == 1)
        renderNoise (prev, cur, outL, outR, monoOut, n, rng);
    else if (cur.type == 2)
        renderSample (sample, prev, cur, outL, outR, monoOut, n);
    else if (wt != nullptr)
    {
        switch (cur.warp)
        {
            case 0: renderWavetable<0> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 1: renderWavetable<1> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 2: renderWavetable<2> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 3: renderWavetable<3> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 4: renderWavetable<4> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 5: renderWavetable<5> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 6: renderWavetable<6> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
            case 7:
            default: renderWavetable<7> (wt, prev, cur, fmIn, outL, outR, monoOut, n); break;
        }
    }
    else
    {
        for (int i = 0; i < n; ++i) { tmpL[i] = tmpR[i] = monoOut[i] = 0.0f; }
    }

    applyInserts (prev, cur, tmpL, tmpR, n);

    const float invN = 1.0f / (float) n;
    float lvl = prev.level;
    const float dl = (cur.level - prev.level) * invN;
    for (int i = 0; i < n; ++i)
    {
        outL[i] += tmpL[i] * lvl;
        outR[i] += tmpR[i] * lvl;
        lvl += dl;
    }
}
} // namespace dw

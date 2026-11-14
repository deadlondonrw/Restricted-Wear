#include "Effects.h"

namespace dw::fx
{
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;

/** Fuller-sounding dry/wet law: both at unity at 50 %. */
inline void dryWet (float mix, float& dry, float& wet) noexcept
{
    dry = juce::jmin (1.0f, 2.0f * (1.0f - mix));
    wet = juce::jmin (1.0f, 2.0f * mix);
}

inline float onePoleCoef (double fs, double hz) noexcept
{
    return (float) (1.0 - std::exp (-2.0 * pi * juce::jlimit (1.0, fs * 0.49, hz) / fs));
}
} // namespace

// ============================================================================================ biquad
void Biquad::setLowPass (double fs, double f, double q)
{
    const double w = 2.0 * pi * juce::jlimit (10.0, fs * 0.49, f) / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) ((1.0 - c) * 0.5 / a0); b1 = (float) ((1.0 - c) / a0); b2 = b0;
    a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
}

void Biquad::setHighPass (double fs, double f, double q)
{
    const double w = 2.0 * pi * juce::jlimit (10.0, fs * 0.49, f) / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    b0 = (float) ((1.0 + c) * 0.5 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
    a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
}

void Biquad::setPeak (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * pi * juce::jlimit (10.0, fs * 0.49, f) / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al / A;
    b0 = (float) ((1.0 + al * A) / a0); b1 = (float) (-2.0 * c / a0); b2 = (float) ((1.0 - al * A) / a0);
    a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al / A) / a0);
}

void Biquad::setLowShelf (double fs, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * pi * juce::jlimit (10.0, fs * 0.49, f) / fs, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1) + (A - 1) * c + sq;
    b0 = (float) (A * ((A + 1) - (A - 1) * c + sq) / a0);
    b1 = (float) (2 * A * ((A - 1) - (A + 1) * c) / a0);
    b2 = (float) (A * ((A + 1) - (A - 1) * c - sq) / a0);
    a1 = (float) (-2 * ((A - 1) + (A + 1) * c) / a0);
    a2 = (float) (((A + 1) + (A - 1) * c - sq) / a0);
}

void Biquad::setHighShelf (double fs, double f, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * pi * juce::jlimit (10.0, fs * 0.49, f) / fs, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1) - (A - 1) * c + sq;
    b0 = (float) (A * ((A + 1) + (A - 1) * c + sq) / a0);
    b1 = (float) (-2 * A * ((A - 1) + (A + 1) * c) / a0);
    b2 = (float) (A * ((A + 1) + (A - 1) * c - sq) / a0);
    a1 = (float) (2 * ((A - 1) - (A + 1) * c) / a0);
    a2 = (float) (((A + 1) - (A - 1) * c - sq) / a0);
}

// ======================================================================================== distortion
void Distortion::prepare (double rate) { fs = rate; reset(); }
void Distortion::reset()
{
    for (auto& u : up) u.reset();
    for (auto& d : down) d.reset();
    toneLp[0] = toneLp[1] = 0.0f;
    driveR = {}; mixR = {};
}

void Distortion::process (float* l, float* r, int n, int mode, float driveDb, float tone, float mix) noexcept
{
    driveR.set (dbToGain (driveDb), n);
    mixR.set (mix, n);
    const float tiltLow = std::exp2 (-(tone - 0.5f) * 2.0f), tiltHigh = std::exp2 ((tone - 0.5f) * 2.0f);
    const float a = onePoleCoef (fs, 900.0);
    const float g0 = driveR.cur;
    const float dg = driveR.step;
    float* chans[2] = { l, r };

    for (int c = 0; c < 2; ++c)
    {
        float* x = chans[c];
        up[c].process (x, os[c], n);
        float g = g0;
        for (int i = 0; i < n; ++i)
        {
            g += dg;
            for (int k = 0; k < 2; ++k)
            {
                float y = os[c][2 * i + k] * g;
                switch (mode)
                {
                    case 1: y = juce::jlimit (-1.0f, 1.0f, y); break;
                    case 2: y = y >= 0.0f ? fastTanh (y) : fastTanh (y * 0.6f) * 1.2f; break;
                    case 3:
                    {
                        float t = (y + 1.0f) * 0.25f;
                        t -= std::floor (t);
                        y = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
                        break;
                    }
                    case 4: y = std::sin (y); break;
                    case 5: y = std::abs (fastTanh (y)) * 1.6f - 0.6f; break;
                    case 0:
                    default: y = fastTanh (y); break;
                }
                os[c][2 * i + k] = y;
            }
        }
        float wetBuf[kMaxBlock];
        down[c].process (os[c], wetBuf, n);

        float m = mixR.cur;
        for (int i = 0; i < n; ++i)
        {
            m += mixR.step;
            toneLp[c] += a * (wetBuf[i] - toneLp[c]);
            const float hp = wetBuf[i] - toneLp[c];
            const float wet = (toneLp[c] * tiltLow + hp * tiltHigh) * 0.8f;
            x[i] = x[i] + (wet - x[i]) * m;
        }
    }
    driveR.finish();
    mixR.finish();
}

// ======================================================================================== saturation
void Saturation::prepare (double rate) { fs = rate; reset(); }
void Saturation::reset() { lp[0] = lp[1] = dcx[0] = dcx[1] = dcy[0] = dcy[1] = 0.0f; driveR = {}; mixR = {}; }

void Saturation::process (float* l, float* r, int n, float driveDb, float bias, float warmth, float mix) noexcept
{
    driveR.set (dbToGain (driveDb), n);
    mixR.set (mix, n);
    const float b = bias * 0.6f;
    const float tb = fastTanh (b);
    const float a = onePoleCoef (fs, 20000.0 - warmth * 16500.0);
    const float dcR = (float) std::exp (-2.0 * pi * 8.0 / fs);
    float* chans[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        float g = driveR.cur, m = mixR.cur;
        float* x = chans[c];
        for (int i = 0; i < n; ++i)
        {
            g += driveR.step; m += mixR.step;
            float y = (fastTanh (g * x[i] + b) - tb) / std::sqrt (g);
            lp[c] += a * (y - lp[c]);
            y = lp[c];
            const float hp = y - dcx[c] + dcR * dcy[c];
            dcx[c] = y; dcy[c] = undenormalise (hp);
            x[i] = x[i] + (hp - x[i]) * m;
        }
    }
    driveR.finish();
    mixR.finish();
}

// ============================================================================================ chorus
void Chorus::prepare (double rate)
{
    fs = rate;
    for (auto& d : line) d.prepare ((int) (fs * 0.07) + 8);
    reset();
}
void Chorus::reset() { for (auto& d : line) d.reset(); phase = 0.0; fbState[0] = fbState[1] = 0.0f; mixR = {}; delayR = {}; }

void Chorus::process (float* l, float* r, int n, float rate, float depth, float delayMs, float fb, float width, float mix) noexcept
{
    mixR.set (mix, n);
    delayR.set (delayMs, n);
    const double inc = (double) rate / fs;
    float* chans[2] = { l, r };
    const float rangeMs = 6.0f * depth;
    float dly = delayR.cur, m = mixR.cur;
    for (int i = 0; i < n; ++i)
    {
        dly += delayR.step; m += mixR.step;
        float dry, wetG;
        dryWet (m, dry, wetG);
        for (int c = 0; c < 2; ++c)
        {
            const double off = c == 0 ? 0.0 : 0.25 * width;
            float acc = 0.0f;
            for (int v = 0; v < 3; ++v)
            {
                const double ph = phase + off + v / 3.0;
                const float lfo = (float) std::sin (2.0 * pi * ph);
                const float dms = dly + rangeMs * (0.5f + 0.5f * lfo) + v * 1.7f;
                acc += line[c].read (juce::jmax (1.0f, dms * 0.001f * (float) fs));
            }
            acc *= (1.0f / 3.0f);
            line[c].push (chans[c][i] + fbState[c] * fb);
            fbState[c] = undenormalise (acc);
            chans[c][i] = chans[c][i] * dry + acc * wetG;
        }
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
    mixR.finish(); delayR.finish();
}

// ============================================================================================ phaser
void Phaser::prepare (double rate) { fs = rate; reset(); }
void Phaser::reset()
{
    for (auto& c : ap) for (auto& s : c) s = 0.0f;
    fbState[0] = fbState[1] = 0.0f;
    phase = 0.0;
    mixR = {};
}

void Phaser::process (float* l, float* r, int n, float rate, float depth, float freq, float fb, int stages, float mix) noexcept
{
    static const int stageCount[] = { 2, 4, 6, 8, 12 };
    const int S = stageCount[juce::jlimit (0, 4, stages)];
    mixR.set (mix, n);
    const double inc = (double) rate / fs;
    float* chans[2] = { l, r };
    float m = mixR.cur;
    for (int i = 0; i < n; ++i)
    {
        m += mixR.step;
        for (int c = 0; c < 2; ++c)
        {
            const float lfo = (float) std::sin (2.0 * pi * (phase + (c == 0 ? 0.0 : 0.25)));
            const float fc = juce::jlimit (20.0f, (float) fs * 0.45f, freq * std::exp2 (depth * 3.0f * lfo));
            const float t = std::tan (kPi * fc / (float) fs);
            const float a = (t - 1.0f) / (t + 1.0f);
            float x = chans[c][i] + fbState[c] * fb;
            for (int s = 0; s < S; ++s)
            {
                const float y = a * x + ap[c][s];
                ap[c][s] = undenormalise (x - a * y);
                x = y;
            }
            fbState[c] = x;
            chans[c][i] = chans[c][i] + (x - chans[c][i]) * m * 0.5f;
        }
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
    mixR.finish();
}

// =========================================================================================== flanger
void Flanger::prepare (double rate)
{
    fs = rate;
    for (auto& d : line) d.prepare ((int) (fs * 0.035) + 8);
    reset();
}
void Flanger::reset() { for (auto& d : line) d.reset(); fbState[0] = fbState[1] = 0.0f; phase = 0.0; mixR = {}; }

void Flanger::process (float* l, float* r, int n, float rate, float depth, float delayMs, float fb, float mix) noexcept
{
    mixR.set (mix, n);
    const double inc = (double) rate / fs;
    float* chans[2] = { l, r };
    float m = mixR.cur;
    for (int i = 0; i < n; ++i)
    {
        m += mixR.step;
        for (int c = 0; c < 2; ++c)
        {
            const float lfo = (float) std::sin (2.0 * pi * (phase + (c == 0 ? 0.0 : 0.25)));
            const float dms = juce::jmax (0.05f, delayMs * (1.0f + depth * 0.95f * lfo));
            const float y = line[c].read (juce::jmax (1.0f, dms * 0.001f * (float) fs));
            line[c].push (chans[c][i] + fbState[c] * fb);
            fbState[c] = undenormalise (y);
            chans[c][i] = chans[c][i] + (y - chans[c][i]) * m * 0.5f;
        }
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
    }
    mixR.finish();
}

// ============================================================================================= delay
void Delay::prepare (double rate)
{
    fs = rate;
    for (auto& d : line) d.prepare ((int) (fs * 4.1) + 8);
    reset();
}
void Delay::reset()
{
    for (auto& d : line) d.reset();
    curDelay[0] = curDelay[1] = -1.0f;
    hpState[0] = hpState[1] = lpState[0] = lpState[1] = 0.0f;
    mixR = {}; fbR = {};
}

void Delay::process (float* l, float* r, int n, float timeMsL, float timeMsR, float fb, bool pingPong, float lowCut,
                     float highCut, float mix) noexcept
{
    mixR.set (mix, n);
    fbR.set (fb, n);
    const float target[2] = { juce::jlimit (1.0f, (float) line[0].capacity() - 4.0f, timeMsL * 0.001f * (float) fs),
                              juce::jlimit (1.0f, (float) line[1].capacity() - 4.0f, timeMsR * 0.001f * (float) fs) };
    for (int c = 0; c < 2; ++c)
        if (curDelay[c] < 0.0f) curDelay[c] = target[c];
    const float slew = 1.0f - std::exp (-1.0f / (0.08f * (float) fs));
    const float aHp = onePoleCoef (fs, lowCut), aLp = onePoleCoef (fs, highCut);
    float m = mixR.cur, f = fbR.cur;
    for (int i = 0; i < n; ++i)
    {
        m += mixR.step; f += fbR.step;
        float y[2];
        for (int c = 0; c < 2; ++c)
        {
            curDelay[c] += (target[c] - curDelay[c]) * slew;
            y[c] = line[c].read (curDelay[c]);
            lpState[c] += aLp * (y[c] - lpState[c]);
            hpState[c] += aHp * (lpState[c] - hpState[c]);
        }
        const float fbL = (lpState[0] - hpState[0]) * f, fbR2 = (lpState[1] - hpState[1]) * f;
        if (pingPong)
        {
            line[0].push (softClip (0.5f * (l[i] + r[i]) + fbR2));
            line[1].push (softClip (fbL));
        }
        else
        {
            line[0].push (softClip (l[i] + fbL));
            line[1].push (softClip (r[i] + fbR2));
        }
        float dry, wet;
        dryWet (m, dry, wet);
        l[i] = l[i] * dry + y[0] * wet;
        r[i] = r[i] * dry + y[1] * wet;
    }
    mixR.finish(); fbR.finish();
}

// ============================================================================================ reverb
void Reverb::prepare (double rate)
{
    fs = rate;
    const double scale = fs / 1000.0;
    for (auto& d : lines) d.prepare ((int) (75.0 * 2.0 * scale) + 256);
    pre.prepare ((int) (0.26 * fs) + 8);
    const float diffMs[4] = { 3.2f, 2.4f, 8.6f, 6.3f };
    for (int i = 0; i < 4; ++i)
    {
        diffLen[i] = diffMs[i] * 0.001f * (float) fs;
        diff[i].prepare ((int) diffLen[i] + 8);
    }
    reset();
}

void Reverb::reset()
{
    for (auto& d : lines) d.reset();
    pre.reset();
    for (auto& d : diff) d.reset();
    for (auto& d : damp) d = 0.0f;
    lastSize = -1.0f;
    lfo = 0.0;
    mixR = {};
}

void Reverb::process (float* l, float* r, int n, float size, float decay, float dampAmt, float predelayMs, float width,
                      float mod, float mix) noexcept
{
    static const float baseMs[kLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
    mixR.set (mix, n);
    const float scale = 0.35f + 1.65f * size;
    float gains[kLines];
    for (int k = 0; k < kLines; ++k)
    {
        baseLen[k] = baseMs[k] * scale * 0.001f * (float) fs;
        gains[k] = std::pow (10.0f, -3.0f * (baseLen[k] / (float) fs) / juce::jmax (0.1f, decay));
    }
    const float aDamp = onePoleCoef (fs, 18000.0 - dampAmt * 16000.0);
    const float preD = juce::jmax (1.0f, predelayMs * 0.001f * (float) fs);
    const double lfoInc = 0.37 / fs;
    const float modDepth = mod * 0.0012f * (float) fs;
    float m = mixR.cur;

    for (int i = 0; i < n; ++i)
    {
        m += mixR.step;
        pre.push (0.5f * (l[i] + r[i]));
        float x = pre.read (preD);
        // input diffusion
        for (int d = 0; d < 4; ++d)
        {
            const float delayed = diff[d].readInt ((int) diffLen[d]);
            const float v = x + 0.6f * delayed;
            diff[d].push (v);
            x = delayed - 0.6f * v;
        }

        float out[kLines];
        const float ml = (float) std::sin (2.0 * pi * lfo) * modDepth;
        for (int k = 0; k < kLines; ++k)
        {
            float d = baseLen[k];
            if (k == 0) d += ml;
            if (k == 5) d -= ml * 0.8f;
            out[k] = lines[k].read (juce::jmax (2.0f, d));
            damp[k] += aDamp * (out[k] - damp[k]);
            out[k] = damp[k] * gains[k];
        }
        // Householder mixing
        float sum = 0.0f;
        for (auto v : out) sum += v;
        sum *= 2.0f / (float) kLines;
        for (int k = 0; k < kLines; ++k)
        {
            const float in = (k % 2 == 0 ? x : -x) * 0.5f;
            lines[k].push (undenormalise (out[k] - sum + in));
        }

        const float wl = out[0] + out[2] - out[4] + out[6];
        const float wr = out[1] - out[3] + out[5] + out[7];
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        float dry, wet;
        dryWet (m, dry, wet);
        l[i] = l[i] * dry + (mid + side) * wet * 0.7f;
        r[i] = r[i] * dry + (mid - side) * wet * 0.7f;
        lfo += lfoInc;
        if (lfo >= 1.0) lfo -= 1.0;
    }
    mixR.finish();
}

// ======================================================================================== compressor
void Compressor::prepare (double rate) { fs = rate; reset(); }
void Compressor::reset() { envDb = -120.0f; lastGr = 0.0f; mixR = {}; }

void Compressor::process (float* l, float* r, int n, float threshDb, float ratio, float attackMs, float releaseMs,
                          float kneeDb, float makeupDb, float mix) noexcept
{
    mixR.set (mix, n);
    const float att = 1.0f - std::exp (-1.0f / (juce::jmax (0.05f, attackMs) * 0.001f * (float) fs));
    const float rel = 1.0f - std::exp (-1.0f / (juce::jmax (1.0f, releaseMs) * 0.001f * (float) fs));
    const float slope = 1.0f / juce::jmax (1.0f, ratio) - 1.0f;
    float m = mixR.cur;
    float maxGr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        m += mixR.step;
        const float peak = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        const float lvl = gainToDb (peak);
        envDb += (lvl - envDb) * (lvl > envDb ? att : rel);
        const float over = envDb - threshDb;
        float gr;
        if (2.0f * over < -kneeDb) gr = 0.0f;
        else if (kneeDb > 0.0f && 2.0f * std::abs (over) <= kneeDb)
            gr = slope * (over + kneeDb * 0.5f) * (over + kneeDb * 0.5f) / (2.0f * kneeDb);
        else gr = slope * over;
        maxGr = juce::jmin (maxGr, gr);
        const float g = dbToGain (gr + makeupDb);
        l[i] = l[i] + (l[i] * g - l[i]) * m;
        r[i] = r[i] + (r[i] * g - r[i]) * m;
    }
    lastGr = maxGr;
    mixR.finish();
}

// ================================================================================================ EQ
void Equalizer::prepare (double rate) { fs = rate; reset(); }
void Equalizer::reset() { lc.reset(); ls.reset(); pk.reset(); hs.reset(); hc.reset(); valid = false; }

void Equalizer::process (float* l, float* r, int n, float lowCut, float lowFreq, float lowGain, float midFreq, float midGain,
                         float midQ, float highFreq, float highGain, float highCut) noexcept
{
    const float now[9] = { lowCut, lowFreq, lowGain, midFreq, midGain, midQ, highFreq, highGain, highCut };
    bool changed = ! valid;
    for (int i = 0; i < 9 && ! changed; ++i)
        changed = std::abs (now[i] - last[i]) > 1.0e-4f * juce::jmax (1.0f, std::abs (now[i]));
    if (changed)
    {
        lc.setHighPass (fs, lowCut, 0.7071);
        ls.setLowShelf (fs, lowFreq, lowGain);
        pk.setPeak (fs, midFreq, midQ, midGain);
        hs.setHighShelf (fs, highFreq, highGain);
        hc.setLowPass (fs, highCut, 0.7071);
        std::copy (now, now + 9, last);
        valid = true;
    }
    const bool useLc = lowCut > 20.5f, useHc = highCut < 19900.0f;
    const bool useLs = std::abs (lowGain) > 0.05f, usePk = std::abs (midGain) > 0.05f, useHs = std::abs (highGain) > 0.05f;
    float* chans[2] = { l, r };
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            float x = chans[c][i];
            if (useLc) x = lc.process (x, c);
            if (useLs) x = ls.process (x, c);
            if (usePk) x = pk.process (x, c);
            if (useHs) x = hs.process (x, c);
            if (useHc) x = hc.process (x, c);
            chans[c][i] = x;
        }
}

// ============================================================================================ imager
void Imager::prepare (double rate) { fs = rate; reset(); }
void Imager::reset() { sideHp.reset(); lastBass = -1.0f; widthR = {}; panR = {}; }

void Imager::process (float* l, float* r, int n, float width, float bassMonoHz, float pan) noexcept
{
    widthR.set (width, n);
    panR.set (pan, n);
    const bool bass = bassMonoHz > 1.0f;
    if (bass && std::abs (bassMonoHz - lastBass) > 0.01f)
    {
        sideHp.setHighPass (fs, bassMonoHz, 0.7071);
        lastBass = bassMonoHz;
    }
    float w = widthR.cur, p = panR.cur;
    for (int i = 0; i < n; ++i)
    {
        w += widthR.step; p += panR.step;
        const float mid = 0.5f * (l[i] + r[i]);
        float side = 0.5f * (l[i] - r[i]) * w;
        if (bass) side = sideHp.process (side, 0);
        const float gl = p > 0.0f ? 1.0f - p : 1.0f;
        const float gr = p < 0.0f ? 1.0f + p : 1.0f;
        l[i] = (mid + side) * gl;
        r[i] = (mid - side) * gr;
    }
    widthR.finish(); panR.finish();
}

// ======================================================================================== bitcrusher
void Bitcrusher::prepare (double rate) { fs = rate; reset(); }
void Bitcrusher::reset() { phase = 1.0; hold[0] = hold[1] = 0.0f; mixR = {}; }

void Bitcrusher::process (float* l, float* r, int n, float bits, float rateHz, float mix) noexcept
{
    mixR.set (mix, n);
    const float q = std::exp2 (juce::jlimit (1.0f, 24.0f, bits) - 1.0f);
    const bool decimate = rateHz < (float) fs * 0.98f;
    const double inc = (double) rateHz / fs;
    float m = mixR.cur;
    for (int i = 0; i < n; ++i)
    {
        m += mixR.step;
        bool take = true;
        if (decimate)
        {
            phase += inc;
            take = phase >= 1.0;
            if (take) phase -= std::floor (phase);
        }
        if (take)
        {
            hold[0] = std::round (l[i] * q) / q;
            hold[1] = std::round (r[i] * q) / q;
        }
        l[i] = l[i] + (hold[0] - l[i]) * m;
        r[i] = r[i] + (hold[1] - r[i]) * m;
    }
    mixR.finish();
}
} // namespace dw::fx

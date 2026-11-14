#pragma once

#include "../Core/DspUtil.h"
#include "Halfband.h"
#include <array>
#include <vector>

namespace dw::fx
{
constexpr int kMaxBlock = 64;

/** Linear ramp helper for per-chunk parameter changes. */
struct Ramp
{
    float cur = 0.0f, target = 0.0f, step = 0.0f;
    bool init = false;
    void set (float t, int n) noexcept
    {
        if (! init) { cur = target = t; init = true; step = 0.0f; return; }
        target = t;
        step = (target - cur) / (float) juce::jmax (1, n);
    }
    float next() noexcept { cur += step; return cur; }
    void finish() noexcept { cur = target; step = 0.0f; }
};

/** Fractional delay line (power-of-two ring, linear or cubic read). */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        buf.assign ((size_t) juce::nextPowerOfTwo (juce::jmax (16, maxSamples + 4)), 0.0f);
        mask = (int) buf.size() - 1;
        w = 0;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    /** delay in samples (>= 1). */
    float read (float delay) const noexcept
    {
        float rp = (float) w - delay;
        while (rp < 0.0f) rp += (float) (mask + 1);
        const int i = (int) rp;
        const float t = rp - (float) i;
        const float a = buf[(size_t) ((i - 1) & mask)], b0 = buf[(size_t) (i & mask)];
        const float c = buf[(size_t) ((i + 1) & mask)], d = buf[(size_t) ((i + 2) & mask)];
        return hermite (a, b0, c, d, t);
    }
    float readInt (int delay) const noexcept { return buf[(size_t) ((w - delay) & mask)]; }
    int capacity() const noexcept { return mask - 4; }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    float process (float x, int ch) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = undenormalise (b1 * x - a1 * y + z2[ch]);
        z2[ch] = undenormalise (b2 * x - a2 * y);
        return y;
    }
    void setLowPass (double fs, double f, double q);
    void setHighPass (double fs, double f, double q);
    void setPeak (double fs, double f, double q, double gainDb);
    void setLowShelf (double fs, double f, double gainDb);
    void setHighShelf (double fs, double f, double gainDb);
};

// ------------------------------------------------------------------------------------- effects
class Distortion
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, int mode, float driveDb, float tone, float mix) noexcept;
private:
    double fs = 48000.0;
    HalfbandInterpolator up[2];
    HalfbandDecimator down[2];
    float os[2][kMaxBlock * 2] {};
    float toneLp[2] {};
    Ramp driveR, mixR;
};

class Saturation
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float driveDb, float bias, float warmth, float mix) noexcept;
private:
    double fs = 48000.0;
    float lp[2] {}, dcx[2] {}, dcy[2] {};
    Ramp driveR, mixR;
};

class Chorus
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float rate, float depth, float delayMs, float fb, float width, float mix) noexcept;
private:
    double fs = 48000.0;
    DelayLine line[2];
    double phase = 0.0;
    float fbState[2] {};
    Ramp mixR, delayR;
};

class Phaser
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float rate, float depth, float freq, float fb, int stages, float mix) noexcept;
private:
    double fs = 48000.0;
    float ap[2][12] {};
    float fbState[2] {};
    double phase = 0.0;
    Ramp mixR;
};

class Flanger
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float rate, float depth, float delayMs, float fb, float mix) noexcept;
private:
    double fs = 48000.0;
    DelayLine line[2];
    float fbState[2] {};
    double phase = 0.0;
    Ramp mixR;
};

class Delay
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float timeMsL, float timeMsR, float fb, bool pingPong, float lowCut,
                  float highCut, float mix) noexcept;
private:
    double fs = 48000.0;
    DelayLine line[2];
    float curDelay[2] { -1.0f, -1.0f };
    float hpState[2] {}, lpState[2] {};
    Ramp mixR, fbR;
};

class Reverb
{
public:
    static constexpr int kLines = 8;
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float size, float decay, float damp, float predelayMs, float width,
                  float mod, float mix) noexcept;
private:
    double fs = 48000.0;
    DelayLine lines[kLines];
    DelayLine pre;
    DelayLine diff[4];
    float diffLen[4] {};
    float baseLen[kLines] {};
    float damp[kLines] {};
    float lastSize = -1.0f;
    double lfo = 0.0;
    Ramp mixR;
};

class Compressor
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float threshDb, float ratio, float attackMs, float releaseMs, float kneeDb,
                  float makeupDb, float mix) noexcept;
    float getGainReductionDb() const noexcept { return lastGr; }
private:
    double fs = 48000.0;
    float envDb = -120.0f;
    float lastGr = 0.0f;
    Ramp mixR;
};

class Equalizer
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float lowCut, float lowFreq, float lowGain, float midFreq, float midGain,
                  float midQ, float highFreq, float highGain, float highCut) noexcept;
private:
    double fs = 48000.0;
    Biquad lc, ls, pk, hs, hc;
    float last[9] {};
    bool valid = false;
};

class Imager
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float width, float bassMonoHz, float pan) noexcept;
private:
    double fs = 48000.0;
    Biquad sideHp;
    float lastBass = -1.0f;
    Ramp widthR, panR;
};

class Bitcrusher
{
public:
    void prepare (double fs);
    void reset();
    void process (float* l, float* r, int n, float bits, float rateHz, float mix) noexcept;
private:
    double fs = 48000.0;
    double phase = 0.0;
    float hold[2] {};
    Ramp mixR;
};
} // namespace dw::fx

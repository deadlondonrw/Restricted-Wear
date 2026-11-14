#pragma once

#include "../Core/DspUtil.h"
#include "../Core/Params.h"
#include "Wavetable.h"

namespace dw
{
// ===================================================================================== sample data
/** Immutable sample asset shared with the audio thread. Level k is decimated by 2^k (band-limited). */
struct SampleAsset
{
    static constexpr int kLevels = 6;
    juce::String name;
    double sourceRate = 44100.0;
    float rootNote = 60.0f;
    int numChannels = 1;
    int length = 0;
    int loopStart = 0, loopEnd = 0;
    bool loop = false;
    std::vector<float> data[kLevels][2]; // each level padded with 4 guard samples

    float read (int level, int ch, double pos) const noexcept
    {
        const auto& d = data[level][juce::jmin (ch, numChannels - 1)];
        const int n = (int) d.size() - 4;
        if (n <= 1) return 0.0f;
        const int i = (int) pos;
        if (i < 0 || i >= n) return 0.0f;
        const float t = (float) (pos - i);
        const float xm1 = i > 0 ? d[(size_t) i - 1] : d[(size_t) i];
        return hermite (xm1, d[(size_t) i], d[(size_t) i + 1], d[(size_t) i + 2], t);
    }

    /** Builds the decimated mip levels from level 0 (call on the message thread). */
    void buildLevels();
};

// ================================================================================ per-tick settings
/** Values for one oscillator resolved for the current control tick (modulation applied). */
struct OscTick
{
    int type = 0;            // 0 wavetable, 1 noise, 2 sample
    float level = 0.75f;
    float pan = 0.0f;
    float freq = 440.0f;     // Hz, fundamental incl. pitch modulation, glide, bend, drift
    float wtPos = 0.0f;
    float bias = 0.0f;
    float amp = 1.0f;
    float phase = 0.0f;
    float shape = 0.0f;
    int warp = 0;
    int unison = 1;
    float detune = 0.2f;
    float spread = 0.7f;
    float blend = 0.75f;
    int fmSrc = 0;           // 0 off, 1 sine, 2.. = OSC (index-2)
    float fmAmt = 0.0f;
    float fmRatio = 1.0f;
    float drive = 0.0f;
    int driveMode = 0;
    float crush = 0.0f;
    bool dcRemove = true;
    float stereoVar = 0.0f;  // analog stereo variation
};

// ====================================================================================== oscillator
class Oscillator
{
public:
    void prepare (double processRate) noexcept
    {
        fs = processRate;
    }

    /** Called on note start. resetPhase=false keeps phases running (legato / mono). */
    void noteOn (const OscTick& t, float phaseRand, Rng& rng, bool resetPhase, double sampleStart) noexcept;

    /**
        Renders n samples (n <= kMaxBlock) adding into outL/outR and writing the mono pre-level wave
        into monoOut (used as an FM source by later oscillators).

        prev/cur are the settings at the start and end of this control tick; continuous values are
        ramped linearly between them.
    */
    void render (const Wavetable* wt, const SampleAsset* sample, const OscTick& prev, const OscTick& cur,
                 const float* fmIn, float* outL, float* outR, float* monoOut, int n, Rng& rng) noexcept;

    static constexpr int kMaxBlock = 128;

    /** Highest harmonic allowed relative to the process rate (0.5 = Nyquist). In ULTRA the process
        rate is doubled, so the full audible band is always present and the decimator removes the rest. */
    float harmonicLimit = 0.5f;

private:
    struct UniVoice
    {
        double phase = 0.0;
        float detuneRatio = 1.0f;
        float gainL = 1.0f, gainR = 1.0f;
    };

    void updateUnison (const OscTick& t) noexcept;
    template <int Warp>
    void renderWavetable (const Wavetable* wt, const OscTick& prev, const OscTick& cur, const float* fmIn,
                          float* outL, float* outR, float* monoOut, int n) noexcept;
    void renderNoise (const OscTick& prev, const OscTick& cur, float* outL, float* outR, float* monoOut, int n, Rng& rng) noexcept;
    void renderSample (const SampleAsset* s, const OscTick& prev, const OscTick& cur, float* outL, float* outR,
                       float* monoOut, int n) noexcept;
    void applyInserts (const OscTick& prev, const OscTick& cur, float* l, float* r, int n) noexcept;

    double fs = 48000.0;
    UniVoice uni[kMaxUnison];
    int lastUnison = 0;
    float lastDetune = -1.0f, lastSpread = -1.0f, lastBlend = -1.0f, lastPan = -10.0f, lastStereoVar = -1.0f;
    float uniNorm = 1.0f;
    float stereoOffsets[kMaxUnison] {};
    int lastLevel = -1;
    double fmPhase = 0.0;

    // noise
    float noiseLpL = 0.0f, noiseLpR = 0.0f, noiseHpL = 0.0f, noiseHpR = 0.0f, shL = 0.0f, shR = 0.0f;
    double shPhase = 0.0;
    // sample
    double samplePos = 0.0;
    bool sampleDone = false;
    // crush state
    float crushHoldL = 0.0f, crushHoldR = 0.0f, crushCount = 0.0f;

    float tmpL[kMaxBlock] {}, tmpR[kMaxBlock] {};
};

// ===================================================================================== warp funcs
namespace warp
{
/** Maximum phase slope (frequency multiplier) introduced by a warp, used for mip selection. */
inline float slope (int mode, float s) noexcept
{
    const float a = std::abs (s);
    switch (mode)
    {
        case 0: { const float k = std::exp2 (a * 3.0f); return k; }                  // bend
        case 1: { const float d = 0.5f + 0.48f * s; return 0.5f / juce::jmin (d, 1.0f - d); } // squeeze
        case 2: return 1.0f / (1.0f - 0.95f * a);                                      // pulse width
        case 3: return 1.0f + a * 15.0f;                                               // sync
        case 4: return 1.0f + a * 7.0f + 1.0f;                                         // formant (+window)
        case 5: return 1.0f + a;                                                       // mirror
        case 6: return 1.0f;                                                           // quantize
        case 7: return 1.0f + a * kTwoPi * 0.25f;                                      // phase FM
        default: return 1.0f;
    }
}
} // namespace warp
} // namespace dw

#pragma once

#include "../Core/Params.h"
#include "Effects.h"
#include <array>
#include <atomic>

namespace dw
{
/** One effects chain (master or per-oscillator lane). The unit order is user-configurable. */
class FxRack
{
public:
    enum Unit { Distortion, Saturation, Chorus, Phaser, Flanger, Delay, Reverb, Compressor, Eq, Imager, Bitcrusher, NumUnits };

    FxRack();

    static const juce::StringArray& unitNames();
    static int unitOnParam (int unit); // FxP index of the unit's on-switch
    static std::array<int, NumUnits> defaultOrder();

    void prepare (double sampleRate);
    void reset();

    /** v: real parameter values indexed by global parameter index. paramRack: which rack's parameters to read. */
    void process (float* l, float* r, int n, const float* v, int paramRack, double bpm) noexcept;

    bool anyUnitOn (const float* v, int paramRack) const noexcept;

    void setOrder (const std::array<int, NumUnits>& o) noexcept
    {
        for (int i = 0; i < NumUnits; ++i) order[(size_t) i].store (o[(size_t) i], std::memory_order_relaxed);
    }
    std::array<int, NumUnits> getOrder() const noexcept
    {
        std::array<int, NumUnits> o {};
        for (int i = 0; i < NumUnits; ++i) o[(size_t) i] = order[(size_t) i].load (std::memory_order_relaxed);
        return o;
    }

    float getCompressorGainReduction() const noexcept { return comp.getGainReductionDb(); }

private:
    std::array<std::atomic<int>, NumUnits> order;
    fx::Distortion dist;
    fx::Saturation sat;
    fx::Chorus chorus;
    fx::Phaser phaser;
    fx::Flanger flanger;
    fx::Delay delay;
    fx::Reverb reverb;
    fx::Compressor comp;
    fx::Equalizer eq;
    fx::Imager imager;
    fx::Bitcrusher crusher;
    bool wasOn[NumUnits] {};
};
} // namespace dw

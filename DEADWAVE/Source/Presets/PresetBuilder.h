#pragma once

#include "../Core/Params.h"
#include "../DSP/FxRack.h"
#include <juce_data_structures/juce_data_structures.h>

namespace dw
{
/**
    Fluent builder for complete DEADWAVE state trees. Used by the factory library and the sound
    generator so both produce exactly the same format as saved user presets.
*/
class PresetBuilder
{
public:
    PresetBuilder (const juce::String& name, const juce::String& category)
        : root ("DEADWAVE"), params ("PARAMS"), waves ("WAVES"), matrix ("MATRIX"), lfos ("LFOSHAPES")
    {
        root.setProperty ("version", 1, nullptr);
        root.setProperty ("preset", name, nullptr);
        root.setProperty ("category", category, nullptr);
        values.assign ((size_t) P::Count, std::numeric_limits<float>::quiet_NaN());
    }

    /** Sets a parameter by ID (real value). Unknown IDs are a programming error. */
    PresetBuilder& set (const juce::String& id, float value)
    {
        const int idx = Registry::get().indexOf (id);
        jassert (idx >= 0);
        if (idx >= 0)
        {
            const auto& s = Registry::get().spec (idx);
            values[(size_t) idx] = juce::jlimit (s.min, s.max, value);
        }
        return *this;
    }

    PresetBuilder& set (int index, float value)
    {
        const auto& s = Registry::get().spec (index);
        values[(size_t) index] = juce::jlimit (s.min, s.max, value);
        return *this;
    }

    float get (int index) const
    {
        const float v = values[(size_t) index];
        return std::isnan (v) ? Registry::get().spec (index).def : v;
    }

    PresetBuilder& waveFactory (int osc, const juce::String& tableName)
    {
        juce::ValueTree w ("WAVE");
        w.setProperty ("osc", osc, nullptr);
        w.setProperty ("factory", tableName, nullptr);
        w.setProperty ("name", tableName, nullptr);
        replaceWave (osc, w);
        return *this;
    }

    PresetBuilder& waveExpr (int osc, const juce::String& name, const juce::String& expr, int frames = 1)
    {
        juce::ValueTree w ("WAVE");
        w.setProperty ("osc", osc, nullptr);
        w.setProperty ("expr", expr, nullptr);
        w.setProperty ("frames", frames, nullptr);
        w.setProperty ("name", name, nullptr);
        replaceWave (osc, w);
        return *this;
    }

    PresetBuilder& waveRandom (int osc, int style, uint32_t seed, float complexity, int frames)
    {
        juce::ValueTree w ("WAVE");
        w.setProperty ("osc", osc, nullptr);
        w.setProperty ("rstyle", style, nullptr);
        w.setProperty ("rseed", (juce::int64) seed, nullptr);
        w.setProperty ("rcomplex", complexity, nullptr);
        w.setProperty ("frames", frames, nullptr);
        w.setProperty ("name", "Generated", nullptr);
        replaceWave (osc, w);
        return *this;
    }

    PresetBuilder& waveTree (int osc, juce::ValueTree w)
    {
        w.setProperty ("osc", osc, nullptr);
        replaceWave (osc, w);
        return *this;
    }

    PresetBuilder& mod (ModSrc src, const juce::String& dstId, float amount)
    {
        if (slot >= kNumModSlots) return *this;
        juce::ValueTree s ("SLOT");
        s.setProperty ("index", slot, nullptr);
        s.setProperty ("src", modSourceId (src), nullptr);
        s.setProperty ("dst", dstId, nullptr);
        matrix.appendChild (s, nullptr);
        set (P::modAmt (slot), amount);
        ++slot;
        return *this;
    }

    PresetBuilder& lfoShape (int lfo, const juce::String& points)
    {
        juce::ValueTree l ("LFO");
        l.setProperty ("index", lfo, nullptr);
        l.setProperty ("pts", points, nullptr);
        lfos.appendChild (l, nullptr);
        return *this;
    }

    PresetBuilder& description (const juce::String& d)
    {
        root.setProperty ("description", d, nullptr);
        return *this;
    }

    juce::ValueTree build() const
    {
        auto r = root.createCopy();
        juce::ValueTree ps ("PARAMS");
        const auto& reg = Registry::get();
        for (int i = 0; i < P::Count; ++i)
        {
            const float v = values[(size_t) i];
            if (std::isnan (v) || std::abs (v - reg.spec (i).def) < 1.0e-6f) continue;
            juce::ValueTree c ("PARAM");
            c.setProperty ("id", reg.spec (i).id, nullptr);
            c.setProperty ("value", v, nullptr);
            ps.appendChild (c, nullptr);
        }
        r.appendChild (ps, nullptr);
        r.appendChild (waves.createCopy(), nullptr);
        r.appendChild (matrix.createCopy(), nullptr);
        r.appendChild (lfos.createCopy(), nullptr);
        return r;
    }

    int usedModSlots() const noexcept { return slot; }

private:
    void replaceWave (int osc, juce::ValueTree w)
    {
        auto existing = waves.getChildWithProperty ("osc", osc);
        if (existing.isValid()) waves.removeChild (existing, nullptr);
        waves.appendChild (w, nullptr);
    }

    juce::ValueTree root, params, waves, matrix, lfos;
    std::vector<float> values;
    int slot = 0;
};
} // namespace dw

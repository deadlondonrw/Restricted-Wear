#include "Params.h"

namespace dw
{
// =================================================================================== choice tables
namespace Choices
{
#define DW_CHOICES(fn, ...) \
    const juce::StringArray& fn() { static const juce::StringArray a { __VA_ARGS__ }; return a; }

DW_CHOICES (oscType, "Wavetable", "Noise", "Sample")
DW_CHOICES (warpMode, "Bend", "Squeeze", "Pulse Width", "Sync", "Formant", "Mirror", "Quantize", "Phase FM")
DW_CHOICES (fmSource, "Off", "Sine", "OSC 1", "OSC 2", "OSC 3", "OSC 4")
DW_CHOICES (oscRoute, "Filter 1", "Filter 2", "Filter 1+2", "Direct")
DW_CHOICES (driveMode, "Soft", "Hard", "Fold", "Asym", "Rectify")
DW_CHOICES (filterType, "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24", "Notch", "Peak", "All Pass",
            "Ladder LP", "Ladder HP", "Comb +", "Comb -", "Formant")
DW_CHOICES (filterRouting, "Serial", "Parallel")
DW_CHOICES (lfoShape, "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random", "Custom")
DW_CHOICES (lfoMode, "Free", "Retrigger", "One Shot")
DW_CHOICES (syncDivision, "8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8",
            "1/8T", "1/16D", "1/16", "1/16T", "1/32", "1/32T", "1/64")
DW_CHOICES (voiceMode, "Poly", "Mono", "Legato")
DW_CHOICES (glideMode, "Off", "Always", "Overlap Only")
DW_CHOICES (notePriority, "Last", "Low", "High")
DW_CHOICES (quality, "Standard", "Ultra")
DW_CHOICES (distMode, "Soft Clip", "Hard Clip", "Tube", "Foldback", "Sine Fold", "Rectify")
DW_CHOICES (phaserStages, "2", "4", "6", "8", "12")
DW_CHOICES (midiFxMode, "Off", "Arpeggiator", "Sequencer")
DW_CHOICES (arpMode, "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord")
DW_CHOICES (arpVelMode, "As Played", "Fixed")
DW_CHOICES (noteDivision, "1/1", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T",
            "1/32", "1/32T", "1/64")
DW_CHOICES (voiceFxRouting, "Master FX", "Direct Out")
#undef DW_CHOICES

double divisionToBeats (int i)
{
    static const double beats[] = { 32.0, 16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0,
                                    0.375, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, i)];
}

double noteDivisionToBeats (int i)
{
    static const double beats[] = { 4.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.375, 0.25,
                                    1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, i)];
}
} // namespace Choices

// =========================================================================================== spec
juce::NormalisableRange<float> ParamSpec::range() const
{
    if (kind == PKind::Bool)
        return { 0.0f, 1.0f, 1.0f };
    if (kind == PKind::Choice)
        return { 0.0f, (float) juce::jmax (1, choices.size() - 1), 1.0f };
    if (kind == PKind::Int)
        return { min, max, 1.0f };

    juce::NormalisableRange<float> r (min, max);
    if (centre > min && centre < max)
        r.setSkewForCentre (centre);
    return r;
}

static juce::String trimNumber (double v, int decimals)
{
    return juce::String (v, decimals);
}

juce::String ParamSpec::format (float v, bool withUnit) const
{
    if (kind == PKind::Bool)
        return v >= 0.5f ? "On" : "Off";
    if (kind == PKind::Choice)
        return choices[juce::jlimit (0, choices.size() - 1, (int) std::lround (v))];

    auto u = [withUnit] (const char* s) { return withUnit ? juce::String (s) : juce::String(); };

    switch (unit)
    {
        case Unit::Percent:   return juce::String (juce::roundToInt (v * 100.0f)) + u ("%");
        case Unit::Bipolar:
        {
            const int p = juce::roundToInt (v * 100.0f);
            return (p > 0 ? "+" : "") + juce::String (p) + u ("%");
        }
        case Unit::Hz:
            if (v >= 1000.0f) return trimNumber (v / 1000.0, v >= 10000.0f ? 1 : 2) + u (" kHz");
            if (v >= 100.0f)  return juce::String (juce::roundToInt (v)) + u (" Hz");
            if (v >= 1.0f)    return trimNumber (v, 1) + u (" Hz");
            return trimNumber (v, 2) + u (" Hz");
        case Unit::dB:        return (v > 0.05f ? "+" : "") + trimNumber (v, 1) + u (" dB");
        case Unit::Semitones:
        {
            if (kind == PKind::Float && std::abs (v - std::round (v)) > 0.05f)
                return (v > 0 ? "+" : "") + trimNumber (v, 1) + u (" st");
            const int s = juce::roundToInt (v);
            return (s > 0 ? "+" : "") + juce::String (s) + u (" st");
        }
        case Unit::Cents:
        {
            const int c = juce::roundToInt (v);
            return (c > 0 ? "+" : "") + juce::String (c) + u (" ct");
        }
        case Unit::Octaves:   return (v > 0 ? "+" : "") + trimNumber (v, 2) + u (" oct");
        case Unit::Seconds:
            if (v < 0.0095f) return trimNumber (v * 1000.0, 1) + u (" ms");
            if (v < 1.0f)    return juce::String (juce::roundToInt (v * 1000.0f)) + u (" ms");
            return trimNumber (v, 2) + u (" s");
        case Unit::Ms:
            if (v < 10.0f)   return trimNumber (v, 2) + u (" ms");
            if (v < 1000.0f) return trimNumber (v, 1) + u (" ms");
            return trimNumber (v / 1000.0, 2) + u (" s");
        case Unit::Degrees:   return juce::String (juce::roundToInt (v * 360.0f)) + u (juce::CharPointer_UTF8 ("\xc2\xb0"));
        case Unit::Ratio:     return trimNumber (v, v < 10.0f ? 3 : 2) + u ("x");
        case Unit::Voices:    return juce::String (juce::roundToInt (v));
        case Unit::Bits:      return trimNumber (v, 1) + u (" bit");
        case Unit::None:
        default:
            if (kind == PKind::Int) return juce::String (juce::roundToInt (v));
            return trimNumber (v, 2);
    }
}

// ======================================================================================= registry
namespace
{
struct Builder
{
    std::vector<ParamSpec>& out;

    ParamSpec& add (juce::String id, juce::String name, juce::String shortName, Group g)
    {
        ParamSpec s;
        s.id = std::move (id);
        s.name = std::move (name);
        s.shortName = std::move (shortName);
        s.group = g;
        out.push_back (std::move (s));
        return out.back();
    }

    ParamSpec& real (juce::String id, juce::String name, juce::String sn, Group g, float mn, float mx, float def,
                     Unit u, float centre = -1.0f, bool mod = false, bool perVoice = false)
    {
        auto& s = add (std::move (id), std::move (name), std::move (sn), g);
        s.kind = PKind::Float;
        s.min = mn; s.max = mx; s.def = def; s.unit = u; s.centre = centre;
        s.modulatable = mod; s.perVoice = perVoice;
        return s;
    }

    ParamSpec& integer (juce::String id, juce::String name, juce::String sn, Group g, int mn, int mx, int def,
                        Unit u = Unit::None)
    {
        auto& s = add (std::move (id), std::move (name), std::move (sn), g);
        s.kind = PKind::Int;
        s.min = (float) mn; s.max = (float) mx; s.def = (float) def; s.unit = u;
        return s;
    }

    ParamSpec& choice (juce::String id, juce::String name, juce::String sn, Group g, const juce::StringArray& c, int def)
    {
        auto& s = add (std::move (id), std::move (name), std::move (sn), g);
        s.kind = PKind::Choice;
        s.choices = c;
        s.min = 0.0f; s.max = (float) (c.size() - 1); s.def = (float) def;
        return s;
    }

    ParamSpec& toggle (juce::String id, juce::String name, juce::String sn, Group g, bool def)
    {
        auto& s = add (std::move (id), std::move (name), std::move (sn), g);
        s.kind = PKind::Bool;
        s.min = 0.0f; s.max = 1.0f; s.def = def ? 1.0f : 0.0f;
        return s;
    }
};

void addOsc (Builder& b, int i)
{
    const auto n = juce::String (i + 1);
    const auto p = "osc" + n + "_";
    const auto N = "OSC " + n + " ";
    const auto g = oscGroup (i);
    // Order must match OscP.
    b.toggle (p + "on", N + "On", "ON", g, i == 0);
    b.choice (p + "type", N + "Type", "TYPE", g, Choices::oscType(), 0);
    b.real (p + "level", N + "Level", "LEVEL", g, 0.0f, 1.0f, 0.75f, Unit::Percent, -1, true, true);
    b.real (p + "pan", N + "Pan", "PAN", g, -1.0f, 1.0f, 0.0f, Unit::Bipolar, -1, true, true);
    b.integer (p + "oct", N + "Octave", "OCT", g, -4, 4, 0, Unit::None);
    b.integer (p + "semi", N + "Semitone", "SEMI", g, -12, 12, 0, Unit::Semitones);
    b.real (p + "fine", N + "Fine", "FINE", g, -100.0f, 100.0f, 0.0f, Unit::Cents, -1, true, true);
    b.real (p + "pitch", N + "Pitch", "PITCH", g, -48.0f, 48.0f, 0.0f, Unit::Semitones, -1, true, true);
    b.toggle (p + "keytrack", N + "Key Track", "KEY", g, true);
    b.real (p + "wtpos", N + "Wave Position", "WT POS", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real (p + "bias", N + "Vertical Bias", "BIAS", g, -1.0f, 1.0f, 0.0f, Unit::Bipolar, -1, true, true);
    b.real (p + "amp", N + "Wave Amplitude", "AMP", g, 0.0f, 2.0f, 1.0f, Unit::Percent, -1, true, true);
    b.real (p + "phase", N + "Phase", "PHASE", g, 0.0f, 1.0f, 0.0f, Unit::Degrees, -1, true, true);
    b.real (p + "phaserand", N + "Phase Random", "RAND", g, 0.0f, 1.0f, 0.0f, Unit::Percent);
    b.real (p + "shape", N + "Wave Shape", "SHAPE", g, -1.0f, 1.0f, 0.0f, Unit::Bipolar, -1, true, true);
    b.choice (p + "warp", N + "Shape Mode", "MODE", g, Choices::warpMode(), 0);
    b.integer (p + "unison", N + "Unison", "UNISON", g, 1, kMaxUnison, 1, Unit::Voices);
    b.real (p + "detune", N + "Unison Detune", "DETUNE", g, 0.0f, 1.0f, 0.2f, Unit::Percent, -1, true, true);
    b.real (p + "spread", N + "Stereo Spread", "SPREAD", g, 0.0f, 1.0f, 0.7f, Unit::Percent, -1, true, true);
    b.real (p + "blend", N + "Unison Blend", "BLEND", g, 0.0f, 1.0f, 0.75f, Unit::Percent, -1, true, true);
    b.choice (p + "fmsrc", N + "FM Source", "FM SRC", g, Choices::fmSource(), 0);
    b.real (p + "fmamt", N + "FM Amount", "FM", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real (p + "fmratio", N + "FM Ratio", "RATIO", g, 0.125f, 16.0f, 1.0f, Unit::Ratio, 1.0f, true, true);
    b.choice (p + "route", N + "Filter Route", "ROUTE", g, Choices::oscRoute(), 0);
    b.real (p + "drive", N + "Drive", "DRIVE", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.choice (p + "drivemode", N + "Drive Mode", "DRV MODE", g, Choices::driveMode(), 0);
    b.real (p + "crush", N + "Crush", "CRUSH", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.toggle (p + "dcremove", N + "Remove Shape DC", "DC", g, true);
    b.toggle (p + "fxon", N + "FX Lane", "FX", g, false);
    b.toggle (p + "fxlink", N + "Link FX", "LINK", g, false);
}

void addFilter (Builder& b, int i)
{
    const auto n = juce::String (i + 1);
    const auto p = "filter" + n + "_";
    const auto N = "Filter " + n + " ";
    const auto g = filterGroup (i);
    b.toggle (p + "on", N + "On", "ON", g, false);
    b.choice (p + "type", N + "Type", "TYPE", g, Choices::filterType(), 1);
    b.real (p + "cutoff", N + "Cutoff", "CUTOFF", g, 20.0f, 20000.0f, 4000.0f, Unit::Hz, 1000.0f, true, true);
    b.real (p + "reso", N + "Resonance", "RES", g, 0.0f, 1.0f, 0.1f, Unit::Percent, -1, true, true);
    b.real (p + "drive", N + "Drive", "DRIVE", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real (p + "mix", N + "Mix", "MIX", g, 0.0f, 1.0f, 1.0f, Unit::Percent, -1, true, true);
    b.real (p + "keytrack", N + "Key Track", "KEY", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real (p + "envamt", N + "Envelope Amount", "ENV", g, -1.0f, 1.0f, 0.0f, Unit::Bipolar, -1, true, true);
    b.real (p + "spread", N + "Stereo Spread", "SPREAD", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
}

void addEnv (Builder& b, int i)
{
    const auto n = juce::String (i + 1);
    const auto p = "env" + n + "_";
    const auto N = "Env " + n + " ";
    const auto g = envGroup (i);
    const bool amp = i == 0;
    b.real (p + "delay", N + "Delay", "DLY", g, 0.0f, 10.0f, 0.0f, Unit::Seconds, 0.5f, true, true);
    b.real (p + "attack", N + "Attack", "A", g, 0.0f, 20.0f, 0.002f, Unit::Seconds, 0.6f, true, true);
    b.real (p + "hold", N + "Hold", "H", g, 0.0f, 10.0f, 0.0f, Unit::Seconds, 0.5f, true, true);
    b.real (p + "decay", N + "Decay", "D", g, 0.001f, 20.0f, amp ? 0.6f : 0.35f, Unit::Seconds, 0.8f, true, true);
    b.real (p + "sustain", N + "Sustain", "S", g, 0.0f, 1.0f, amp ? 0.8f : 0.0f, Unit::Percent, -1, true, true);
    b.real (p + "release", N + "Release", "R", g, 0.001f, 20.0f, amp ? 0.15f : 0.3f, Unit::Seconds, 0.8f, true, true);
    b.real (p + "acurve", N + "Attack Curve", "A CRV", g, -1.0f, 1.0f, 0.25f, Unit::Bipolar);
    b.real (p + "dcurve", N + "Decay Curve", "D CRV", g, -1.0f, 1.0f, 0.6f, Unit::Bipolar);
    b.real (p + "rcurve", N + "Release Curve", "R CRV", g, -1.0f, 1.0f, 0.6f, Unit::Bipolar);
}

void addLfo (Builder& b, int i)
{
    const auto n = juce::String (i + 1);
    const auto p = "lfo" + n + "_";
    const auto N = "LFO " + n + " ";
    const auto g = lfoGroup (i);
    b.choice (p + "shape", N + "Shape", "SHAPE", g, Choices::lfoShape(), 0);
    b.choice (p + "mode", N + "Mode", "MODE", g, Choices::lfoMode(), 1);
    b.toggle (p + "sync", N + "Tempo Sync", "SYNC", g, true);
    b.real (p + "rate", N + "Rate", "RATE", g, 0.01f, 50.0f, 2.0f, Unit::Hz, 2.0f, true, true);
    b.choice (p + "div", N + "Sync Rate", "DIV", g, Choices::syncDivision(), 7);
    b.real (p + "phase", N + "Phase", "PHASE", g, 0.0f, 1.0f, 0.0f, Unit::Degrees, -1, true, true);
    b.real (p + "smooth", N + "Smooth", "SMOOTH", g, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real (p + "rise", N + "Rise", "RISE", g, 0.0f, 10.0f, 0.0f, Unit::Seconds, 1.0f);
    b.toggle (p + "bipolar", N + "Bipolar", "BIPOLAR", g, true);
}

void addRack (Builder& b, int r)
{
    const auto p = r == 0 ? juce::String ("fx_") : "osc" + juce::String (r) + "fx_";
    const auto N = r == 0 ? juce::String ("FX ") : "OSC " + juce::String (r) + " FX ";
    const auto g = rackGroup (r);
    auto m = [&] (ParamSpec& s) -> ParamSpec& { s.modulatable = true; s.perVoice = false; return s; };

    b.toggle (p + "rack_on", N + "Rack On", "RACK", g, true);

    b.toggle (p + "dist_on", N + "Distortion On", "DIST", g, false);
    b.choice (p + "dist_mode", N + "Distortion Mode", "MODE", g, Choices::distMode(), 0);
    m (b.real (p + "dist_drive", N + "Distortion Drive", "DRIVE", g, 0.0f, 48.0f, 12.0f, Unit::dB));
    m (b.real (p + "dist_tone", N + "Distortion Tone", "TONE", g, 0.0f, 1.0f, 0.5f, Unit::Percent));
    m (b.real (p + "dist_mix", N + "Distortion Mix", "MIX", g, 0.0f, 1.0f, 1.0f, Unit::Percent));

    b.toggle (p + "sat_on", N + "Saturation On", "SAT", g, false);
    m (b.real (p + "sat_drive", N + "Saturation Drive", "DRIVE", g, 0.0f, 24.0f, 6.0f, Unit::dB));
    m (b.real (p + "sat_bias", N + "Saturation Bias", "BIAS", g, -1.0f, 1.0f, 0.2f, Unit::Bipolar));
    m (b.real (p + "sat_tone", N + "Saturation Warmth", "WARMTH", g, 0.0f, 1.0f, 0.5f, Unit::Percent));
    m (b.real (p + "sat_mix", N + "Saturation Mix", "MIX", g, 0.0f, 1.0f, 1.0f, Unit::Percent));

    b.toggle (p + "cho_on", N + "Chorus On", "CHORUS", g, false);
    m (b.real (p + "cho_rate", N + "Chorus Rate", "RATE", g, 0.01f, 10.0f, 0.8f, Unit::Hz, 1.0f));
    m (b.real (p + "cho_depth", N + "Chorus Depth", "DEPTH", g, 0.0f, 1.0f, 0.5f, Unit::Percent));
    m (b.real (p + "cho_delay", N + "Chorus Delay", "DELAY", g, 1.0f, 40.0f, 12.0f, Unit::Ms));
    m (b.real (p + "cho_fb", N + "Chorus Feedback", "FDBK", g, -0.95f, 0.95f, 0.0f, Unit::Bipolar));
    m (b.real (p + "cho_width", N + "Chorus Width", "WIDTH", g, 0.0f, 1.0f, 1.0f, Unit::Percent));
    m (b.real (p + "cho_mix", N + "Chorus Mix", "MIX", g, 0.0f, 1.0f, 0.5f, Unit::Percent));

    b.toggle (p + "pha_on", N + "Phaser On", "PHASER", g, false);
    m (b.real (p + "pha_rate", N + "Phaser Rate", "RATE", g, 0.01f, 10.0f, 0.3f, Unit::Hz, 1.0f));
    m (b.real (p + "pha_depth", N + "Phaser Depth", "DEPTH", g, 0.0f, 1.0f, 0.7f, Unit::Percent));
    m (b.real (p + "pha_freq", N + "Phaser Frequency", "FREQ", g, 50.0f, 12000.0f, 700.0f, Unit::Hz, 800.0f));
    m (b.real (p + "pha_fb", N + "Phaser Feedback", "FDBK", g, -0.95f, 0.95f, 0.4f, Unit::Bipolar));
    b.choice (p + "pha_stages", N + "Phaser Stages", "STAGES", g, Choices::phaserStages(), 2);
    m (b.real (p + "pha_mix", N + "Phaser Mix", "MIX", g, 0.0f, 1.0f, 0.5f, Unit::Percent));

    b.toggle (p + "fla_on", N + "Flanger On", "FLANGER", g, false);
    m (b.real (p + "fla_rate", N + "Flanger Rate", "RATE", g, 0.01f, 10.0f, 0.2f, Unit::Hz, 1.0f));
    m (b.real (p + "fla_depth", N + "Flanger Depth", "DEPTH", g, 0.0f, 1.0f, 0.6f, Unit::Percent));
    m (b.real (p + "fla_delay", N + "Flanger Delay", "DELAY", g, 0.1f, 15.0f, 2.0f, Unit::Ms, 3.0f));
    m (b.real (p + "fla_fb", N + "Flanger Feedback", "FDBK", g, -0.95f, 0.95f, 0.5f, Unit::Bipolar));
    m (b.real (p + "fla_mix", N + "Flanger Mix", "MIX", g, 0.0f, 1.0f, 0.5f, Unit::Percent));

    b.toggle (p + "dly_on", N + "Delay On", "DELAY", g, false);
    b.toggle (p + "dly_sync", N + "Delay Sync", "SYNC", g, true);
    m (b.real (p + "dly_time", N + "Delay Time", "TIME", g, 1.0f, 2000.0f, 375.0f, Unit::Ms, 300.0f));
    b.choice (p + "dly_div", N + "Delay Division", "DIV", g, Choices::syncDivision(), 9);
    m (b.real (p + "dly_stereo", N + "Delay Stereo Offset", "OFFSET", g, -1.0f, 1.0f, 0.0f, Unit::Bipolar));
    m (b.real (p + "dly_fb", N + "Delay Feedback", "FDBK", g, 0.0f, 0.98f, 0.35f, Unit::Percent));
    b.toggle (p + "dly_pingpong", N + "Delay Ping Pong", "PING", g, false);
    m (b.real (p + "dly_lowcut", N + "Delay Low Cut", "LO CUT", g, 20.0f, 2000.0f, 120.0f, Unit::Hz, 200.0f));
    m (b.real (p + "dly_highcut", N + "Delay High Cut", "HI CUT", g, 1000.0f, 20000.0f, 8000.0f, Unit::Hz, 6000.0f));
    m (b.real (p + "dly_mix", N + "Delay Mix", "MIX", g, 0.0f, 1.0f, 0.25f, Unit::Percent));

    b.toggle (p + "rev_on", N + "Reverb On", "REVERB", g, false);
    m (b.real (p + "rev_size", N + "Reverb Size", "SIZE", g, 0.0f, 1.0f, 0.5f, Unit::Percent));
    m (b.real (p + "rev_decay", N + "Reverb Decay", "DECAY", g, 0.2f, 20.0f, 2.5f, Unit::Seconds, 2.0f));
    m (b.real (p + "rev_damp", N + "Reverb Damping", "DAMP", g, 0.0f, 1.0f, 0.5f, Unit::Percent));
    m (b.real (p + "rev_predelay", N + "Reverb Predelay", "PRE", g, 0.0f, 250.0f, 10.0f, Unit::Ms, 40.0f));
    m (b.real (p + "rev_width", N + "Reverb Width", "WIDTH", g, 0.0f, 1.0f, 1.0f, Unit::Percent));
    m (b.real (p + "rev_mod", N + "Reverb Modulation", "MOD", g, 0.0f, 1.0f, 0.3f, Unit::Percent));
    m (b.real (p + "rev_mix", N + "Reverb Mix", "MIX", g, 0.0f, 1.0f, 0.25f, Unit::Percent));

    b.toggle (p + "cmp_on", N + "Compressor On", "COMP", g, false);
    m (b.real (p + "cmp_thresh", N + "Compressor Threshold", "THRESH", g, -60.0f, 0.0f, -18.0f, Unit::dB));
    m (b.real (p + "cmp_ratio", N + "Compressor Ratio", "RATIO", g, 1.0f, 20.0f, 4.0f, Unit::Ratio, 4.0f));
    m (b.real (p + "cmp_attack", N + "Compressor Attack", "ATTACK", g, 0.1f, 200.0f, 5.0f, Unit::Ms, 10.0f));
    m (b.real (p + "cmp_release", N + "Compressor Release", "RELEASE", g, 5.0f, 2000.0f, 120.0f, Unit::Ms, 120.0f));
    m (b.real (p + "cmp_knee", N + "Compressor Knee", "KNEE", g, 0.0f, 24.0f, 6.0f, Unit::dB));
    m (b.real (p + "cmp_makeup", N + "Compressor Makeup", "MAKEUP", g, -12.0f, 24.0f, 0.0f, Unit::dB));
    m (b.real (p + "cmp_mix", N + "Compressor Mix", "MIX", g, 0.0f, 1.0f, 1.0f, Unit::Percent));

    b.toggle (p + "eq_on", N + "EQ On", "EQ", g, false);
    m (b.real (p + "eq_lowcut", N + "EQ Low Cut", "LO CUT", g, 20.0f, 1000.0f, 20.0f, Unit::Hz, 120.0f));
    m (b.real (p + "eq_lowfreq", N + "EQ Low Freq", "LO FRQ", g, 20.0f, 1000.0f, 120.0f, Unit::Hz, 150.0f));
    m (b.real (p + "eq_lowgain", N + "EQ Low Gain", "LO", g, -18.0f, 18.0f, 0.0f, Unit::dB));
    m (b.real (p + "eq_midfreq", N + "EQ Mid Freq", "MID FRQ", g, 100.0f, 10000.0f, 1000.0f, Unit::Hz, 1000.0f));
    m (b.real (p + "eq_midgain", N + "EQ Mid Gain", "MID", g, -18.0f, 18.0f, 0.0f, Unit::dB));
    m (b.real (p + "eq_midq", N + "EQ Mid Q", "Q", g, 0.1f, 10.0f, 0.7f, Unit::Ratio, 1.0f));
    m (b.real (p + "eq_highfreq", N + "EQ High Freq", "HI FRQ", g, 1000.0f, 20000.0f, 6000.0f, Unit::Hz, 5000.0f));
    m (b.real (p + "eq_highgain", N + "EQ High Gain", "HI", g, -18.0f, 18.0f, 0.0f, Unit::dB));
    m (b.real (p + "eq_highcut", N + "EQ High Cut", "HI CUT", g, 1000.0f, 20000.0f, 20000.0f, Unit::Hz, 6000.0f));

    b.toggle (p + "img_on", N + "Imager On", "IMAGER", g, false);
    m (b.real (p + "img_width", N + "Imager Width", "WIDTH", g, 0.0f, 2.0f, 1.0f, Unit::Percent));
    m (b.real (p + "img_bassmono", N + "Imager Bass Mono", "MONO <", g, 0.0f, 500.0f, 0.0f, Unit::Hz, 120.0f));
    m (b.real (p + "img_pan", N + "Imager Pan", "PAN", g, -1.0f, 1.0f, 0.0f, Unit::Bipolar));

    b.toggle (p + "crs_on", N + "Bitcrusher On", "CRUSH", g, false);
    m (b.real (p + "crs_bits", N + "Bitcrusher Bits", "BITS", g, 1.0f, 24.0f, 12.0f, Unit::Bits));
    m (b.real (p + "crs_rate", N + "Bitcrusher Rate", "RATE", g, 200.0f, 48000.0f, 48000.0f, Unit::Hz, 8000.0f));
    m (b.real (p + "crs_mix", N + "Bitcrusher Mix", "MIX", g, 0.0f, 1.0f, 1.0f, Unit::Percent));
}
} // namespace

const Registry& Registry::get()
{
    static const Registry r;
    return r;
}

Registry::Registry()
{
    all.reserve ((size_t) P::Count);
    Builder b { all };

    for (int i = 0; i < kNumOsc; ++i)     addOsc (b, i);
    jassert ((int) all.size() == P::FilterBase);
    for (int i = 0; i < kNumFilters; ++i) addFilter (b, i);
    jassert ((int) all.size() == P::EnvBase);
    for (int i = 0; i < kNumEnvs; ++i)    addEnv (b, i);
    jassert ((int) all.size() == P::LfoBase);
    for (int i = 0; i < kNumLfos; ++i)    addLfo (b, i);
    jassert ((int) all.size() == P::VoiceBase);

    const auto gv = Group::Voice;
    b.integer ("polyphony", "Polyphony", "VOICES", gv, 1, kMaxVoices, 16, Unit::Voices);
    b.choice ("voice_mode", "Voice Mode", "MODE", gv, Choices::voiceMode(), 0);
    b.choice ("glide_mode", "Glide Mode", "GLIDE", gv, Choices::glideMode(), 0);
    b.real ("glide_time", "Glide Time", "TIME", gv, 0.0f, 10.0f, 0.08f, Unit::Seconds, 0.3f, true, true);
    b.real ("glide_curve", "Glide Curve", "CURVE", gv, -1.0f, 1.0f, 0.0f, Unit::Bipolar);
    b.toggle ("glide_constrate", "Glide Constant Rate", "RATE", gv, false);
    b.choice ("note_priority", "Note Priority", "PRIORITY", gv, Choices::notePriority(), 0);
    b.toggle ("retrig_hard", "Hard Retrigger", "HARD", gv, false);
    b.toggle ("return_held", "Return To Held Note", "RETURN", gv, true);
    b.integer ("bend_up", "Pitch Bend Up", "BEND +", gv, 0, 48, 2, Unit::Semitones);
    b.integer ("bend_down", "Pitch Bend Down", "BEND -", gv, 0, 48, 2, Unit::Semitones);
    b.real ("vel_sens", "Velocity Sensitivity", "VEL", gv, 0.0f, 1.0f, 0.7f, Unit::Percent);
    jassert ((int) all.size() == P::AnalogBase);

    const auto ga = Group::Analog;
    b.real ("analog_drift", "Analog Drift", "DRIFT", ga, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real ("analog_instab", "Analog Instability", "INSTAB", ga, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real ("analog_sat", "Analog Saturation", "SAT", ga, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real ("analog_filter", "Analog Filter Nonlinearity", "NONLIN", ga, 0.0f, 1.0f, 0.0f, Unit::Percent, -1, true, true);
    b.real ("analog_stereo", "Analog Stereo Variation", "STEREO", ga, 0.0f, 1.0f, 0.0f, Unit::Percent);
    jassert ((int) all.size() == P::GlobalBase);

    const auto gg = Group::Global;
    { auto& s = b.real ("master_vol", "Master Volume", "MASTER", gg, -48.0f, 6.0f, -3.0f, Unit::dB, -12.0f, true, false); s.randomizable = false; }
    { auto& s = b.choice ("quality", "Quality", "QUALITY", gg, Choices::quality(), 0); s.randomizable = false; }
    b.choice ("filter_routing", "Filter Routing", "ROUTING", gg, Choices::filterRouting(), 0);
    b.choice ("oscfx_output", "Osc FX Output", "OSC FX OUT", gg, Choices::voiceFxRouting(), 0);
    jassert ((int) all.size() == P::MacroBase);

    for (int i = 0; i < kNumMacros; ++i)
    {
        auto& s = b.real ("macro" + juce::String (i + 1), "Macro " + juce::String (i + 1), "MACRO " + juce::String (i + 1),
                          Group::Macro, 0.0f, 1.0f, 0.0f, Unit::Percent);
        s.randomizable = false;
    }
    jassert ((int) all.size() == P::ModAmtBase);

    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& s = b.real ("mod" + juce::String (i + 1) + "_amt", "Mod " + juce::String (i + 1) + " Amount", "AMT",
                          Group::Mod, -1.0f, 1.0f, 0.0f, Unit::Bipolar);
        s.randomizable = false;
    }
    jassert ((int) all.size() == P::FxBase);

    for (int r = 0; r < kNumRacks; ++r) addRack (b, r);
    jassert ((int) all.size() == P::ArpBase);

    const auto gArp = Group::Arp;
    b.choice ("midifx_mode", "MIDI FX Mode", "MIDI FX", gArp, Choices::midiFxMode(), 0);
    b.choice ("arp_mode", "Arp Mode", "MODE", gArp, Choices::arpMode(), 0);
    b.choice ("arp_rate", "Arp Rate", "RATE", gArp, Choices::noteDivision(), 10);
    b.integer ("arp_octaves", "Arp Octaves", "OCTAVES", gArp, 1, 4, 1);
    b.real ("arp_gate", "Arp Gate", "GATE", gArp, 0.05f, 1.2f, 0.7f, Unit::Percent);
    b.real ("arp_swing", "Arp Swing", "SWING", gArp, 0.0f, 1.0f, 0.0f, Unit::Percent);
    b.choice ("arp_velmode", "Arp Velocity Mode", "VEL MODE", gArp, Choices::arpVelMode(), 0);
    b.real ("arp_vel", "Arp Velocity", "VELOCITY", gArp, 0.0f, 1.0f, 0.8f, Unit::Percent);
    b.toggle ("arp_hold", "Arp Hold", "HOLD", gArp, false);
    b.integer ("arp_steps", "Arp Pattern Length", "STEPS", gArp, 1, 16, 16);
    b.integer ("arp_pitchstep", "Arp Cycle Transpose", "TRANSP", gArp, -12, 12, 0, Unit::Semitones);
    jassert ((int) all.size() == P::SeqBase);

    const auto gSeq = Group::Seq;
    b.choice ("seq_rate", "Seq Rate", "RATE", gSeq, Choices::noteDivision(), 10);
    b.real ("seq_swing", "Seq Swing", "SWING", gSeq, 0.0f, 1.0f, 0.0f, Unit::Percent);
    b.integer ("seq_length", "Seq Length", "LENGTH", gSeq, 1, kNumSeqSteps, kNumSeqSteps);
    b.real ("seq_gatescale", "Seq Gate Scale", "GATE", gSeq, 0.1f, 2.0f, 1.0f, Unit::Percent);
    jassert ((int) all.size() == P::SeqStepBase);

    for (int s = 0; s < kNumSeqSteps; ++s)
    {
        const auto n = juce::String (s + 1);
        b.toggle ("seq" + n + "_on", "Seq Step " + n + " On", "ON", gSeq, true);
        b.integer ("seq" + n + "_pitch", "Seq Step " + n + " Pitch", "PITCH", gSeq, -24, 24, 0, Unit::Semitones);
        b.real ("seq" + n + "_vel", "Seq Step " + n + " Velocity", "VEL", gSeq, 0.0f, 1.0f, 0.8f, Unit::Percent);
        b.real ("seq" + n + "_gate", "Seq Step " + n + " Gate", "GATE", gSeq, 0.05f, 1.0f, 0.5f, Unit::Percent);
    }
    jassert ((int) all.size() == P::Count);

    for (int i = 0; i < (int) all.size(); ++i)
    {
        byId[all[(size_t) i].id] = i;
        if (all[(size_t) i].modulatable)
            modDests.push_back (i);
    }
}

int Registry::indexOf (const juce::String& id) const
{
    auto it = byId.find (id);
    return it == byId.end() ? -1 : it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout Registry::createLayout() const
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& s : all)
    {
        const juce::ParameterID pid { s.id, 1 };
        const ParamSpec* spec = &s;

        switch (s.kind)
        {
            case PKind::Float:
            {
                auto attrs = juce::AudioParameterFloatAttributes()
                                 .withStringFromValueFunction ([spec] (float v, int) { return spec->format (v); })
                                 .withValueFromStringFunction ([spec] (const juce::String& t)
                                 {
                                     auto v = t.retainCharacters ("0123456789.-+").getFloatValue();
                                     if (spec->unit == Unit::Percent || spec->unit == Unit::Bipolar) v *= 0.01f;
                                     if (spec->unit == Unit::Degrees) v /= 360.0f;
                                     if (spec->unit == Unit::Hz && t.containsIgnoreCase ("k")) v *= 1000.0f;
                                     return juce::jlimit (spec->min, spec->max, v);
                                 });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, s.name, s.range(), s.def, attrs));
                break;
            }
            case PKind::Int:
            {
                auto attrs = juce::AudioParameterIntAttributes()
                                 .withStringFromValueFunction ([spec] (int v, int) { return spec->format ((float) v); });
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, s.name, (int) s.min, (int) s.max, (int) s.def, attrs));
                break;
            }
            case PKind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, s.choices, (int) s.def));
                break;
            case PKind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def >= 0.5f));
                break;
        }
    }
    return layout;
}

// ===================================================================================== mod sources
const char* modSourceId (ModSrc s)
{
    switch (s)
    {
        case ModSrc::Env1: return "env1";
        case ModSrc::Env2: return "env2";
        case ModSrc::Env3: return "env3";
        case ModSrc::Env4: return "env4";
        case ModSrc::Lfo1: return "lfo1";
        case ModSrc::Lfo2: return "lfo2";
        case ModSrc::Lfo3: return "lfo3";
        case ModSrc::Lfo4: return "lfo4";
        case ModSrc::Velocity: return "velocity";
        case ModSrc::Note: return "note";
        case ModSrc::ModWheel: return "modwheel";
        case ModSrc::Aftertouch: return "aftertouch";
        case ModSrc::PitchBend: return "pitchbend";
        case ModSrc::Random: return "random";
        case ModSrc::Macro1: return "macro1";
        case ModSrc::Macro2: return "macro2";
        case ModSrc::Macro3: return "macro3";
        case ModSrc::Macro4: return "macro4";
        case ModSrc::None:
        case ModSrc::NumSources:
        default: return "none";
    }
}

juce::String modSourceName (ModSrc s)
{
    switch (s)
    {
        case ModSrc::Env1: return "ENV 1";
        case ModSrc::Env2: return "ENV 2";
        case ModSrc::Env3: return "ENV 3";
        case ModSrc::Env4: return "ENV 4";
        case ModSrc::Lfo1: return "LFO 1";
        case ModSrc::Lfo2: return "LFO 2";
        case ModSrc::Lfo3: return "LFO 3";
        case ModSrc::Lfo4: return "LFO 4";
        case ModSrc::Velocity: return "VELOCITY";
        case ModSrc::Note: return "NOTE";
        case ModSrc::ModWheel: return "MOD WHEEL";
        case ModSrc::Aftertouch: return "AFTERTOUCH";
        case ModSrc::PitchBend: return "PITCH BEND";
        case ModSrc::Random: return "RANDOM";
        case ModSrc::Macro1: return "MACRO 1";
        case ModSrc::Macro2: return "MACRO 2";
        case ModSrc::Macro3: return "MACRO 3";
        case ModSrc::Macro4: return "MACRO 4";
        default: return "NONE";
    }
}

ModSrc modSourceFromId (const juce::String& id)
{
    for (int i = 1; i < (int) ModSrc::NumSources; ++i)
        if (id == modSourceId ((ModSrc) i))
            return (ModSrc) i;
    return ModSrc::None;
}

bool modSourceIsBipolar (ModSrc s)
{
    return s == ModSrc::PitchBend || s == ModSrc::Random
        || (s >= ModSrc::Lfo1 && s <= ModSrc::Lfo4); // LFO polarity is handled by the LFO itself
}

} // namespace dw

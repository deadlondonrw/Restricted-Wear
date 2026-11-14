// DEADWAVE factory library.
//
// Every preset is designed individually with the small DSL below. Presets differ in wavetable and
// position, shape mode, filter type, envelopes, modulation routing, effects and voice behaviour.
// The test runner renders every preset and flags near-duplicates by audio features.

#include "PresetBuilder.h"
#include "PresetManager.h"

namespace dw
{
const juce::StringArray& presetCategories()
{
    static const juce::StringArray c { "Bass", "Sub Bass", "Reese", "Growl", "Lead", "Pad", "Pluck", "Keys", "Bell",
                                       "Atmospheric", "Cinematic", "Texture", "Drone", "Experimental", "Digital",
                                       "Analog", "Dark", "Ethereal", "Aggressive", "FX", "Init" };
    return c;
}

namespace
{
// filter types
enum { LP12, LP24, HP12, HP24, BP12, BP24, NOTCH, PEAK, AP, LADLP, LADHP, COMBP, COMBN, FORMANT };
// LFO shapes
enum { SINE, TRI, SAWUP, SAWDN, SQR, SH, SRAND, CUSTOM };
// sync divisions
enum { B8, B4, B2, B1, D2, D2T, D4D, D4, D4T, D8D, D8, D8T, D16D, D16, D16T, D32, D32T, D64 };
// warp modes
enum { BEND, SQUEEZE, PWIDTH, SYNC, FORMANTW, MIRROR, QUANT, PHASEFM };
// distortion
enum { SOFT, HARD, TUBE, FOLDBACK, SINEFOLD, RECTIFY };

struct D
{
    PresetBuilder b;
    juce::String name, cat, desc;

    D (const juce::String& n, const juce::String& c, const juce::String& d) : b (n, c), name (n), cat (c), desc (d)
    {
        b.description (d);
    }

    static juce::String o (int i, const char* p) { return "osc" + juce::String (i + 1) + "_" + p; }

    D& p (const juce::String& id, float v) { b.set (id, v); return *this; }

    /** Wavetable oscillator. */
    D& osc (int i, const char* table, float wt = 0.0f, float level = 0.7f, int oct = 0, int uni = 1, float det = 0.2f, float spread = 0.7f)
    {
        b.set (o (i, "on"), 1.0f);
        b.set (o (i, "type"), 0.0f);
        b.waveFactory (i, table);
        b.set (o (i, "wtpos"), wt);
        b.set (o (i, "level"), level);
        b.set (o (i, "oct"), (float) oct);
        b.set (o (i, "unison"), (float) uni);
        b.set (o (i, "detune"), det);
        b.set (o (i, "spread"), spread);
        if (uni > 1) b.set (o (i, "phaserand"), 1.0f);
        return *this;
    }
    D& oscExpr (int i, const char* nm, const char* expr, int frames, float wt = 0.0f, float level = 0.7f, int oct = 0, int uni = 1, float det = 0.2f)
    {
        b.set (o (i, "on"), 1.0f);
        b.waveExpr (i, nm, expr, frames);
        b.set (o (i, "wtpos"), wt);
        b.set (o (i, "level"), level);
        b.set (o (i, "oct"), (float) oct);
        b.set (o (i, "unison"), (float) uni);
        b.set (o (i, "detune"), det);
        if (uni > 1) b.set (o (i, "phaserand"), 1.0f);
        return *this;
    }
    D& oscRand (int i, int style, uint32_t seed, float cx, int frames, float wt = 0.0f, float level = 0.6f, int oct = 0, int uni = 1)
    {
        b.set (o (i, "on"), 1.0f);
        b.waveRandom (i, style, seed, cx, frames);
        b.set (o (i, "wtpos"), wt);
        b.set (o (i, "level"), level);
        b.set (o (i, "oct"), (float) oct);
        b.set (o (i, "unison"), (float) uni);
        if (uni > 1) b.set (o (i, "phaserand"), 1.0f);
        return *this;
    }
    D& noise (int i, float level, float color, float sh = 0.0f)
    {
        b.set (o (i, "on"), 1.0f);
        b.set (o (i, "type"), 1.0f);
        b.set (o (i, "level"), level);
        b.set (o (i, "shape"), color);
        b.set (o (i, "wtpos"), sh);
        b.set (o (i, "spread"), 1.0f);
        return *this;
    }
    D& ox (int i, const char* param, float v) { b.set (o (i, param), v); return *this; }
    D& warp (int i, int mode, float shape) { b.set (o (i, "warp"), (float) mode); b.set (o (i, "shape"), shape); return *this; }
    D& route (int i, int r) { b.set (o (i, "route"), (float) r); return *this; }
    D& fm (int i, int src, float amt, float ratio = 1.0f) { b.set (o (i, "fmsrc"), (float) src); b.set (o (i, "fmamt"), amt); b.set (o (i, "fmratio"), ratio); return *this; }

    D& filt (int i, int type, float cutoff, float reso, float env = 0.0f, float drive = 0.0f, float key = 0.3f)
    {
        const juce::String f = "filter" + juce::String (i + 1) + "_";
        b.set (f + "on", 1.0f);
        b.set (f + "type", (float) type);
        b.set (f + "cutoff", cutoff);
        b.set (f + "reso", reso);
        b.set (f + "envamt", env);
        b.set (f + "drive", drive);
        b.set (f + "keytrack", key);
        return *this;
    }
    D& parallel() { b.set ("filter_routing", 1.0f); return *this; }

    D& env (int i, float a, float d, float s, float r, float ac = 0.25f, float dc = 0.6f, float rc = 0.6f)
    {
        const juce::String e = "env" + juce::String (i + 1) + "_";
        b.set (e + "attack", a); b.set (e + "decay", d); b.set (e + "sustain", s); b.set (e + "release", r);
        b.set (e + "acurve", ac); b.set (e + "dcurve", dc); b.set (e + "rcurve", rc);
        return *this;
    }
    D& amp (float a, float d, float s, float r) { return env (0, a, d, s, r); }

    D& lfoSync (int i, int shape, int div, int mode = 1, float smooth = 0.0f)
    {
        const juce::String l = "lfo" + juce::String (i + 1) + "_";
        b.set (l + "shape", (float) shape); b.set (l + "sync", 1.0f); b.set (l + "div", (float) div);
        b.set (l + "mode", (float) mode); b.set (l + "smooth", smooth);
        return *this;
    }
    D& lfoHz (int i, int shape, float hz, int mode = 0, float smooth = 0.0f, float rise = 0.0f)
    {
        const juce::String l = "lfo" + juce::String (i + 1) + "_";
        b.set (l + "shape", (float) shape); b.set (l + "sync", 0.0f); b.set (l + "rate", hz);
        b.set (l + "mode", (float) mode); b.set (l + "smooth", smooth); b.set (l + "rise", rise);
        return *this;
    }
    D& lfoShape (int i, const char* pts) { b.lfoShape (i, pts); b.set ("lfo" + juce::String (i + 1) + "_shape", (float) CUSTOM); return *this; }
    D& uni (int i, bool bipolar) { b.set ("lfo" + juce::String (i + 1) + "_bipolar", bipolar ? 1.0f : 0.0f); return *this; }
    D& mod (ModSrc s, const juce::String& dst, float amt) { b.mod (s, dst, amt); return *this; }

    D& mono (bool legato = true, int glideMode = 2, float time = 0.08f)
    {
        b.set ("voice_mode", legato ? 2.0f : 1.0f);
        b.set ("glide_mode", (float) glideMode);
        b.set ("glide_time", time);
        return *this;
    }
    D& poly (int voices) { b.set ("polyphony", (float) voices); return *this; }
    D& analog (float drift, float instab, float sat = 0.0f, float nonlin = 0.0f, float stereo = 0.0f)
    {
        b.set ("analog_drift", drift); b.set ("analog_instab", instab); b.set ("analog_sat", sat);
        b.set ("analog_filter", nonlin); b.set ("analog_stereo", stereo);
        return *this;
    }
    D& ultra() { b.set ("quality", 1.0f); return *this; }
    D& vol (float db) { b.set ("master_vol", db); return *this; }

    // ---- effects (master rack) ----
    D& dist (int mode, float drive, float tone = 0.5f, float mix = 1.0f)
    { p ("fx_dist_on", 1); p ("fx_dist_mode", (float) mode); p ("fx_dist_drive", drive); p ("fx_dist_tone", tone); return p ("fx_dist_mix", mix); }
    D& sat (float drive, float bias = 0.2f, float warm = 0.5f, float mix = 1.0f)
    { p ("fx_sat_on", 1); p ("fx_sat_drive", drive); p ("fx_sat_bias", bias); p ("fx_sat_tone", warm); return p ("fx_sat_mix", mix); }
    D& chorus (float rate, float depth, float mix, float delay = 12.0f, float fb = 0.0f)
    { p ("fx_cho_on", 1); p ("fx_cho_rate", rate); p ("fx_cho_depth", depth); p ("fx_cho_delay", delay); p ("fx_cho_fb", fb); return p ("fx_cho_mix", mix); }
    D& phaser (float rate, float depth, float freq, float fb, int stages, float mix)
    { p ("fx_pha_on", 1); p ("fx_pha_rate", rate); p ("fx_pha_depth", depth); p ("fx_pha_freq", freq); p ("fx_pha_fb", fb); p ("fx_pha_stages", (float) stages); return p ("fx_pha_mix", mix); }
    D& flanger (float rate, float depth, float delay, float fb, float mix)
    { p ("fx_fla_on", 1); p ("fx_fla_rate", rate); p ("fx_fla_depth", depth); p ("fx_fla_delay", delay); p ("fx_fla_fb", fb); return p ("fx_fla_mix", mix); }
    D& delay (int div, float fb, float mix, bool ping = true, float hiCut = 7000.0f, float loCut = 150.0f)
    { p ("fx_dly_on", 1); p ("fx_dly_sync", 1); p ("fx_dly_div", (float) div); p ("fx_dly_fb", fb); p ("fx_dly_mix", mix); p ("fx_dly_pingpong", ping ? 1.0f : 0.0f); p ("fx_dly_highcut", hiCut); return p ("fx_dly_lowcut", loCut); }
    D& reverb (float size, float decay, float mix, float damp = 0.5f, float pre = 10.0f)
    { p ("fx_rev_on", 1); p ("fx_rev_size", size); p ("fx_rev_decay", decay); p ("fx_rev_mix", mix); p ("fx_rev_damp", damp); return p ("fx_rev_predelay", pre); }
    D& comp (float thresh, float ratio, float att = 5.0f, float rel = 120.0f, float makeup = 2.0f)
    { p ("fx_cmp_on", 1); p ("fx_cmp_thresh", thresh); p ("fx_cmp_ratio", ratio); p ("fx_cmp_attack", att); p ("fx_cmp_release", rel); return p ("fx_cmp_makeup", makeup); }
    D& eq (float lowCut, float lowGain, float midFreq, float midGain, float highGain, float highCut = 20000.0f)
    { p ("fx_eq_on", 1); p ("fx_eq_lowcut", lowCut); p ("fx_eq_lowgain", lowGain); p ("fx_eq_midfreq", midFreq); p ("fx_eq_midgain", midGain); p ("fx_eq_highgain", highGain); return p ("fx_eq_highcut", highCut); }
    D& imager (float width, float bassMono = 0.0f) { p ("fx_img_on", 1); p ("fx_img_width", width); return p ("fx_img_bassmono", bassMono); }
    D& crush (float bits, float rate, float mix = 1.0f) { p ("fx_crs_on", 1); p ("fx_crs_bits", bits); p ("fx_crs_rate", rate); return p ("fx_crs_mix", mix); }
    D& bassMono() { return imager (1.0f, 140.0f); }

    D& arp (int mode, int rate, int octaves = 1, float gate = 0.6f, float swing = 0.0f)
    { p ("midifx_mode", 1); p ("arp_mode", (float) mode); p ("arp_rate", (float) rate); p ("arp_octaves", (float) octaves); p ("arp_gate", gate); return p ("arp_swing", swing); }
    D& seq (int rate, std::initializer_list<int> pitches, float gate = 0.5f)
    {
        p ("midifx_mode", 2); p ("seq_rate", (float) rate);
        int s = 0;
        for (int pt : pitches)
        {
            if (s >= kNumSeqSteps) break;
            const juce::String st = "seq" + juce::String (s + 1) + "_";
            if (pt <= -99) p (st + "on", 0.0f);
            else p (st + "pitch", (float) pt);
            p (st + "gate", gate);
            ++s;
        }
        return p ("seq_length", (float) juce::jmax (1, s));
    }

    FactoryPreset done()
    {
        // Standard macro layout: 1 tone, 2 wave, 3 space, 4 drive.
        const bool f1 = b.get (P::filt (0, FiltP::On)) >= 0.5f;
        if (b.usedModSlots() < kNumModSlots - 4)
        {
            if (f1) b.mod (ModSrc::Macro1, "filter1_cutoff", 0.35f);
            else b.mod (ModSrc::Macro1, "osc1_shape", 0.4f);
            b.mod (ModSrc::Macro2, "osc1_wtpos", 0.4f);
            if (b.get (P::fx (0, FxP::RevOn)) >= 0.5f) b.mod (ModSrc::Macro3, "fx_rev_mix", 0.35f);
            else if (b.get (P::fx (0, FxP::DlyOn)) >= 0.5f) b.mod (ModSrc::Macro3, "fx_dly_mix", 0.35f);
            else b.mod (ModSrc::Macro3, "osc1_detune", 0.3f);
            b.mod (ModSrc::Macro4, "osc1_drive", 0.5f);
        }
        return { name, cat, desc, b.build() };
    }
};

using List = std::vector<FactoryPreset>;
using M = ModSrc;

// ================================================================================================ BASS
void bass (List& L)
{
    L.push_back (D ("Rubber Pluck Bass", "Bass", "Short resonant ladder envelope on a saw with a sine sub. Classic and punchy.")
        .osc (0, "Saw Tilt", 0.8f, 0.7f, -1).osc (2, "Sub Shapes", 0.0f, 0.5f, -2).route (2, 3)
        .filt (0, LADLP, 260.0f, 0.45f, 0.55f, 0.2f).env (1, 0.001f, 0.22f, 0.0f, 0.2f).amp (0.001f, 0.4f, 0.7f, 0.08f)
        .mono (false, 0).bassMono().done());
    L.push_back (D ("Acid Line 303", "Bass", "Squelchy mono acid: overlap notes for slides. Resonant ladder, accent via velocity.")
        .osc (0, "Basic Shapes", 0.5f, 0.75f, -1).filt (0, LADLP, 380.0f, 0.78f, 0.6f, 0.35f, 0.4f)
        .env (1, 0.001f, 0.18f, 0.05f, 0.12f).amp (0.001f, 0.3f, 0.85f, 0.05f).mono (true, 2, 0.06f)
        .mod (M::Velocity, "filter1_envamt", 0.25f).dist (TUBE, 10.0f, 0.55f, 0.45f).delay (D8D, 0.3f, 0.12f).done());
    L.push_back (D ("Square Pump", "Bass", "Hollow square bass with a quarter-note filter pump.")
        .osc (0, "Basic Shapes", 0.75f, 0.5f, -1).osc (1, "Sub Shapes", 0.0f, 0.35f, -1).route (1, 3)
        .filt (0, LP24, 900.0f, 0.2f, 0.2f).lfoSync (0, SAWDN, D4, 0).mod (M::Lfo1, "filter1_cutoff", -0.3f)
        .amp (0.002f, 0.5f, 0.8f, 0.1f).bassMono().comp (-16.0f, 4.0f, 5.0f, 120.0f, 0.0f).done());
    L.push_back (D ("FM Thump", "Bass", "Two-operator FM bass: modulation index falls with the filter envelope.")
        .osc (0, "Sub Shapes", 0.0f, 0.75f, -1).fm (0, 1, 0.45f, 2.0f).mod (M::Env2, "osc1_fmamt", -0.35f)
        .env (1, 0.001f, 0.15f, 0.0f, 0.1f).amp (0.001f, 0.6f, 0.6f, 0.1f).sat (6.0f, 0.1f, 0.6f).bassMono().done());
    L.push_back (D ("Wooden Bass", "Bass", "Plucked, woody tone from a band-pass and skewed triangle.")
        .osc (0, "Skew Triangle", 0.6f, 0.8f, -1).filt (0, BP12, 600.0f, 0.35f, 0.35f).filt (1, LP12, 2200.0f, 0.1f)
        .env (1, 0.001f, 0.12f, 0.0f, 0.1f).amp (0.001f, 0.45f, 0.25f, 0.12f).eq (40.0f, 3.0f, 900.0f, -2.0f, 1.0f).done());
    L.push_back (D ("Wide Detune Bass", "Bass", "Unison saw stack with a mono centre below 140 Hz.")
        .osc (0, "Saw Tilt", 0.75f, 0.6f, -1, 5, 0.22f, 1.0f).osc (2, "Sub Shapes", 0.1f, 0.45f, -2).route (2, 3)
        .filt (0, LP24, 1200.0f, 0.15f, 0.3f).env (1, 0.001f, 0.35f, 0.2f, 0.2f).amp (0.002f, 0.4f, 0.9f, 0.12f)
        .imager (1.3f, 140.0f).done());
    L.push_back (D ("Bias Drive Bass", "Bass", "Vertical bias pushes a sine into asymmetric saturation: even harmonics, punchy midrange.")
        .osc (0, "Basic Shapes", 0.0f, 0.8f, -1).ox (0, "bias", 0.35f).ox (0, "drive", 0.55f).ox (0, "drivemode", 0.0f)
        .mod (M::Env2, "osc1_bias", 0.3f).env (1, 0.001f, 0.25f, 0.0f, 0.2f).filt (0, LP24, 2500.0f, 0.1f)
        .amp (0.001f, 0.5f, 0.75f, 0.1f).bassMono().done());
    L.push_back (D ("Funk Envelope", "Bass", "Envelope-filter funk bass: velocity opens the band.")
        .osc (0, "PWM", 0.3f, 0.7f, -1).filt (0, BP24, 400.0f, 0.55f, 0.65f, 0.1f).env (1, 0.005f, 0.2f, 0.1f, 0.15f)
        .mod (M::Velocity, "filter1_envamt", 0.3f).amp (0.001f, 0.3f, 0.7f, 0.08f).comp (-18.0f, 3.0f).done());
    L.push_back (D ("Sync Stab Bass", "Bass", "Hard-sync sweep by envelope, mid-heavy and modern.")
        .osc (0, "Sync Saw", 0.0f, 0.7f, -1).mod (M::Env2, "osc1_wtpos", 0.6f).env (1, 0.001f, 0.25f, 0.0f, 0.2f)
        .filt (0, LP24, 3000.0f, 0.1f).amp (0.001f, 0.35f, 0.5f, 0.1f).ultra().dist (SOFT, 6.0f, 0.5f, 0.5f).bassMono().done());
    L.push_back (D ("Organ Bass", "Bass", "Drawbar organ in the low register with gentle key click.")
        .osc (0, "Organ", 0.25f, 0.75f, -1).noise (3, 0.06f, 0.6f).env (2, 0.0f, 0.02f, 0.0f, 0.02f).mod (M::Env3, "osc4_level", 0.6f)
        .amp (0.003f, 0.2f, 1.0f, 0.08f).chorus (0.6f, 0.3f, 0.2f).done());
    L.push_back (D ("Glide Sine Bass", "Bass", "Pure gliding sine with saturation: overlap notes to slide (FL Studio piano roll).")
        .osc (0, "Sub Shapes", 0.0f, 0.85f, -1).mono (true, 2, 0.12f).p ("glide_curve", 0.4f)
        .amp (0.003f, 0.5f, 0.95f, 0.1f).sat (9.0f, 0.15f, 0.6f).bassMono().done());
    L.push_back (D ("Hollow Comb Bass", "Bass", "Comb-filtered saw: hollow, nasal low end that tracks the key.")
        .osc (0, "Saw Tilt", 0.9f, 0.65f, -1).filt (0, COMBP, 110.0f, 0.55f, 0.0f, 0.0f, 1.0f).filt (1, LP24, 2400.0f, 0.1f, 0.3f)
        .env (1, 0.001f, 0.3f, 0.2f, 0.2f).amp (0.001f, 0.4f, 0.8f, 0.1f).bassMono().done());
}

// ============================================================================================ SUB BASS
void subBass (List& L)
{
    L.push_back (D ("Pure Sub", "Sub Bass", "Clean sine sub with a tiny attack click removed. Mono.")
        .osc (0, "Sub Shapes", 0.0f, 0.85f, -1).amp (0.004f, 0.5f, 1.0f, 0.12f).mono (false, 0).imager (0.0f).done());
    L.push_back (D ("808 Drop", "Sub Bass", "808-style: pitch envelope drop and long decay. Glide on overlap.")
        .osc (0, "Sub Shapes", 0.15f, 0.8f, -1).mod (M::Env3, "osc1_pitch", 0.12f).env (2, 0.0f, 0.09f, 0.0f, 0.05f, 0.0f, 0.8f)
        .amp (0.001f, 0.9f, 0.0f, 0.3f).mono (true, 2, 0.15f).dist (TUBE, 14.0f, 0.55f, 0.55f).imager (0.0f).done());
    L.push_back (D ("Warm Sub Saturated", "Sub Bass", "Sine with tube saturation for small speakers.")
        .osc (0, "Sub Shapes", 0.25f, 0.8f, -1).dist (TUBE, 12.0f, 0.35f, 0.5f).filt (0, LP12, 900.0f, 0.0f)
        .amp (0.003f, 0.5f, 1.0f, 0.1f).imager (0.0f).done());
    L.push_back (D ("Triangle Sub", "Sub Bass", "Rounded triangle sub with a touch of third harmonic.")
        .osc (0, "Sub Shapes", 0.66f, 0.8f, -1).amp (0.002f, 0.5f, 1.0f, 0.1f).mono (false, 0).imager (0.0f).done());
    L.push_back (D ("Sub With Air", "Sub Bass", "Sine sub plus a quiet band-passed noise for definition.")
        .osc (0, "Sub Shapes", 0.0f, 0.8f, -1).route (0, 3).noise (1, 0.12f, 0.3f).route (1, 1).filt (1, BP12, 2500.0f, 0.3f).parallel()
        .env (2, 0.0f, 0.08f, 0.0f, 0.05f).mod (M::Env3, "osc2_level", 0.6f).ox (1, "level", 0.0f).amp (0.002f, 0.6f, 0.9f, 0.1f).done());
    L.push_back (D ("Rolling Sub", "Sub Bass", "Sub with 1/8 sidechain-style amplitude dip.")
        .osc (0, "Sub Shapes", 0.0f, 0.85f, -1).lfoSync (0, CUSTOM, D8, 0).lfoShape (0, "0,0,0.8;0.35,1,0;0.999,1,0;").uni (0, false)
        .p ("osc1_amp", 0.0f).mod (M::Lfo1, "osc1_amp", 0.5f)
        .amp (0.002f, 0.5f, 1.0f, 0.1f).imager (0.0f).done());
    L.push_back (D ("Gliding 808 Distort", "Sub Bass", "Distorted 808 with long portamento for trap slides.")
        .osc (0, "Sub Shapes", 0.33f, 0.8f, -1).mono (true, 1, 0.18f).dist (SOFT, 18.0f, 0.4f, 0.6f)
        .amp (0.001f, 1.8f, 0.2f, 0.25f).filt (0, LP24, 2000.0f, 0.1f).imager (0.0f).done());
    L.push_back (D ("Deep Pulse Sub", "Sub Bass", "Narrow pulse low-passed to a soft pulse sub with motion.")
        .osc (0, "PWM", 0.3f, 0.75f, -1).filt (0, LP24, 260.0f, 0.1f, 0.0f, 0.0f, 0.6f).lfoHz (0, SINE, 0.3f, 0).mod (M::Lfo1, "osc1_wtpos", 0.25f)
        .amp (0.003f, 0.5f, 1.0f, 0.12f).imager (0.0f).done());
}

// =============================================================================================== REESE
void reese (List& L)
{
    L.push_back (D ("Classic Reese", "Reese", "Two detuned saws through a moving low-pass. Drum and bass staple.")
        .osc (0, "Saw Tilt", 1.0f, 0.6f, -1, 2, 0.18f, 0.4f).osc (1, "Saw Tilt", 1.0f, 0.6f, -1).ox (1, "fine", 18.0f)
        .filt (0, LP24, 900.0f, 0.2f).lfoHz (0, SINE, 0.15f, 0).mod (M::Lfo1, "filter1_cutoff", 0.15f).mono (true, 2, 0.07f)
        .bassMono().done());
    L.push_back (D ("Neuro Reese", "Reese", "Detuned saws, phaser and drive with a slow notch sweep.")
        .osc (0, "Saw Tilt", 0.9f, 0.6f, -1, 4, 0.25f, 0.6f).filt (0, NOTCH, 700.0f, 0.4f).lfoHz (0, TRI, 0.2f, 0)
        .mod (M::Lfo1, "filter1_cutoff", 0.3f).phaser (0.15f, 0.8f, 500.0f, 0.5f, 3, 0.5f).dist (TUBE, 14.0f, 0.5f, 0.6f)
        .mono (true, 2, 0.06f).bassMono().done());
    L.push_back (D ("Hollow Reese", "Reese", "Hollow squares detuned: dark and wide.")
        .osc (0, "Hollow Square", 0.4f, 0.6f, -1, 3, 0.2f, 0.8f).osc (1, "Saw Tilt", 0.7f, 0.4f, -1).ox (1, "fine", -12.0f)
        .filt (0, LP24, 700.0f, 0.15f).mono (true, 2).bassMono().done());
    L.push_back (D ("Wobble Reese", "Reese", "Reese with a 1/4-dotted filter LFO and growl drive.")
        .osc (0, "Saw Tilt", 1.0f, 0.6f, -1, 2, 0.15f).osc (1, "PWM", 0.4f, 0.5f, -1).ox (1, "fine", 15.0f)
        .filt (0, LADLP, 500.0f, 0.4f, 0.0f, 0.3f).lfoSync (0, SINE, D4D, 1).mod (M::Lfo1, "filter1_cutoff", 0.35f)
        .dist (SOFT, 12.0f, 0.5f, 0.5f).mono (true, 2).bassMono().done());
    L.push_back (D ("Phase Reese", "Reese", "Phase-offset unison and flanger shimmer on top of a solid core.")
        .osc (0, "Saw Tilt", 0.85f, 0.65f, -1, 6, 0.12f, 0.9f).ox (0, "blend", 0.5f).flanger (0.08f, 0.6f, 2.5f, 0.4f, 0.35f)
        .filt (0, LP24, 1600.0f, 0.1f).mono (true, 2).bassMono().done());
    L.push_back (D ("Dark Reese Sub", "Reese", "Low-passed reese layered with a clean sub for heavy rooms.")
        .osc (0, "Saw Tilt", 0.6f, 0.5f, -1, 2, 0.2f).osc (2, "Sub Shapes", 0.0f, 0.55f, -2).route (2, 3)
        .filt (0, LP24, 420.0f, 0.25f).lfoHz (0, SRAND, 0.4f, 0, 0.5f).mod (M::Lfo1, "filter1_cutoff", 0.12f).mono (true, 2).bassMono().done());
    L.push_back (D ("Metal Reese", "Reese", "Comb-filtered detuned saws: metallic, robotic edge.")
        .osc (0, "Saw Tilt", 1.0f, 0.55f, -1, 3, 0.2f).filt (0, COMBN, 220.0f, 0.5f, 0.0f, 0.0f, 1.0f).filt (1, LP24, 3000.0f, 0.1f)
        .lfoHz (0, TRI, 0.12f, 0).mod (M::Lfo1, "filter1_cutoff", 0.08f).mono (true, 2).bassMono().done());
    L.push_back (D ("Soft Reese Pad Bass", "Reese", "Gentle reese with slow attack - works for bass pads.")
        .osc (0, "Saw Tilt", 0.5f, 0.55f, -1, 4, 0.18f, 1.0f).filt (0, LP12, 800.0f, 0.1f).amp (0.08f, 0.5f, 0.9f, 0.5f)
        .chorus (0.3f, 0.4f, 0.3f).poly (8).done());
}

// =============================================================================================== GROWL
void growl (List& L)
{
    L.push_back (D ("Vowel Growl", "Growl", "Formant-vowel wavetable swept at 1/8 triplets. Talking bass.")
        .osc (0, "Vocal Growl", 0.0f, 0.75f, -1).lfoSync (0, SINE, D8T, 1).mod (M::Lfo1, "osc1_wtpos", 0.9f)
        .filt (0, FORMANT, 400.0f, 0.5f).parallel().ox (0, "route", 0.0f).dist (TUBE, 16.0f, 0.5f, 0.5f).mono (true, 2)
        .comp (-20.0f, 6.0f, 2.0f, 80.0f, 4.0f).bassMono().done());
    L.push_back (D ("Riddim Grit", "Growl", "Square-ish growl with downsampled bite and 1/16 movement.")
        .osc (0, "Growl", 0.3f, 0.7f, -1).lfoSync (0, SQR, D16, 1, 0.3f).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .crush (10.0f, 12000.0f, 0.6f).dist (HARD, 10.0f, 0.4f, 0.5f).mono (true, 0).bassMono().done());
    L.push_back (D ("Yoy Mouth", "Growl", "Formant filter sweep driven by envelope: 'yoy' articulation.")
        .osc (0, "Saw Tilt", 0.9f, 0.7f, -1, 2, 0.1f).filt (0, FORMANT, 200.0f, 0.6f, 0.5f).env (1, 0.02f, 0.3f, 0.3f, 0.2f)
        .dist (SOFT, 14.0f, 0.5f, 0.6f).mono (true, 2).bassMono().done());
    L.push_back (D ("Fold Growl", "Growl", "Wavefold table with LFO-driven fold depth and bias asymmetry.")
        .osc (0, "Wavefold", 0.2f, 0.65f, -1).ox (0, "bias", 0.2f).lfoSync (0, TRI, D8, 1).mod (M::Lfo1, "osc1_wtpos", 0.6f)
        .mod (M::Lfo1, "osc1_bias", 0.2f).filt (0, LP24, 4000.0f, 0.2f).mono (true, 2).comp (-18.0f, 5.0f).bassMono().done());
    L.push_back (D ("FM Snarl", "Growl", "FM from OSC 1 into a formant oscillator, index wobbled by LFO.")
        .osc (0, "Sub Shapes", 0.0f, 0.0f, -1).osc (1, "Formant Vowels", 0.3f, 0.7f, -1).fm (1, 2, 0.35f)
        .lfoSync (0, SINE, D8, 1).mod (M::Lfo1, "osc2_fmamt", 0.3f).mod (M::Lfo1, "osc2_wtpos", 0.4f)
        .dist (TUBE, 12.0f, 0.5f, 0.5f).mono (true, 2).bassMono().done());
    L.push_back (D ("Comb Growl", "Growl", "Comb filter swept by LFO for a hollow, screaming growl.")
        .osc (0, "Saw Tilt", 1.0f, 0.65f, -1).filt (0, COMBP, 300.0f, 0.7f, 0.0f, 0.2f, 0.0f).lfoSync (0, SAWDN, D8, 1)
        .mod (M::Lfo1, "filter1_cutoff", 0.4f).dist (FOLDBACK, 8.0f, 0.5f, 0.4f).mono (true, 2).bassMono().done());
    L.push_back (D ("Wobble Classic", "Growl", "Low-pass wobble: rate follows the mod wheel (1/4 to 1/16 feel).")
        .osc (0, "Saw Tilt", 0.9f, 0.6f, -1, 2, 0.12f).osc (1, "Basic Shapes", 0.75f, 0.45f, -1).filt (0, LADLP, 300.0f, 0.5f)
        .lfoSync (0, SINE, D4, 1).mod (M::Lfo1, "filter1_cutoff", 0.45f).mod (M::ModWheel, "filter1_reso", 0.3f)
        .dist (SOFT, 10.0f, 0.5f, 0.5f).mono (true, 2).bassMono().done());
    L.push_back (D ("Phase Mouth", "Growl", "Phase-FM warp sweeps on a vowel table: chewy and liquid.")
        .osc (0, "Formant Vowels", 0.5f, 0.7f, -1).warp (0, PHASEFM, 0.2f).lfoSync (0, TRI, D8D, 1).mod (M::Lfo1, "osc1_shape", 0.5f)
        .mod (M::Lfo1, "osc1_wtpos", 0.3f).sat (10.0f, 0.3f, 0.4f).mono (true, 2).bassMono().done());
}

// =============================================================================================== LEADS
void lead (List& L)
{
    L.push_back (D ("Supersaw Lead", "Lead", "Seven-voice supersaw with delay and reverb. Trance and EDM.")
        .osc (0, "Saw Tilt", 1.0f, 0.55f, 0, 7, 0.28f, 0.9f).filt (0, LP12, 9000.0f, 0.1f).amp (0.005f, 0.4f, 0.85f, 0.25f)
        .delay (D8D, 0.35f, 0.18f).reverb (0.6f, 2.5f, 0.2f).done());
    L.push_back (D ("Glide Saw Lead", "Lead", "Mono legato saw with glide on overlapping notes. Vibrato on the mod wheel.")
        .osc (0, "Saw Tilt", 0.85f, 0.7f).osc (1, "Saw Tilt", 0.85f, 0.4f).ox (1, "fine", 9.0f).filt (0, LADLP, 3500.0f, 0.25f, 0.15f)
        .mono (true, 2, 0.09f).lfoHz (1, SINE, 5.5f, 0, 0.0f, 0.4f)
        .mod (M::Lfo2, "osc1_fine", 0.08f).mod (M::Lfo2, "osc2_fine", 0.08f).delay (D8, 0.3f, 0.15f).done());
    L.push_back (D ("Square Chip Lead", "Lead", "Pulse lead with quick pitch blip and bitcrush. Retro games.")
        .osc (0, "Chip", 0.1f, 0.65f).mod (M::Env3, "osc1_pitch", 0.125f).env (2, 0.0f, 0.04f, 0.0f, 0.02f)
        .crush (8.0f, 22000.0f, 0.7f).amp (0.001f, 0.2f, 0.8f, 0.08f).mono (false, 1, 0.03f).done());
    L.push_back (D ("Sync Scream", "Lead", "Hard-sync lead with envelope-swept ratio. Cuts through anything.")
        .osc (0, "Basic Shapes", 0.5f, 0.65f).warp (0, SYNC, 0.15f).mod (M::Env2, "osc1_shape", 0.4f).env (1, 0.001f, 0.6f, 0.2f, 0.3f)
        .mod (M::ModWheel, "osc1_shape", 0.4f).ultra().filt (0, LP24, 7000.0f, 0.15f).dist (SOFT, 8.0f, 0.6f, 0.4f).delay (D16D, 0.3f, 0.12f).done());
    L.push_back (D ("Soft Sine Lead", "Lead", "Breathy sine-triangle lead with noise air and delayed vibrato.")
        .osc (0, "Basic Shapes", 0.12f, 0.7f).noise (1, 0.05f, 0.5f).route (1, 1).filt (1, HP12, 3000.0f, 0.1f).parallel()
        .lfoHz (1, SINE, 5.0f, 1, 0.0f, 0.8f).mod (M::Lfo2, "osc1_fine", 0.1f).amp (0.03f, 0.3f, 0.9f, 0.3f)
        .reverb (0.5f, 2.0f, 0.25f).delay (D4, 0.3f, 0.15f).mono (true, 2, 0.05f).done());
    L.push_back (D ("Fat Analog Lead", "Lead", "Three detuned saws through a driven ladder. Analog drift on.")
        .osc (0, "Saw Tilt", 0.9f, 0.55f).osc (1, "Saw Tilt", 0.9f, 0.5f).ox (1, "fine", -8.0f).osc (2, "PWM", 0.2f, 0.45f, -1)
        .filt (0, LADLP, 2600.0f, 0.3f, 0.25f, 0.25f).analog (0.3f, 0.25f, 0.2f, 0.5f).mono (true, 2, 0.06f).chorus (0.5f, 0.3f, 0.2f).done());
    L.push_back (D ("Vowel Lead", "Lead", "Talking lead: aftertouch and mod wheel morph the vowel.")
        .osc (0, "Formant Vowels", 0.0f, 0.7f, 0, 3, 0.1f).mod (M::ModWheel, "osc1_wtpos", 0.9f).mod (M::Aftertouch, "osc1_wtpos", 0.5f)
        .amp (0.01f, 0.4f, 0.85f, 0.2f).delay (D8, 0.25f, 0.15f).reverb (0.4f, 1.6f, 0.15f).done());
    L.push_back (D ("FM Brass Lead", "Lead", "FM brass: index swells with the envelope, slight attack bend.")
        .osc (0, "Basic Shapes", 0.0f, 0.7f).fm (0, 1, 0.1f, 1.0f).mod (M::Env2, "osc1_fmamt", 0.35f).env (1, 0.06f, 0.4f, 0.6f, 0.3f)
        .mod (M::Env3, "osc1_pitch", -0.01f).env (2, 0.0f, 0.08f, 0.0f, 0.05f).amp (0.03f, 0.3f, 0.85f, 0.2f).reverb (0.5f, 1.8f, 0.18f).done());
    L.push_back (D ("Pluck Lead Delay", "Lead", "Short plucky lead drenched in ping-pong 1/8 dotted delay.")
        .osc (0, "Hollow Square", 0.6f, 0.65f, 0, 3, 0.12f).filt (0, LP24, 1500.0f, 0.3f, 0.5f).env (1, 0.001f, 0.25f, 0.1f, 0.2f)
        .amp (0.001f, 0.6f, 0.4f, 0.3f).delay (D8D, 0.5f, 0.3f).reverb (0.5f, 2.0f, 0.15f).done());
    L.push_back (D ("Mirror Lead", "Lead", "Mirror-warped saw: octave-like brightness controlled by LFO.")
        .osc (0, "Saw Tilt", 1.0f, 0.6f, 0, 2, 0.08f).warp (0, MIRROR, 0.3f).lfoHz (0, TRI, 0.4f, 0).mod (M::Lfo1, "osc1_shape", 0.3f)
        .filt (0, LP24, 6000.0f, 0.1f).delay (D4, 0.3f, 0.15f).mono (true, 2).done());
    L.push_back (D ("Whistle Lead", "Lead", "Clean resonant band-pass whistle with portamento.")
        .noise (0, 0.5f, 0.0f).filt (0, BP24, 1000.0f, 0.92f, 0.0f, 0.0f, 1.0f).osc (1, "Basic Shapes", 0.0f, 0.25f, 1)
        .mono (true, 1, 0.1f).amp (0.03f, 0.3f, 0.9f, 0.25f).reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Stab Lead Unison", "Lead", "Wide unison square stab with short envelope - house chord leads.")
        .osc (0, "Basic Shapes", 0.7f, 0.55f, 0, 5, 0.2f, 1.0f).filt (0, LP24, 2000.0f, 0.2f, 0.45f).env (1, 0.001f, 0.2f, 0.0f, 0.15f)
        .amp (0.001f, 0.35f, 0.0f, 0.2f).reverb (0.5f, 1.5f, 0.2f).done());
}

// ================================================================================================ PADS
void pad (List& L)
{
    L.push_back (D ("Warm Analog Pad", "Pad", "Slow detuned saws, gentle ladder, drift and chorus.")
        .osc (0, "Saw Tilt", 0.5f, 0.5f, 0, 6, 0.18f, 1.0f).osc (1, "PWM", 0.4f, 0.4f, -1, 2, 0.1f).filt (0, LADLP, 1400.0f, 0.15f)
        .amp (0.7f, 1.5f, 0.85f, 1.8f).analog (0.4f, 0.3f, 0.1f, 0.3f, 0.4f).chorus (0.4f, 0.5f, 0.35f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Glass Pad", "Pad", "Glass partials morphing slowly with a shimmering reverb.")
        .osc (0, "Glass", 0.3f, 0.55f, 0, 4, 0.12f).osc (1, "Basic Shapes", 0.0f, 0.4f, -1).lfoHz (0, SINE, 0.08f, 0)
        .mod (M::Lfo1, "osc1_wtpos", 0.4f).amp (0.9f, 2.0f, 0.8f, 3.0f).reverb (0.85f, 7.0f, 0.4f, 0.3f).done());
    L.push_back (D ("Vowel Choir Pad", "Pad", "Formant table with very slow vowel drift: synthetic choir.")
        .osc (0, "Formant Vowels", 0.2f, 0.55f, 0, 6, 0.15f, 1.0f).lfoHz (0, TRI, 0.05f, 0).mod (M::Lfo1, "osc1_wtpos", 0.6f)
        .amp (1.0f, 2.0f, 0.9f, 2.5f).chorus (0.3f, 0.6f, 0.4f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Harmonic Bloom", "Pad", "Harmonics bloom in as the envelope opens the wave position.")
        .osc (0, "Harmonic Sweep", 0.0f, 0.55f, 0, 4, 0.12f).mod (M::Env2, "osc1_wtpos", 0.8f).env (1, 2.5f, 3.0f, 0.6f, 3.0f)
        .amp (1.2f, 2.0f, 0.85f, 2.5f).reverb (0.7f, 5.0f, 0.3f).done());
    L.push_back (D ("Organ Pad", "Pad", "Drawbar pad with Leslie-like chorus and phaser.")
        .osc (0, "Organ", 0.5f, 0.6f, 0, 2, 0.06f).lfoHz (0, SINE, 0.2f, 0).mod (M::Lfo1, "osc1_wtpos", 0.15f)
        .amp (0.3f, 1.0f, 0.95f, 1.0f).chorus (5.5f, 0.2f, 0.4f, 6.0f).phaser (0.3f, 0.5f, 900.0f, 0.3f, 1, 0.3f).reverb (0.6f, 2.5f, 0.2f).done());
    L.push_back (D ("Dream Strings", "Pad", "String-machine ensemble: PWM, chorus and long reverb.")
        .osc (0, "PWM", 0.2f, 0.55f, 0, 4, 0.15f, 1.0f).lfoHz (0, TRI, 0.6f, 0).mod (M::Lfo1, "osc1_wtpos", 0.3f)
        .filt (0, LP12, 4500.0f, 0.1f).amp (0.5f, 1.0f, 0.9f, 1.5f).chorus (0.7f, 0.7f, 0.5f, 15.0f).reverb (0.75f, 4.5f, 0.3f).done());
    L.push_back (D ("Evolving Table Pad", "Pad", "Random-spectral wavetable crawling through 32 frames.")
        .oscRand (0, 0, 4711u, 0.7f, 32, 0.0f, 0.55f, 0, 4).lfoHz (0, SRAND, 0.1f, 0, 0.8f).mod (M::Lfo1, "osc1_wtpos", 0.5f)
        .p ("osc1_detune", 0.15f).amp (1.0f, 2.0f, 0.85f, 3.0f).filt (0, LP12, 5000.0f, 0.1f).reverb (0.8f, 6.0f, 0.35f).done());
    L.push_back (D ("Soft Square Pad", "Pad", "Hollow squares, gentle low-pass, warm and simple.")
        .osc (0, "Hollow Square", 0.2f, 0.55f, 0, 4, 0.1f).filt (0, LP24, 2200.0f, 0.1f).amp (0.6f, 1.0f, 0.9f, 1.5f)
        .reverb (0.6f, 3.0f, 0.25f).done());
    L.push_back (D ("Wide Detune Pad", "Pad", "Twelve-voice unison wall for big chords.")
        .osc (0, "Saw Tilt", 0.75f, 0.45f, 0, 12, 0.3f, 1.0f).filt (0, LP24, 3200.0f, 0.1f).amp (0.4f, 1.5f, 0.85f, 2.0f)
        .imager (1.4f, 200.0f).reverb (0.8f, 5.0f, 0.3f).done());
    L.push_back (D ("Filter Sweep Pad", "Pad", "Resonant low-pass sweeping over two bars.")
        .osc (0, "Saw Tilt", 0.9f, 0.5f, 0, 5, 0.2f).filt (0, LP24, 600.0f, 0.55f).lfoSync (0, SINE, B2, 0).mod (M::Lfo1, "filter1_cutoff", 0.4f)
        .amp (0.5f, 1.0f, 0.9f, 2.0f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Bias Shimmer Pad", "Pad", "Vertical bias and drive add slowly moving even harmonics.")
        .osc (0, "Basic Shapes", 0.12f, 0.55f, 0, 4, 0.12f).ox (0, "drive", 0.4f).lfoHz (0, SINE, 0.07f, 0).mod (M::Lfo1, "osc1_bias", 0.35f)
        .amp (0.9f, 2.0f, 0.85f, 2.5f).chorus (0.3f, 0.5f, 0.3f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Pulse Width Pad", "Pad", "Pulse-width warp swept by a slow LFO on a sine-saw blend.")
        .osc (0, "Basic Shapes", 0.45f, 0.55f, 0, 3, 0.1f).warp (0, PWIDTH, 0.3f).lfoHz (0, TRI, 0.12f, 0).mod (M::Lfo1, "osc1_shape", 0.25f)
        .amp (0.6f, 1.5f, 0.85f, 2.0f).filt (0, LP12, 4000.0f, 0.1f).reverb (0.7f, 4.0f, 0.3f).done());
}

// =============================================================================================== PLUCK
void pluck (List& L)
{
    L.push_back (D ("Classic Pluck", "Pluck", "Saw through an enveloped ladder with a short tail.")
        .osc (0, "Saw Tilt", 0.9f, 0.65f, 0, 3, 0.12f).filt (0, LADLP, 400.0f, 0.3f, 0.6f).env (1, 0.001f, 0.25f, 0.0f, 0.2f)
        .amp (0.001f, 0.6f, 0.0f, 0.3f).reverb (0.5f, 1.8f, 0.2f).done());
    L.push_back (D ("Marimba Pluck", "Pluck", "FM mallet tone with fast decay.")
        .osc (0, "Basic Shapes", 0.0f, 0.75f).fm (0, 1, 0.4f, 4.0f).mod (M::Env2, "osc1_fmamt", -0.4f).env (1, 0.0f, 0.06f, 0.0f, 0.05f)
        .amp (0.001f, 0.5f, 0.0f, 0.4f).reverb (0.4f, 1.2f, 0.15f).done());
    L.push_back (D ("Digital Pluck", "Pluck", "Quantised steps sweep down on each note: crisp and modern.")
        .osc (0, "Digital Steps", 0.0f, 0.65f).mod (M::Env2, "osc1_wtpos", 0.9f).env (1, 0.0f, 0.3f, 0.0f, 0.1f)
        .amp (0.001f, 0.4f, 0.0f, 0.25f).delay (D8, 0.35f, 0.2f).done());
    L.push_back (D ("Glass Harp", "Pluck", "Glass partials with a band-pass decay and long reverb.")
        .osc (0, "Glass", 0.7f, 0.6f).filt (0, BP12, 2500.0f, 0.3f, 0.3f).env (1, 0.0f, 0.4f, 0.0f, 0.2f)
        .amp (0.001f, 1.2f, 0.0f, 1.0f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Arp Pluck 1/16", "Pluck", "Arpeggiated pluck at 1/16 over two octaves - hold a chord.")
        .osc (0, "Hollow Square", 0.5f, 0.6f, 0, 2, 0.1f).filt (0, LP24, 700.0f, 0.35f, 0.55f).env (1, 0.001f, 0.15f, 0.0f, 0.1f)
        .amp (0.001f, 0.3f, 0.0f, 0.2f).arp (0, 10, 2, 0.5f).delay (D8D, 0.35f, 0.2f).reverb (0.5f, 1.8f, 0.15f).done());
    L.push_back (D ("Koto Pluck", "Pluck", "Comb filter excited by a short noise burst: plucked string.")
        .noise (0, 0.6f, 0.2f).env (2, 0.0f, 0.02f, 0.0f, 0.01f).mod (M::Env3, "osc1_level", 1.0f).p ("osc1_level", 0.0f)
        .filt (0, COMBP, 220.0f, 0.95f, 0.0f, 0.0f, 1.0f).amp (0.001f, 2.0f, 0.0f, 1.0f).reverb (0.4f, 1.5f, 0.15f).done());
    L.push_back (D ("Sync Pluck", "Pluck", "Hard-sync burst that collapses to the fundamental.")
        .osc (0, "Sync Saw", 0.0f, 0.6f).mod (M::Env2, "osc1_wtpos", 0.8f).env (1, 0.0f, 0.18f, 0.0f, 0.1f)
        .filt (0, LP24, 5000.0f, 0.1f, 0.3f).amp (0.001f, 0.35f, 0.0f, 0.25f).reverb (0.5f, 1.8f, 0.2f).done());
    L.push_back (D ("Pizzicato", "Pluck", "Short string pizzicato from a bowed saw table.")
        .osc (0, "Saw Tilt", 0.6f, 0.65f, 0, 2, 0.08f).filt (0, LP12, 1200.0f, 0.1f, 0.5f).env (1, 0.0f, 0.12f, 0.0f, 0.1f)
        .amp (0.002f, 0.25f, 0.0f, 0.15f).reverb (0.6f, 2.0f, 0.25f).done());
    L.push_back (D ("Fold Pluck", "Pluck", "Wavefold amount follows the envelope: rich attack, round tail.")
        .osc (0, "Wavefold", 0.0f, 0.65f).mod (M::Env2, "osc1_wtpos", 0.8f).env (1, 0.0f, 0.2f, 0.0f, 0.1f)
        .amp (0.001f, 0.6f, 0.0f, 0.3f).delay (D16D, 0.3f, 0.15f).done());
    L.push_back (D ("Seq Pluck Phrase", "Pluck", "Step-sequenced pluck phrase: hold a single note.")
        .osc (0, "Saw Tilt", 0.8f, 0.6f, 0, 2, 0.1f).filt (0, LP24, 900.0f, 0.4f, 0.5f).env (1, 0.0f, 0.15f, 0.0f, 0.1f)
        .amp (0.001f, 0.3f, 0.0f, 0.2f).seq (10, { 0, 12, 7, -99, 3, 12, 10, 7, 0, 15, 12, -99, 7, 5, 3, 10 }, 0.4f)
        .delay (D8D, 0.3f, 0.15f).done());
}

// ================================================================================================ KEYS
void keys (List& L)
{
    L.push_back (D ("FM Electric Piano", "Keys", "Two-operator EP: bright tine attack, mellow body. Velocity = brightness.")
        .osc (0, "Basic Shapes", 0.0f, 0.7f).fm (0, 1, 0.3f, 1.0f).mod (M::Env2, "osc1_fmamt", 0.2f).mod (M::Velocity, "osc1_fmamt", 0.25f)
        .env (1, 0.0f, 0.5f, 0.0f, 0.3f).amp (0.001f, 2.0f, 0.2f, 0.4f).chorus (0.6f, 0.3f, 0.25f).done());
    L.push_back (D ("Drawbar Organ", "Keys", "Full drawbar registration with percussion click.")
        .osc (0, "Organ", 0.8f, 0.6f).noise (3, 0.04f, 0.5f).env (2, 0.0f, 0.015f, 0.0f, 0.01f).mod (M::Env3, "osc4_level", 0.5f).p ("osc4_level", 0.0f)
        .amp (0.003f, 0.2f, 1.0f, 0.05f).chorus (6.0f, 0.15f, 0.35f, 5.0f).done());
    L.push_back (D ("Clavinet Funk", "Keys", "Bright pulse with band-pass envelope: clav-style.")
        .osc (0, "PWM", 0.6f, 0.65f).filt (0, BP12, 1600.0f, 0.4f, 0.3f).env (1, 0.0f, 0.15f, 0.2f, 0.1f)
        .amp (0.001f, 0.6f, 0.3f, 0.1f).phaser (0.6f, 0.6f, 1200.0f, 0.3f, 1, 0.3f).done());
    L.push_back (D ("Soft Keys", "Keys", "Sine-triangle keys with gentle decay and room.")
        .osc (0, "Basic Shapes", 0.18f, 0.7f).osc (1, "Basic Shapes", 0.0f, 0.3f, 1).amp (0.002f, 1.5f, 0.25f, 0.5f)
        .reverb (0.5f, 1.8f, 0.2f).done());
    L.push_back (D ("Toy Piano", "Keys", "Inharmonic FM toy piano with short tail.")
        .osc (0, "Basic Shapes", 0.0f, 0.65f).fm (0, 1, 0.3f, 3.5f).mod (M::Env2, "osc1_fmamt", -0.25f).env (1, 0.0f, 0.2f, 0.0f, 0.1f)
        .amp (0.001f, 0.8f, 0.0f, 0.5f).reverb (0.4f, 1.2f, 0.15f).done());
    L.push_back (D ("Wurly Drive", "Keys", "Reed-piano character: asymmetric bias drive and tremolo.")
        .osc (0, "Basic Shapes", 0.15f, 0.7f).ox (0, "bias", 0.25f).ox (0, "drive", 0.35f).mod (M::Velocity, "osc1_drive", 0.3f)
        .lfoHz (0, SINE, 5.0f, 0).mod (M::Lfo1, "osc1_level", 0.12f).amp (0.002f, 1.2f, 0.3f, 0.3f).done());
    L.push_back (D ("Harpsichord", "Keys", "Bright double-course pluck with octave layer.")
        .osc (0, "Saw Tilt", 1.0f, 0.5f).osc (1, "Hollow Square", 0.8f, 0.35f, 1).filt (0, HP12, 300.0f, 0.1f)
        .amp (0.001f, 0.9f, 0.0f, 0.4f).reverb (0.5f, 1.5f, 0.2f).done());
    L.push_back (D ("Lo-Fi Keys", "Keys", "Detuned soft keys with tape wobble and bitcrush.")
        .osc (0, "Basic Shapes", 0.2f, 0.65f, 0, 2, 0.08f).lfoHz (0, SRAND, 2.5f, 0, 0.6f).mod (M::Lfo1, "osc1_fine", 0.06f)
        .amp (0.002f, 1.3f, 0.3f, 0.4f).crush (11.0f, 16000.0f, 0.6f).eq (100.0f, 0.0f, 1000.0f, 0.0f, -6.0f, 9000.0f).analog (0.5f, 0.3f).done());
    L.push_back (D ("Bell Piano", "Keys", "Piano body with a metallic bell partial on top.")
        .osc (0, "Basic Shapes", 0.08f, 0.6f).osc (1, "Glass", 0.6f, 0.3f, 1).env (2, 0.0f, 0.6f, 0.0f, 0.4f).mod (M::Env3, "osc2_level", 0.4f).p ("osc2_level", 0.0f)
        .amp (0.001f, 2.0f, 0.15f, 0.5f).reverb (0.6f, 2.5f, 0.2f).done());
}

// ================================================================================================ BELLS
void bell (List& L)
{
    L.push_back (D ("FM Tubular Bell", "Bell", "Inharmonic FM (ratio 3.5) with a long ring.")
        .osc (0, "Basic Shapes", 0.0f, 0.65f).fm (0, 1, 0.35f, 3.5f).mod (M::Env2, "osc1_fmamt", -0.2f).env (1, 0.0f, 2.0f, 0.0f, 2.0f)
        .amp (0.001f, 4.0f, 0.0f, 3.0f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Glass Bell", "Bell", "Square-number partials, crystalline and pure.")
        .osc (0, "Glass", 1.0f, 0.6f).amp (0.001f, 3.0f, 0.0f, 2.5f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Metal Chime", "Bell", "Prime partials and ring-mod-like FM: wind chime.")
        .osc (0, "Metallic", 0.8f, 0.55f).fm (0, 1, 0.15f, 2.76f).amp (0.001f, 2.5f, 0.0f, 2.0f).delay (D4D, 0.4f, 0.2f).reverb (0.8f, 5.0f, 0.3f).done());
    L.push_back (D ("Music Box", "Bell", "Tiny comb-plucked music box an octave up.")
        .osc (0, "Basic Shapes", 0.0f, 0.6f, 1).fm (0, 1, 0.2f, 5.0f).mod (M::Env2, "osc1_fmamt", -0.2f).env (1, 0.0f, 0.1f, 0.0f, 0.1f)
        .amp (0.001f, 1.5f, 0.0f, 1.0f).reverb (0.5f, 2.0f, 0.25f).done());
    L.push_back (D ("Gamelan", "Bell", "Detuned metallic pair beating like a gamelan.")
        .osc (0, "Metallic", 0.5f, 0.5f).osc (1, "Metallic", 0.5f, 0.45f).ox (1, "fine", 7.0f).fm (0, 1, 0.1f, 2.0f)
        .amp (0.001f, 3.0f, 0.0f, 2.0f).reverb (0.6f, 3.0f, 0.25f).done());
    L.push_back (D ("Octave Bell Pad", "Bell", "Octave-stack partials with a swelling tail.")
        .osc (0, "Octave Stack", 0.6f, 0.55f, 0, 2, 0.05f).env (1, 0.0f, 1.0f, 0.0f, 1.0f).mod (M::Env2, "osc1_wtpos", 0.3f)
        .amp (0.001f, 3.0f, 0.2f, 3.0f).reverb (0.85f, 6.0f, 0.35f).done());
    L.push_back (D ("Phase Bell", "Bell", "Phase-FM warp on a sine for an electric bell.")
        .osc (0, "Basic Shapes", 0.0f, 0.65f).warp (0, PHASEFM, 0.8f).mod (M::Env2, "osc1_shape", -0.6f).env (1, 0.0f, 1.0f, 0.0f, 1.0f)
        .amp (0.001f, 2.5f, 0.0f, 2.0f).chorus (0.4f, 0.3f, 0.2f).reverb (0.7f, 3.5f, 0.25f).done());
    L.push_back (D ("Bell Arp", "Bell", "Bright bells arpeggiated up and down over three octaves.")
        .osc (0, "Glass", 0.8f, 0.55f).fm (0, 1, 0.1f, 3.0f).amp (0.001f, 0.8f, 0.0f, 0.6f).arp (2, 10, 3, 0.4f)
        .delay (D8D, 0.4f, 0.2f).reverb (0.7f, 3.0f, 0.25f).done());
}

// ========================================================================================= ATMOSPHERIC
void atmospheric (List& L)
{
    L.push_back (D ("Wind Atmosphere", "Atmospheric", "Filtered noise with random cutoff gusts and huge reverb.")
        .noise (0, 0.45f, -0.2f).filt (0, BP12, 900.0f, 0.4f).lfoHz (0, SRAND, 0.2f, 0, 0.8f).mod (M::Lfo1, "filter1_cutoff", 0.35f)
        .amp (1.5f, 2.0f, 0.9f, 3.0f).reverb (0.9f, 9.0f, 0.5f).done());
    L.push_back (D ("Distant Choir", "Atmospheric", "Vowel table pad heard from far away: predelay and dark reverb.")
        .osc (0, "Formant Vowels", 0.4f, 0.5f, 0, 6, 0.2f, 1.0f).filt (0, LP12, 2500.0f, 0.1f).amp (2.0f, 3.0f, 0.85f, 4.0f)
        .reverb (0.95f, 10.0f, 0.55f, 0.8f, 80.0f).done());
    L.push_back (D ("Space Station", "Atmospheric", "Slow-moving spectral table with sample-and-hold blips.")
        .osc (0, "Spectral Noise", 0.0f, 0.5f, 0, 3, 0.1f).lfoHz (0, SINE, 0.03f, 0).mod (M::Lfo1, "osc1_wtpos", 0.7f)
        .osc (1, "Basic Shapes", 0.0f, 0.15f, 2).lfoHz (1, SH, 3.0f, 0).mod (M::Lfo2, "osc2_pitch", 0.2f)
        .amp (2.0f, 2.0f, 0.9f, 4.0f).delay (D4D, 0.6f, 0.3f).reverb (0.9f, 8.0f, 0.45f).done());
    L.push_back (D ("Underwater", "Atmospheric", "Low-passed bubbles: random pitch blips through a dark filter.")
        .osc (0, "Basic Shapes", 0.0f, 0.4f, 1).lfoHz (0, SH, 8.0f, 0).mod (M::Lfo1, "osc1_pitch", 0.15f).filt (0, LP24, 700.0f, 0.5f)
        .noise (1, 0.2f, -0.7f).amp (1.0f, 2.0f, 0.9f, 3.0f).chorus (0.2f, 0.8f, 0.5f).reverb (0.85f, 6.0f, 0.4f).done());
    L.push_back (D ("Aurora", "Atmospheric", "Glass and octave partials drifting in stereo.")
        .osc (0, "Glass", 0.5f, 0.45f, 0, 8, 0.2f, 1.0f).osc (1, "Octave Stack", 0.3f, 0.35f, 1, 4, 0.15f).lfoHz (0, SINE, 0.06f, 0)
        .mod (M::Lfo1, "osc1_wtpos", 0.4f).mod (M::Lfo1, "osc2_wtpos", -0.3f).analog (0.3f, 0.2f, 0.0f, 0.0f, 0.8f)
        .amp (2.5f, 3.0f, 0.9f, 5.0f).reverb (0.95f, 10.0f, 0.5f).done());
    L.push_back (D ("Night Field", "Atmospheric", "Crickets-like digital noise bursts over a soft drone.")
        .noise (0, 0.15f, 0.6f, 0.8f).lfoSync (0, SQR, D16, 0).mod (M::Lfo1, "osc1_level", 0.3f).osc (1, "Sub Shapes", 0.0f, 0.4f, -1)
        .amp (1.5f, 2.0f, 0.9f, 3.0f).reverb (0.8f, 6.0f, 0.4f).done());
    L.push_back (D ("Frozen Air", "Atmospheric", "High, airy noise and glass through a resonant high-pass.")
        .osc (0, "Glass", 0.9f, 0.35f, 1, 4, 0.1f).noise (1, 0.25f, 0.4f).filt (0, HP24, 1500.0f, 0.3f)
        .amp (2.0f, 2.0f, 0.9f, 5.0f).reverb (0.95f, 12.0f, 0.55f, 0.2f).done());
    L.push_back (D ("Rain Texture Pad", "Atmospheric", "Random steps of filtered noise like rain on glass.")
        .noise (0, 0.3f, 0.2f, 0.5f).filt (0, BP24, 2500.0f, 0.5f).lfoHz (0, SH, 12.0f, 0).mod (M::Lfo1, "filter1_cutoff", 0.25f)
        .osc (1, "Hollow Square", 0.3f, 0.3f, 0, 3, 0.1f).amp (1.0f, 2.0f, 0.9f, 3.0f).reverb (0.85f, 7.0f, 0.45f).done());
}

// =========================================================================================== CINEMATIC
void cinematic (List& L)
{
    L.push_back (D ("Trailer Brass", "Cinematic", "Huge low brass: swelling FM index and saturation.")
        .osc (0, "Saw Tilt", 0.7f, 0.55f, -1, 4, 0.12f).osc (1, "Basic Shapes", 0.75f, 0.4f, -1).filt (0, LP24, 600.0f, 0.2f, 0.5f)
        .env (1, 0.4f, 1.0f, 0.6f, 1.0f).amp (0.3f, 1.0f, 0.9f, 1.2f).sat (12.0f, 0.3f, 0.6f).reverb (0.85f, 5.0f, 0.3f).done());
    L.push_back (D ("Braam", "Cinematic", "Distorted low cluster with slow filter rise.")
        .osc (0, "Saw Tilt", 0.9f, 0.5f, -2, 6, 0.2f).osc (1, "Hollow Square", 0.5f, 0.4f, -1, 2).filt (0, LADLP, 300.0f, 0.4f, 0.5f, 0.4f)
        .env (1, 2.0f, 2.0f, 0.5f, 2.0f).amp (0.5f, 2.0f, 0.9f, 2.0f).dist (TUBE, 18.0f, 0.4f, 0.5f).reverb (0.9f, 7.0f, 0.35f).done());
    L.push_back (D ("Tension Strings", "Cinematic", "Tremolo strings with slow vibrato build.")
        .osc (0, "Saw Tilt", 0.8f, 0.5f, 0, 6, 0.15f, 1.0f).lfoSync (0, SINE, D16, 0).mod (M::Lfo1, "osc1_level", 0.2f)
        .lfoHz (1, SINE, 5.0f, 1, 0.0f, 2.0f).mod (M::Lfo2, "osc1_fine", 0.06f).filt (0, LP12, 4000.0f, 0.1f)
        .amp (0.6f, 1.0f, 0.9f, 1.5f).reverb (0.85f, 5.0f, 0.35f).done());
    L.push_back (D ("Epic Choir Swell", "Cinematic", "Formant choir that opens over time with a heavy hall.")
        .osc (0, "Formant Vowels", 0.0f, 0.5f, 0, 8, 0.15f, 1.0f).mod (M::Env2, "osc1_wtpos", 0.5f).env (1, 3.0f, 4.0f, 0.6f, 3.0f)
        .amp (1.5f, 3.0f, 0.9f, 4.0f).reverb (0.95f, 9.0f, 0.45f, 0.6f, 40.0f).done());
    L.push_back (D ("Dark Ostinato", "Cinematic", "Pulsing low pluck sequence for underscore.")
        .osc (0, "Hollow Square", 0.5f, 0.6f, -1, 2, 0.1f).filt (0, LP24, 500.0f, 0.3f, 0.4f).env (1, 0.0f, 0.2f, 0.0f, 0.1f)
        .amp (0.001f, 0.3f, 0.0f, 0.2f).seq (7, { 0, 0, 12, 0, 0, 7, 0, 3 }, 0.5f).reverb (0.7f, 3.0f, 0.25f).done());
    L.push_back (D ("Riser Swell", "Cinematic", "Eight-second swell: pitch, cutoff and noise rise together.")
        .osc (0, "Saw Tilt", 0.8f, 0.5f, 0, 5, 0.2f).noise (1, 0.25f, 0.3f).env (2, 8.0f, 1.0f, 1.0f, 1.0f, -0.4f)
        .mod (M::Env3, "osc1_pitch", 0.12f).mod (M::Env3, "filter1_cutoff", 0.5f).filt (0, LP24, 300.0f, 0.3f)
        .amp (0.5f, 1.0f, 1.0f, 2.0f).reverb (0.9f, 6.0f, 0.4f).done());
    L.push_back (D ("Low Horn", "Cinematic", "Muted horn: band-limited saw with breath noise.")
        .osc (0, "Saw Tilt", 0.4f, 0.6f, -1).noise (1, 0.05f, -0.3f).filt (0, LP24, 900.0f, 0.15f, 0.3f).env (1, 0.15f, 0.5f, 0.6f, 0.5f)
        .amp (0.12f, 0.8f, 0.9f, 0.6f).reverb (0.8f, 4.0f, 0.3f).done());
    L.push_back (D ("Hybrid Pulse", "Cinematic", "Gated 1/16 pulse layer for action cues.")
        .osc (0, "PWM", 0.4f, 0.55f, 0, 3, 0.1f).lfoSync (0, SQR, D16, 0).uni (0, false).p ("osc1_level", 0.0f).mod (M::Lfo1, "osc1_level", 0.7f)
        .filt (0, LP24, 2000.0f, 0.3f).amp (0.01f, 1.0f, 1.0f, 0.5f).delay (D8D, 0.3f, 0.15f).reverb (0.7f, 3.0f, 0.25f).done());
}

// ============================================================================================= TEXTURE
void texture (List& L)
{
    L.push_back (D ("Granular Shimmer", "Texture", "Random-frame scanning at high rate: pseudo-granular sparkle.")
        .oscRand (0, 0, 9001u, 0.9f, 64, 0.0f, 0.5f, 0, 4).lfoHz (0, SH, 14.0f, 0).mod (M::Lfo1, "osc1_wtpos", 1.0f)
        .amp (0.5f, 2.0f, 0.9f, 3.0f).reverb (0.9f, 7.0f, 0.45f).delay (D16D, 0.5f, 0.25f).done());
    L.push_back (D ("Static Field", "Texture", "Crackling S&H noise through a phaser.")
        .noise (0, 0.35f, 0.3f, 0.7f).lfoHz (0, SRAND, 0.5f, 0).mod (M::Lfo1, "osc1_wtpos", 0.3f)
        .phaser (0.1f, 0.8f, 1200.0f, 0.6f, 4, 0.6f).amp (0.3f, 2.0f, 0.9f, 2.0f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Bit Rain", "Texture", "Bit-crushed random steps scattered by delay.")
        .oscRand (0, 3, 31337u, 0.6f, 16, 0.0f, 0.45f).lfoHz (0, SH, 6.0f, 0).mod (M::Lfo1, "osc1_wtpos", 0.8f)
        .crush (6.0f, 9000.0f, 0.8f).delay (D8T, 0.55f, 0.35f).amp (0.1f, 1.0f, 0.8f, 1.5f).done());
    L.push_back (D ("Glitch Loop", "Texture", "Stepped custom LFO slices the amplitude and wave position.")
        .osc (0, "Digital Steps", 0.3f, 0.6f).lfoSync (0, CUSTOM, B1, 0).lfoShape (0, "0,1,0;0.12,0,0;0.25,0.8,0;0.31,0,0;0.5,1,0;0.56,0.2,0;0.75,0.6,0;0.81,0,0;")
        .uni (0, false).p ("osc1_level", 0.15f).mod (M::Lfo1, "osc1_level", 0.6f).mod (M::Lfo1, "osc1_wtpos", 0.5f)
        .amp (0.01f, 1.0f, 1.0f, 0.5f).reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Metallic Scrape", "Texture", "Comb resonance swept by slow random motion.")
        .noise (0, 0.4f, 0.0f).filt (0, COMBN, 150.0f, 0.8f, 0.0f, 0.0f, 0.5f).lfoHz (0, SRAND, 0.3f, 0, 0.7f)
        .mod (M::Lfo1, "filter1_cutoff", 0.3f).amp (0.5f, 2.0f, 0.9f, 2.0f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Paper Noise", "Texture", "Thin crinkled noise with high-pass flutter.")
        .noise (0, 0.35f, 0.7f, 0.3f).filt (0, HP12, 2500.0f, 0.4f).lfoHz (0, SH, 20.0f, 0).mod (M::Lfo1, "filter1_cutoff", 0.2f)
        .amp (0.05f, 1.0f, 0.8f, 1.0f).reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Chaos Table", "Texture", "Logistic-map wavetable scanned by a smooth random LFO.")
        .oscRand (0, 4, 777u, 0.8f, 32, 0.0f, 0.5f, 0, 3).lfoHz (0, SRAND, 0.8f, 0, 0.5f).mod (M::Lfo1, "osc1_wtpos", 0.8f)
        .filt (0, LP12, 6000.0f, 0.1f).amp (0.3f, 2.0f, 0.85f, 2.0f).reverb (0.8f, 5.0f, 0.3f).done());
    L.push_back (D ("Vinyl Bed", "Texture", "Dark crackle and hiss for lo-fi backgrounds.")
        .noise (0, 0.2f, -0.6f, 0.9f).noise (1, 0.08f, 0.2f).filt (0, LP12, 3000.0f, 0.1f).amp (0.2f, 1.0f, 1.0f, 1.0f)
        .eq (200.0f, 0.0f, 1000.0f, 3.0f, -6.0f, 8000.0f).done());
}

// =============================================================================================== DRONE
void drone (List& L)
{
    L.push_back (D ("Deep Drone", "Drone", "Two low detuned layers beating slowly.")
        .osc (0, "Saw Tilt", 0.4f, 0.5f, -2, 3, 0.1f).osc (1, "Hollow Square", 0.3f, 0.4f, -1).ox (1, "fine", 4.0f)
        .filt (0, LP24, 500.0f, 0.2f).lfoHz (0, SINE, 0.04f, 0).mod (M::Lfo1, "filter1_cutoff", 0.15f)
        .amp (3.0f, 2.0f, 1.0f, 6.0f).analog (0.6f, 0.4f).reverb (0.9f, 8.0f, 0.4f).done());
    L.push_back (D ("Organ Drone", "Drone", "Endless drawbar drone with slow registration morph.")
        .osc (0, "Organ", 0.0f, 0.55f, -1, 2, 0.05f).lfoHz (0, TRI, 0.02f, 0).mod (M::Lfo1, "osc1_wtpos", 1.0f)
        .amp (2.0f, 2.0f, 1.0f, 5.0f).chorus (0.2f, 0.4f, 0.3f).reverb (0.85f, 6.0f, 0.35f).done());
    L.push_back (D ("Harmonic Drone", "Drone", "Harmonic sweep table breathing across every partial.")
        .osc (0, "Harmonic Sweep", 0.5f, 0.5f, -1, 4, 0.08f).lfoHz (0, SINE, 0.025f, 0).mod (M::Lfo1, "osc1_wtpos", 0.5f)
        .amp (3.0f, 2.0f, 1.0f, 6.0f).reverb (0.9f, 9.0f, 0.4f).done());
    L.push_back (D ("Dark Ritual", "Drone", "Formant growl at sub tempo with ominous tremolo.")
        .osc (0, "Vocal Growl", 0.3f, 0.5f, -2, 3, 0.1f).lfoHz (0, SINE, 0.05f, 0).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .lfoHz (1, TRI, 3.0f, 0).mod (M::Lfo2, "osc1_level", 0.15f).filt (0, LP24, 900.0f, 0.3f).amp (3.0f, 2.0f, 1.0f, 6.0f)
        .reverb (0.95f, 10.0f, 0.45f).done());
    L.push_back (D ("Tanpura", "Drone", "Buzzing plucked drone: rich harmonics, slow repeat.")
        .osc (0, "Saw Tilt", 0.6f, 0.55f, -1, 2, 0.04f).filt (0, PEAK, 1500.0f, 0.5f).lfoSync (0, SAWDN, B1, 0).mod (M::Lfo1, "osc1_level", 0.25f)
        .amp (0.5f, 2.0f, 1.0f, 4.0f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Engine Hum", "Drone", "Mechanical hum: sync table with slow rotating phaser.")
        .osc (0, "Sync Sine", 0.2f, 0.5f, -2).lfoHz (0, SINE, 0.07f, 0).mod (M::Lfo1, "osc1_wtpos", 0.2f)
        .phaser (0.05f, 0.9f, 600.0f, 0.5f, 3, 0.5f).filt (0, LP24, 1200.0f, 0.2f).amp (2.0f, 2.0f, 1.0f, 4.0f).done());
    L.push_back (D ("Bowed Metal Drone", "Drone", "Metallic partials, slow attack, bowing noise.")
        .osc (0, "Metallic", 0.4f, 0.45f, -1, 3, 0.06f).noise (1, 0.06f, -0.2f).filt (0, BP12, 800.0f, 0.4f)
        .lfoHz (0, SRAND, 0.1f, 0, 0.8f).mod (M::Lfo1, "filter1_cutoff", 0.2f).amp (4.0f, 2.0f, 1.0f, 6.0f).reverb (0.9f, 8.0f, 0.4f).done());
}

// ======================================================================================== EXPERIMENTAL
void experimental (List& L)
{
    L.push_back (D ("Math Spiral", "Experimental", "Mathematical table: phase-modulated sine with frame-dependent depth.")
        .oscExpr (0, "Spiral", "sin(tau*x + (1+f*6)*sin(tau*x*(1+floor(f*4))))", 32, 0.0f, 0.6f).lfoHz (0, TRI, 0.2f, 0)
        .mod (M::Lfo1, "osc1_wtpos", 0.9f).amp (0.05f, 1.0f, 0.8f, 1.0f).reverb (0.6f, 3.0f, 0.25f).done());
    L.push_back (D ("Cross FM Mess", "Experimental", "OSC 1 frequency-modulates OSC 2 with random index jumps.")
        .osc (0, "Basic Shapes", 0.6f, 0.0f).osc (1, "Metallic", 0.3f, 0.6f).fm (1, 2, 0.4f).lfoHz (0, SH, 5.0f, 0)
        .mod (M::Lfo1, "osc2_fmamt", 0.4f).amp (0.01f, 0.5f, 0.7f, 0.5f).delay (D16T, 0.45f, 0.25f).done());
    L.push_back (D ("Quantize Melt", "Experimental", "Phase quantisation melting into a sine by envelope.")
        .osc (0, "Saw Tilt", 1.0f, 0.6f).warp (0, QUANT, 0.9f).mod (M::Env2, "osc1_shape", -0.9f).env (1, 0.0f, 1.5f, 0.0f, 1.0f)
        .amp (0.001f, 2.0f, 0.5f, 1.0f).reverb (0.6f, 3.0f, 0.25f).done());
    L.push_back (D ("Bias Bounce", "Experimental", "Vertical bias modulated at audio-ish LFO rate: thumping tremolo bass.")
        .osc (0, "Basic Shapes", 0.25f, 0.6f, -1).lfoHz (0, SQR, 12.0f, 0).mod (M::Lfo1, "osc1_bias", 0.4f).ox (0, "drive", 0.4f)
        .amp (0.01f, 0.5f, 0.9f, 0.3f).filt (0, LP24, 3000.0f, 0.2f).done());
    L.push_back (D ("Formant Robot", "Experimental", "Formant warp sweeping a sine: robotic speech.")
        .osc (0, "Basic Shapes", 0.0f, 0.6f).warp (0, FORMANTW, 0.3f).lfoSync (0, SH, D16, 0).mod (M::Lfo1, "osc1_shape", 0.5f)
        .amp (0.005f, 0.4f, 0.8f, 0.2f).crush (12.0f, 18000.0f, 0.5f).done());
    L.push_back (D ("Random Walker", "Experimental", "Smooth random LFOs on pitch, shape and pan.")
        .oscRand (0, 2, 2024u, 0.5f, 8, 0.3f, 0.6f, 0, 2).lfoHz (0, SRAND, 1.0f, 0, 0.6f).mod (M::Lfo1, "osc1_pitch", 0.02f)
        .lfoHz (1, SRAND, 0.7f, 0, 0.6f).mod (M::Lfo2, "osc1_shape", 0.5f).mod (M::Lfo2, "osc1_pan", 0.5f)
        .amp (0.05f, 1.0f, 0.8f, 1.0f).reverb (0.6f, 3.0f, 0.3f).done());
    L.push_back (D ("Self PM Scream", "Experimental", "Self phase-modulation at maximum with sync on top.")
        .osc (0, "Sync Saw", 0.4f, 0.55f).warp (0, PHASEFM, 0.9f).lfoHz (0, TRI, 0.3f, 0).mod (M::Lfo1, "osc1_wtpos", 0.5f)
        .ultra().filt (0, LP24, 5000.0f, 0.3f).amp (0.01f, 0.5f, 0.8f, 0.3f).done());
    L.push_back (D ("Ring Spectra", "Experimental", "Random spectral tables cross-modulating each other.")
        .oscRand (0, 0, 5150u, 0.6f, 16, 0.0f, 0.0f).oscRand (1, 0, 8150u, 0.7f, 16, 0.5f, 0.55f).fm (1, 2, 0.3f)
        .lfoHz (0, SINE, 0.15f, 0).mod (M::Lfo1, "osc1_wtpos", 0.6f).mod (M::Lfo1, "osc2_wtpos", -0.5f)
        .amp (0.1f, 1.0f, 0.8f, 1.5f).reverb (0.7f, 4.0f, 0.3f).done());
}

// ============================================================================================= DIGITAL
void digital (List& L)
{
    L.push_back (D ("8-Bit Arp", "Digital", "NES-style pulse arpeggio at 1/32 - hold a chord.")
        .osc (0, "Chip", 0.25f, 0.55f).amp (0.001f, 0.1f, 0.9f, 0.05f).arp (0, 12, 1, 0.9f).crush (8.0f, 20000.0f, 1.0f).done());
    L.push_back (D ("Data Stream", "Digital", "Sample-and-hold wave position on digital steps.")
        .osc (0, "Digital Steps", 0.5f, 0.6f).lfoSync (0, SH, D16, 0).mod (M::Lfo1, "osc1_wtpos", 0.6f)
        .amp (0.001f, 0.4f, 0.8f, 0.2f).delay (D16, 0.4f, 0.2f).done());
    L.push_back (D ("Bitcrush Saw", "Digital", "Aliased, lo-res saw with downsampling mod.")
        .osc (0, "Saw Tilt", 1.0f, 0.6f).crush (5.0f, 7000.0f).mod (M::ModWheel, "fx_crs_rate", 0.5f).amp (0.001f, 0.4f, 0.8f, 0.2f).done());
    L.push_back (D ("Pixel Bell", "Digital", "Quantised FM bell with crushed tail.")
        .osc (0, "FM 1-3", 0.5f, 0.6f).warp (0, QUANT, 0.6f).amp (0.001f, 1.2f, 0.0f, 0.8f).crush (9.0f, 16000.0f, 0.6f)
        .reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Glitch Bass", "Digital", "Stepped bass with 1/32 wave-position jumps.")
        .osc (0, "Digital Steps", 0.2f, 0.7f, -1).lfoSync (0, SH, D32, 0).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .filt (0, LP24, 2500.0f, 0.2f).amp (0.001f, 0.3f, 0.8f, 0.1f).mono (false, 0).bassMono().done());
    L.push_back (D ("Modem", "Digital", "Two square tones hopping in frequency: retro data noise.")
        .osc (0, "Chip", 0.0f, 0.45f).osc (1, "Chip", 0.0f, 0.4f, 1).lfoSync (0, SH, D32, 0).mod (M::Lfo1, "osc1_pitch", 0.15f)
        .lfoSync (1, SH, D16, 0).mod (M::Lfo2, "osc2_pitch", 0.12f).amp (0.001f, 0.2f, 1.0f, 0.05f).done());
    L.push_back (D ("Digital Choir", "Digital", "Vowel table through hard quantise: vocoder-like.")
        .osc (0, "Formant Vowels", 0.4f, 0.55f, 0, 4, 0.1f).warp (0, QUANT, 0.5f).lfoHz (0, TRI, 0.2f, 0)
        .mod (M::Lfo1, "osc1_wtpos", 0.5f).amp (0.3f, 1.0f, 0.9f, 1.0f).reverb (0.7f, 3.0f, 0.3f).done());
    L.push_back (D ("Gate Sequence", "Digital", "Trance-gate pattern from a stepped custom LFO.")
        .osc (0, "Saw Tilt", 0.9f, 0.55f, 0, 5, 0.2f).lfoSync (0, CUSTOM, B1, 0)
        .lfoShape (0, "0,1,0;0.06,0,0;0.125,1,0;0.19,0,0;0.25,1,0;0.3,0,0;0.375,0,0;0.5,1,0;0.56,0,0;0.625,1,0;0.69,0,0;0.75,0,0;0.875,1,0;0.94,0,0;")
        .uni (0, false).p ("osc1_level", 0.0f).mod (M::Lfo1, "osc1_level", 0.6f).filt (0, LP24, 4000.0f, 0.2f)
        .amp (0.01f, 1.0f, 1.0f, 0.3f).reverb (0.6f, 2.5f, 0.2f).done());
}

// ============================================================================================== ANALOG
void analogPresets (List& L)
{
    L.push_back (D ("Vintage Brass", "Analog", "Polysynth brass: saws, ladder envelope swell, drift.")
        .osc (0, "Saw Tilt", 0.85f, 0.55f, 0, 2, 0.08f).osc (1, "Saw Tilt", 0.85f, 0.45f).ox (1, "fine", -6.0f)
        .filt (0, LADLP, 900.0f, 0.2f, 0.45f, 0.1f).env (1, 0.12f, 0.6f, 0.4f, 0.4f).amp (0.05f, 0.5f, 0.85f, 0.4f)
        .analog (0.45f, 0.35f, 0.15f, 0.5f, 0.3f).chorus (0.4f, 0.3f, 0.2f).done());
    L.push_back (D ("Poly Strings", "Analog", "Paraphonic string machine with ensemble chorus.")
        .osc (0, "Saw Tilt", 0.7f, 0.5f, 0, 3, 0.1f).osc (1, "Saw Tilt", 0.7f, 0.4f, 1).filt (0, LP12, 3500.0f, 0.1f)
        .amp (0.3f, 1.0f, 0.9f, 1.2f).analog (0.4f, 0.3f).chorus (0.9f, 0.6f, 0.5f, 18.0f).reverb (0.6f, 3.0f, 0.25f).done());
    L.push_back (D ("Moog-ish Lead", "Analog", "Three oscillators into a driven ladder - monophonic.")
        .osc (0, "Saw Tilt", 0.95f, 0.5f).osc (1, "Basic Shapes", 0.75f, 0.45f).ox (1, "fine", 7.0f).osc (2, "Saw Tilt", 0.9f, 0.4f, -1)
        .filt (0, LADLP, 1800.0f, 0.35f, 0.3f, 0.35f).env (1, 0.01f, 0.4f, 0.4f, 0.3f).mono (true, 2, 0.05f)
        .analog (0.35f, 0.25f, 0.25f, 0.6f).done());
    L.push_back (D ("PWM Pad Analog", "Analog", "Classic pulse-width-modulated pad with slow drift.")
        .osc (0, "PWM", 0.4f, 0.55f, 0, 2, 0.06f).lfoHz (0, TRI, 0.5f, 0).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .filt (0, LP24, 2400.0f, 0.15f).amp (0.4f, 1.0f, 0.9f, 1.5f).analog (0.5f, 0.3f, 0.0f, 0.3f, 0.4f).reverb (0.6f, 3.0f, 0.25f).done());
    L.push_back (D ("Analog Sync Lead", "Analog", "Vintage sync sweep with pitch envelope.")
        .osc (0, "Sync Saw", 0.2f, 0.6f).mod (M::Env2, "osc1_wtpos", 0.5f).env (1, 0.0f, 0.8f, 0.2f, 0.4f).filt (0, LADLP, 4000.0f, 0.2f)
        .analog (0.3f, 0.3f, 0.2f, 0.4f).mono (true, 2, 0.04f).delay (D8, 0.3f, 0.15f).done());
    L.push_back (D ("Tape Keys", "Analog", "Soft square keys with tape saturation and wow.")
        .osc (0, "Hollow Square", 0.1f, 0.6f).filt (0, LP12, 2000.0f, 0.1f, 0.3f).env (1, 0.0f, 0.5f, 0.2f, 0.4f)
        .lfoHz (0, SINE, 0.6f, 0).mod (M::Lfo1, "osc1_fine", 0.04f).amp (0.002f, 1.5f, 0.3f, 0.5f).sat (8.0f, 0.2f, 0.7f).done());
    L.push_back (D ("Analog Perc", "Analog", "Self-oscillating ladder blip for percussive sequences.")
        .osc (0, "Basic Shapes", 0.6f, 0.2f).filt (0, LADLP, 800.0f, 0.95f, 0.6f, 0.2f, 1.0f).env (1, 0.0f, 0.1f, 0.0f, 0.1f)
        .amp (0.001f, 0.25f, 0.0f, 0.15f).analog (0.2f, 0.2f, 0.0f, 0.7f).done());
    L.push_back (D ("Warm Saw Stack", "Analog", "Saturated saw stack with gentle filter - chords and leads.")
        .osc (0, "Saw Tilt", 0.8f, 0.5f, 0, 4, 0.12f).filt (0, LADLP, 2800.0f, 0.15f).analog (0.4f, 0.3f, 0.3f, 0.4f, 0.3f)
        .amp (0.01f, 0.6f, 0.85f, 0.4f).sat (6.0f, 0.15f, 0.6f).done());
}

// ================================================================================================ DARK
void dark (List& L)
{
    L.push_back (D ("Abyss", "Dark", "Sub-heavy low-passed cluster with slow modulation.")
        .osc (0, "Hollow Square", 0.6f, 0.5f, -2, 3, 0.1f).filt (0, LP24, 300.0f, 0.3f).lfoHz (0, SINE, 0.05f, 0)
        .mod (M::Lfo1, "filter1_cutoff", 0.15f).amp (2.0f, 2.0f, 1.0f, 4.0f).reverb (0.9f, 8.0f, 0.4f, 0.8f).done());
    L.push_back (D ("Haunted Bell", "Dark", "Detuned inharmonic bell pitched down with dark reverb.")
        .osc (0, "Metallic", 0.6f, 0.5f, -1).fm (0, 1, 0.2f, 1.41f).ox (0, "fine", -15.0f).amp (0.001f, 4.0f, 0.0f, 3.0f)
        .filt (0, LP12, 2500.0f, 0.1f).reverb (0.9f, 8.0f, 0.45f, 0.8f).done());
    L.push_back (D ("Creeping Pad", "Dark", "Minor-feeling formant pad creeping through vowels.")
        .osc (0, "Vocal Growl", 0.1f, 0.5f, -1, 4, 0.12f).lfoHz (0, SINE, 0.04f, 0).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .filt (0, LP24, 900.0f, 0.2f).amp (1.5f, 2.0f, 0.9f, 3.0f).reverb (0.9f, 7.0f, 0.4f).done());
    L.push_back (D ("Shadow Bass", "Dark", "Dark sine/saw bass with a slow wobble.")
        .osc (0, "Saw Tilt", 0.3f, 0.6f, -1).osc (1, "Sub Shapes", 0.0f, 0.5f, -1).filt (0, LP24, 400.0f, 0.3f)
        .lfoSync (0, SINE, D2, 1).mod (M::Lfo1, "filter1_cutoff", 0.2f).mono (true, 2).bassMono().done());
    L.push_back (D ("Black Ice", "Dark", "Cold, thin glass partials an octave down.")
        .osc (0, "Glass", 0.4f, 0.5f, -1, 3, 0.08f).filt (0, HP12, 400.0f, 0.2f).amp (1.0f, 2.0f, 0.8f, 3.0f)
        .reverb (0.85f, 6.0f, 0.4f, 0.3f).done());
    L.push_back (D ("Nightmare Drone", "Dark", "Distorted low drone with random filter.")
        .osc (0, "Saw Tilt", 0.6f, 0.45f, -2, 4, 0.15f).filt (0, LADLP, 250.0f, 0.5f, 0.0f, 0.4f).lfoHz (0, SRAND, 0.2f, 0, 0.7f)
        .mod (M::Lfo1, "filter1_cutoff", 0.2f).dist (TUBE, 14.0f, 0.3f, 0.4f).amp (3.0f, 2.0f, 1.0f, 5.0f).reverb (0.9f, 9.0f, 0.4f).done());
    L.push_back (D ("Dark Pluck", "Dark", "Muted low pluck, short and heavy.")
        .osc (0, "Hollow Square", 0.7f, 0.65f, -1).filt (0, LP24, 300.0f, 0.4f, 0.4f).env (1, 0.0f, 0.15f, 0.0f, 0.1f)
        .amp (0.001f, 0.4f, 0.0f, 0.2f).reverb (0.6f, 2.5f, 0.2f, 0.8f).done());
    L.push_back (D ("Cellar Keys", "Dark", "Muffled, slightly detuned piano-like keys.")
        .osc (0, "Basic Shapes", 0.3f, 0.65f, -1, 2, 0.06f).filt (0, LP12, 800.0f, 0.1f, 0.3f).env (1, 0.0f, 0.4f, 0.1f, 0.3f)
        .amp (0.002f, 1.5f, 0.2f, 0.6f).reverb (0.7f, 3.0f, 0.3f, 0.9f).done());
}

// ============================================================================================ ETHEREAL
void ethereal (List& L)
{
    L.push_back (D ("Angel Pad", "Ethereal", "Airy formant pad with shimmer delay.")
        .osc (0, "Formant Vowels", 0.7f, 0.5f, 0, 8, 0.15f, 1.0f).osc (1, "Glass", 0.5f, 0.3f, 1, 2).filt (0, HP12, 300.0f, 0.1f)
        .amp (1.5f, 2.0f, 0.9f, 4.0f).delay (D4D, 0.6f, 0.25f).reverb (0.95f, 9.0f, 0.45f, 0.2f).done());
    L.push_back (D ("Crystal Arp", "Ethereal", "Glass arpeggio over two octaves with long tails.")
        .osc (0, "Glass", 0.9f, 0.5f).amp (0.001f, 1.2f, 0.0f, 1.5f).arp (5, 10, 2, 0.5f).delay (D8D, 0.5f, 0.3f)
        .reverb (0.9f, 6.0f, 0.4f, 0.2f).done());
    L.push_back (D ("Breath Pad", "Ethereal", "Breathy noise and sine layers - soft and intimate.")
        .osc (0, "Basic Shapes", 0.05f, 0.5f, 0, 3, 0.08f).noise (1, 0.12f, 0.3f).route (1, 1).filt (1, BP12, 2000.0f, 0.3f).parallel()
        .amp (1.0f, 2.0f, 0.9f, 3.0f).chorus (0.3f, 0.5f, 0.3f).reverb (0.9f, 7.0f, 0.4f).done());
    L.push_back (D ("Floating", "Ethereal", "Octave-stack partials gently gliding between notes.")
        .osc (0, "Octave Stack", 0.5f, 0.5f, 0, 4, 0.1f).p ("glide_mode", 1.0f).p ("glide_time", 0.4f)
        .amp (0.8f, 2.0f, 0.9f, 3.0f).reverb (0.9f, 8.0f, 0.45f).done());
    L.push_back (D ("Heavenly Strings", "Ethereal", "High strings with slow vibrato and bright reverb.")
        .osc (0, "Saw Tilt", 0.6f, 0.45f, 1, 6, 0.15f, 1.0f).filt (0, LP12, 6000.0f, 0.1f).lfoHz (0, SINE, 4.8f, 1, 0.0f, 1.5f)
        .mod (M::Lfo1, "osc1_fine", 0.05f).amp (0.7f, 1.0f, 0.9f, 2.5f).reverb (0.9f, 7.0f, 0.4f, 0.2f).done());
    L.push_back (D ("Dream Bells", "Ethereal", "Soft FM bells with chorus and huge space.")
        .osc (0, "Basic Shapes", 0.0f, 0.38f).fm (0, 1, 0.2f, 2.0f).amp (0.005f, 3.0f, 0.1f, 3.0f).chorus (0.3f, 0.6f, 0.4f)
        .reverb (0.95f, 10.0f, 0.5f, 0.3f).done());
    L.push_back (D ("Silk", "Ethereal", "Smooth triangle choir with gentle stereo motion.")
        .osc (0, "Skew Triangle", 0.0f, 0.5f, 0, 6, 0.1f, 1.0f).lfoHz (0, SINE, 0.1f, 0).mod (M::Lfo1, "osc1_wtpos", 0.3f)
        .analog (0.2f, 0.1f, 0.0f, 0.0f, 0.6f).amp (1.0f, 2.0f, 0.9f, 3.0f).reverb (0.85f, 6.0f, 0.4f).done());
}

// =========================================================================================== AGGRESSIVE
void aggressive (List& L)
{
    L.push_back (D ("Distorted Saw Lead", "Aggressive", "Hard-clipped supersaw lead for festival drops.")
        .osc (0, "Saw Tilt", 1.0f, 0.55f, 0, 5, 0.2f).dist (HARD, 18.0f, 0.6f, 0.7f).comp (-18.0f, 6.0f).filt (0, LP24, 8000.0f, 0.1f)
        .amp (0.001f, 0.4f, 0.9f, 0.2f).done());
    L.push_back (D ("Hardstyle Kick Lead", "Aggressive", "Pitch-dropped distorted lead with tube scream.")
        .osc (0, "Saw Tilt", 0.8f, 0.6f, 0, 3, 0.15f).mod (M::Env3, "osc1_pitch", 0.15f).env (2, 0.0f, 0.08f, 0.0f, 0.05f)
        .dist (TUBE, 24.0f, 0.5f, 0.8f).filt (0, LP24, 5000.0f, 0.3f).amp (0.001f, 0.4f, 0.8f, 0.2f).mono (true, 2, 0.05f).done());
    L.push_back (D ("Industrial Grind", "Aggressive", "Foldback and bitcrush on a sync table.")
        .osc (0, "Sync Saw", 0.5f, 0.55f).lfoSync (0, SAWDN, D8, 1).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .dist (FOLDBACK, 14.0f, 0.5f, 0.6f).crush (8.0f, 14000.0f, 0.5f).amp (0.001f, 0.4f, 0.8f, 0.2f).done());
    L.push_back (D ("Scream Bass", "Aggressive", "Screeching resonant bass with heavy drive.")
        .osc (0, "Saw Tilt", 1.0f, 0.6f, -1, 2, 0.15f).filt (0, LADLP, 600.0f, 0.75f, 0.5f, 0.6f).env (1, 0.0f, 0.3f, 0.3f, 0.2f)
        .dist (SOFT, 16.0f, 0.6f, 0.6f).comp (-20.0f, 6.0f).mono (true, 2).bassMono().done());
    L.push_back (D ("Rectified Lead", "Aggressive", "Full-wave rectified square: octave-up snarl.")
        .osc (0, "Basic Shapes", 0.75f, 0.6f, -1).dist (RECTIFY, 12.0f, 0.5f, 0.6f).filt (0, LP24, 4000.0f, 0.2f)
        .amp (0.001f, 0.4f, 0.85f, 0.2f).mono (true, 2).done());
    L.push_back (D ("Brutal Stab", "Aggressive", "Detuned stab through tube distortion and compression.")
        .osc (0, "Hollow Square", 0.9f, 0.55f, 0, 6, 0.25f).filt (0, LP24, 1500.0f, 0.3f, 0.5f).env (1, 0.0f, 0.2f, 0.0f, 0.1f)
        .amp (0.001f, 0.3f, 0.0f, 0.2f).dist (TUBE, 18.0f, 0.5f, 0.7f).comp (-16.0f, 8.0f).reverb (0.5f, 1.5f, 0.15f).done());
    L.push_back (D ("Saw Chainsaw", "Aggressive", "Fast 1/32 amplitude gating on a distorted saw stack.")
        .osc (0, "Saw Tilt", 1.0f, 0.55f, -1, 4, 0.2f).lfoSync (0, SQR, D32, 0).uni (0, false).p ("osc1_level", 0.2f)
        .mod (M::Lfo1, "osc1_level", 0.5f).dist (HARD, 16.0f, 0.5f, 0.6f).amp (0.001f, 0.4f, 0.9f, 0.2f).mono (false, 0).done());
}

// ================================================================================================== FX
void fxPresets (List& L)
{
    L.push_back (D ("White Noise Riser", "FX", "Eight-second filtered noise riser. Hold one note.")
        .noise (0, 0.5f, 0.2f).filt (0, BP12, 200.0f, 0.4f).env (2, 8.0f, 1.0f, 1.0f, 1.0f, -0.5f).mod (M::Env3, "filter1_cutoff", 0.7f)
        .amp (0.5f, 1.0f, 1.0f, 1.5f).reverb (0.8f, 4.0f, 0.3f).done());
    L.push_back (D ("Pitch Up Sweep", "FX", "Saw stack sweeping up two octaves.")
        .osc (0, "Saw Tilt", 1.0f, 0.5f, 0, 5, 0.25f).env (2, 4.0f, 1.0f, 1.0f, 0.5f, -0.3f).mod (M::Env3, "osc1_pitch", 0.25f)
        .filt (0, LP24, 400.0f, 0.3f).mod (M::Env3, "filter1_cutoff", 0.5f).amp (0.1f, 1.0f, 1.0f, 1.0f).reverb (0.8f, 4.0f, 0.3f).done());
    L.push_back (D ("Downlifter", "FX", "Falling pitch and closing filter with long reverb.")
        .osc (0, "Saw Tilt", 1.0f, 0.5f, 1, 4, 0.2f).noise (1, 0.2f, 0.4f).env (2, 0.0f, 3.0f, 0.0f, 1.0f, 0.0f, 0.3f)
        .p ("osc1_pitch", -24.0f).mod (M::Env3, "osc1_pitch", 0.25f).filt (0, LP24, 600.0f, 0.2f).mod (M::Env3, "filter1_cutoff", 0.5f)
        .amp (0.001f, 3.0f, 0.0f, 1.0f).reverb (0.9f, 6.0f, 0.45f).done());
    L.push_back (D ("Impact Boom", "FX", "Sub drop plus noise burst through a big reverb.")
        .osc (0, "Sub Shapes", 0.0f, 0.8f, -1).mod (M::Env3, "osc1_pitch", 0.15f).env (2, 0.0f, 0.4f, 0.0f, 0.3f)
        .noise (1, 0.4f, -0.3f).env (3, 0.0f, 0.15f, 0.0f, 0.1f).mod (M::Env4, "osc2_level", 1.0f).p ("osc2_level", 0.0f)
        .amp (0.001f, 2.5f, 0.0f, 2.0f).reverb (0.9f, 6.0f, 0.4f).dist (SOFT, 6.0f, 0.4f, 0.5f).done());
    L.push_back (D ("Laser Zap", "FX", "Fast exponential pitch drop: sci-fi zap.")
        .osc (0, "Basic Shapes", 0.75f, 0.55f, 1).p ("osc1_pitch", -24.0f).mod (M::Env3, "osc1_pitch", 0.4f).env (2, 0.0f, 0.15f, 0.0f, 0.1f, 0.0f, 0.9f)
        .amp (0.001f, 0.3f, 0.0f, 0.1f).delay (D16, 0.4f, 0.2f).done());
    L.push_back (D ("Alarm", "FX", "Two-tone siren from a square LFO on pitch.")
        .osc (0, "Basic Shapes", 0.6f, 0.5f).lfoHz (0, SQR, 2.0f, 0).mod (M::Lfo1, "osc1_pitch", 0.04f).amp (0.01f, 0.5f, 1.0f, 0.2f)
        .reverb (0.6f, 2.0f, 0.2f).done());
    L.push_back (D ("Sci-Fi Computer", "FX", "Random pitched blips at 1/32.")
        .osc (0, "Chip", 0.0f, 0.5f, 1).lfoSync (0, SH, D32, 0).mod (M::Lfo1, "osc1_pitch", 0.3f).amp (0.001f, 0.1f, 1.0f, 0.05f)
        .delay (D16T, 0.4f, 0.2f).done());
    L.push_back (D ("Wind Sweep", "FX", "Noise with a slow sine-swept band-pass in stereo.")
        .noise (0, 0.5f, 0.0f).filt (0, BP24, 600.0f, 0.5f, 0.0f, 0.0f, 0.0f).p ("filter1_spread", 0.6f).lfoHz (0, SINE, 0.1f, 0)
        .mod (M::Lfo1, "filter1_cutoff", 0.45f).amp (1.0f, 1.0f, 1.0f, 2.0f).reverb (0.8f, 4.0f, 0.3f).done());
    L.push_back (D ("Reverse Swell", "FX", "Reverse-like envelope with long curved attack and abrupt end.")
        .osc (0, "Glass", 0.5f, 0.5f, 0, 4, 0.1f).env (0, 2.5f, 0.1f, 1.0f, 0.02f, -0.8f, 0.6f, 0.6f).reverb (0.9f, 5.0f, 0.4f).done());
}

// ============================================================ extra designs to broaden each category
void extras (List& L)
{
    // more bass
    L.push_back (D ("Mirror Bass", "Bass", "Mirror warp on a pulse: octave-shifted growl that opens with velocity.")
        .osc (0, "PWM", 0.5f, 0.7f, -1).warp (0, MIRROR, 0.4f).mod (M::Velocity, "osc1_shape", 0.4f).filt (0, LP24, 1600.0f, 0.2f, 0.3f)
        .amp (0.001f, 0.4f, 0.8f, 0.1f).mono (false, 0).bassMono().done());
    L.push_back (D ("Squeeze Bass", "Bass", "Squeeze warp swept by a 1/8 LFO: rhythmic tone shaping.")
        .osc (0, "Saw Tilt", 0.8f, 0.7f, -1).warp (0, SQUEEZE, 0.0f).lfoSync (0, TRI, D8, 1).mod (M::Lfo1, "osc1_shape", 0.6f)
        .filt (0, LP24, 2000.0f, 0.2f).amp (0.001f, 0.4f, 0.8f, 0.1f).mono (true, 2).bassMono().done());
    L.push_back (D ("Drive Mode Bass", "Bass", "Oscillator fold insert on a triangle for a buzzy bass.")
        .osc (0, "Basic Shapes", 0.25f, 0.7f, -1).ox (0, "drive", 0.6f).ox (0, "drivemode", 2.0f).mod (M::Env2, "osc1_drive", -0.4f)
        .env (1, 0.0f, 0.3f, 0.0f, 0.2f).filt (0, LP24, 3000.0f, 0.1f).amp (0.001f, 0.4f, 0.8f, 0.1f).bassMono().done());
    // more lead
    L.push_back (D ("Chiptune Glide", "Lead", "Pulse lead with constant-rate glide and vibrato.")
        .osc (0, "Chip", 0.1f, 0.6f).mono (true, 1, 0.06f).p ("glide_constrate", 1.0f).lfoHz (0, TRI, 6.0f, 1, 0.0f, 0.3f)
        .mod (M::Lfo1, "osc1_fine", 0.1f).amp (0.001f, 0.2f, 0.9f, 0.1f).delay (D8, 0.3f, 0.15f).done());
    L.push_back (D ("Comb Lead", "Lead", "Bright comb-filtered lead with envelope-swept resonance.")
        .osc (0, "Saw Tilt", 1.0f, 0.55f, 0, 2, 0.1f).filt (0, COMBP, 440.0f, 0.6f, 0.2f, 0.0f, 1.0f).env (1, 0.0f, 0.5f, 0.3f, 0.3f)
        .amp (0.005f, 0.4f, 0.85f, 0.25f).delay (D8D, 0.35f, 0.2f).done());
    L.push_back (D ("Pulse Width Lead", "Lead", "Pulse-width warp lead with aftertouch-controlled width.")
        .osc (0, "Basic Shapes", 0.5f, 0.6f).warp (0, PWIDTH, 0.2f).mod (M::Aftertouch, "osc1_shape", 0.5f).mod (M::ModWheel, "osc1_shape", 0.4f)
        .filt (0, LP24, 5000.0f, 0.15f).mono (true, 2, 0.05f).delay (D8, 0.3f, 0.15f).done());
    // more pad
    L.push_back (D ("Noise Swell Pad", "Pad", "Pad with a noise breath that fades in on a separate envelope.")
        .osc (0, "Hollow Square", 0.3f, 0.5f, 0, 4, 0.1f).noise (1, 0.0f, 0.2f).env (2, 2.0f, 2.0f, 0.6f, 2.0f).mod (M::Env3, "osc2_level", 0.2f)
        .amp (0.8f, 2.0f, 0.9f, 2.0f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Phaser Pad", "Pad", "Slow 12-stage phaser over a saw pad.")
        .osc (0, "Saw Tilt", 0.7f, 0.5f, 0, 4, 0.15f).filt (0, LP12, 3500.0f, 0.1f).phaser (0.08f, 0.9f, 700.0f, 0.6f, 4, 0.6f)
        .amp (0.6f, 1.5f, 0.9f, 2.0f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Lane FX Pad", "Pad", "OSC 2 runs through its own flanger lane while OSC 1 stays clean.")
        .osc (0, "Organ", 0.3f, 0.5f, 0, 2, 0.08f).osc (1, "Glass", 0.6f, 0.4f, 1, 3).ox (1, "fxon", 1.0f)
        .p ("osc2fx_fla_on", 1.0f).p ("osc2fx_fla_mix", 0.6f).p ("osc2fx_rev_on", 1.0f).p ("osc2fx_rev_mix", 0.5f)
        .amp (0.6f, 1.5f, 0.9f, 2.0f).reverb (0.6f, 3.0f, 0.2f).done());
    // more pluck / keys
    L.push_back (D ("Bias Pluck", "Pluck", "Envelope on vertical bias through drive: pluck that changes timbre as it decays.")
        .osc (0, "Basic Shapes", 0.0f, 0.65f).ox (0, "drive", 0.5f).mod (M::Env2, "osc1_bias", 0.5f).env (1, 0.0f, 0.3f, 0.0f, 0.2f)
        .amp (0.001f, 0.5f, 0.0f, 0.3f).reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Sitar Pluck", "Pluck", "Buzzing peak filter and bend-up attack.")
        .osc (0, "Saw Tilt", 0.9f, 0.6f).filt (0, PEAK, 2200.0f, 0.6f).mod (M::Env3, "osc1_pitch", 0.005f).env (2, 0.0f, 0.08f, 0.0f, 0.05f)
        .amp (0.001f, 1.2f, 0.0f, 0.5f).reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Vibraphone", "Keys", "Sine-based mallet with tremolo.")
        .osc (0, "Basic Shapes", 0.0f, 0.65f).fm (0, 1, 0.12f, 4.0f).mod (M::Env2, "osc1_fmamt", -0.1f).env (1, 0.0f, 0.3f, 0.0f, 0.2f)
        .lfoHz (0, SINE, 5.0f, 0).mod (M::Lfo1, "osc1_level", 0.15f).amp (0.001f, 2.5f, 0.0f, 1.5f).reverb (0.6f, 2.5f, 0.25f).done());
    // more atmosphere / cinematic / texture
    L.push_back (D ("Glacier", "Atmospheric", "Very slow glass and noise morph with frozen reverb.")
        .osc (0, "Glass", 0.0f, 0.45f, -1, 6, 0.1f).noise (1, 0.1f, -0.4f).lfoHz (0, SINE, 0.02f, 0).mod (M::Lfo1, "osc1_wtpos", 0.8f)
        .amp (4.0f, 3.0f, 0.9f, 6.0f).reverb (0.98f, 15.0f, 0.55f, 0.6f).done());
    L.push_back (D ("War Drums Pad", "Cinematic", "Low pulsing pad with 1/8 amplitude rhythm and big hall.")
        .osc (0, "Hollow Square", 0.4f, 0.55f, -1, 3, 0.12f).lfoSync (0, SAWDN, D8, 0).mod (M::Lfo1, "osc1_level", 0.3f)
        .filt (0, LP24, 700.0f, 0.2f).amp (0.3f, 1.0f, 1.0f, 2.0f).reverb (0.9f, 6.0f, 0.35f).done());
    L.push_back (D ("Morse Texture", "Texture", "Stepped amplitude bursts of a band-passed tone.")
        .osc (0, "Basic Shapes", 0.0f, 0.5f, 1).filt (0, BP12, 1500.0f, 0.4f).lfoSync (0, SH, D16, 0).uni (0, false)
        .p ("osc1_level", 0.0f).mod (M::Lfo1, "osc1_level", 0.7f).amp (0.01f, 1.0f, 1.0f, 0.5f).delay (D8D, 0.5f, 0.25f).done());
    // more experimental / digital / analog / dark / ethereal / aggressive / fx
    L.push_back (D ("Fractal Steps", "Experimental", "Mathematical stepped table from nested floor functions.")
        .oscExpr (0, "Fractal", "sign(sin(tau*x*(1+floor(f*7))))*abs(sin(tau*x))^(0.2+f)", 16, 0.0f, 0.55f).lfoSync (0, SAWUP, B1, 0)
        .mod (M::Lfo1, "osc1_wtpos", 0.9f).filt (0, LP24, 5000.0f, 0.2f).amp (0.01f, 1.0f, 0.8f, 0.5f).done());
    L.push_back (D ("Bit Organ", "Digital", "Organ registrations through a 6-bit converter.")
        .osc (0, "Organ", 0.6f, 0.55f).crush (6.0f, 22050.0f, 0.8f).amp (0.003f, 0.2f, 1.0f, 0.05f).chorus (5.0f, 0.15f, 0.3f).done());
    L.push_back (D ("Juno Chord", "Analog", "Square-saw chord stab with lush ensemble.")
        .osc (0, "Basic Shapes", 0.6f, 0.55f, 0, 2, 0.06f).filt (0, LP24, 1800.0f, 0.2f, 0.4f).env (1, 0.0f, 0.3f, 0.2f, 0.3f)
        .amp (0.002f, 0.6f, 0.5f, 0.4f).chorus (0.5f, 0.6f, 0.5f).analog (0.3f, 0.2f).reverb (0.5f, 2.0f, 0.2f).done());
    L.push_back (D ("Dark Arp", "Dark", "Low down-up arpeggio through a dark ladder.")
        .osc (0, "Saw Tilt", 0.5f, 0.6f, -1).filt (0, LADLP, 500.0f, 0.35f, 0.3f).env (1, 0.0f, 0.2f, 0.0f, 0.1f)
        .amp (0.001f, 0.3f, 0.0f, 0.2f).arp (3, 10, 2, 0.5f).reverb (0.7f, 3.0f, 0.3f, 0.8f).done());
    L.push_back (D ("Glass Choir", "Ethereal", "Vowel and glass layers alternating in stereo.")
        .osc (0, "Formant Vowels", 0.5f, 0.45f, 0, 4, 0.1f).ox (0, "pan", -0.4f).osc (1, "Glass", 0.7f, 0.4f, 1, 2).ox (1, "pan", 0.4f)
        .lfoHz (0, SINE, 0.1f, 0).mod (M::Lfo1, "osc1_pan", 0.4f).mod (M::Lfo1, "osc2_pan", -0.4f).amp (1.0f, 2.0f, 0.9f, 3.0f)
        .reverb (0.9f, 7.0f, 0.4f).done());
    L.push_back (D ("Overdrive Organ", "Aggressive", "Rock organ through tube distortion and a fast rotor.")
        .osc (0, "Organ", 0.8f, 0.55f).dist (TUBE, 16.0f, 0.6f, 0.6f).chorus (6.0f, 0.2f, 0.4f, 5.0f).amp (0.003f, 0.2f, 1.0f, 0.05f).done());
    L.push_back (D ("Bubble Pops", "FX", "Random filter pings: water bubbles.")
        .osc (0, "Basic Shapes", 0.0f, 0.4f, 1).filt (0, BP24, 1200.0f, 0.8f).lfoHz (0, SH, 7.0f, 0).mod (M::Lfo1, "filter1_cutoff", 0.35f)
        .mod (M::Lfo1, "osc1_pitch", 0.1f).amp (0.01f, 1.0f, 1.0f, 0.5f).reverb (0.5f, 2.0f, 0.3f).done());
}

// ======================================================== second wave of designs (one idea each)
void moreDesigns (List& L)
{
    L.push_back (D ("Talking Bass", "Bass", "Formant filter on a saw bass: the mod wheel moves the vowel.")
        .osc (0, "Saw Tilt", 0.9f, 0.7f, -1).filt (0, FORMANT, 200.0f, 0.55f, 0.0f, 0.2f, 0.0f).mod (M::ModWheel, "filter1_cutoff", 0.6f)
        .mod (M::Env2, "filter1_cutoff", 0.15f).env (1, 0.0f, 0.3f, 0.0f, 0.2f).amp (0.001f, 0.4f, 0.85f, 0.1f).mono (true, 2).bassMono().done());
    L.push_back (D ("Hoover", "Bass", "Rave hoover: wide PWM unison with a pitch dip on every note and chorus.")
        .osc (0, "PWM", 0.5f, 0.55f, -1, 7, 0.35f, 1.0f).osc (1, "Saw Tilt", 1.0f, 0.4f, 0, 3, 0.25f).mod (M::Env3, "osc1_pitch", -0.02f)
        .mod (M::Env3, "osc2_pitch", -0.02f).env (2, 0.0f, 0.25f, 0.0f, 0.2f).lfoHz (0, TRI, 0.8f, 0).mod (M::Lfo1, "osc1_wtpos", 0.3f)
        .mono (true, 1, 0.12f).chorus (0.6f, 0.6f, 0.4f).amp (0.005f, 0.5f, 0.85f, 0.2f).done());
    L.push_back (D ("Picked Bass", "Bass", "Pick noise transient over a key-tracked low-pass saw.")
        .osc (0, "Saw Tilt", 0.6f, 0.7f, -1).noise (1, 0.0f, 0.5f).env (2, 0.0f, 0.012f, 0.0f, 0.01f).mod (M::Env3, "osc2_level", 0.5f)
        .filt (0, LP24, 500.0f, 0.15f, 0.35f, 0.0f, 0.8f).env (1, 0.0f, 0.18f, 0.2f, 0.1f).amp (0.001f, 0.8f, 0.4f, 0.1f).bassMono().done());
    L.push_back (D ("Sub Octave Stack", "Sub Bass", "Sine with its octave, gently driven: audible on small speakers.")
        .osc (0, "Sub Shapes", 0.33f, 0.8f, -1).ox (0, "drive", 0.25f).amp (0.003f, 0.5f, 1.0f, 0.12f).mono (false, 0).imager (0.0f).done());
    L.push_back (D ("Liquid Reese", "Reese", "Soft reese with phaser drift and a little space - liquid drum and bass.")
        .osc (0, "Saw Tilt", 0.7f, 0.55f, -1, 3, 0.15f, 0.7f).filt (0, LP24, 700.0f, 0.15f).phaser (0.07f, 0.7f, 600.0f, 0.4f, 2, 0.4f)
        .reverb (0.4f, 1.5f, 0.12f).mono (true, 2).bassMono().done());
    L.push_back (D ("Bit Growl", "Growl", "Growl table through a sample-and-hold bitcrusher rate.")
        .osc (0, "Growl", 0.5f, 0.7f, -1).lfoSync (0, SH, D16, 1).mod (M::Lfo1, "osc1_wtpos", 0.5f).crush (8.0f, 9000.0f, 0.7f)
        .mod (M::Lfo1, "fx_crs_rate", 0.3f).mono (true, 2).bassMono().done());
    L.push_back (D ("Theremin", "Lead", "Pure sine that always glides, with a wandering vibrato.")
        .osc (0, "Basic Shapes", 0.0f, 0.7f).mono (true, 1, 0.25f).lfoHz (0, SINE, 6.0f, 0).mod (M::Lfo1, "osc1_fine", 0.07f)
        .lfoHz (1, SRAND, 0.5f, 0, 0.7f).mod (M::Lfo2, "osc1_fine", 0.04f).amp (0.08f, 0.5f, 0.9f, 0.4f).reverb (0.6f, 2.5f, 0.25f).done());
    L.push_back (D ("Hyper Lead", "Lead", "Sixteen-voice saw with an octave layer and soft clipping.")
        .osc (0, "Saw Tilt", 1.0f, 0.5f, 0, 16, 0.32f, 1.0f).osc (1, "Saw Tilt", 1.0f, 0.3f, 1, 4, 0.2f).dist (SOFT, 6.0f, 0.6f, 0.4f)
        .filt (0, LP12, 9000.0f, 0.1f).amp (0.003f, 0.4f, 0.85f, 0.3f).delay (D8D, 0.3f, 0.15f).done());
    L.push_back (D ("Detuned Square Lead", "Lead", "Two squares slightly detuned with slow pulse-width motion.")
        .osc (0, "PWM", 0.0f, 0.55f).osc (1, "PWM", 0.0f, 0.5f).ox (1, "fine", 11.0f).lfoHz (0, TRI, 0.3f, 0).mod (M::Lfo1, "osc1_wtpos", 0.4f)
        .mod (M::Lfo1, "osc2_wtpos", -0.4f).filt (0, LP24, 4500.0f, 0.15f).mono (true, 2, 0.05f).done());
    L.push_back (D ("Spectral Morph Pad", "Pad", "Random spectral wavetable morphed in ULTRA quality.")
        .oscRand (0, 0, 123457u, 0.8f, 64, 0.0f, 0.5f, 0, 6).p ("osc1_detune", 0.2f).lfoHz (0, SINE, 0.05f, 0).mod (M::Lfo1, "osc1_wtpos", 0.9f)
        .ultra().amp (1.0f, 2.0f, 0.9f, 3.0f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Comb Pad", "Pad", "Saw and noise ringing through a key-tracked comb.")
        .osc (0, "Saw Tilt", 0.6f, 0.4f, 0, 3, 0.12f).noise (1, 0.15f, 0.0f).filt (0, COMBP, 220.0f, 0.75f, 0.0f, 0.0f, 1.0f)
        .filt (1, LP12, 5000.0f, 0.1f).amp (0.8f, 2.0f, 0.9f, 2.5f).reverb (0.7f, 4.0f, 0.3f).done());
    L.push_back (D ("Formant Strings", "Pad", "Strings shaped by a fixed vowel formant for a woody ensemble.")
        .osc (0, "Saw Tilt", 0.7f, 0.5f, 0, 6, 0.15f, 1.0f).filt (0, FORMANT, 1500.0f, 0.4f).p ("filter1_mix", 0.6f)
        .amp (0.5f, 1.0f, 0.9f, 1.5f).chorus (0.5f, 0.5f, 0.3f).reverb (0.7f, 3.5f, 0.3f).done());
    L.push_back (D ("Steel Drum", "Pluck", "FM steel pan: ratio 2 bell with a fast drop in brightness.")
        .osc (0, "Basic Shapes", 0.0f, 0.6f).fm (0, 1, 0.35f, 2.0f).mod (M::Env2, "osc1_fmamt", -0.3f).env (1, 0.0f, 0.25f, 0.0f, 0.2f)
        .amp (0.001f, 0.9f, 0.0f, 0.5f).reverb (0.5f, 1.8f, 0.2f).done());
    L.push_back (D ("Kalimba", "Pluck", "Thumb piano: soft fundamental with a short inharmonic tine.")
        .osc (0, "Basic Shapes", 0.0f, 0.6f, 1).fm (0, 1, 0.25f, 6.3f).mod (M::Env2, "osc1_fmamt", -0.25f).env (1, 0.0f, 0.05f, 0.0f, 0.05f)
        .amp (0.001f, 1.0f, 0.0f, 0.6f).reverb (0.5f, 1.5f, 0.2f).done());
    L.push_back (D ("Pipe Organ", "Keys", "Full registration in a large hall with slow attack.")
        .osc (0, "Organ", 0.82f, 0.5f, 0, 2, 0.04f).osc (1, "Organ", 0.0f, 0.35f, -1).amp (0.06f, 0.5f, 1.0f, 0.6f)
        .reverb (0.95f, 6.0f, 0.4f, 0.4f, 30.0f).done());
    L.push_back (D ("Rhodes Tremolo", "Keys", "FM electric piano with stereo auto-pan tremolo.")
        .osc (0, "Basic Shapes", 0.0f, 0.65f).fm (0, 1, 0.22f, 1.0f).mod (M::Velocity, "osc1_fmamt", 0.2f).env (1, 0.0f, 0.6f, 0.0f, 0.3f)
        .mod (M::Env2, "osc1_fmamt", 0.15f).lfoHz (0, SINE, 4.0f, 0).mod (M::Lfo1, "osc1_pan", 0.6f).amp (0.001f, 2.5f, 0.2f, 0.5f).done());
    L.push_back (D ("Church Bell", "Bell", "Low, long, beating metallic bell.")
        .osc (0, "Metallic", 0.7f, 0.5f, -1).osc (1, "Glass", 0.4f, 0.3f, 0).ox (1, "fine", -6.0f).fm (0, 1, 0.12f, 2.4f)
        .amp (0.001f, 6.0f, 0.0f, 5.0f).reverb (0.9f, 7.0f, 0.4f).done());
    L.push_back (D ("Deep Space Hum", "Atmospheric", "Sub hum and dark noise through a slowly sweeping comb.")
        .osc (0, "Sub Shapes", 0.0f, 0.4f, -2).noise (1, 0.3f, -0.6f).route (1, 0).route (0, 3).filt (0, COMBN, 60.0f, 0.7f, 0.0f, 0.0f, 0.0f)
        .lfoHz (0, SINE, 0.03f, 0).mod (M::Lfo1, "filter1_cutoff", 0.2f).amp (3.0f, 2.0f, 1.0f, 5.0f).reverb (0.9f, 8.0f, 0.4f).done());
    L.push_back (D ("Dark Swell Strings", "Cinematic", "Low strings swelling over four seconds.")
        .osc (0, "Saw Tilt", 0.5f, 0.5f, -1, 8, 0.16f, 1.0f).filt (0, LP24, 500.0f, 0.15f, 0.4f).env (1, 4.0f, 2.0f, 0.7f, 3.0f)
        .amp (3.0f, 2.0f, 0.9f, 3.5f).reverb (0.9f, 6.0f, 0.35f).done());
    L.push_back (D ("Granular Glass", "Texture", "Glass table jumping between frames at random, through delay.")
        .osc (0, "Glass", 0.5f, 0.45f, 1, 2, 0.06f).lfoHz (0, SH, 18.0f, 0).mod (M::Lfo1, "osc1_wtpos", 0.9f).mod (M::Lfo1, "osc1_pan", 0.5f)
        .amp (0.2f, 2.0f, 0.9f, 2.0f).delay (D16T, 0.55f, 0.3f).reverb (0.8f, 5.0f, 0.35f).done());
    L.push_back (D ("Fifths Drone", "Drone", "Root and fifth with beating detune and slow filter breath.")
        .osc (0, "Saw Tilt", 0.4f, 0.45f, -1, 2, 0.05f).osc (1, "Saw Tilt", 0.4f, 0.4f, -1).ox (1, "semi", 7.0f).ox (1, "fine", 3.0f)
        .filt (0, LP24, 700.0f, 0.2f).lfoHz (0, SINE, 0.06f, 0).mod (M::Lfo1, "filter1_cutoff", 0.15f).amp (2.0f, 2.0f, 1.0f, 5.0f)
        .reverb (0.85f, 6.0f, 0.35f).done());
    L.push_back (D ("Audio Rate Wobble", "Experimental", "LFO at 40 Hz on wave position: amplitude-modulation sidebands.")
        .osc (0, "Basic Shapes", 0.5f, 0.6f).lfoHz (0, SINE, 40.0f, 1).mod (M::Lfo1, "osc1_wtpos", 0.4f).mod (M::ModWheel, "lfo1_rate", 0.4f)
        .amp (0.01f, 0.5f, 0.85f, 0.3f).filt (0, LP24, 6000.0f, 0.1f).done());
    L.push_back (D ("Arcade Lead", "Digital", "Chip pulse with octave-flipping 1/32 arpeggio.")
        .osc (0, "Chip", 0.3f, 0.55f).amp (0.001f, 0.1f, 0.9f, 0.05f).arp (2, 12, 2, 0.8f).crush (7.0f, 22050.0f, 1.0f).done());
    L.push_back (D ("Poly Brass Swell", "Analog", "Brass that swells open, drift and ensemble.")
        .osc (0, "Saw Tilt", 0.8f, 0.5f, 0, 3, 0.1f).filt (0, LADLP, 500.0f, 0.15f, 0.6f, 0.1f).env (1, 0.6f, 0.8f, 0.5f, 0.5f)
        .amp (0.3f, 0.6f, 0.9f, 0.6f).analog (0.45f, 0.3f, 0.1f, 0.4f, 0.3f).chorus (0.5f, 0.4f, 0.3f).done());
    L.push_back (D ("Grave Pad", "Dark", "Hollow, low, closed pad for horror underscore.")
        .osc (0, "Hollow Square", 0.8f, 0.5f, -1, 4, 0.12f).filt (0, LP24, 350.0f, 0.3f).lfoHz (0, SRAND, 0.1f, 0, 0.8f)
        .mod (M::Lfo1, "filter1_cutoff", 0.1f).amp (2.0f, 2.0f, 0.9f, 4.0f).reverb (0.9f, 8.0f, 0.4f, 0.9f).done());
    L.push_back (D ("Shimmer Strings", "Ethereal", "High strings with an octave layer and a bright, long hall.")
        .osc (0, "Saw Tilt", 0.6f, 0.4f, 1, 6, 0.14f, 1.0f).osc (1, "Basic Shapes", 0.0f, 0.25f, 2).filt (0, HP12, 300.0f, 0.1f)
        .amp (0.8f, 1.0f, 0.9f, 3.0f).delay (D4D, 0.5f, 0.2f).reverb (0.95f, 9.0f, 0.45f, 0.15f).done());
    L.push_back (D ("Sync Bass Drive", "Aggressive", "Hard-sync bass with LFO-driven ratio and tube drive.")
        .osc (0, "Basic Shapes", 0.5f, 0.6f, -1).warp (0, SYNC, 0.3f).lfoSync (0, TRI, D8, 1).mod (M::Lfo1, "osc1_shape", 0.4f)
        .ultra().dist (TUBE, 14.0f, 0.5f, 0.6f).filt (0, LP24, 3500.0f, 0.2f).mono (true, 2).bassMono().done());
    L.push_back (D ("Siren Sweep", "FX", "Slow triangle siren over an octave.")
        .osc (0, "Basic Shapes", 0.45f, 0.5f).lfoHz (0, TRI, 0.4f, 0).mod (M::Lfo1, "osc1_pitch", 0.07f).amp (0.05f, 0.5f, 1.0f, 0.5f)
        .reverb (0.7f, 3.0f, 0.3f).done());
    L.push_back (D ("Noise Snare", "FX", "Synth snare: noise burst through a band-pass plus a short tone.")
        .noise (0, 0.6f, 0.2f).filt (0, BP12, 1800.0f, 0.3f).osc (1, "Basic Shapes", 0.0f, 0.4f, 0).route (1, 3).p ("osc2_keytrack", 0.0f)
        .ox (1, "pitch", -2.0f).amp (0.001f, 0.18f, 0.0f, 0.12f).reverb (0.4f, 1.0f, 0.15f).done());
    L.push_back (D ("Kick Drum", "FX", "Synth kick: sine with a fast pitch drop and a click.")
        .osc (0, "Basic Shapes", 0.0f, 0.85f).p ("osc1_keytrack", 0.0f).p ("osc1_pitch", -24.0f).mod (M::Env3, "osc1_pitch", 0.3f)
        .env (2, 0.0f, 0.06f, 0.0f, 0.05f, 0.0f, 0.85f).ox (0, "drive", 0.3f).amp (0.0005f, 0.35f, 0.0f, 0.1f).imager (0.0f).done());
    L.push_back (D ("Wide Pluck Chords", "Pluck", "Wide unison pluck for chord stabs with stereo delay.")
        .osc (0, "Saw Tilt", 0.85f, 0.5f, 0, 8, 0.22f, 1.0f).filt (0, LP24, 900.0f, 0.2f, 0.55f).env (1, 0.0f, 0.3f, 0.0f, 0.2f)
        .amp (0.001f, 0.6f, 0.0f, 0.4f).delay (D8D, 0.4f, 0.25f).reverb (0.6f, 2.5f, 0.25f).done());
    L.push_back (D ("Seq Bass Groove", "Bass", "Sequenced 1/16 bass groove: hold one note.")
        .osc (0, "Saw Tilt", 0.8f, 0.7f, -1).filt (0, LADLP, 450.0f, 0.5f, 0.45f, 0.2f).env (1, 0.0f, 0.12f, 0.0f, 0.1f)
        .amp (0.001f, 0.2f, 0.6f, 0.06f).seq (10, { 0, -99, 0, 12, -99, 0, 10, 0, 0, -99, 7, 0, 12, -99, 3, 5 }, 0.45f)
        .mono (false, 0).bassMono().done());
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = []
    {
        List out;
        {
            PresetBuilder b ("Init", "Init");
            b.description ("Single wavetable oscillator (Basic Shapes), filter off. Default starting point.");
            out.push_back ({ "Init", "Init", "Default starting point", b.build() });
        }
        bass (out);
        subBass (out);
        reese (out);
        growl (out);
        lead (out);
        pad (out);
        pluck (out);
        keys (out);
        bell (out);
        atmospheric (out);
        cinematic (out);
        texture (out);
        drone (out);
        experimental (out);
        digital (out);
        analogPresets (out);
        dark (out);
        ethereal (out);
        aggressive (out);
        fxPresets (out);
        extras (out);
        moreDesigns (out);
        return out;
    }();
    return list;
}
} // namespace dw

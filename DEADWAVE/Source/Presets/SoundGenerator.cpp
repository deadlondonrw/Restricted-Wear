#include "SoundGenerator.h"
#include "../Core/DspUtil.h"
#include "../DSP/WaveGen.h"
#include "PresetBuilder.h"

namespace dw
{
namespace
{
struct LexEntry
{
    const char* words; // '|' separated synonyms
    const char* note;
    void (*apply) (GenTraits&);
};

void up (float& v, float a) { v = juce::jlimit (0.0f, 1.0f, v + a); }

const LexEntry lexicon[] = {
    // ---- categories
    { "bass|basses|bassline", "category: bass", [] (GenTraits& t) { t.category = "Bass"; } },
    { "sub|808|subby", "category: sub bass", [] (GenTraits& t) { t.category = "Sub Bass"; up (t.brightness, -0.3f); } },
    { "reese|neuro", "category: reese", [] (GenTraits& t) { t.category = "Reese"; up (t.movement, 0.2f); } },
    { "growl|wobble|dubstep|yoy|riddim", "category: growl", [] (GenTraits& t) { t.category = "Growl"; up (t.movement, 0.35f); up (t.aggression, 0.3f); } },
    { "lead|leads|solo|melody", "category: lead", [] (GenTraits& t) { t.category = "Lead"; } },
    { "pad|pads|chords|string|strings", "category: pad", [] (GenTraits& t) { t.category = "Pad"; up (t.attack, 0.3f); up (t.length, 0.3f); } },
    { "pluck|plucks|plucky|stab", "category: pluck", [] (GenTraits& t) { t.category = "Pluck"; up (t.attack, -0.3f); up (t.length, -0.2f); } },
    { "keys|piano|ep|rhodes|organ", "category: keys", [] (GenTraits& t) { t.category = "Keys"; } },
    { "bell|bells|chime|mallet", "category: bell", [] (GenTraits& t) { t.category = "Bell"; t.metallic = true; } },
    { "atmosphere|atmospheric|ambient|ambience", "category: atmospheric", [] (GenTraits& t) { t.category = "Atmospheric"; up (t.space, 0.4f); up (t.attack, 0.3f); } },
    { "cinematic|epic|trailer|film|score", "category: cinematic", [] (GenTraits& t) { t.category = "Cinematic"; up (t.space, 0.3f); up (t.width, 0.3f); } },
    { "texture|textural|granular", "category: texture", [] (GenTraits& t) { t.category = "Texture"; up (t.complexity, 0.3f); } },
    { "drone|drones|hum", "category: drone", [] (GenTraits& t) { t.category = "Drone"; up (t.length, 0.5f); } },
    { "experimental|weird|strange|alien|chaotic", "category: experimental", [] (GenTraits& t) { t.category = "Experimental"; up (t.complexity, 0.4f); } },
    { "digital|cyber|glitch|glitchy", "category: digital", [] (GenTraits& t) { t.category = "Digital"; up (t.warmth, -0.4f); } },
    { "analog|analogue|vintage|retro|moog", "category: analog", [] (GenTraits& t) { t.category = "Analog"; up (t.warmth, 0.4f); } },
    { "fx|riser|rise|impact|sweep|whoosh|downlifter", "category: fx", [] (GenTraits& t) { t.category = "FX"; up (t.movement, 0.3f); } },
    // ---- tone
    { "bright|brilliant|crisp|sparkly|shiny", "brighter", [] (GenTraits& t) { up (t.brightness, 0.3f); } },
    { "dark|darker|muffled|dull|deep|murky", "darker", [] (GenTraits& t) { up (t.brightness, -0.3f); } },
    { "warm|warmer|smooth|round|mellow", "warmer", [] (GenTraits& t) { up (t.warmth, 0.3f); up (t.brightness, -0.1f); up (t.aggression, -0.1f); } },
    { "cold|sterile|clinical", "colder", [] (GenTraits& t) { up (t.warmth, -0.3f); } },
    { "aggressive|angry|brutal|hard|heavy|nasty|filthy", "more aggressive", [] (GenTraits& t) { up (t.aggression, 0.35f); } },
    { "distorted|dirty|gritty|crunchy|saturated|overdriven", "distortion", [] (GenTraits& t) { up (t.aggression, 0.3f); } },
    { "clean|pure|clear|soft|gentle", "cleaner", [] (GenTraits& t) { up (t.aggression, -0.3f); } },
    { "wide|huge|massive|big|stereo|lush", "wider", [] (GenTraits& t) { up (t.width, 0.3f); } },
    { "narrow|mono|centered|tight", "narrower", [] (GenTraits& t) { up (t.width, -0.35f); } },
    { "evolving|moving|animated|alive|morphing|modulated", "more movement", [] (GenTraits& t) { up (t.movement, 0.35f); } },
    { "static|still|steady", "less movement", [] (GenTraits& t) { up (t.movement, -0.35f); } },
    { "complex|rich|thick|dense|layered", "more complex", [] (GenTraits& t) { up (t.complexity, 0.3f); } },
    { "simple|minimal|thin|basic", "simpler", [] (GenTraits& t) { up (t.complexity, -0.3f); } },
    { "slow|swell|swelling|fading", "slower attack", [] (GenTraits& t) { up (t.attack, 0.35f); } },
    { "punchy|snappy|fast|percussive|tight", "faster attack", [] (GenTraits& t) { up (t.attack, -0.4f); } },
    { "long|sustained|endless|infinite", "longer", [] (GenTraits& t) { up (t.length, 0.35f); } },
    { "short|staccato|tiny|blip", "shorter", [] (GenTraits& t) { up (t.length, -0.35f); } },
    { "spacious|reverb|hall|cathedral|cavernous|distant", "more space", [] (GenTraits& t) { up (t.space, 0.35f); } },
    { "dry|close|intimate", "drier", [] (GenTraits& t) { up (t.space, -0.35f); } },
    { "ethereal|airy|heavenly|dreamy|angelic", "ethereal", [] (GenTraits& t) { up (t.space, 0.3f); up (t.brightness, 0.15f); up (t.aggression, -0.2f); if (t.category == "Lead") t.category = "Ethereal"; } },
    { "glassy|glass|metallic|metal|bell-like|crystal", "metallic partials", [] (GenTraits& t) { t.metallic = true; } },
    { "vocal|voice|talking|vowel|formant|choir", "vocal formants", [] (GenTraits& t) { t.vocal = true; } },
    { "glide|slide|portamento|sliding|legato", "mono glide", [] (GenTraits& t) { t.glide = true; } },
    { "acid|303|squelchy|squelch", "acid", [] (GenTraits& t) { t.acid = true; t.glide = true; t.category = "Bass"; } },
    { "chiptune|8bit|8-bit|retro-game|nes|gameboy", "chiptune", [] (GenTraits& t) { t.chip = true; up (t.warmth, -0.5f); } },
    { "detuned|supersaw|unison|fat", "heavy unison", [] (GenTraits& t) { up (t.width, 0.3f); up (t.complexity, 0.2f); } },
    { "dark-ambient|horror|scary|haunted|eerie", "eerie", [] (GenTraits& t) { up (t.brightness, -0.35f); up (t.movement, 0.2f); up (t.space, 0.3f); } },
};

struct Gen
{
    PresetBuilder& b;
    Rng& rng;
    float r (float lo, float hi) { return lo + (hi - lo) * rng.uni(); }
    template <size_t N>
    const char* pick (const char* (&a)[N]) { return a[(size_t) (rng.uni() * (float) N) % N]; }
    int ri (int lo, int hi) { return lo + (int) (rng.uni() * (float) (hi - lo + 1)) % (hi - lo + 1); }
};
} // namespace

GenTraits SoundGenerator::parseText (const juce::String& text, juce::StringArray& understood)
{
    GenTraits t;
    understood.clear();
    auto words = juce::StringArray::fromTokens (text.toLowerCase().replaceCharacters (",.;:!?/()", "         "), " ", "");
    words.removeEmptyStrings();
    float intensity = 1.0f;
    for (const auto& w : words)
    {
        if (w == "very" || w == "extremely" || w == "super" || w == "really") { intensity = 1.6f; continue; }
        if (w == "slightly" || w == "little" || w == "bit" || w == "somewhat") { intensity = 0.5f; continue; }
        for (const auto& e : lexicon)
        {
            const auto syn = juce::StringArray::fromTokens (e.words, "|", "");
            if (! syn.contains (w)) continue;
            GenTraits before = t;
            e.apply (t);
            if (intensity != 1.0f)
            {
                auto scale = [&] (float a, float& b) { b = juce::jlimit (0.0f, 1.0f, a + (b - a) * intensity); };
                scale (before.brightness, t.brightness); scale (before.movement, t.movement); scale (before.aggression, t.aggression);
                scale (before.width, t.width); scale (before.complexity, t.complexity); scale (before.attack, t.attack);
                scale (before.length, t.length); scale (before.warmth, t.warmth); scale (before.space, t.space);
            }
            understood.addIfNotAlreadyThere (w + " -> " + e.note);
            break;
        }
        intensity = 1.0f;
    }
    return t;
}

juce::ValueTree SoundGenerator::generate (const GenTraits& t, uint32_t seed, const juce::String& name)
{
    PresetBuilder b (name, t.category);
    Rng rng (seed * 2654435761u + 7u);
    Gen g { b, rng };
    const auto& cat = t.category;
    const float bright = t.brightness, move = t.movement, aggr = t.aggression, wide = t.width, cplx = t.complexity;

    auto osc = [] (int i, const char* p) { return "osc" + juce::String (i + 1) + "_" + p; };
    auto setOsc = [&] (int i, const char* p, float v) { b.set (osc (i, p), v); };

    // ------------------------------------------------------------------ wavetable palette
    const char* bright1[] = { "Basic Shapes", "Saw Tilt", "PWM", "Hollow Square", "Sync Saw", "Comb Sweep" };
    const char* soft1[] = { "Basic Shapes", "Sub Shapes", "Harmonic Sweep", "Organ", "Skew Triangle" };
    const char* vocal1[] = { "Formant Vowels", "Vocal Growl", "Growl" };
    const char* metal1[] = { "Glass", "Metallic", "FM 1-3", "FM 1-1", "Octave Stack" };
    const char* gritty1[] = { "Wavefold", "Tanh Drive", "Sync Saw", "Phase Distortion", "Digital Steps" };
    const char* air1[] = { "Spectral Noise", "Harmonic Sweep", "Glass", "Octave Stack", "Formant Vowels" };

    const char* main = t.vocal ? g.pick (vocal1) : t.metallic ? g.pick (metal1) : t.chip ? "Chip"
                     : aggr > 0.6f ? g.pick (gritty1) : bright > 0.45f ? g.pick (bright1) : g.pick (soft1);
    b.waveFactory (0, main);

    // ------------------------------------------------------------------ envelope archetypes
    float A = 0.002f + t.attack * t.attack * 2.5f;
    float D = 0.15f + t.length * 1.2f;
    float S = 0.75f;
    float R = 0.06f + t.length * t.length * 3.0f;
    bool mono = t.glide;
    int unison = 1 + (int) std::round (wide * 4.0f + cplx * 2.0f);
    float cutoff = 400.0f * std::pow (40.0f, bright);
    float reso = 0.1f + 0.3f * cplx;
    int ftype = 1;
    float envAmt = 0.0f;
    int octave = 0;

    if (cat == "Bass" || cat == "Reese" || cat == "Growl" || cat == "Sub Bass")
    {
        octave = -1;
        A = 0.001f; S = cat == "Sub Bass" ? 1.0f : 0.8f; R = 0.05f + 0.2f * t.length;
        unison = cat == "Reese" ? 3 + (int) (wide * 4.0f) : (cat == "Sub Bass" ? 1 : 1 + (int) (wide * 2.0f));
        cutoff = 150.0f * std::pow (25.0f, bright);
        envAmt = 0.25f + 0.25f * (1.0f - t.length);
        ftype = t.acid ? 9 : (cat == "Growl" ? 13 : 1);
        if (cat == "Sub Bass") { b.waveFactory (0, "Sub Shapes"); setOsc (0, "wtpos", g.r (0.0f, 0.4f)); cutoff = 600.0f; envAmt = 0.0f; }
        if (cat == "Reese") { b.waveFactory (0, "Saw Tilt"); setOsc (0, "wtpos", 0.7f + 0.3f * bright); setOsc (0, "detune", 0.18f + 0.15f * wide); }
        if (cat == "Growl") { b.waveFactory (0, g.pick (vocal1)); }
        if (t.acid) { mono = true; reso = 0.55f + 0.3f * aggr; cutoff = 300.0f + 900.0f * bright; envAmt = 0.55f; D = 0.25f; S = 0.2f; }
        // sub layer
        if (cat != "Sub Bass")
        {
            b.set ("osc3_on", 1.0f);
            b.waveFactory (2, "Sub Shapes");
            setOsc (2, "oct", -1.0f);
            setOsc (2, "level", 0.5f);
            setOsc (2, "route", 3.0f); // direct: sub bypasses the filter
        }
    }
    else if (cat == "Pad" || cat == "Atmospheric" || cat == "Cinematic" || cat == "Drone" || cat == "Ethereal")
    {
        A = 0.3f + t.attack * 2.5f; D = 1.5f; S = 0.8f; R = 1.0f + t.length * 4.0f;
        unison = 4 + (int) (wide * 6.0f);
        if (cat == "Drone") { A = 1.5f; R = 6.0f; S = 1.0f; }
        b.waveFactory (0, t.vocal ? g.pick (vocal1) : g.pick (air1));
        b.set ("osc2_on", 1.0f);
        b.waveFactory (1, t.metallic ? g.pick (metal1) : g.pick (soft1));
        setOsc (1, "oct", cat == "Cinematic" || cat == "Drone" ? -1.0f : 1.0f);
        setOsc (1, "level", 0.45f);
        setOsc (1, "unison", (float) juce::jmax (2, unison / 2));
        setOsc (1, "detune", 0.25f);
        ftype = 0;
        envAmt = 0.0f;
        if (cat == "Atmospheric" || cat == "Drone")
        {
            b.set ("osc4_on", 1.0f);
            setOsc (3, "type", 1.0f);           // noise
            setOsc (3, "shape", -0.5f + bright * 0.7f);
            setOsc (3, "level", 0.12f + 0.1f * cplx);
            setOsc (3, "spread", 1.0f);
        }
    }
    else if (cat == "Pluck" || cat == "Keys" || cat == "Bell")
    {
        A = 0.001f; S = cat == "Keys" ? 0.35f : 0.0f;
        D = cat == "Bell" ? 1.5f + t.length * 3.0f : 0.25f + t.length * 0.9f;
        R = cat == "Bell" ? D * 0.8f : 0.15f + t.length * 0.5f;
        envAmt = cat == "Bell" ? 0.0f : 0.45f;
        unison = cat == "Keys" ? 1 : 1 + (int) (wide * 3.0f);
        if (cat == "Bell")
        {
            b.waveFactory (0, g.pick (metal1));
            setOsc (0, "fmsrc", 1.0f);
            setOsc (0, "fmratio", g.r (0.0f, 1.0f) < 0.5f ? 3.5f : 2.76f);
            setOsc (0, "fmamt", 0.25f + 0.25f * bright);
            b.mod (ModSrc::Env2, osc (0, "fmamt"), -0.25f);
        }
        if (cat == "Keys") { b.waveFactory (0, g.r (0, 1) < 0.5f ? "FM 1-3" : "Organ"); b.mod (ModSrc::Velocity, "filter1_cutoff", 0.25f); }
    }
    else if (cat == "Lead" || cat == "Analog" || cat == "Digital" || cat == "Aggressive" || cat == "Dark")
    {
        A = 0.003f + t.attack * 0.4f; D = 0.4f; S = 0.85f; R = 0.12f + t.length * 0.8f;
        unison = 1 + (int) (wide * 6.0f);
        envAmt = 0.15f;
        if (cat == "Analog") { b.waveFactory (0, g.r (0, 1) < 0.5f ? "Saw Tilt" : "PWM"); ftype = 9; }
        if (cat == "Digital") { b.waveFactory (0, g.r (0, 1) < 0.5f ? "Digital Steps" : "Chip"); setOsc (0, "warp", 6.0f); setOsc (0, "shape", 0.3f); }
        if (cat == "Dark") cutoff *= 0.4f;
        b.set ("osc2_on", 1.0f);
        b.waveFactory (1, g.pick (bright1));
        setOsc (1, "fine", g.r (5.0f, 14.0f));
        setOsc (1, "level", 0.5f);
            }
    else if (cat == "Texture" || cat == "Experimental" || cat == "FX")
    {
        b.waveRandom (0, g.ri (0, 4), rng.next(), 0.4f + 0.6f * cplx, 32);
        A = cat == "FX" ? 0.01f : 0.05f + t.attack; D = 2.0f; S = 0.6f; R = 1.5f + t.length * 2.0f;
        unison = 1 + (int) (wide * 5.0f);
        b.set ("osc2_on", 1.0f);
        b.waveRandom (1, g.ri (0, 4), rng.next(), 0.5f, 16);
        setOsc (1, "fmsrc", 2.0f); // FM from OSC 1
        setOsc (1, "fmamt", 0.2f + 0.4f * cplx);
        setOsc (1, "level", 0.4f);
        if (cat == "FX")
        {
            // riser: slow upward pitch envelope + filter sweep + noise
            b.set ("env3_attack", 3.0f + t.length * 3.0f);
            b.set ("env3_sustain", 1.0f);
            b.mod (ModSrc::Env3, osc (0, "pitch"), 0.25f);
            b.mod (ModSrc::Env3, osc (1, "pitch"), 0.25f);
            b.mod (ModSrc::Env3, "filter1_cutoff", 0.5f);
            b.set ("osc4_on", 1.0f);
            setOsc (3, "type", 1.0f);
            setOsc (3, "level", 0.3f);
            S = 1.0f;
        }
    }

    // ------------------------------------------------------------------ oscillator 1 common
    setOsc (0, "oct", (float) octave);
    if (b.get (P::osc (1, OscP::On)) >= 0.5f) setOsc (1, "oct", b.get (P::osc (1, OscP::Octave)) + (float) octave);
    setOsc (0, "unison", (float) juce::jlimit (1, 16, unison));
    if (unison > 1)
    {
        if (b.get (P::osc (0, OscP::Detune)) == Registry::get().spec (P::osc (0, OscP::Detune)).def)
            setOsc (0, "detune", 0.12f + 0.25f * wide);
        setOsc (0, "spread", 0.5f + 0.5f * wide);
        setOsc (0, "phaserand", 1.0f);
    }
    if (b.get (P::osc (0, OscP::WtPos)) == 0.0f) setOsc (0, "wtpos", g.r (0.1f, 0.7f) * (0.4f + bright));
    setOsc (0, "level", unison > 4 ? 0.62f : 0.72f);
    if (aggr > 0.45f) { setOsc (0, "drive", (aggr - 0.4f) * 0.9f); setOsc (0, "drivemode", (float) g.ri (0, 3)); }
    if (rng.uni() < cplx * 0.6f) { setOsc (0, "warp", (float) g.ri (0, 7)); setOsc (0, "shape", g.r (-0.4f, 0.4f) * cplx); }
    if (rng.uni() < 0.25f + 0.4f * aggr) setOsc (0, "bias", g.r (-0.35f, 0.35f));

    // ------------------------------------------------------------------ filter / envelopes
    b.set ("filter1_on", cat == "Sub Bass" ? 0.0f : 1.0f);
    b.set ("filter1_type", (float) ftype);
    b.set ("filter1_cutoff", juce::jlimit (60.0f, 18000.0f, cutoff));
    b.set ("filter1_reso", juce::jlimit (0.0f, 0.95f, reso));
    b.set ("filter1_envamt", envAmt);
    b.set ("filter1_keytrack", 0.35f);
    b.set ("filter1_drive", aggr * 0.5f);
    b.set ("env1_attack", A); b.set ("env1_decay", D); b.set ("env1_sustain", S); b.set ("env1_release", R);
    b.set ("env2_attack", juce::jmin (A, 0.01f)); b.set ("env2_decay", 0.12f + t.length * 0.8f); b.set ("env2_sustain", 0.0f);
    b.set ("env2_release", R);

    // ------------------------------------------------------------------ movement
    if (move > 0.2f)
    {
        const bool growl = cat == "Growl";
        b.set ("lfo1_sync", 1.0f);
        b.set ("lfo1_div", (float) (growl ? (move > 0.6f ? 13 : 10) : (move > 0.6f ? 7 : 3)));
        b.set ("lfo1_shape", growl ? 0.0f : (float) g.ri (0, 6));
        b.set ("lfo1_mode", growl ? 1.0f : 0.0f);
        b.set ("lfo1_smooth", 0.2f);
        b.mod (ModSrc::Lfo1, osc (0, "wtpos"), 0.15f + 0.45f * move);
        if (b.get (P::filt (0, FiltP::On)) >= 0.5f) b.mod (ModSrc::Lfo1, "filter1_cutoff", (growl ? 0.35f : 0.12f) * move);
        if (move > 0.55f)
        {
            b.set ("lfo2_sync", 0.0f);
            b.set ("lfo2_rate", 0.05f + 0.3f * move);
            b.set ("lfo2_mode", 0.0f);
            b.set ("lfo2_shape", 6.0f);
            b.mod (ModSrc::Lfo2, osc (0, "shape"), 0.2f * move);
            b.mod (ModSrc::Lfo2, osc (0, "pan"), 0.15f * wide);
        }
    }
    b.mod (ModSrc::Velocity, "filter1_cutoff", 0.12f);

    // ------------------------------------------------------------------ voice
    if (mono || cat == "Bass" || cat == "Reese" || cat == "Growl" || cat == "Sub Bass")
    {
        b.set ("voice_mode", t.glide ? 2.0f : (cat == "Bass" ? 0.0f : 1.0f));
        if (t.glide) { b.set ("glide_mode", 2.0f); b.set ("glide_time", 0.06f + 0.15f * t.length); }
    }

    // ------------------------------------------------------------------ analog character
    b.set ("analog_drift", t.warmth * 0.35f);
    b.set ("analog_instab", t.warmth * 0.25f);
    b.set ("analog_filter", t.warmth * 0.6f);
    b.set ("analog_stereo", wide * 0.5f);

    // ------------------------------------------------------------------ effects
    if (aggr > 0.5f)
    {
        b.set ("fx_dist_on", 1.0f);
        b.set ("fx_dist_mode", (float) g.ri (0, 4));
        b.set ("fx_dist_drive", 6.0f + aggr * 22.0f);
        b.set ("fx_dist_mix", 0.4f + 0.5f * aggr);
        b.set ("fx_cmp_on", 1.0f);
        b.set ("fx_cmp_thresh", -20.0f);
        b.set ("fx_cmp_ratio", 5.0f);
    }
    else if (t.warmth > 0.4f)
    {
        b.set ("fx_sat_on", 1.0f);
        b.set ("fx_sat_drive", 3.0f + t.warmth * 6.0f);
    }
    if (wide > 0.55f && cat != "Sub Bass")
    {
        b.set ("fx_cho_on", 1.0f);
        b.set ("fx_cho_mix", 0.25f + 0.25f * wide);
    }
    if (t.space > 0.2f)
    {
        b.set ("fx_rev_on", 1.0f);
        b.set ("fx_rev_size", 0.3f + 0.6f * t.space);
        b.set ("fx_rev_decay", 1.0f + t.space * 7.0f);
        b.set ("fx_rev_mix", juce::jmin (0.6f, 0.1f + 0.4f * t.space));
    }
    if (t.space > 0.35f && cat != "Pad")
    {
        b.set ("fx_dly_on", 1.0f);
        b.set ("fx_dly_div", 9.0f);
        b.set ("fx_dly_pingpong", 1.0f);
        b.set ("fx_dly_mix", 0.15f + 0.15f * t.space);
    }
    if (t.chip) { b.set ("fx_crs_on", 1.0f); b.set ("fx_crs_bits", 6.0f); b.set ("fx_crs_rate", 16000.0f); }
    if (cat == "Bass" || cat == "Sub Bass" || cat == "Reese" || cat == "Growl")
    {
        b.set ("fx_img_on", 1.0f);
        b.set ("fx_img_bassmono", 120.0f);
    }

    // ------------------------------------------------------------------ macros
    b.mod (ModSrc::Macro1, "filter1_cutoff", 0.35f);
    b.mod (ModSrc::Macro2, osc (0, "wtpos"), 0.5f);
    b.mod (ModSrc::Macro3, "fx_rev_mix", 0.4f);
    b.mod (ModSrc::Macro4, osc (0, "drive"), 0.6f);
    b.description ("Generated locally from traits: brightness " + juce::String (bright, 2) + ", movement " + juce::String (move, 2)
                   + ", aggression " + juce::String (aggr, 2) + ", width " + juce::String (wide, 2));
    return b.build();
}

// ================================================================================== randomisation
const juce::StringArray& SoundGenerator::lockableGroups()
{
    static const juce::StringArray n { "OSC 1", "OSC 2", "OSC 3", "OSC 4", "WAVETABLES", "FILTERS", "ENVELOPES", "LFOS",
                                       "FX", "VOICE", "MATRIX", "ARP/SEQ" };
    return n;
}

std::vector<Group> SoundGenerator::groupsFor (const juce::String& n)
{
    if (n.startsWith ("OSC ")) return { oscGroup (n.getLastCharacters (1).getIntValue() - 1) };
    if (n == "FILTERS") return { Group::Filter1, Group::Filter2 };
    if (n == "ENVELOPES") return { Group::Env1, Group::Env2, Group::Env3, Group::Env4 };
    if (n == "LFOS") return { Group::Lfo1, Group::Lfo2, Group::Lfo3, Group::Lfo4 };
    if (n == "FX") return { Group::FxMaster, Group::FxOsc1, Group::FxOsc2, Group::FxOsc3, Group::FxOsc4 };
    if (n == "VOICE") return { Group::Voice, Group::Analog };
    if (n == "MATRIX") return { Group::Mod };
    if (n == "ARP/SEQ") return { Group::Arp, Group::Seq };
    if (n == "WAVETABLES") return { Group::NumGroups };
    return {};
}

juce::ValueTree SoundGenerator::randomize (const juce::ValueTree& current, float intensity, const std::set<Group>& locked,
                                           uint32_t seed, bool full)
{
    const auto& reg = Registry::get();
    Rng rng (seed * 747796405u + 2891336453u);
    auto out = current.createCopy();
    out.setProperty ("preset", full ? "Random" : current.getProperty ("preset", "Init").toString() + " *", nullptr);

    std::vector<float> v ((size_t) P::Count);
    for (int i = 0; i < P::Count; ++i) v[(size_t) i] = reg.spec (i).def;
    auto params = out.getChildWithName ("PARAMS");
    for (const auto& c : params)
    {
        const int idx = reg.indexOf (c["id"].toString());
        if (idx >= 0) v[(size_t) idx] = (float) c["value"];
    }
    auto isLocked = [&] (int i) { return locked.count (reg.spec (i).group) > 0; };
    const bool fxLocked = locked.count (Group::FxMaster) > 0;

    for (int i = 0; i < P::Count; ++i)
    {
        const auto& s = reg.spec (i);
        if (! s.randomizable || isLocked (i) || s.group == Group::Mod || s.group == Group::Macro) continue;
        const auto range = s.range();
        float n = range.convertTo0to1 (v[(size_t) i]);
        const bool fxOnSwitch = s.group >= Group::FxMaster && s.group <= Group::FxOsc4 && s.kind == PKind::Bool;

        if (full)
        {
            switch (s.kind)
            {
                case PKind::Bool: n = rng.uni() < (fxOnSwitch ? 0.22f : 0.5f) ? 1.0f : 0.0f; break;
                case PKind::Choice:
                case PKind::Int: n = rng.uni(); break;
                case PKind::Float: n = rng.uni(); break;
            }
        }
        else
        {
            switch (s.kind)
            {
                case PKind::Bool: if (rng.uni() < intensity * (fxOnSwitch ? 0.12f : 0.1f)) n = 1.0f - n; break;
                case PKind::Choice:
                case PKind::Int: if (rng.uni() < intensity * 0.25f) n = rng.uni(); break;
                case PKind::Float: n = juce::jlimit (0.0f, 1.0f, n + rng.bi() * intensity * 0.35f); break;
            }
        }
        v[(size_t) i] = range.convertFrom0to1 (n);
    }

    // ---- musical constraints so random sounds stay playable ------------------------------------
    auto clampP = [&] (int i, float lo, float hi) { if (! isLocked (i)) v[(size_t) i] = juce::jlimit (lo, hi, v[(size_t) i]); };
    if (! isLocked (P::osc (0, OscP::On))) v[(size_t) P::osc (0, OscP::On)] = 1.0f;
    for (int o = 0; o < kNumOsc; ++o)
    {
        clampP (P::osc (o, OscP::Level), 0.25f, 0.85f);
        clampP (P::osc (o, OscP::Octave), -2.0f, 1.0f);
        clampP (P::osc (o, OscP::Pitch), -12.0f, 12.0f);
        clampP (P::osc (o, OscP::Amp), 0.6f, 1.4f);
        clampP (P::osc (o, OscP::Bias), -0.5f, 0.5f);
        clampP (P::osc (o, OscP::FmAmt), 0.0f, 0.6f);
        clampP (P::osc (o, OscP::Crush), 0.0f, 0.4f);
        if (full && ! isLocked (P::osc (o, OscP::Type))) v[(size_t) P::osc (o, OscP::Type)] = rng.uni() < 0.12f ? 1.0f : 0.0f;
        if (! isLocked (P::osc (o, OscP::FxOn)) && full) v[(size_t) P::osc (o, OscP::FxOn)] = 0.0f;
        clampP (P::osc (o, OscP::Fine), -20.0f, 20.0f);
    }
    for (int f = 0; f < kNumFilters; ++f)
    {
        clampP (P::filt (f, FiltP::Cutoff), 150.0f, 18000.0f);
        clampP (P::filt (f, FiltP::Reso), 0.0f, 0.8f);
        if (full && ! isLocked (P::filt (f, FiltP::Type)) && v[(size_t) P::filt (f, FiltP::Type)] >= 11.0f && rng.uni() < 0.5f)
            v[(size_t) P::filt (f, FiltP::Type)] = 1.0f;
    }
    clampP (P::env (0, EnvP::Delay), 0.0f, 0.05f);
    clampP (P::env (0, EnvP::Attack), 0.0f, 1.5f);
    clampP (P::env (0, EnvP::Release), 0.02f, 4.0f);
    if (! isLocked (P::env (0, EnvP::Sustain)) && v[(size_t) P::env (0, EnvP::Sustain)] < 0.1f)
        v[(size_t) P::env (0, EnvP::Decay)] = juce::jmax (0.25f, v[(size_t) P::env (0, EnvP::Decay)]);
    clampP (P::voice (VoiceP::GlideTime), 0.0f, 0.5f);
    clampP (P::voice (VoiceP::BendUp), 0.0f, 12.0f);
    clampP (P::voice (VoiceP::BendDown), 0.0f, 12.0f);
    if (! fxLocked)
    {
        for (int r = 0; r < kNumRacks; ++r)
        {
            clampP (P::fx (r, FxP::DistDrive), 0.0f, 30.0f);
            clampP (P::fx (r, FxP::DlyFeedback), 0.0f, 0.75f);
            clampP (P::fx (r, FxP::DlyMix), 0.0f, 0.45f);
            clampP (P::fx (r, FxP::RevMix), 0.0f, 0.5f);
            clampP (P::fx (r, FxP::RevDecay), 0.3f, 8.0f);
            clampP (P::fx (r, FxP::CrsBits), 4.0f, 24.0f);
            clampP (P::fx (r, FxP::CrsRate), 4000.0f, 48000.0f);
            clampP (P::fx (r, FxP::EqLowGain), -9.0f, 9.0f);
            clampP (P::fx (r, FxP::EqMidGain), -9.0f, 9.0f);
            clampP (P::fx (r, FxP::EqHighGain), -9.0f, 9.0f);
            clampP (P::fx (r, FxP::ChoFeedback), -0.5f, 0.5f);
            clampP (P::fx (r, FxP::FlaFeedback), -0.7f, 0.7f);
            clampP (P::fx (r, FxP::PhaFeedback), -0.7f, 0.7f);
            clampP (P::fx (r, FxP::CmpMakeup), -3.0f, 6.0f);
            clampP (P::fx (r, FxP::ImgWidth), 0.5f, 1.6f);
        }
        if (full) v[(size_t) P::fx (0, FxP::RackOn)] = 1.0f;
    }
    if (! locked.count (Group::Arp) && full) v[(size_t) P::arp (ArpP::MidiFxMode)] = 0.0f;

    // write params back
    juce::ValueTree ps ("PARAMS");
    for (int i = 0; i < P::Count; ++i)
    {
        if (std::abs (v[(size_t) i] - reg.spec (i).def) < 1.0e-6f) continue;
        juce::ValueTree c ("PARAM");
        c.setProperty ("id", reg.spec (i).id, nullptr);
        c.setProperty ("value", v[(size_t) i], nullptr);
        ps.appendChild (c, nullptr);
    }
    if (params.isValid()) out.removeChild (params, nullptr);
    out.appendChild (ps, nullptr);

    // ---- wavetables ---------------------------------------------------------------------------
    if (! locked.count (Group::NumGroups)) // NumGroups is the sentinel for the WAVETABLES lock
    {
        auto waves = out.getChildWithName ("WAVES");
        if (! waves.isValid()) { waves = juce::ValueTree ("WAVES"); out.appendChild (waves, nullptr); }
        for (int o = 0; o < kNumOsc; ++o)
        {
            if (locked.count (oscGroup (o))) continue;
            if (! full && rng.uni() > intensity * 0.5f) continue;
            juce::ValueTree w ("WAVE");
            w.setProperty ("osc", o, nullptr);
            if (rng.uni() < 0.65f)
            {
                const auto& names = wavegen::factoryTableNames();
                const auto nm = names[(int) (rng.uni() * (float) names.size()) % names.size()];
                w.setProperty ("factory", nm, nullptr);
                w.setProperty ("name", nm, nullptr);
            }
            else
            {
                w.setProperty ("rstyle", (int) (rng.uni() * 5.0f) % 5, nullptr);
                w.setProperty ("rseed", (juce::int64) rng.next(), nullptr);
                w.setProperty ("rcomplex", 0.3f + 0.6f * rng.uni(), nullptr);
                w.setProperty ("frames", 16, nullptr);
                w.setProperty ("name", "Random", nullptr);
            }
            auto old = waves.getChildWithProperty ("osc", o);
            if (old.isValid()) waves.removeChild (old, nullptr);
            waves.appendChild (w, nullptr);
        }
    }

    // ---- matrix -------------------------------------------------------------------------------
    if (! locked.count (Group::Mod) && full)
    {
        auto mm = out.getChildWithName ("MATRIX");
        if (mm.isValid()) out.removeChild (mm, nullptr);
        mm = juce::ValueTree ("MATRIX");
        const char* dests[] = { "osc1_wtpos", "osc1_shape", "osc1_bias", "filter1_cutoff", "osc2_wtpos", "osc1_pan", "osc1_fine",
                                "filter1_reso", "osc2_fmamt", "osc1_amp", "fx_cho_mix", "osc3_wtpos" };
        const ModSrc srcs[] = { ModSrc::Lfo1, ModSrc::Lfo2, ModSrc::Env2, ModSrc::Env3, ModSrc::Velocity, ModSrc::ModWheel, ModSrc::Lfo3 };
        const int count = 2 + (int) (rng.uni() * 4.0f);
        auto ps2 = out.getChildWithName ("PARAMS");
        for (int k = 0; k < count; ++k)
        {
            juce::ValueTree s ("SLOT");
            s.setProperty ("index", k, nullptr);
            s.setProperty ("src", modSourceId (srcs[(size_t) (rng.uni() * 7.0f) % 7]), nullptr);
            s.setProperty ("dst", dests[(size_t) (rng.uni() * 12.0f) % 12], nullptr);
            mm.appendChild (s, nullptr);
            juce::ValueTree c ("PARAM");
            c.setProperty ("id", "mod" + juce::String (k + 1) + "_amt", nullptr);
            c.setProperty ("value", rng.bi() * 0.5f, nullptr);
            ps2.appendChild (c, nullptr);
        }
        out.appendChild (mm, nullptr);
    }
    return out;
}
} // namespace dw

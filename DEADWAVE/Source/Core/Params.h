#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// =================================================================================================
// DEADWAVE parameter registry
//
// Every host-visible parameter is described once here. The registry is the single source of truth
// for: APVTS layout, stable automation IDs, display formatting, modulation destinations, the
// randomiser / sound generator and UI binding.
//
// IDs are persisted by hosts and presets and must NEVER be renamed. Indices (P::...) are runtime-only
// and may change between versions.
// =================================================================================================

namespace dw
{
constexpr int kNumOsc = 4;
constexpr int kNumFilters = 2;
constexpr int kNumEnvs = 4;
constexpr int kNumLfos = 4;
constexpr int kNumMacros = 4;
constexpr int kNumModSlots = 32;
constexpr int kNumSeqSteps = 16;
constexpr int kNumRacks = 5; // 0 = master, 1..4 = oscillator racks
constexpr int kMaxVoices = 32;
constexpr int kMaxUnison = 16;

// ---------------------------------------------------------------------------------------- groups
enum class Group
{
    Osc1, Osc2, Osc3, Osc4,
    Filter1, Filter2,
    Env1, Env2, Env3, Env4,
    Lfo1, Lfo2, Lfo3, Lfo4,
    Voice, Analog, Global,
    FxMaster, FxOsc1, FxOsc2, FxOsc3, FxOsc4,
    Mod, Macro, Arp, Seq,
    NumGroups
};

inline Group oscGroup (int i)    { return (Group) ((int) Group::Osc1 + i); }
inline Group filterGroup (int i) { return (Group) ((int) Group::Filter1 + i); }
inline Group envGroup (int i)    { return (Group) ((int) Group::Env1 + i); }
inline Group lfoGroup (int i)    { return (Group) ((int) Group::Lfo1 + i); }
inline Group rackGroup (int r)   { return (Group) ((int) Group::FxMaster + r); }

// --------------------------------------------------------------------------- per-group layouts
struct OscP
{
    enum
    {
        On, Type, Level, Pan, Octave, Semi, Fine, KeyTrack,
        WtPos, Bias, Amp, Phase, PhaseRand, Shape, WarpMode,
        Unison, Detune, Spread, Blend,
        FmSrc, FmAmt, FmRatio,
        Route, Drive, DriveMode, Crush, DcRemove,
        FxOn, FxLink,
        Count
    };
};

struct FiltP
{
    enum { On, Type, Cutoff, Reso, Drive, Mix, KeyTrack, EnvAmt, Spread, Count };
};

struct EnvP
{
    enum { Delay, Attack, Hold, Decay, Sustain, Release, AttackCurve, DecayCurve, ReleaseCurve, Count };
};

struct LfoP
{
    enum { Shape, Mode, Sync, Rate, Div, Phase, Smooth, Rise, Bipolar, Count };
};

struct FxP
{
    enum
    {
        RackOn,
        DistOn, DistMode, DistDrive, DistTone, DistMix,
        SatOn, SatDrive, SatBias, SatTone, SatMix,
        ChoOn, ChoRate, ChoDepth, ChoDelay, ChoFeedback, ChoWidth, ChoMix,
        PhaOn, PhaRate, PhaDepth, PhaFreq, PhaFeedback, PhaStages, PhaMix,
        FlaOn, FlaRate, FlaDepth, FlaDelay, FlaFeedback, FlaMix,
        DlyOn, DlySync, DlyTime, DlyDiv, DlyStereo, DlyFeedback, DlyPingPong, DlyLowCut, DlyHighCut, DlyMix,
        RevOn, RevSize, RevDecay, RevDamp, RevPredelay, RevWidth, RevMod, RevMix,
        CmpOn, CmpThresh, CmpRatio, CmpAttack, CmpRelease, CmpKnee, CmpMakeup, CmpMix,
        EqOn, EqLowCut, EqLowFreq, EqLowGain, EqMidFreq, EqMidGain, EqMidQ, EqHighFreq, EqHighGain, EqHighCut,
        ImgOn, ImgWidth, ImgBassMono, ImgPan,
        CrsOn, CrsBits, CrsRate, CrsMix,
        Count
    };
};

struct VoiceP
{
    enum
    {
        Polyphony, Mode, GlideMode, GlideTime, GlideCurve, GlideConstRate, Priority, Retrigger, ReturnToHeld,
        BendUp, BendDown, VelSens, Count
    };
};

struct AnalogP
{
    enum { Drift, Instability, Saturation, FilterNonlin, StereoVar, Count };
};

struct GlobalP
{
    enum { MasterVol, Quality, FilterRouting, VoiceFxRouting, Count };
};

struct ArpP
{
    enum { MidiFxMode, ArpMode, ArpRate, ArpOctaves, ArpGate, ArpSwing, ArpVelMode, ArpVel, ArpHold, ArpSteps, ArpPitchStep, Count };
};

struct SeqP
{
    enum { Rate, Swing, Length, GateScale, Count };
};

struct SeqStepP
{
    enum { On, Pitch, Velocity, Gate, Count };
};

// ------------------------------------------------------------------------- global index layout
namespace P
{
constexpr int OscBase      = 0;
constexpr int FilterBase   = OscBase + kNumOsc * OscP::Count;
constexpr int EnvBase      = FilterBase + kNumFilters * FiltP::Count;
constexpr int LfoBase      = EnvBase + kNumEnvs * EnvP::Count;
constexpr int VoiceBase    = LfoBase + kNumLfos * LfoP::Count;
constexpr int AnalogBase   = VoiceBase + VoiceP::Count;
constexpr int GlobalBase   = AnalogBase + AnalogP::Count;
constexpr int MacroBase    = GlobalBase + GlobalP::Count;
constexpr int ModAmtBase   = MacroBase + kNumMacros;
constexpr int FxBase       = ModAmtBase + kNumModSlots;
constexpr int ArpBase      = FxBase + kNumRacks * FxP::Count;
constexpr int SeqBase      = ArpBase + ArpP::Count;
constexpr int SeqStepBase  = SeqBase + SeqP::Count;
constexpr int Count        = SeqStepBase + kNumSeqSteps * SeqStepP::Count;

constexpr int osc (int i, int p)     { return OscBase + i * OscP::Count + p; }
constexpr int filt (int i, int p)    { return FilterBase + i * FiltP::Count + p; }
constexpr int env (int i, int p)     { return EnvBase + i * EnvP::Count + p; }
constexpr int lfo (int i, int p)     { return LfoBase + i * LfoP::Count + p; }
constexpr int voice (int p)          { return VoiceBase + p; }
constexpr int analog (int p)         { return AnalogBase + p; }
constexpr int global (int p)         { return GlobalBase + p; }
constexpr int macro (int i)          { return MacroBase + i; }
constexpr int modAmt (int slot)      { return ModAmtBase + slot; }
constexpr int fx (int rack, int p)   { return FxBase + rack * FxP::Count + p; }
constexpr int arp (int p)            { return ArpBase + p; }
constexpr int seq (int p)            { return SeqBase + p; }
constexpr int seqStep (int s, int p) { return SeqStepBase + s * SeqStepP::Count + p; }
} // namespace P

// ------------------------------------------------------------------------------- choice tables
namespace Choices
{
const juce::StringArray& oscType();
const juce::StringArray& warpMode();
const juce::StringArray& fmSource();
const juce::StringArray& oscRoute();
const juce::StringArray& driveMode();
const juce::StringArray& filterType();
const juce::StringArray& filterRouting();
const juce::StringArray& lfoShape();
const juce::StringArray& lfoMode();
const juce::StringArray& syncDivision();
const juce::StringArray& voiceMode();
const juce::StringArray& glideMode();
const juce::StringArray& notePriority();
const juce::StringArray& quality();
const juce::StringArray& distMode();
const juce::StringArray& phaserStages();
const juce::StringArray& midiFxMode();
const juce::StringArray& arpMode();
const juce::StringArray& arpVelMode();
const juce::StringArray& noteDivision();
const juce::StringArray& voiceFxRouting();

/** Length of a sync division in quarter notes (beats). */
double divisionToBeats (int divisionIndex);
/** Length of a note division (arp/seq) in beats. */
double noteDivisionToBeats (int divisionIndex);
} // namespace Choices

// ---------------------------------------------------------------------------------- spec / registry
enum class PKind { Float, Choice, Bool, Int };

enum class Unit { None, Percent, Hz, dB, Semitones, Cents, Seconds, Ms, Degrees, Ratio, Octaves, Bipolar, Voices, Bits };

struct ParamSpec
{
    juce::String id;
    juce::String name;      // full name (host)
    juce::String shortName; // UI label
    PKind kind = PKind::Float;
    Unit unit = Unit::None;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    float centre = -1.0f; // skew centre (value at slider midpoint); < 0 = linear
    juce::StringArray choices;
    Group group = Group::Global;
    bool modulatable = false;
    bool perVoice = false;    // value can differ per voice (voice-level modulation)
    bool randomizable = true;

    juce::NormalisableRange<float> range() const;
    juce::String format (float realValue, bool withUnit = true) const;
};

class Registry
{
public:
    static const Registry& get();

    const std::vector<ParamSpec>& specs() const noexcept { return all; }
    const ParamSpec& spec (int index) const noexcept { return all[(size_t) index]; }
    int indexOf (const juce::String& id) const;

    /** Indices of all modulatable parameters, in UI order. */
    const std::vector<int>& modDestinations() const noexcept { return modDests; }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout() const;

private:
    Registry();
    std::vector<ParamSpec> all;
    std::vector<int> modDests;
    std::unordered_map<juce::String, int> byId;
};

// ------------------------------------------------------------------------------- mod sources
enum class ModSrc
{
    None = 0,
    Env1, Env2, Env3, Env4,
    Lfo1, Lfo2, Lfo3, Lfo4,
    Velocity, Note, ModWheel, Aftertouch, PitchBend, Random,
    Macro1, Macro2, Macro3, Macro4,
    NumSources
};

const char* modSourceId (ModSrc s);       // stable, persisted
juce::String modSourceName (ModSrc s);    // display
ModSrc modSourceFromId (const juce::String& id);
bool modSourceIsBipolar (ModSrc s);

} // namespace dw

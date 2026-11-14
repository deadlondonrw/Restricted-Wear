#pragma once

#include "../Core/Params.h"
#include "../MIDI/MidiFx.h"
#include "FxRack.h"
#include "Halfband.h"
#include "Voice.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace dw
{
struct PreviewClip
{
    std::vector<float> l, r;
    double rate = 44100.0;
};

/** Transport information captured from the host for one block. */
struct TransportInfo
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
};

/**
    The real-time synthesis engine. Owns voices, modulation, MIDI FX and effect racks.
    All public "set" functions are message-thread only and lock-free with respect to the audio thread.
*/
class SynthEngine
{
public:
    static constexpr int kPhysicalVoices = kMaxVoices + 8; // spare voices let stolen notes fade out
    static constexpr int kChunk = 32;                      // control-rate block (base-rate samples)

    explicit SynthEngine (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate, int maxBlockSize);
    void release();
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TransportInfo& transport) noexcept;

    // ---------------------------------------------------------------- message-thread asset updates
    void setWavetable (int osc, std::unique_ptr<Wavetable> wt) { tables[osc].publish (std::move (wt)); }
    void setSample (int osc, std::unique_ptr<SampleAsset> s) { samples[osc].publish (std::move (s)); }
    void setLfoShape (int lfo, std::unique_ptr<LfoShapeTable> t) { lfoShapes[lfo].publish (std::move (t)); }
    void setModSlot (int slot, ModSrc src, int dstParamIndex) noexcept
    {
        modSrc[(size_t) slot].store ((int) src, std::memory_order_relaxed);
        modDst[(size_t) slot].store (dstParamIndex, std::memory_order_relaxed);
    }
    void setRackOrder (int rack, const std::array<int, FxRack::NumUnits>& order) noexcept { racks[rack].setOrder (order); }
    std::array<int, FxRack::NumUnits> getRackOrder (int rack) const noexcept { return racks[rack].getOrder(); }
    void playPreview (std::unique_ptr<PreviewClip> clip) { preview.publish (std::move (clip)); previewStart.store (true); }
    void stopPreview() noexcept { previewStop.store (true); }
    void collectGarbage();
    void panic() noexcept { panicRequest.store (true); }

    // ------------------------------------------------------------------------------ UI telemetry
    int getActiveVoiceCount() const noexcept { return uiActiveVoices.load (std::memory_order_relaxed); }
    float getOutputPeak() const noexcept { return uiPeak.load (std::memory_order_relaxed); }
    float getModSourceValueForUi (ModSrc s) const noexcept { return uiSrc[(size_t) s].load (std::memory_order_relaxed); }
    float getModulatedNormalised (int paramIndex) const noexcept;
    int getMidiFxStep() const noexcept { return midiFx.getCurrentStep(); }
    float getWtPosForUi (int osc) const noexcept { return uiWtPos[(size_t) osc].load (std::memory_order_relaxed); }
    float getLfoPhaseForUi (int lfo) const noexcept { return uiLfoPhase[(size_t) lfo].load (std::memory_order_relaxed); }
    double getSampleRate() const noexcept { return baseRate; }

private:
    void readParameters() noexcept;
    void buildModulation() noexcept;
    void handleMidi (const juce::MidiMessage& m) noexcept;
    void noteOn (int note, float vel) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff (bool hard) noexcept;
    void monoNoteOn (int note, float vel) noexcept;
    void monoNoteOff (int note) noexcept;
    int pickMonoNote() const noexcept;
    Voice* findFreeVoice() noexcept;
    void renderChunk (float* outL, float* outR, int n, const TransportInfo& t) noexcept;
    void updateGlobalModulation (double bpm) noexcept;
    float glideSourcePitch (bool anyHeldBefore) const noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::atomic<float>*> raw;
    std::vector<juce::NormalisableRange<float>> ranges;
    std::vector<float> base, baseNorm, values; // values = base + global modulation (FX / global dests)
    std::vector<uint8_t> modActive, isPerVoice;
    std::vector<int> voiceDests, globalDests;
    std::vector<float> globalAccum;
    ModSlot slots[kNumModSlots];
    int numSlots = 0;

    std::array<std::atomic<int>, kNumModSlots> modSrc, modDst;

    ObjectExchange<Wavetable> tables[kNumOsc];
    ObjectExchange<SampleAsset> samples[kNumOsc];
    ObjectExchange<LfoShapeTable> lfoShapes[kNumLfos];
    ObjectExchange<PreviewClip> preview;
    std::atomic<bool> previewStart { false }, previewStop { false }, panicRequest { false };
    double previewPos = -1.0;

    Voice voices[kPhysicalVoices];
    EngineContext ctx;
    Lfo globalLfo[kNumLfos];
    MidiFx midiFx;
    juce::MidiBuffer fxMidi;

    FxRack racks[kNumRacks];
    HalfbandDecimator decim[1 + kNumOsc][2];

    double baseRate = 48000.0;
    int maxBlock = 512;
    int currentOs = 1;
    uint32_t ageCounter = 1;
    uint32_t noteSeed = 0x1234567u;
    Rng rng;

    // MIDI state
    bool sustainDown = false;
    float modWheel = 0.0f, channelPressure = 0.0f;
    float bendTarget = 0.0f, bendSmoothed = 0.0f;
    bool keyDown[128] {};
    float keyVel[128] {};
    bool pendingMonoOff[128] {};
    int monoStack[128] {};
    int monoCount = 0;
    float lastPitch = -1.0f;
    int lastVoiceMode = -1;

    // buses (process rate, max 2x chunk)
    float busL[1 + kNumOsc][kChunk * 2] {}, busR[1 + kNumOsc][kChunk * 2] {};
    float outBufL[1 + kNumOsc][kChunk] {}, outBufR[1 + kNumOsc][kChunk] {};
    bool laneUsed[kNumOsc] {};
    bool laneWasActive[kNumOsc] {};

    std::atomic<int> uiActiveVoices { 0 };
    std::atomic<float> uiPeak { 0.0f };
    std::array<std::atomic<float>, (size_t) ModSrc::NumSources> uiSrc;
    std::array<std::atomic<float>, kNumOsc> uiWtPos;
    std::array<std::atomic<float>, kNumLfos> uiLfoPhase;
    std::vector<std::atomic<float>> uiModNorm;
    float lastVoiceSrc[(int) ModSrc::NumSources] {};
};
} // namespace dw

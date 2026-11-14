#pragma once

#include "Core/Params.h"
#include "DSP/SynthEngine.h"
#include "Presets/StateIO.h"
#include "Wave/WaveDocument.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace dw
{
class PresetManager;

/** One modulation-matrix routing (amount lives in the mod<n>_amt parameter). */
struct ModRouting
{
    ModSrc src = ModSrc::None;
    juce::String dstId; // parameter ID (stable)
    bool isUsed() const noexcept { return src != ModSrc::None && dstId.isNotEmpty(); }
};

class DeadwaveProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::Timer,
                          private juce::AsyncUpdater
{
public:
    DeadwaveProcessor();
    ~DeadwaveProcessor() override;

    // ------------------------------------------------------------------------- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DEADWAVE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return currentPresetName; }
    void changeProgramName (int, const juce::String& n) override { currentPresetName = n; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ----------------------------------------------------------------------------- full state
    /** Complete sound state: parameters + wavetables + samples + LFO shapes + matrix + FX order. */
    juce::ValueTree captureState() const;
    void applyState (const juce::ValueTree& state);
    /** Resets every parameter and asset to the INIT sound. */
    void resetToInit();

    // ------------------------------------------------------------------------------- documents
    WaveDocument& getWave (int osc) { return waves[osc]; }
    /** Call after editing a wave document (debounced rebuild of the band-limited table). */
    void waveChanged (int osc);
    std::shared_ptr<const SampleAsset> getSample (int osc) const;
    void setSample (int osc, std::unique_ptr<SampleAsset> s);
    void clearSample (int osc);

    const stateio::LfoPoints& getLfoPoints (int lfo) const { return lfoPoints[lfo]; }
    void setLfoPoints (int lfo, const stateio::LfoPoints& pts);

    // ------------------------------------------------------------------------------ modulation
    const ModRouting& getRouting (int slot) const { return routings[(size_t) slot]; }
    /** Adds (or updates) a routing; returns the slot or -1 if the matrix is full. */
    int addModulation (ModSrc src, const juce::String& dstId, float amount);
    void setRouting (int slot, ModSrc src, const juce::String& dstId);
    void clearRouting (int slot);
    /** Slots modulating the given parameter. */
    juce::Array<int> getRoutingsForParam (const juce::String& dstId) const;

    // ---------------------------------------------------------------------------------- misc
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    SynthEngine& getEngine() { return engine; }
    PresetManager& getPresetManager() { return *presets; }
    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }
    float getValue (int index) const { return params[(size_t) index]->convertFrom0to1 (params[(size_t) index]->getValue()); }
    void setValue (int index, float realValue);

    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }
    void setCurrentPresetName (const juce::String& n) { currentPresetName = n; sendChangeMessage(); }

    std::array<int, FxRack::NumUnits> getRackOrder (int rack) const { return rackOrders[(size_t) rack]; }
    void setRackOrder (int rack, const std::array<int, FxRack::NumUnits>& order);

    juce::ValueTree& getUiState() { return uiState; }
    uint64_t getStateSerial() const noexcept { return stateSerial; }

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void applyParameters (const juce::ValueTree& state);
    void applyAssets (const juce::ValueTree& state);
    void syncRoutingToEngine (int slot);
    void rebuildDirtyTables();

    juce::AudioProcessorValueTreeState apvts;
    SynthEngine engine;
    std::vector<juce::RangedAudioParameter*> params;

    WaveDocument waves[kNumOsc];
    uint64_t builtVersion[kNumOsc] {};
    std::shared_ptr<const SampleAsset> sampleAssets[kNumOsc];
    stateio::LfoPoints lfoPoints[kNumLfos];
    std::array<ModRouting, kNumModSlots> routings;
    std::array<std::array<int, FxRack::NumUnits>, kNumRacks> rackOrders;
    std::unique_ptr<PresetManager> presets;
    juce::ValueTree uiState { "UI" };
    juce::String currentPresetName { "Init" };
    uint64_t stateSerial = 1;

    /** Guards wave documents / assets against hosts that save or restore state off the message thread. */
    mutable juce::CriticalSection stateLock;
    juce::ValueTree pendingState; // restored off the message thread, applied asynchronously

    /** Background builder so long tables never stall the message thread. */
    class TableBuilder : public juce::Thread
    {
    public:
        explicit TableBuilder (DeadwaveProcessor& p) : juce::Thread ("DEADWAVE table builder"), owner (p) {}
        void request (int osc, wavegen::Frames frames, juce::String name);
        void run() override;
    private:
        DeadwaveProcessor& owner;
        juce::CriticalSection lock;
        struct Job { bool pending = false; wavegen::Frames frames; juce::String name; };
        Job jobs[kNumOsc];
        juce::WaitableEvent wake;
    };
    TableBuilder builder { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeadwaveProcessor)
};
} // namespace dw

// DEADNOISE - JUCE audio processor (VST3 wrapper around the DSP engine).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DeadnoiseEngine.h"

namespace ParamID
{
    inline constexpr const char* reduction  = "reduction";
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* voice      = "voice";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* gateOn     = "gateOn";
    inline constexpr const char* gateThresh = "gateThresh";
    inline constexpr const char* gateRange  = "gateRange";
    inline constexpr const char* inGain     = "inGain";
    inline constexpr const char* outGain    = "outGain";
    inline constexpr const char* monitor    = "monitor";
}

class DeadnoiseAudioProcessor : public juce::AudioProcessor
{
public:
    DeadnoiseAudioProcessor();
    ~DeadnoiseAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DEADNOISE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    deadnoise::DeadnoiseEngine& getEngine() noexcept { return engine; }

    void startLearning();
    void cancelLearning();
    void clearProfile();
    void resetToDefaults();

    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }
    int getCurrentLatency() const noexcept { return currentLatency.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    class Worker : public juce::Thread
    {
    public:
        explicit Worker (DeadnoiseAudioProcessor& p) : juce::Thread ("DEADNOISE worker"), owner (p) {}
        void run() override;
    private:
        DeadnoiseAudioProcessor& owner;
    };

    void updateEngineParams() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    deadnoise::DeadnoiseEngine engine;
    Worker worker { *this };

    // raw parameter pointers (lock-free reads on the audio thread)
    std::atomic<float>* pReduction = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pVoice = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pGateOn = nullptr;
    std::atomic<float>* pGateThresh = nullptr;
    std::atomic<float>* pGateRange = nullptr;
    std::atomic<float>* pInGain = nullptr;
    std::atomic<float>* pOutGain = nullptr;
    std::atomic<float>* pMonitor = nullptr;

    // latency-preserving bypass delay
    juce::AudioBuffer<float> bypassDelay;
    int bypassWrite = 0;

    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentLatency { 0 };
    int preparedChannels = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeadnoiseAudioProcessor)
};

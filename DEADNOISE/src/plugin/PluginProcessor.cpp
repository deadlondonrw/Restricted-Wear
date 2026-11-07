// DEADNOISE - JUCE audio processor.
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace deadnoise;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DeadnoiseAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };
    auto dbs = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::reduction, 1 }, "Noise Reduction",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Processing Mode",
        StringArray { "Natural", "Maximum Suppression" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::voice, 1 }, "Voice Preservation",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::mix, 1 }, "Wet/Dry",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (pct)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::gateOn, 1 }, "Noise Gate", false));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::gateThresh, 1 }, "Gate Threshold",
        NormalisableRange<float> (-90.0f, -20.0f, 0.1f), -55.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbs)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::gateRange, 1 }, "Gate Range",
        NormalisableRange<float> (0.0f, 80.0f, 0.1f), 30.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float v, int) { return "-" + String (v, 1) + " dB"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::inGain, 1 }, "Input Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbs)));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::outGain, 1 }, "Output Gain",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbs)));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::monitor, 1 }, "Monitoring",
        StringArray { "Processed", "Original", "Removed Noise" }, 0));

    return layout;
}

//==============================================================================
DeadnoiseAudioProcessor::DeadnoiseAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DEADNOISE", createLayout())
{
    pReduction  = apvts.getRawParameterValue (ParamID::reduction);
    pMode       = apvts.getRawParameterValue (ParamID::mode);
    pVoice      = apvts.getRawParameterValue (ParamID::voice);
    pMix        = apvts.getRawParameterValue (ParamID::mix);
    pGateOn     = apvts.getRawParameterValue (ParamID::gateOn);
    pGateThresh = apvts.getRawParameterValue (ParamID::gateThresh);
    pGateRange  = apvts.getRawParameterValue (ParamID::gateRange);
    pInGain     = apvts.getRawParameterValue (ParamID::inGain);
    pOutGain    = apvts.getRawParameterValue (ParamID::outGain);
    pMonitor    = apvts.getRawParameterValue (ParamID::monitor);

    // Prepare with sensible defaults so the editor and state restore work even
    // before the host calls prepareToPlay.
    engine.prepare (48000.0, 512, 2);
    currentLatency.store (engine.getLatencySamples());
    setLatencySamples (engine.getLatencySamples());

    worker.startThread (juce::Thread::Priority::normal);
}

DeadnoiseAudioProcessor::~DeadnoiseAudioProcessor()
{
    worker.stopThread (2000);
}

void DeadnoiseAudioProcessor::Worker::run()
{
    while (! threadShouldExit())
    {
        owner.engine.getDenoiser().serviceBackgroundTasks();
        wait (15);
    }
}

//==============================================================================
bool DeadnoiseAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void DeadnoiseAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jlimit (1, 2, getTotalNumInputChannels());
    preparedChannels = channels;

    updateEngineParams();
    engine.prepare (sampleRate, juce::jmax (samplesPerBlock, 64), channels);

    const int latency = engine.getLatencySamples();
    currentSampleRate.store (sampleRate);
    currentLatency.store (latency);
    setLatencySamples (latency);

    bypassDelay.setSize (juce::jmax (2, channels), latency + 1, false, true, false);
    bypassDelay.clear();
    bypassWrite = 0;
}

void DeadnoiseAudioProcessor::releaseResources() {}

void DeadnoiseAudioProcessor::updateEngineParams() noexcept
{
    EngineParams p;
    p.reduction         = pReduction->load() * 0.01f;
    p.maximumMode       = pMode->load() > 0.5f;
    p.voicePreservation = pVoice->load() * 0.01f;
    p.mix               = pMix->load() * 0.01f;
    p.gateEnabled       = pGateOn->load() > 0.5f;
    p.gateThresholdDb   = pGateThresh->load();
    p.gateRangeDb       = pGateRange->load();
    p.inputGainDb       = pInGain->load();
    p.outputGainDb      = pOutGain->load();
    p.monitor           = (MonitorMode) juce::jlimit (0, 2, (int) std::lround (pMonitor->load()));
    engine.setParams (p);
}

void DeadnoiseAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numSamples <= 0) return;

    updateEngineParams();
    const int nch = juce::jmin (juce::jmin (numIn, buffer.getNumChannels()), preparedChannels > 0 ? preparedChannels : 2);
    engine.process (buffer.getArrayOfWritePointers(), nch, numSamples);
}

void DeadnoiseAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Keep the reported latency while bypassed, so bypassing does not shift
    // the signal in time.
    const int numSamples = buffer.getNumSamples();
    const int latency = currentLatency.load();
    const int size = bypassDelay.getNumSamples();
    if (size != latency + 1) return;
    const int nch = juce::jmin (buffer.getNumChannels(), bypassDelay.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    int w = bypassWrite;
    for (int i = 0; i < numSamples; ++i)
    {
        int r = w - latency;
        if (r < 0) r += size;
        for (int ch = 0; ch < nch; ++ch)
        {
            float* d = bypassDelay.getWritePointer (ch);
            float* io = buffer.getWritePointer (ch);
            d[w] = io[i];
            io[i] = d[r];
        }
        if (++w >= size) w = 0;
    }
    bypassWrite = w;
}

//==============================================================================
void DeadnoiseAudioProcessor::startLearning()   { engine.getDenoiser().requestLearn(); }
void DeadnoiseAudioProcessor::cancelLearning()  { engine.getDenoiser().cancelLearn(); }
void DeadnoiseAudioProcessor::clearProfile()    { engine.getDenoiser().clearProfile(); }

void DeadnoiseAudioProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

//==============================================================================
void DeadnoiseAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;

    xml->setAttribute ("pluginVersion", DEADNOISE_VERSION_STRING);
    const NoiseProfile profile = engine.getDenoiser().getProfile();
    if (profile.isValid())
    {
        const auto bytes = profile.serialise();
        juce::MemoryBlock mb (bytes.data(), bytes.size());
        auto* child = xml->createNewChildElement ("NOISE_PROFILE");
        child->setAttribute ("data", mb.toBase64Encoding());
    }
    copyXmlToBinary (*xml, destData);
}

void DeadnoiseAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    NoiseProfile profile;
    bool haveProfile = false;
    if (auto* child = xml->getChildByName ("NOISE_PROFILE"))
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (child->getStringAttribute ("data")))
            haveProfile = NoiseProfile::deserialise (static_cast<const uint8_t*> (mb.getData()), mb.getSize(), profile);
        xml->removeChildElement (child, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (haveProfile) engine.getDenoiser().setProfile (profile);
    else             engine.getDenoiser().clearProfile();
}

//==============================================================================
juce::AudioProcessorEditor* DeadnoiseAudioProcessor::createEditor()
{
    return new DeadnoiseAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeadnoiseAudioProcessor();
}

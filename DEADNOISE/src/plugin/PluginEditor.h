// DEADNOISE - plugin editor.
#pragma once

#include "PluginProcessor.h"

namespace dnui
{
    namespace colours
    {
        const juce::Colour background { 0xff0a0a0b };
        const juce::Colour panel      { 0xff131315 };
        const juce::Colour panelHi    { 0xff1a1a1d };
        const juce::Colour border     { 0xff26262a };
        const juce::Colour track      { 0xff2c2c31 };
        const juce::Colour text       { 0xfff2f2f2 };
        const juce::Colour textDim    { 0xff8e8e93 };
        const juce::Colour textFaint  { 0xff5a5a60 };
        const juce::Colour accent     { 0xffd9d9dc };
        const juce::Colour warn       { 0xffd8a657 };
        const juce::Colour clip       { 0xffe5484d };
    }

    juce::Font uiFont (float height, bool bold = false, float tracking = 0.0f);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (12.0f, true, 0.08f); }
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };

    // Rotary knob with caption and value read-out
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
              const juce::String& caption, bool large = false);
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::Slider slider;
    private:
        juce::String caption;
        bool large;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Segmented selector bound to a choice parameter
    class Segmented : public juce::Component
    {
    public:
        Segmented (juce::RangedAudioParameter& param, juce::StringArray labels);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    private:
        juce::StringArray labels;
        int selected = 0;
        juce::ParameterAttachment attachment;
    };

    class LevelMeter : public juce::Component
    {
    public:
        explicit LevelMeter (juce::String captionText) : caption (std::move (captionText)) {}
        void pushPeak (float linearPeak, float dtSeconds);
        void paint (juce::Graphics&) override;
    private:
        juce::String caption;
        float levelDb = -100.0f, holdDb = -100.0f, holdTime = 0.0f;
        bool clipped = false;
    };

    class SpectrumView : public juce::Component
    {
    public:
        explicit SpectrumView (deadnoise::SpectralDenoiser& d) : denoiser (d) {}
        void refresh();
        void paint (juce::Graphics&) override;
    private:
        deadnoise::SpectralDenoiser& denoiser;
        float in[deadnoise::SpectralDenoiser::kDisplayPoints] {};
        float out[deadnoise::SpectralDenoiser::kDisplayPoints] {};
        float nz[deadnoise::SpectralDenoiser::kDisplayPoints] {};
        bool valid = false;
    };
}

class DeadnoiseAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit DeadnoiseAudioProcessorEditor (DeadnoiseAudioProcessor&);
    ~DeadnoiseAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateLearnStatus();
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;

    DeadnoiseAudioProcessor& proc;
    dnui::LookAndFeel lnf;

    dnui::SpectrumView spectrum;
    dnui::Knob reduction, voice, mix, inGain, outGain, gateThresh, gateRange;
    dnui::Segmented mode, monitor;
    juce::TextButton learnButton { "LEARN BACKGROUND" }, clearButton { "CLEAR" }, resetButton { "RESET" };
    juce::ToggleButton gateToggle { "NOISE GATE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateAttachment;
    dnui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };

    juce::String profileStatus, profileDetail, learnMessage;
    juce::Colour profileColour = dnui::colours::textDim;
    float learnProgress = 0.0f;
    bool learning = false;
    float reductionDb = 0.0f, speechActivity = 0.0f;
    bool gateOpen = true;
    double lastTimerMs = 0.0;

    juce::Rectangle<int> headerArea, spectrumArea, learnArea, reductionArea, voiceArea, gateArea, monitorArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeadnoiseAudioProcessorEditor)
};

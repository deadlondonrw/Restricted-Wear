#pragma once

#include "../Presets/SoundGenerator.h"
#include "Widgets.h"

namespace dw::ui
{
class GeneratorView : public juce::Component
{
public:
    explicit GeneratorView (DeadwaveProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void apply (const juce::ValueTree& state, const juce::String& name);
    void fromText();
    void fromParams();
    void randomize (bool full);
    GenTraits traitsFromSliders() const;

    DeadwaveProcessor& proc;
    Panel textPanel { "DESCRIBE A SOUND" }, paramPanel { "DESIGN BY PARAMETERS" }, randPanel { "RANDOMIZE" };

    juce::TextEditor prompt;
    FlatButton textGo { "GENERATE FROM TEXT" };
    juce::Label understood, textNote;

    juce::ComboBox category;
    struct TraitSlider { juce::String name; juce::Slider slider; std::unique_ptr<Caption> caption; };
    std::vector<std::unique_ptr<TraitSlider>> traits;
    juce::ToggleButton glide { "GLIDE" }, acid { "ACID" }, chip { "CHIP" }, vocal { "VOCAL" }, metallic { "METALLIC" };
    FlatButton paramGo { "GENERATE PRESET" };

    juce::Slider intensity;
    Caption intensityCap { "INTENSITY", Colours::muted, 10.0f }, lockCap { "LOCKED GROUPS ARE KEPT", Colours::muted, 10.0f };
    std::vector<std::unique_ptr<juce::ToggleButton>> locks;
    FlatButton randPartial { "RANDOMIZE (KEEP CHARACTER)" }, randFull { "NEW RANDOM SOUND" };
    FlatButton undo { "UNDO LAST GENERATION" };
    juce::ValueTree previous;
    uint32_t seed = 0x5EED1234u;
};
} // namespace dw::ui

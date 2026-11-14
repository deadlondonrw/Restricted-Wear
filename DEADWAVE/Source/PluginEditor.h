#pragma once

#include "PluginProcessor.h"
#include "GUI/LookAndFeel.h"

namespace dw
{
class MainView;

class DeadwaveEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeadwaveEditor (DeadwaveProcessor&);
    ~DeadwaveEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DeadwaveProcessor& processor;
    std::unique_ptr<ui::DeadLookAndFeel> lookAndFeel;
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    bool constructed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeadwaveEditor)
};
} // namespace dw

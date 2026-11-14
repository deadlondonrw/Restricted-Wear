#include "PluginEditor.h"

namespace dw
{
// Temporary editor used until the full DEADWAVE interface is wired in.
DeadwaveEditor::DeadwaveEditor (DeadwaveProcessor& p) : juce::AudioProcessorEditor (p), processor (p)
{
    content = std::make_unique<juce::GenericAudioProcessorEditor> (p);
    addAndMakeVisible (*content);
    setSize (900, 600);
}

DeadwaveEditor::~DeadwaveEditor() = default;

void DeadwaveEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0a0a0b)); }

void DeadwaveEditor::resized()
{
    if (content) content->setBounds (getLocalBounds());
}
} // namespace dw

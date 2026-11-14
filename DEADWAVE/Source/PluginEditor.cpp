#include "PluginEditor.h"
#include "GUI/MainView.h"

namespace dw
{
DeadwaveEditor::DeadwaveEditor (DeadwaveProcessor& p) : juce::AudioProcessorEditor (p), processor (p)
{
    lookAndFeel = std::make_unique<ui::DeadLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());
    juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

    auto view = std::make_unique<ui::MainView> (p);
    view->setSize (ui::Metrics::baseWidth, ui::Metrics::baseHeight);
    addAndMakeVisible (*view);
    content = std::move (view);
    tooltips = std::make_unique<juce::TooltipWindow> (this, 600);

    const float scale = juce::jlimit (0.6f, 2.0f, (float) processor.getUiState().getProperty ("scale", 1.0f));
    setResizable (true, true);
    setResizeLimits (ui::Metrics::baseWidth * 6 / 10, ui::Metrics::baseHeight * 6 / 10, ui::Metrics::baseWidth * 2, ui::Metrics::baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) ui::Metrics::baseWidth / (double) ui::Metrics::baseHeight);
    setSize (juce::roundToInt (ui::Metrics::baseWidth * scale), juce::roundToInt (ui::Metrics::baseHeight * scale));
    constructed = true;
}

DeadwaveEditor::~DeadwaveEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
    content.reset();
}

void DeadwaveEditor::paint (juce::Graphics& g) { g.fillAll (ui::Colours::bg); }

void DeadwaveEditor::resized()
{
    const float scale = (float) getWidth() / (float) ui::Metrics::baseWidth;
    if (content)
    {
        content->setTransform (juce::AffineTransform::scale (scale));
        content->setBounds (0, 0, ui::Metrics::baseWidth, ui::Metrics::baseHeight);
    }
    if (constructed)
        processor.getUiState().setProperty ("scale", scale, nullptr);
}
} // namespace dw

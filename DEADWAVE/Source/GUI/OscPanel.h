#pragma once

#include "Widgets.h"

namespace dw::ui
{
/** Live oscillator preview: draws the actual output cycle (WT position, bias, amplitude, phase, shape). */
class WaveThumb : public juce::Component, public juce::SettableTooltipClient
{
public:
    WaveThumb (DeadwaveProcessor& p, int osc);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void refresh();
    std::function<void()> onOpen;
    bool large = false;

private:
    DeadwaveProcessor& proc;
    int osc;
    uint64_t lastVersion = 0;
    float lastKey[8] {};
    bool hover = false;
};

class OscCard : public Panel
{
public:
    OscCard (DeadwaveProcessor& p, int osc);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    std::function<void (int)> onOpenEditor;
    std::vector<ParamKnob*> getKnobs();

private:
    DeadwaveProcessor& proc;
    int osc;
    ParamToggle onToggle;
    Segmented type;
    WaveThumb thumb;
    ParamKnob wtpos, bias, amp, phase, shape;
    ParamChoice warp, route;
    ParamKnob level, pan, unison, detune, spread;
    ParamKnob oct, semi, fine, pitch, blend, rand, fm;
    ParamChoice fmSrc, driveMode;
    ParamKnob ratio, drive, crush;
    ParamToggle fxOn, fxLink;
    Caption warpCap { "SHAPE MODE", Colours::muted, 9.5f }, routeCap { "ROUTE", Colours::muted, 9.5f },
            fmCap { "FM SOURCE", Colours::muted, 9.5f }, drvCap { "DRV MODE", Colours::muted, 9.5f };
};
} // namespace dw::ui

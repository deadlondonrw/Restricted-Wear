#pragma once

#include "Widgets.h"

namespace dw::ui
{
class FxUnitPanel : public Panel
{
public:
    FxUnitPanel (DeadwaveProcessor& p, int paramRack, int unit, int position, int count);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    std::function<void (int unit, int delta)> onMove;

private:
    DeadwaveProcessor& proc;
    int rack, unit;
    std::unique_ptr<ParamToggle> on;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
    std::vector<std::unique_ptr<juce::Component>> extras; // choices / toggles
    FlatButton upBtn { "<", true }, downBtn { ">", true };
};

class FxView : public juce::Component
{
public:
    explicit FxView (DeadwaveProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    void showRack (int rack);

private:
    void rebuild();
    int paramRackFor (int rack) const;

    DeadwaveProcessor& proc;
    int rack = 0;
    int builtParamRack = -1;
    Segmented rackSel { { "MASTER", "OSC 1 LANE", "OSC 2 LANE", "OSC 3 LANE", "OSC 4 LANE" } };
    std::unique_ptr<ParamToggle> rackOn, laneOn, laneLink;
    std::unique_ptr<ParamChoice> laneOutput;
    Caption info { "", Colours::dim, 11.0f };
    Caption outCap { "OSC LANES OUTPUT", Colours::muted, 9.5f };
    std::vector<std::unique_ptr<FxUnitPanel>> units;
};
} // namespace dw::ui

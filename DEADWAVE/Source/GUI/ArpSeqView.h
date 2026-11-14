#pragma once

#include "Widgets.h"

namespace dw::ui
{
/** 16-step grid: click toggles steps, drag bars to edit pitch / velocity / gate. */
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (DeadwaveProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    enum Lane { Pitch, Velocity, Gate, NumLanes };

private:
    juce::Rectangle<float> laneArea (int lane) const;
    juce::Rectangle<float> toggleArea() const;
    void edit (juce::Point<float> p, bool begin);
    DeadwaveProcessor& proc;
    int playing = -1, dragLane = -1, lastStep = -1;
    float key = 0.0f;
};

class ArpSeqView : public juce::Component
{
public:
    explicit ArpSeqView (DeadwaveProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    DeadwaveProcessor& proc;
    Segmented mode;
    Panel arpPanel { "ARPEGGIATOR" }, seqPanel { "STEP SEQUENCER" };
    ParamChoice arpMode, arpRate, arpVelMode, seqRate;
    ParamKnob arpOct, arpGate, arpSwing, arpVel, arpSteps, arpTrans, seqSwing, seqLength, seqGate;
    ParamToggle arpHold;
    Caption c1 { "MODE", Colours::muted, 9.5f }, c2 { "RATE", Colours::muted, 9.5f }, c3 { "VELOCITY", Colours::muted, 9.5f },
            c4 { "RATE", Colours::muted, 9.5f };
    StepGrid grid;
    juce::Label help;
};
} // namespace dw::ui

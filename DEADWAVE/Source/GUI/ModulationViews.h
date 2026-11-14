#pragma once

#include "Widgets.h"

namespace dw::ui
{
/** Filter magnitude response preview. */
class FilterResponse : public juce::Component
{
public:
    FilterResponse (DeadwaveProcessor& p, int filter) : proc (p), f (filter) {}
    void paint (juce::Graphics&) override;
    void refresh();
private:
    DeadwaveProcessor& proc;
    int f;
    float key[6] {};
};

class FilterPanel : public Panel
{
public:
    FilterPanel (DeadwaveProcessor& p, int filter);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
private:
    DeadwaveProcessor& proc;
    int f;
    ParamToggle on;
    ParamChoice type;
    FilterResponse response;
    ParamKnob cutoff, reso, drive, mix, key, env, spread;
    std::unique_ptr<Segmented> routing;
};

/** DAHDSR curve display (reads the parameters live). */
class EnvGraph : public juce::Component
{
public:
    EnvGraph (DeadwaveProcessor& p) : proc (p) {}
    void setEnv (int e) { env = e; repaint(); }
    void paint (juce::Graphics&) override;
    void refresh();
private:
    DeadwaveProcessor& proc;
    int env = 0;
    float key[10] {};
};

class EnvPanel : public Panel
{
public:
    EnvPanel (DeadwaveProcessor& p, bool large);
    void resized() override;
    void refresh();
private:
    void select (int e);
    DeadwaveProcessor& proc;
    bool large;
    int current = 0;
    Segmented tabs { { "ENV 1", "ENV 2", "ENV 3", "ENV 4" } };
    EnvGraph graph;
    std::vector<std::unique_ptr<ParamKnob>> knobs[kNumEnvs];
};

/** Editable custom LFO shape (points + curvature) with playhead. */
class LfoShapeEditor : public juce::Component
{
public:
    LfoShapeEditor (DeadwaveProcessor& p) : proc (p) {}
    void setLfo (int l) { lfo = l; selected = -1; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh();
    bool editable = true;
    int gridDivisions = 8;
private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f); }
    int hitPoint (juce::Point<float> p) const;
    void commit (stateio::LfoPoints pts);
    DeadwaveProcessor& proc;
    int lfo = 0, selected = -1, curveSeg = -1;
    float curveStart = 0.0f, dragStartY = 0.0f;
    float lastPhase = -1.0f;
    uint64_t lastSerial = 0;
};

class LfoPanel : public Panel
{
public:
    LfoPanel (DeadwaveProcessor& p, bool large);
    void resized() override;
    void refresh();
private:
    void select (int l);
    DeadwaveProcessor& proc;
    bool large;
    int current = 0;
    Segmented tabs { { "LFO 1", "LFO 2", "LFO 3", "LFO 4" } };
    LfoShapeEditor editor;
    struct Set
    {
        std::unique_ptr<ParamChoice> shape, div;
        std::unique_ptr<Segmented> mode;
        std::unique_ptr<ParamToggle> sync, bipolar;
        std::unique_ptr<ParamKnob> rate, phase, smooth, rise;
    };
    Set sets[kNumLfos];
    std::unique_ptr<FlatButton> presetButton;
};

class VoicePanel : public Panel
{
public:
    explicit VoicePanel (DeadwaveProcessor& p);
    void resized() override;
    void refresh();
private:
    DeadwaveProcessor& proc;
    Segmented mode, glide;
    ParamKnob voices, time, curve, bendUp, bendDown, vel;
    ParamChoice priority;
    ParamToggle constRate, hard, ret;
    Caption prioCap { "PRIORITY", Colours::muted, 9.5f };
};

class MacroPanel : public Panel
{
public:
    explicit MacroPanel (DeadwaveProcessor& p);
    void resized() override;
    void refresh();
private:
    std::vector<std::unique_ptr<ParamKnob>> knobs;
    std::vector<std::unique_ptr<ModSourceChip>> chips;
};

class AnalogPanel : public Panel
{
public:
    explicit AnalogPanel (DeadwaveProcessor& p);
    void resized() override;
    void refresh();
private:
    std::vector<std::unique_ptr<ParamKnob>> knobs;
};

/** 32-slot modulation matrix. */
class MatrixView : public juce::Component, private juce::ChangeListener
{
public:
    explicit MatrixView (DeadwaveProcessor& p);
    ~MatrixView() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { rebuild(); }
    void rebuild();
    struct Row : public juce::Component
    {
        Row (DeadwaveProcessor& p, int slot);
        void resized() override;
        void paint (juce::Graphics&) override;
        void update();
        DeadwaveProcessor& proc;
        int slot;
        juce::ComboBox src;
        FlatButton dst { "-" };
        juce::Slider amount;
        std::unique_ptr<juce::SliderParameterAttachment> att;
        FlatButton remove { "X", true };
    };
    DeadwaveProcessor& proc;
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<Row>> rows;
};

/** Full MOD page: large LFO and envelope editors, macros and analog character. */
class ModPage : public juce::Component
{
public:
    explicit ModPage (DeadwaveProcessor& p);
    void resized() override;
    void refresh();
private:
    LfoPanel lfo;
    EnvPanel env;
    MacroPanel macros;
    AnalogPanel analog;
};

/** Opens a hierarchical destination menu; calls back with a parameter ID. */
void showDestinationMenu (juce::Component* target, std::function<void (const juce::String&)> onPick);
} // namespace dw::ui

#pragma once

#include "ArpSeqView.h"
#include "BrowserView.h"
#include "FxView.h"
#include "GeneratorView.h"
#include "ModulationViews.h"
#include "OscPanel.h"
#include "WaveEditor.h"

namespace dw::ui
{
/** DEADNOISE-style status chip: dot + tracked label + value. */
class StatusChip : public juce::Component
{
public:
    StatusChip (juce::String l) : label (std::move (l)) {}
    void paint (juce::Graphics&) override;
    void set (const juce::String& v, bool active)
    {
        if (v != value || active != on) { value = v; on = active; repaint(); }
    }
private:
    juce::String label, value;
    bool on = false;
};

class OscPage : public juce::Component
{
public:
    explicit OscPage (DeadwaveProcessor& p);
    void resized() override;
    void refresh();
    std::function<void (int)> onOpenEditor;
private:
    std::vector<std::unique_ptr<OscCard>> cards;
    FilterPanel f1, f2;
    EnvPanel env;
    LfoPanel lfo;
    VoicePanel voice;
};

class MainView : public juce::Component,
                 public juce::DragAndDropContainer,
                 private juce::Timer,
                 private juce::ChangeListener
{
public:
    explicit MainView (DeadwaveProcessor& p);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int page);
    void openWaveEditor (int osc);
    int getPage() const noexcept { return page; }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showBrowser (bool show);
    void stepPreset (int delta);
    juce::Rectangle<int> pageArea() const;

    DeadwaveProcessor& proc;
    int page = 0;

    // header
    FlatButton prevBtn { "<" }, nextBtn { ">" }, presetName { "INIT" }, saveBtn { "SAVE" }, browseBtn { "BROWSE" }, menuBtn { "..." };
    StatusChip voicesChip { "VOICES" }, outChip { "OUTPUT" };
    Segmented quality;
    ParamKnob master;

    Segmented tabs { { "OSCILLATORS", "MODULATION", "MATRIX", "EFFECTS", "ARP / SEQ", "GENERATE" } };

    OscPage oscPage;
    ModPage modPage;
    MatrixView matrixPage;
    FxView fxPage;
    ArpSeqView arpPage;
    GeneratorView genPage;
    WaveEditor editor;
    BrowserView browser;

    std::vector<std::unique_ptr<ModSourceChip>> chips;
    int tick = 0;
};
} // namespace dw::ui

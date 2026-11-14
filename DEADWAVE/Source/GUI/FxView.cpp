#include "FxView.h"
#include "../DSP/FxRack.h"

namespace dw::ui
{
namespace
{
struct UnitDef
{
    std::vector<int> knobs;
    std::vector<int> choices;
    std::vector<int> toggles;
};

UnitDef unitDef (int unit)
{
    switch (unit)
    {
        case FxRack::Distortion: return { { FxP::DistDrive, FxP::DistTone, FxP::DistMix }, { FxP::DistMode }, {} };
        case FxRack::Saturation: return { { FxP::SatDrive, FxP::SatBias, FxP::SatTone, FxP::SatMix }, {}, {} };
        case FxRack::Chorus: return { { FxP::ChoRate, FxP::ChoDepth, FxP::ChoDelay, FxP::ChoFeedback, FxP::ChoWidth, FxP::ChoMix }, {}, {} };
        case FxRack::Phaser: return { { FxP::PhaRate, FxP::PhaDepth, FxP::PhaFreq, FxP::PhaFeedback, FxP::PhaMix }, { FxP::PhaStages }, {} };
        case FxRack::Flanger: return { { FxP::FlaRate, FxP::FlaDepth, FxP::FlaDelay, FxP::FlaFeedback, FxP::FlaMix }, {}, {} };
        case FxRack::Delay: return { { FxP::DlyTime, FxP::DlyStereo, FxP::DlyFeedback, FxP::DlyLowCut, FxP::DlyHighCut, FxP::DlyMix }, { FxP::DlyDiv }, { FxP::DlySync, FxP::DlyPingPong } };
        case FxRack::Reverb: return { { FxP::RevSize, FxP::RevDecay, FxP::RevDamp, FxP::RevPredelay, FxP::RevWidth, FxP::RevMod, FxP::RevMix }, {}, {} };
        case FxRack::Compressor: return { { FxP::CmpThresh, FxP::CmpRatio, FxP::CmpAttack, FxP::CmpRelease, FxP::CmpKnee, FxP::CmpMakeup, FxP::CmpMix }, {}, {} };
        case FxRack::Eq: return { { FxP::EqLowCut, FxP::EqLowFreq, FxP::EqLowGain, FxP::EqMidFreq, FxP::EqMidGain, FxP::EqMidQ, FxP::EqHighFreq, FxP::EqHighGain, FxP::EqHighCut }, {}, {} };
        case FxRack::Imager: return { { FxP::ImgWidth, FxP::ImgBassMono, FxP::ImgPan }, {}, {} };
        case FxRack::Bitcrusher: return { { FxP::CrsBits, FxP::CrsRate, FxP::CrsMix }, {}, {} };
        default: return {};
    }
}
} // namespace

// ======================================================================================== unit panel
FxUnitPanel::FxUnitPanel (DeadwaveProcessor& p, int paramRack, int u, int position, int count)
    : proc (p), rack (paramRack), unit (u)
{
    on = std::make_unique<ParamToggle> (p, P::fx (rack, FxRack::unitOnParam (u)));
    addAndMakeVisible (*on);
    const auto def = unitDef (u);
    for (int k : def.knobs)
    {
        knobs.push_back (std::make_unique<ParamKnob> (p, P::fx (rack, k)));
        knobs.back()->setCompact (def.knobs.size() > 5);
        addAndMakeVisible (*knobs.back());
    }
    for (int c : def.choices)
    {
        extras.push_back (std::make_unique<ParamChoice> (p, P::fx (rack, c)));
        addAndMakeVisible (*extras.back());
    }
    for (int t : def.toggles)
    {
        extras.push_back (std::make_unique<ParamToggle> (p, P::fx (rack, t), Registry::get().spec (P::fx (rack, t)).shortName));
        addAndMakeVisible (*extras.back());
    }
    upBtn.setEnabled (position > 0);
    downBtn.setEnabled (position < count - 1);
    upBtn.setTooltip ("Move earlier in the chain");
    downBtn.setTooltip ("Move later in the chain");
    upBtn.onClick = [this] { if (onMove) onMove (unit, -1); };
    downBtn.onClick = [this] { if (onMove) onMove (unit, 1); };
    addAndMakeVisible (upBtn);
    addAndMakeVisible (downBtn);
}

void FxUnitPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const bool active = proc.getValue (P::fx (rack, FxRack::unitOnParam (unit))) >= 0.5f;
    drawTracked (g, FxRack::unitNames()[unit], juce::Rectangle<float> (50.0f, 8.0f, 160.0f, 18.0f), font (11.5f, true),
                 active ? Colours::text : Colours::secondary, juce::Justification::centredLeft, 0.14f);
}

void FxUnitPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto head = r.removeFromTop (20);
    on->setBounds (head.removeFromLeft (36));
    downBtn.setBounds (head.removeFromRight (22));
    upBtn.setBounds (head.removeFromRight (22));
    head.removeFromRight (6);
    int ex = 0;
    for (auto& e : extras)
    {
        if (dynamic_cast<ParamChoice*> (e.get()) != nullptr)
        {
            e->setBounds (head.removeFromRight (86).reduced (0, 0));
            head.removeFromRight (4);
        }
        ++ex;
    }
    r.removeFromTop (4);
    auto toggleRow = r;
    bool hasToggles = false;
    for (auto& e : extras) if (dynamic_cast<ParamToggle*> (e.get()) != nullptr) hasToggles = true;
    if (hasToggles)
    {
        toggleRow = r.removeFromBottom (20);
        for (auto& e : extras)
            if (dynamic_cast<ParamToggle*> (e.get()) != nullptr)
                e->setBounds (toggleRow.removeFromLeft (90));
    }
    const int n = (int) knobs.size();
    const int perRow = n > 5 ? (n + 1) / 2 : n;
    const int rows = (n + perRow - 1) / perRow;
    const int rowH = juce::jmin (rows == 1 ? 74 : 60, r.getHeight() / juce::jmax (1, rows));
    r = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), rowH * rows));
    for (int row = 0; row < rows; ++row)
    {
        auto rr = r.removeFromTop (rowH);
        const int count = juce::jmin (perRow, n - row * perRow);
        const int w = juce::jmin (rows == 1 ? 64 : 56, rr.getWidth() / juce::jmax (1, perRow));
        for (int i = 0; i < count; ++i)
            knobs[(size_t) (row * perRow + i)]->setBounds (rr.getX() + i * (rr.getWidth() / perRow) + (rr.getWidth() / perRow - w) / 2, rr.getY(), w, rowH);
    }
}

void FxUnitPanel::refresh()
{
    const bool active = proc.getValue (P::fx (rack, FxRack::unitOnParam (unit))) >= 0.5f;
    for (auto& k : knobs)
    {
        k->setEnabled (active);
        k->refreshModulation();
    }
    if (unit == FxRack::Delay && knobs.size() > 0)
    {
        const bool sync = proc.getValue (P::fx (rack, FxP::DlySync)) >= 0.5f;
        knobs[0]->setEnabled (active && ! sync);
        for (auto& e : extras)
            if (dynamic_cast<ParamChoice*> (e.get()) != nullptr) e->setEnabled (active && sync);
    }
    repaint (0, 0, getWidth(), 28);
}

// =========================================================================================== FX view
FxView::FxView (DeadwaveProcessor& p) : proc (p)
{
    addAndMakeVisible (rackSel);
    addAndMakeVisible (info);
    addAndMakeVisible (outCap);
    rackSel.onChange = [this] (int i) { showRack (i); };
    showRack (0);
}

int FxView::paramRackFor (int r) const
{
    if (r == 0) return 0;
    const int o = r - 1;
    if (proc.getValue (P::osc (o, OscP::FxLink)) >= 0.5f)
        for (int j = 0; j < o; ++j)
            if (proc.getValue (P::osc (j, OscP::FxLink)) >= 0.5f) return 1 + j;
    return r;
}

void FxView::showRack (int r)
{
    rack = r;
    rackSel.setSelected (r, false);
    rackOn.reset(); laneOn.reset(); laneLink.reset(); laneOutput.reset();
    if (r == 0)
    {
        rackOn = std::make_unique<ParamToggle> (proc, P::fx (0, FxP::RackOn), "MASTER FX");
        laneOutput = std::make_unique<ParamChoice> (proc, P::global (GlobalP::VoiceFxRouting));
        addAndMakeVisible (*rackOn);
        addAndMakeVisible (*laneOutput);
        outCap.setVisible (true);
    }
    else
    {
        laneOn = std::make_unique<ParamToggle> (proc, P::osc (r - 1, OscP::FxOn), "LANE ON");
        laneLink = std::make_unique<ParamToggle> (proc, P::osc (r - 1, OscP::FxLink), "LINK FX");
        laneOn->setTooltip ("Route this oscillator through its own FX lane (after the filters and amp envelope)");
        laneLink->setTooltip ("Linked oscillators share the FX settings of the lowest-numbered linked oscillator");
        addAndMakeVisible (*laneOn);
        addAndMakeVisible (*laneLink);
        outCap.setVisible (false);
    }
    builtParamRack = -1;
    rebuild();
    resized();
}

void FxView::rebuild()
{
    const int pr = paramRackFor (rack);
    if (pr == builtParamRack && ! units.empty()) return;
    builtParamRack = pr;
    for (auto& u : units) removeChildComponent (u.get());
    units.clear();
    const auto order = proc.getRackOrder (rack);
    for (int k = 0; k < FxRack::NumUnits; ++k)
    {
        auto panel = std::make_unique<FxUnitPanel> (proc, pr, order[(size_t) k], k, FxRack::NumUnits);
        panel->onMove = [this] (int unit, int delta)
        {
            auto o = proc.getRackOrder (rack);
            int pos = 0;
            for (int i = 0; i < FxRack::NumUnits; ++i) if (o[(size_t) i] == unit) pos = i;
            const int np = juce::jlimit (0, FxRack::NumUnits - 1, pos + delta);
            std::swap (o[(size_t) pos], o[(size_t) np]);
            proc.setRackOrder (rack, o);
            // Linked lanes process with their own order but the shared parameters.
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<FxView> (this)]
            {
                if (safe != nullptr) { safe->builtParamRack = -1; safe->rebuild(); safe->resized(); }
            });
        };
        addAndMakeVisible (*panel);
        units.push_back (std::move (panel));
    }

    juce::String text;
    if (rack == 0)
        text = "Signal flow: voices > MASTER chain (left to right, top to bottom). Oscillators with a lane enabled are processed in their lane first.";
    else if (pr != rack)
        text = "LINKED: this lane uses the settings of OSC " + juce::String (pr) + " LANE. Editing here edits the shared settings.";
    else
        text = "Lane for OSC " + juce::String (rack) + ": per-oscillator processing after the voice filters and amp envelope.";
    info.setText (text);
}

void FxView::paint (juce::Graphics&) {}

void FxView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (28);
    rackSel.setBounds (top.removeFromLeft (560).reduced (0, 2));
    top.removeFromLeft (16);
    if (rackOn) rackOn->setBounds (top.removeFromLeft (130));
    if (laneOn) laneOn->setBounds (top.removeFromLeft (110));
    if (laneLink) laneLink->setBounds (top.removeFromLeft (110));
    if (laneOutput)
    {
        auto c = top.removeFromRight (150);
        laneOutput->setBounds (c.removeFromBottom (22));
        outCap.setBounds (top.removeFromRight (120).withTrimmedTop (6));
    }
    info.setBounds (r.removeFromTop (22).withTrimmedTop (4));
    r.removeFromTop (6);
    const int cols = 3;
    const int rows = (FxRack::NumUnits + cols - 1) / cols;
    const int gap = Metrics::gap;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols;
    const int h = (r.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < (int) units.size(); ++i)
    {
        const int c = i % cols, row = i / cols;
        units[(size_t) i]->setBounds (r.getX() + c * (w + gap), r.getY() + row * (h + gap), w, h);
    }
}

void FxView::refresh()
{
    if (paramRackFor (rack) != builtParamRack)
    {
        rebuild();
        resized();
    }
    for (auto& u : units) u->refresh();
}
} // namespace dw::ui

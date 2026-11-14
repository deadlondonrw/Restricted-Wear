#include "MainView.h"

namespace dw::ui
{
// ===================================================================================== status chip
void StatusChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    g.setColour (on ? Colours::text : Colours::muted);
    g.fillEllipse (12.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
    drawTracked (g, label, { 26.0f, 0.0f, 70.0f, r.getHeight() }, font (10.0f, true), Colours::muted, juce::Justification::centredLeft, 0.12f);
    g.setColour (Colours::text);
    g.setFont (font (12.0f, true));
    g.drawText (value, r.withTrimmedLeft (90.0f).withTrimmedRight (10.0f), juce::Justification::centredLeft, false);
}

// ========================================================================================= osc page
OscPage::OscPage (DeadwaveProcessor& p) : f1 (p, 0), f2 (p, 1), env (p, false), lfo (p, false), voice (p)
{
    for (int i = 0; i < kNumOsc; ++i)
    {
        cards.push_back (std::make_unique<OscCard> (p, i));
        cards.back()->onOpenEditor = [this] (int o) { if (onOpenEditor) onOpenEditor (o); };
        addAndMakeVisible (*cards.back());
    }
    for (juce::Component* c : std::initializer_list<juce::Component*> { &f1, &f2, &env, &lfo, &voice })
        addAndMakeVisible (c);
}

void OscPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (408);
    r.removeFromTop (Metrics::gap);
    const int gap = Metrics::gap;
    const int w = (top.getWidth() - gap * 3) / 4;
    for (int i = 0; i < kNumOsc; ++i)
        cards[(size_t) i]->setBounds (top.getX() + i * (w + gap), top.getY(), w, top.getHeight());

    // bottom row: filter, filter, envelopes, LFO, voice
    const int fw = 228, ew = 262, vw = 236;
    f1.setBounds (r.removeFromLeft (fw));
    r.removeFromLeft (gap);
    f2.setBounds (r.removeFromLeft (fw));
    r.removeFromLeft (gap);
    voice.setBounds (r.removeFromRight (vw));
    r.removeFromRight (gap);
    env.setBounds (r.removeFromLeft (ew));
    r.removeFromLeft (gap);
    lfo.setBounds (r);
}

void OscPage::refresh()
{
    for (auto& c : cards) c->refresh();
    f1.refresh();
    f2.refresh();
    env.refresh();
    lfo.refresh();
    voice.refresh();
}

// ======================================================================================== main view
MainView::MainView (DeadwaveProcessor& p)
    : proc (p), quality (p, P::global (GlobalP::Quality), { "STANDARD", "ULTRA" }), master (p, P::global (GlobalP::MasterVol)),
      oscPage (p), modPage (p), matrixPage (p), fxPage (p), arpPage (p), genPage (p), editor (p), browser (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevBtn, &nextBtn, &presetName, &saveBtn, &browseBtn, &menuBtn,
                                                                        &voicesChip, &outChip, &quality, &master, &tabs })
        addAndMakeVisible (c);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &oscPage, &modPage, &matrixPage, &fxPage, &arpPage, &genPage, &editor, &browser })
        addChildComponent (c);

    master.setCompact (true);
    quality.fontHeight = 9.5f;
    quality.setTooltip ("ULTRA renders voices at 2x with band-limited decimation: cleaner sync, FM, drive and filters at higher CPU");
    presetName.getProperties().set ("fontHeight", 12.5f);
    presetName.setTooltip ("Open the preset browser");
    presetName.onClick = [this] { showBrowser (true); };
    browseBtn.onClick = [this] { showBrowser (! browser.isVisible()); };
    saveBtn.onClick = [this] { browser.saveAs(); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    menuBtn.setTooltip ("More");
    menuBtn.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Initialise sound");
        m.addItem (2, "Panic (all notes off)");
        m.addSeparator();
        juce::PopupMenu size;
        for (int pct : { 75, 90, 100, 125, 150, 175, 200 }) size.addItem (100 + pct, juce::String (pct) + "%");
        m.addSubMenu ("Interface size", size);
        m.addItem (3, "Open user folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn), [this] (int r)
        {
            if (r == 1) { proc.resetToInit(); proc.setCurrentPresetName ("Init"); }
            else if (r == 2) proc.getEngine().panic();
            else if (r == 3) PresetManager::rootDirectory().revealToUser();
            else if (r > 100)
            {
                const float s = (float) (r - 100) / 100.0f;
                if (auto* ed = findParentComponentOfClass<juce::AudioProcessorEditor>())
                    ed->setSize (juce::roundToInt (Metrics::baseWidth * s), juce::roundToInt (Metrics::baseHeight * s));
            }
        });
    };

    tabs.onChange = [this] (int i) { showPage (i); };
    tabs.fontHeight = 10.5f;
    oscPage.onOpenEditor = [this] (int o) { openWaveEditor (o); };
    editor.onClose = [this] { editor.setVisible (false); showPage (page); };
    browser.onClose = [this] { showBrowser (false); };

    for (int s = 1; s < (int) ModSrc::NumSources; ++s)
    {
        chips.push_back (std::make_unique<ModSourceChip> (p, (ModSrc) s));
        addAndMakeVisible (*chips.back());
    }

    proc.addChangeListener (this);
    showPage (0);
    startTimerHz (30);
}

MainView::~MainView()
{
    proc.removeChangeListener (this);
}

juce::Rectangle<int> MainView::pageArea() const
{
    return { 12, 100, Metrics::baseWidth - 24, Metrics::baseHeight - 100 - 50 };
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    // DEADNOISE-family logotype
    drawTracked (g, "DEADWAVE", { 18.0f, 12.0f, 260.0f, 30.0f }, font (24.0f, false, true), Colours::text, juce::Justification::centredLeft, 0.36f);
    drawTracked (g, "WAVETABLE SYNTHESIZER", { 20.0f, 41.0f, 260.0f, 12.0f }, font (9.5f, true), Colours::muted, juce::Justification::centredLeft, 0.2f);

    // modulation source strip
    auto strip = juce::Rectangle<float> (12.0f, (float) Metrics::baseHeight - 44.0f, (float) Metrics::baseWidth - 24.0f, 34.0f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (strip, Metrics::panelRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (strip.reduced (0.5f), Metrics::panelRadius, 1.0f);
    drawTracked (g, "MOD SOURCES", strip.withWidth (110.0f).withTrimmedLeft (12.0f), font (10.0f, true), Colours::secondary, juce::Justification::centredLeft);
    g.setColour (Colours::muted);
    g.setFont (font (9.5f));
    g.drawText ("v" DEADWAVE_VERSION_STRING, juce::Rectangle<int> (Metrics::baseWidth - 120, 74, 108, 14), juce::Justification::centredRight, false);
}

void MainView::resized()
{
    // Header (DEADNOISE layout: logo left, status chips right)
    auto head = juce::Rectangle<int> (0, 0, Metrics::baseWidth, 64).reduced (12, 12);
    head.removeFromLeft (270);
    master.setBounds (juce::Rectangle<int> (head.getRight() - 54, 4, 54, 58));
    head.removeFromRight (54);
    head.removeFromRight (10);
    auto q = head.removeFromRight (176);
    quality.setBounds (q.reduced (0, 4));
    head.removeFromRight (8);
    outChip.setBounds (head.removeFromRight (150));
    head.removeFromRight (8);
    voicesChip.setBounds (head.removeFromRight (140));
    head.removeFromRight (20);
    auto pr = head;
    prevBtn.setBounds (pr.removeFromLeft (30));
    pr.removeFromLeft (4);
    menuBtn.setBounds (pr.removeFromRight (34));
    pr.removeFromRight (4);
    browseBtn.setBounds (pr.removeFromRight (76));
    pr.removeFromRight (4);
    saveBtn.setBounds (pr.removeFromRight (60));
    pr.removeFromRight (4);
    nextBtn.setBounds (pr.removeFromRight (30));
    pr.removeFromRight (4);
    presetName.setBounds (pr);

    tabs.setBounds (12, 68, 760, 26);

    const auto area = pageArea();
    for (juce::Component* c : std::initializer_list<juce::Component*> { &oscPage, &modPage, &matrixPage, &fxPage, &arpPage, &genPage, &editor, &browser })
        c->setBounds (area);

    auto strip = juce::Rectangle<int> (130, Metrics::baseHeight - 40, Metrics::baseWidth - 24 - 130, 26);
    const int n = (int) chips.size();
    const int cw = juce::jmin (66, (strip.getWidth() - 70) / n);
    for (int i = 0; i < n; ++i)
        chips[(size_t) i]->setBounds (strip.getX() + i * (cw + 2), strip.getY(), cw, strip.getHeight());
}

void MainView::showPage (int p)
{
    page = p;
    tabs.setSelected (p, false);
    browser.setVisible (false);
    editor.setVisible (false);
    juce::Component* pages[] = { &oscPage, &modPage, &matrixPage, &fxPage, &arpPage, &genPage };
    for (int i = 0; i < 6; ++i) pages[i]->setVisible (i == p);
}

void MainView::openWaveEditor (int osc)
{
    browser.setVisible (false);
    editor.setOsc (osc);
    editor.setVisible (true);
    editor.toFront (true);
    editor.grabKeyboardFocus();
}

void MainView::showBrowser (bool show)
{
    browser.setVisible (show);
    if (show) browser.toFront (true);
}

void MainView::stepPreset (int delta)
{
    auto& pm = proc.getPresetManager();
    std::vector<int> all;
    for (int i = 0; i < (int) pm.getAll().size(); ++i) all.push_back (i);
    pm.loadNext (delta, all);
}

void MainView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    presetName.setButtonText (proc.getCurrentPresetName());
}

void MainView::timerCallback()
{
    ++tick;
    if (editor.isVisible()) editor.refresh();
    else
    {
        switch (page)
        {
            case 0: oscPage.refresh(); break;
            case 1: modPage.refresh(); break;
            case 2: if (tick % 6 == 0) matrixPage.refresh(); break;
            case 3: fxPage.refresh(); break;
            case 4: arpPage.refresh(); break;
            default: break;
        }
    }
    for (auto& c : chips) c->refresh();
    master.refreshModulation();
    const int v = proc.getEngine().getActiveVoiceCount();
    voicesChip.set (juce::String (v), v > 0);
    const float pk = proc.getEngine().getOutputPeak();
    outChip.set (pk > 1.0e-5f ? juce::String (gainToDb (pk), 1) + " dB" : "-inf", pk > 1.0e-5f);
    if (presetName.getButtonText() != proc.getCurrentPresetName())
        presetName.setButtonText (proc.getCurrentPresetName());
}
} // namespace dw::ui

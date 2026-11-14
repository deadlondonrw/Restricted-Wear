#include "ArpSeqView.h"

namespace dw::ui
{
// ========================================================================================= step grid
juce::Rectangle<float> StepGrid::toggleArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f).removeFromTop (22.0f);
}

juce::Rectangle<float> StepGrid::laneArea (int lane) const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f);
    r.removeFromTop (30.0f);
    const float h = (r.getHeight() - 16.0f) / 3.0f;
    return { r.getX() + 64.0f, r.getY() + (h + 8.0f) * (float) lane, r.getWidth() - 64.0f, h };
}

void StepGrid::refresh()
{
    const int step = proc.getEngine().getMidiFxStep();
    float k = 0.0f;
    for (int s = 0; s < kNumSeqSteps; ++s)
        for (int p = 0; p < SeqStepP::Count; ++p) k += proc.getValue (P::seqStep (s, p)) * (float) (s * 7 + p + 1);
    k += proc.getValue (P::seq (SeqP::Length)) * 1000.0f;
    if (step != playing || std::abs (k - key) > 1e-4f) { playing = step; key = k; repaint(); }
}

void StepGrid::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);

    const int length = (int) proc.getValue (P::seq (SeqP::Length));
    const auto ta = toggleArea();
    const float cw = (ta.getWidth() - 64.0f) / (float) kNumSeqSteps;
    const char* laneNames[] = { "PITCH", "VELOCITY", "GATE" };
    for (int lane = 0; lane < NumLanes; ++lane)
    {
        const auto la = laneArea (lane);
        drawTracked (g, laneNames[lane], { la.getX() - 62.0f, la.getY(), 58.0f, 14.0f }, font (9.5f, true), Colours::muted, juce::Justification::centredLeft);
        if (lane == Pitch)
        {
            g.setColour (Colours::tick);
            g.drawHorizontalLine (juce::roundToInt (la.getCentreY()), la.getX(), la.getRight());
        }
    }
    for (int s = 0; s < kNumSeqSteps; ++s)
    {
        const bool on = proc.getValue (P::seqStep (s, SeqStepP::On)) >= 0.5f;
        const bool inLen = s < length;
        const float x = ta.getX() + 64.0f + cw * (float) s;
        auto cell = juce::Rectangle<float> (x + 2.0f, ta.getY(), cw - 4.0f, ta.getHeight());
        g.setColour (s == playing ? Colours::accent : (on && inLen ? Colours::text : Colours::track));
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (on && inLen ? Colours::bg : Colours::muted);
        g.setFont (font (10.0f, true));
        g.drawText (juce::String (s + 1), cell, juce::Justification::centred, false);
        for (int lane = 0; lane < NumLanes; ++lane)
        {
            const auto la = laneArea (lane);
            auto col = juce::Rectangle<float> (x + 3.0f, la.getY(), cw - 6.0f, la.getHeight());
            g.setColour (Colours::panel);
            g.fillRect (col);
            const juce::Colour c = ! inLen ? Colours::track : (on ? (s == playing ? Colours::accent : Colours::secondary) : Colours::tick);
            g.setColour (c);
            if (lane == Pitch)
            {
                const float v = proc.getValue (P::seqStep (s, SeqStepP::Pitch)) / 24.0f;
                const float cy = col.getCentreY();
                const float y = cy - v * col.getHeight() * 0.5f;
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (col.getX(), juce::jmin (cy, y), col.getRight(), juce::jmax (cy, y) + 1.0f));
                g.setFont (font (9.5f));
                const int st = (int) proc.getValue (P::seqStep (s, SeqStepP::Pitch));
                g.setColour (Colours::value);
                g.drawText ((st > 0 ? "+" : "") + juce::String (st), col.withHeight (12.0f).withY (v >= 0 ? cy + 2.0f : cy - 14.0f),
                            juce::Justification::centred, false);
            }
            else
            {
                const float v = lane == Velocity ? proc.getValue (P::seqStep (s, SeqStepP::Velocity))
                                                 : proc.getValue (P::seqStep (s, SeqStepP::Gate));
                const float top = col.getBottom() - col.getHeight() * v;
                g.fillRect (col.withTop (top).withWidth (col.getWidth()).reduced (col.getWidth() * 0.25f, 0.0f));
                g.fillRect (juce::Rectangle<float> (col.getX(), top - 1.0f, col.getWidth(), 2.0f));
            }
        }
    }
}

void StepGrid::edit (juce::Point<float> p, bool begin)
{
    const auto ta = toggleArea();
    const float cw = (ta.getWidth() - 64.0f) / (float) kNumSeqSteps;
    const int s = juce::jlimit (0, kNumSeqSteps - 1, (int) ((p.x - ta.getX() - 64.0f) / cw));
    if (begin)
    {
        dragLane = -1;
        if (ta.contains (p))
        {
            const int idx = P::seqStep (s, SeqStepP::On);
            proc.setValue (idx, proc.getValue (idx) >= 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
        for (int lane = 0; lane < NumLanes; ++lane)
            if (laneArea (lane).expanded (0.0f, 4.0f).contains (p)) dragLane = lane;
    }
    if (dragLane < 0) return;
    const auto la = laneArea (dragLane);
    if (dragLane == Pitch)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, (la.getCentreY() - p.y) / (la.getHeight() * 0.5f));
        proc.setValue (P::seqStep (s, SeqStepP::Pitch), std::round (v * 24.0f));
    }
    else
    {
        const float v = juce::jlimit (0.0f, 1.0f, (la.getBottom() - p.y) / la.getHeight());
        proc.setValue (P::seqStep (s, dragLane == Velocity ? SeqStepP::Velocity : SeqStepP::Gate), dragLane == Gate ? juce::jmax (0.05f, v) : v);
    }
    lastStep = s;
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e) { edit (e.position, true); }
void StepGrid::mouseDrag (const juce::MouseEvent& e) { edit (e.position, false); }
void StepGrid::mouseUp (const juce::MouseEvent&) { dragLane = -1; }

// ========================================================================================= arp / seq
ArpSeqView::ArpSeqView (DeadwaveProcessor& p)
    : proc (p), mode (p, P::arp (ArpP::MidiFxMode), { "OFF", "ARPEGGIATOR", "SEQUENCER" }),
      arpMode (p, P::arp (ArpP::ArpMode)), arpRate (p, P::arp (ArpP::ArpRate)), arpVelMode (p, P::arp (ArpP::ArpVelMode)),
      seqRate (p, P::seq (SeqP::Rate)),
      arpOct (p, P::arp (ArpP::ArpOctaves)), arpGate (p, P::arp (ArpP::ArpGate)), arpSwing (p, P::arp (ArpP::ArpSwing)),
      arpVel (p, P::arp (ArpP::ArpVel)), arpSteps (p, P::arp (ArpP::ArpSteps), "CYCLE"), arpTrans (p, P::arp (ArpP::ArpPitchStep)),
      seqSwing (p, P::seq (SeqP::Swing)), seqLength (p, P::seq (SeqP::Length)), seqGate (p, P::seq (SeqP::GateScale)),
      arpHold (p, P::arp (ArpP::ArpHold), "HOLD"), grid (p)
{
    addAndMakeVisible (mode);
    addAndMakeVisible (arpPanel);
    addAndMakeVisible (seqPanel);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &arpMode, &arpRate, &arpVelMode, &arpOct, &arpGate, &arpSwing,
                                                                        &arpVel, &arpSteps, &arpTrans, &arpHold, &c1, &c2, &c3 })
        arpPanel.addAndMakeVisible (c);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &seqRate, &seqSwing, &seqLength, &seqGate, &grid, &c4 })
        seqPanel.addAndMakeVisible (c);
    arpSteps.setTooltip ("Notes per cycle; after each cycle the pattern restarts transposed by TRANSP (up to 3 cycles)");
    arpGate.setTooltip ("Gate above 100% overlaps notes: with LEGATO + glide OVERLAP this produces slides");
    help.setFont (font (11.5f));
    help.setColour (juce::Label::textColourId, Colours::dim);
    help.setJustificationType (juce::Justification::topLeft);
    help.setText ("The arpeggiator and sequencer follow the host tempo and position while the transport runs, and an "
                  "internal clock (started by your first key) when it is stopped. The sequencer plays its pattern "
                  "relative to the most recent held key.", juce::dontSendNotification);
    addAndMakeVisible (help);
}

void ArpSeqView::paint (juce::Graphics&) {}

void ArpSeqView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (28);
    mode.setBounds (top.removeFromLeft (420).reduced (0, 2));
    top.removeFromLeft (16);
    help.setBounds (top.withHeight (36));
    r.removeFromTop (10);
    auto arpR = r.removeFromLeft (360);
    r.removeFromLeft (Metrics::gap);
    arpPanel.setBounds (arpR);
    seqPanel.setBounds (r);

    auto a = arpPanel.getLocalBounds().reduced (12, 10);
    a.removeFromTop (24);
    auto row = a.removeFromTop (36);
    auto m1 = row.removeFromLeft (row.getWidth() / 2 - 4);
    c1.setBounds (m1.removeFromTop (12));
    arpMode.setBounds (m1.withHeight (22));
    row.removeFromLeft (8);
    c2.setBounds (row.removeFromTop (12));
    arpRate.setBounds (row.withHeight (22));
    a.removeFromTop (12);
    layoutRow (a.removeFromTop (72), { &arpOct, &arpGate, &arpSwing }, 70);
    a.removeFromTop (8);
    layoutRow (a.removeFromTop (72), { &arpSteps, &arpTrans, &arpVel }, 70);
    a.removeFromTop (12);
    auto vr = a.removeFromTop (36);
    auto vm = vr.removeFromLeft (150);
    c3.setBounds (vm.removeFromTop (12));
    arpVelMode.setBounds (vm.withHeight (22));
    vr.removeFromLeft (16);
    arpHold.setBounds (vr.withTrimmedTop (12).withHeight (22));

    auto s = seqPanel.getLocalBounds().reduced (12, 10);
    s.removeFromTop (24);
    auto srow = s.removeFromTop (64);
    auto rc = srow.removeFromLeft (120);
    c4.setBounds (rc.removeFromTop (12));
    seqRate.setBounds (rc.withHeight (22));
    srow.removeFromLeft (20);
    layoutRow (srow.removeFromLeft (240), { &seqSwing, &seqLength, &seqGate }, 64);
    s.removeFromTop (8);
    grid.setBounds (s);
}

void ArpSeqView::refresh()
{
    grid.refresh();
    const int m = (int) proc.getValue (P::arp (ArpP::MidiFxMode));
    arpPanel.highlighted = m == 1;
    seqPanel.highlighted = m == 2;
    arpPanel.repaint();
    seqPanel.repaint();
}
} // namespace dw::ui

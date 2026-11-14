#include "OscPanel.h"

namespace dw::ui
{
// ========================================================================================= thumb
WaveThumb::WaveThumb (DeadwaveProcessor& p, int o) : proc (p), osc (o)
{
    setTooltip ("Click to open the Wave Editor");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void WaveThumb::refresh()
{
    float key[8] = {
        proc.getEngine().getWtPosForUi (osc),
        proc.getValue (P::osc (osc, OscP::Bias)),
        proc.getValue (P::osc (osc, OscP::Amp)),
        proc.getValue (P::osc (osc, OscP::Phase)),
        proc.getValue (P::osc (osc, OscP::Shape)),
        proc.getValue (P::osc (osc, OscP::WarpMode)),
        proc.getValue (P::osc (osc, OscP::Type)) + 10.0f * proc.getValue (P::osc (osc, OscP::On)),
        proc.getValue (P::osc (osc, OscP::DcRemove))
    };
    bool changed = proc.getWave (osc).getVersion() != lastVersion;
    for (int i = 0; i < 8; ++i)
        if (std::abs (key[i] - lastKey[i]) > 1.0e-4f) changed = true;
    if (changed)
    {
        std::copy (key, key + 8, lastKey);
        lastVersion = proc.getWave (osc).getVersion();
        repaint();
    }
}

void WaveThumb::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && e.getDistanceFromDragStart() < 4 && onOpen) onOpen();
}

void WaveThumb::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (hover ? Colours::muted : Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);

    auto plot = r.reduced (6.0f, 8.0f);
    const float cy = plot.getCentreY();
    const float scaleY = plot.getHeight() * 0.5f / 1.25f; // +/-1.25 visible so bias stays in view
    g.setColour (Colours::grid);
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * (float) i / 4.0f), plot.getY(), plot.getBottom());
    g.setColour (Colours::tick);
    g.drawHorizontalLine (juce::roundToInt (cy), plot.getX(), plot.getRight());

    const bool on = proc.getValue (P::osc (osc, OscP::On)) >= 0.5f;
    const int type = (int) proc.getValue (P::osc (osc, OscP::Type));
    const auto lineColour = on ? Colours::text : Colours::muted;
    const int N = juce::jmax (64, (int) plot.getWidth());

    auto& doc = proc.getWave (osc);
    juce::String caption, right;

    if (type == 0)
    {
        const float pos = lastKey[0];
        const int nf = doc.getNumFrames();
        const float fp = pos * (float) (nf - 1);
        const int fa = juce::jlimit (0, nf - 1, (int) fp), fb = juce::jmin (nf - 1, fa + 1);
        const float fm = fp - (float) fa;
        const auto& A = doc.getSamples (fa);
        const auto& B = doc.getSamples (fb);
        double dcA = 0.0, dcB = 0.0;
        for (int i = 0; i < kWaveSize; ++i) { dcA += A[(size_t) i]; dcB += B[(size_t) i]; }
        const float dc = lastKey[7] >= 0.5f ? (float) ((dcA + (dcB - dcA) * fm) / kWaveSize) : 0.0f;
        auto read = [&] (float x)
        {
            const float idx = x * (float) kWaveSize;
            const int i = juce::jlimit (0, kWaveSize - 1, (int) idx);
            const int j = (i + 1) % kWaveSize;
            const float t = idx - (float) i;
            const float a = A[(size_t) i] + (A[(size_t) j] - A[(size_t) i]) * t;
            const float b = B[(size_t) i] + (B[(size_t) j] - B[(size_t) i]) * t;
            return a + (b - a) * fm;
        };

        // raw frame (ghost) vs actual output shape
        juce::Path ghost, out;
        const float bias = lastKey[1], amp = lastKey[2], ph = lastKey[3], shp = lastKey[4];
        const int mode = (int) lastKey[5];
        for (int k = 0; k <= N; ++k)
        {
            const float p = (float) k / (float) N;
            const float x = plot.getX() + p * plot.getWidth();
            const float raw = read (std::fmod (p, 1.0f));
            const float v = amp * (warp::displayValue (mode, shp, p + ph, read) - dc) + bias;
            const float y0 = juce::jlimit (plot.getY(), plot.getBottom(), cy - raw * scaleY);
            const float y1 = juce::jlimit (plot.getY(), plot.getBottom(), cy - v * scaleY);
            if (k == 0) { ghost.startNewSubPath (x, y0); out.startNewSubPath (x, y1); }
            else { ghost.lineTo (x, y0); out.lineTo (x, y1); }
        }
        g.setColour (Colours::tick);
        g.strokePath (ghost, juce::PathStrokeType (1.0f));
        g.setColour (lineColour);
        g.strokePath (out, juce::PathStrokeType (large ? 2.0f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (std::abs (bias) > 0.001f)
        {
            g.setColour (Colours::accent.withAlpha (0.55f));
            const float by = cy - bias * scaleY;
            g.drawHorizontalLine (juce::roundToInt (by), plot.getX(), plot.getX() + 10.0f);
        }
        caption = doc.getName();
        right = juce::String (juce::roundToInt (fp) + 1) + " / " + juce::String (nf);
    }
    else if (type == 1)
    {
        juce::Path p;
        Rng rng (1234u);
        float lp = 0.0f;
        const float color = proc.getValue (P::osc (osc, OscP::Shape));
        const float a = color < 0.0f ? 1.0f - std::abs (color) * 0.9f : 1.0f;
        for (int k = 0; k <= N; ++k)
        {
            lp += a * (rng.bi() - lp);
            const float x = plot.getX() + plot.getWidth() * (float) k / (float) N;
            const float y = cy - lp * scaleY * 0.8f * proc.getValue (P::osc (osc, OscP::Amp)) - proc.getValue (P::osc (osc, OscP::Bias)) * scaleY;
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, juce::jlimit (plot.getY(), plot.getBottom(), y));
        }
        g.setColour (lineColour.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
        caption = "NOISE";
    }
    else
    {
        auto s = proc.getSample (osc);
        if (s != nullptr && s->length > 1)
        {
            const auto& d = s->data[0][0];
            juce::Path p;
            for (int k = 0; k < N; ++k)
            {
                const int a = (int) ((int64_t) s->length * k / N), b = (int) ((int64_t) s->length * (k + 1) / N);
                float pk = 0.0f;
                for (int i = a; i < b; i += juce::jmax (1, (b - a) / 32)) pk = juce::jmax (pk, std::abs (d[(size_t) i]));
                const float x = plot.getX() + plot.getWidth() * (float) k / (float) N;
                p.addLineSegment ({ x, cy - pk * scaleY, x, cy + pk * scaleY }, 1.0f);
            }
            g.setColour (lineColour.withAlpha (0.75f));
            g.fillPath (p);
            const float sx = plot.getX() + plot.getWidth() * proc.getValue (P::osc (osc, OscP::WtPos));
            g.setColour (Colours::accent);
            g.drawVerticalLine (juce::roundToInt (sx), plot.getY(), plot.getBottom());
            caption = s->name;
        }
        else
            caption = "NO SAMPLE - CLICK TO IMPORT";
    }

    g.setFont (font (10.0f, true));
    g.setColour (Colours::muted);
    g.drawText (caption.toUpperCase(), r.reduced (7.0f, 4.0f), juce::Justification::bottomLeft, true);
    if (right.isNotEmpty())
        g.drawText (right, r.reduced (7.0f, 4.0f), juce::Justification::bottomRight, false);
    if (hover)
    {
        g.setColour (Colours::secondary);
        g.drawText ("EDIT", r.reduced (7.0f, 4.0f), juce::Justification::topRight, false);
    }
}

// ========================================================================================== card
OscCard::OscCard (DeadwaveProcessor& p, int o)
    : Panel ("OSC " + juce::String (o + 1)), proc (p), osc (o),
      onToggle (p, P::osc (o, OscP::On)),
      type (p, P::osc (o, OscP::Type), { "WT", "NOISE", "SAMPLE" }),
      thumb (p, o),
      wtpos (p, P::osc (o, OscP::WtPos)), bias (p, P::osc (o, OscP::Bias)), amp (p, P::osc (o, OscP::Amp)),
      phase (p, P::osc (o, OscP::Phase)), shape (p, P::osc (o, OscP::Shape)),
      warp (p, P::osc (o, OscP::WarpMode)), route (p, P::osc (o, OscP::Route)),
      level (p, P::osc (o, OscP::Level)), pan (p, P::osc (o, OscP::Pan)), unison (p, P::osc (o, OscP::Unison)),
      detune (p, P::osc (o, OscP::Detune)), spread (p, P::osc (o, OscP::Spread)),
      oct (p, P::osc (o, OscP::Octave)), semi (p, P::osc (o, OscP::Semi)), fine (p, P::osc (o, OscP::Fine)), pitch (p, P::osc (o, OscP::Pitch)),
      blend (p, P::osc (o, OscP::Blend)), rand (p, P::osc (o, OscP::PhaseRand)), fm (p, P::osc (o, OscP::FmAmt)),
      fmSrc (p, P::osc (o, OscP::FmSrc)), driveMode (p, P::osc (o, OscP::DriveMode)),
      ratio (p, P::osc (o, OscP::FmRatio)), drive (p, P::osc (o, OscP::Drive)), crush (p, P::osc (o, OscP::Crush)),
      fxOn (p, P::osc (o, OscP::FxOn), "FX"), fxLink (p, P::osc (o, OscP::FxLink), "LINK")
{
    title = {};
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &onToggle, &type, &thumb, &wtpos, &bias, &amp, &phase, &shape, &warp, &route, &level, &pan, &unison,
             &detune, &spread, &oct, &semi, &fine, &pitch, &blend, &rand, &fm, &fmSrc, &driveMode, &ratio, &drive, &crush,
             &fxOn, &fxLink, &warpCap, &routeCap, &fmCap, &drvCap })
        addAndMakeVisible (c);
    for (auto* k : { &oct, &semi, &fine, &pitch, &blend, &rand, &fm, &ratio, &drive, &crush })
        k->setCompact (true);
    thumb.onOpen = [this] { if (onOpenEditor) onOpenEditor (osc); };
    type.fontHeight = 9.5f;
    fxOn.setTooltip ("Give this oscillator its own FX lane (edit it on the FX page)");
    fxLink.setTooltip ("LINK FX: every linked oscillator uses the FX settings of the first linked oscillator");
}

std::vector<ParamKnob*> OscCard::getKnobs()
{
    return { &wtpos, &bias, &amp, &phase, &shape, &level, &pan, &unison, &detune, &spread, &oct, &semi, &fine, &pitch,
             &blend, &rand, &fm, &ratio, &drive, &crush };
}

void OscCard::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const bool on = proc.getValue (P::osc (osc, OscP::On)) >= 0.5f;
    drawTracked (g, "OSC " + juce::String (osc + 1), juce::Rectangle<float> (50.0f, 8.0f, 80.0f, 18.0f), font (12.5f, true),
                 on ? Colours::text : Colours::secondary, juce::Justification::centredLeft, 0.14f);
}

void OscCard::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto head = r.removeFromTop (20);
    onToggle.setBounds (head.removeFromLeft (36));
    type.setBounds (head.removeFromRight (142));
    r.removeFromTop (6);
    thumb.setBounds (r.removeFromTop (80));
    r.removeFromTop (6);

    const int knobH = 58;
    layoutRow (r.removeFromTop (knobH), { &wtpos, &bias, &amp, &phase, &shape }, 54);
    r.removeFromTop (2);
    auto combos = r.removeFromTop (34);
    {
        auto left = combos.removeFromLeft (combos.getWidth() / 2 - 3);
        combos.removeFromLeft (6);
        warpCap.setBounds (left.removeFromTop (12));
        warp.setBounds (left.withHeight (20));
        routeCap.setBounds (combos.removeFromTop (12));
        route.setBounds (combos.withHeight (20));
    }
    r.removeFromTop (4);
    layoutRow (r.removeFromTop (knobH), { &level, &pan, &unison, &detune, &spread }, 54);
    r.removeFromTop (2);
    layoutRow (r.removeFromTop (46), { &oct, &semi, &fine, &pitch, &blend, &rand, &fm }, 40);
    r.removeFromTop (4);
    auto bottom = r.removeFromTop (46);
    {
        const int kw = 44;
        auto c1 = bottom.removeFromLeft (bottom.getWidth() - 3 * kw - 72);
        fmCap.setBounds (c1.removeFromTop (12));
        fmSrc.setBounds (c1.removeFromTop (22).withTrimmedRight (6));
        ratio.setBounds (bottom.removeFromLeft (kw));
        drive.setBounds (bottom.removeFromLeft (kw));
        auto c2 = bottom.removeFromLeft (72);
        drvCap.setBounds (c2.removeFromTop (12).withTrimmedLeft (2));
        driveMode.setBounds (c2.removeFromTop (22).reduced (2, 0));
        crush.setBounds (bottom.removeFromLeft (kw));
    }
    r.removeFromTop (2);
    auto foot = r.removeFromTop (22);
    fxOn.setBounds (foot.removeFromLeft (foot.getWidth() / 2));
    fxLink.setBounds (foot);
}

void OscCard::refresh()
{
    thumb.refresh();
    const bool on = proc.getValue (P::osc (osc, OscP::On)) >= 0.5f;
    if (on != wtpos.isEnabled())
    {
        for (auto* k : getKnobs()) k->setEnabled (on);
        repaint();
    }
    for (auto* k : getKnobs()) k->refreshModulation();
}
} // namespace dw::ui

#include "WaveEditor.h"
#include "../Presets/PresetManager.h"

namespace dw::ui
{
namespace
{
constexpr float kViewRange = 1.25f;

juce::Colour lerpColour (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t)); }

float sampleAt (const std::vector<float>& s, float x)
{
    x -= std::floor (x);
    const float idx = x * (float) kWaveSize;
    const int i = juce::jlimit (0, kWaveSize - 1, (int) idx);
    const int j = (i + 1) % kWaveSize;
    return s[(size_t) i] + (s[(size_t) j] - s[(size_t) i]) * (idx - (float) i);
}
} // namespace

// =========================================================================================== canvas
juce::Point<float> WaveCanvas::toWave (juce::Point<float> p) const
{
    const auto pl = plot();
    float x = juce::jlimit (0.0f, 1.0f, (p.x - pl.getX()) / pl.getWidth());
    if (grid > 0) x = std::round (x * (float) grid) / (float) grid;
    const float y = juce::jlimit (-2.0f, 2.0f, (pl.getCentreY() - p.y) / (pl.getHeight() * 0.5f / kViewRange));
    return { x, y };
}

juce::Point<float> WaveCanvas::toScreen (float x, float y) const
{
    const auto pl = plot();
    return { pl.getX() + x * pl.getWidth(), pl.getCentreY() - y * pl.getHeight() * 0.5f / kViewRange };
}

void WaveCanvas::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);

    if (view == View::Wave3D) { paint3D (g); return; }
    if (view == View::Spectrum) { paintSpectrum (g, false); return; }
    if (mode == Mode::Harmonics) { paintSpectrum (g, true); return; }
    paint2D (g);
}

void WaveCanvas::paint2D (juce::Graphics& g)
{
    auto& doc = proc.getWave (osc);
    const auto pl = plot();
    g.setColour (Colours::grid);
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (juce::roundToInt (pl.getX() + pl.getWidth() * (float) i / 8.0f), pl.getY(), pl.getBottom());
    for (float v : { -1.0f, -0.5f, 0.5f, 1.0f })
        g.drawHorizontalLine (juce::roundToInt (toScreen (0, v).y), pl.getX(), pl.getRight());
    g.setColour (Colours::tick);
    g.drawHorizontalLine (juce::roundToInt (pl.getCentreY()), pl.getX(), pl.getRight());
    g.setFont (font (9.5f));
    for (float v : { -1.0f, 1.0f })
        g.drawText (v > 0 ? "+1" : "-1", juce::Rectangle<float> (pl.getX() + 3.0f, toScreen (0, v).y - 12.0f, 24.0f, 11.0f), juce::Justification::left);

    const int f = doc.getSelectedFrame();
    const int N = juce::jmax (128, (int) pl.getWidth());
    auto pathOf = [&] (const std::vector<float>& s)
    {
        juce::Path p;
        for (int k = 0; k <= N; ++k)
        {
            const float x = (float) k / (float) N;
            const auto pt = toScreen (x, juce::jlimit (-kViewRange, kViewRange, sampleAt (s, juce::jmin (x, 0.99999f))));
            if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    };

    // neighbours
    g.setColour (Colours::tick.withAlpha (0.8f));
    if (f > 0) g.strokePath (pathOf (doc.getSamples (f - 1)), juce::PathStrokeType (1.0f));
    if (f < doc.getNumFrames() - 1) g.strokePath (pathOf (doc.getSamples (f + 1)), juce::PathStrokeType (1.0f));

    // actual oscillator output for this frame (bias, amp, phase, shape applied): amber
    {
        const auto& s = doc.getSamples (f);
        double dc = 0.0;
        for (auto v : s) dc += v;
        const float dcv = proc.getValue (P::osc (osc, OscP::DcRemove)) >= 0.5f ? (float) (dc / kWaveSize) : 0.0f;
        const float bias = proc.getValue (P::osc (osc, OscP::Bias)), amp = proc.getValue (P::osc (osc, OscP::Amp));
        const float ph = proc.getValue (P::osc (osc, OscP::Phase)), shp = proc.getValue (P::osc (osc, OscP::Shape));
        const int wm = (int) proc.getValue (P::osc (osc, OscP::WarpMode));
        const bool differs = std::abs (bias) > 1e-4f || std::abs (amp - 1.0f) > 1e-4f || std::abs (ph) > 1e-4f || std::abs (shp) > 1e-4f || std::abs (dcv) > 1e-3f;
        if (differs)
        {
            juce::Path p;
            auto read = [&] (float x) { return sampleAt (s, x); };
            for (int k = 0; k <= N; ++k)
            {
                const float x = (float) k / (float) N;
                const float v = amp * (warp::displayValue (wm, shp, x + ph, read) - dcv) + bias;
                const auto pt = toScreen (x, juce::jlimit (-kViewRange, kViewRange, v));
                if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            g.setColour (Colours::accent.withAlpha (0.75f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
            if (std::abs (bias) > 1e-4f)
            {
                g.setColour (Colours::accent.withAlpha (0.35f));
                const float y = toScreen (0, bias).y;
                for (float x = pl.getX(); x < pl.getRight(); x += 8.0f)
                    g.drawHorizontalLine (juce::roundToInt (y), x, x + 4.0f);
            }
        }
    }

    g.setColour (Colours::text);
    g.strokePath (pathOf (doc.getSamples (f)), juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (mode == Mode::Points)
    {
        const auto& fr = doc.getFrame (f);
        if (fr.pointMode)
            for (int i = 0; i < (int) fr.points.size(); ++i)
            {
                const auto pt = toScreen (fr.points[(size_t) i].x, juce::jlimit (-kViewRange, kViewRange, fr.points[(size_t) i].y));
                const bool sel = i == dragPoint;
                g.setColour (sel ? Colours::accent : Colours::bg);
                g.fillEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
                g.setColour (sel ? Colours::accent : Colours::text);
                g.drawEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f, 1.4f);
            }
    }

    if (hoverWave.x >= 0.0f)
    {
        g.setColour (Colours::muted);
        g.setFont (font (10.5f));
        g.drawText ("x " + juce::String (hoverWave.x, 3) + "   y " + juce::String (hoverWave.y, 3),
                    getLocalBounds().reduced (10, 6), juce::Justification::bottomRight, false);
    }
}

void WaveCanvas::paint3D (juce::Graphics& g)
{
    auto& doc = proc.getWave (osc);
    const int nf = doc.getNumFrames();
    const auto pl = plot().reduced (10.0f, 10.0f);
    const int shown = juce::jmin (nf, 64);
    const float depthX = pl.getWidth() * 0.28f, depthY = pl.getHeight() * 0.42f;
    const float w = pl.getWidth() - depthX, amp = pl.getHeight() * 0.22f;
    const int sel = doc.getSelectedFrame();
    const float wt = proc.getEngine().getWtPosForUi (osc) * (float) (nf - 1);
    for (int k = shown - 1; k >= 0; --k)
    {
        const int f = shown == 1 ? 0 : juce::roundToInt ((float) k * (float) (nf - 1) / (float) (shown - 1));
        const float t = shown == 1 ? 0.0f : (float) k / (float) (shown - 1);
        const float ox = pl.getX() + depthX * t, oy = pl.getBottom() - amp - depthY * t;
        juce::Path p;
        const auto& s = doc.getSamples (f);
        for (int i = 0; i <= 96; ++i)
        {
            const float x = (float) i / 96.0f;
            const float y = oy - juce::jlimit (-kViewRange, kViewRange, sampleAt (s, juce::jmin (x, 0.9999f))) * amp;
            if (i == 0) p.startNewSubPath (ox + x * w, y); else p.lineTo (ox + x * w, y);
        }
        const bool isSel = f == sel;
        const bool isPlay = std::abs ((float) f - wt) < (float) nf / (float) shown * 0.5f;
        g.setColour (isSel ? Colours::text : isPlay ? Colours::accent : lerpColour (Colours::track, Colours::secondary, 1.0f - t));
        g.strokePath (p, juce::PathStrokeType (isSel || isPlay ? 1.6f : 1.0f));
    }
    g.setColour (Colours::muted);
    g.setFont (font (10.5f));
    g.drawText (juce::String (nf) + " FRAMES   WHITE = SELECTED   AMBER = PLAYING (WT POS)", getLocalBounds().reduced (10, 6),
                juce::Justification::bottomLeft, false);
}

void WaveCanvas::paintSpectrum (juce::Graphics& g, bool editable)
{
    auto& doc = proc.getWave (osc);
    const int f = doc.getSelectedFrame();
    const int count = editable ? harmonicCount : 256;
    std::vector<float> m, p;
    doc.getHarmonics (f, m, p, count);
    const auto pl = plot();
    const auto bars = pl.withTrimmedBottom (editable ? 16.0f : 0.0f);
    g.setColour (Colours::grid);
    for (float v : { 0.25f, 0.5f, 0.75f })
        g.drawHorizontalLine (juce::roundToInt (bars.getBottom() - bars.getHeight() * v), bars.getX(), bars.getRight());

    const float bw = bars.getWidth() / (float) count;
    for (int h = 1; h <= count; ++h)
    {
        float hgt;
        if (editable) hgt = std::sqrt (juce::jlimit (0.0f, 1.0f, m[(size_t) h - 1]));
        else
        {
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-6f, m[(size_t) h - 1]));
            hgt = juce::jlimit (0.0f, 1.0f, (db + 96.0f) / 96.0f);
        }
        const float x = bars.getX() + bw * (float) (h - 1);
        auto bar = juce::Rectangle<float> (x + (bw > 4.0f ? 1.0f : 0.0f), bars.getBottom() - bars.getHeight() * hgt,
                                           juce::jmax (1.0f, bw - (bw > 4.0f ? 2.0f : 0.0f)), bars.getHeight() * hgt);
        g.setColour (h % 2 == 1 ? Colours::text : Colours::secondary);
        g.fillRect (bar);
        if (editable)
        {
            float ph = std::fmod (p[(size_t) h - 1], juce::MathConstants<float>::twoPi);
            if (ph < 0) ph += juce::MathConstants<float>::twoPi;
            g.setColour (Colours::accent.withAlpha (m[(size_t) h - 1] > 1e-4f ? 0.9f : 0.25f));
            g.fillRect (x + bw * 0.5f - 1.0f, pl.getBottom() - 12.0f * ph / juce::MathConstants<float>::twoPi - 2.0f, 2.0f, 2.0f);
        }
    }
    g.setColour (Colours::muted);
    g.setFont (font (10.5f));
    g.drawText (editable ? "DRAG: MAGNITUDE   ALT-DRAG: PHASE   HARMONICS 1-" + juce::String (count)
                         : "SPECTRUM (dB) - HARMONICS 1-256",
                getLocalBounds().reduced (12, 6), juce::Justification::topRight, false);
}

int WaveCanvas::hitPoint (juce::Point<float> p) const
{
    const auto& fr = proc.getWave (osc).getFrame (proc.getWave (osc).getSelectedFrame());
    if (! fr.pointMode) return -1;
    for (int i = 0; i < (int) fr.points.size(); ++i)
        if (toScreen (fr.points[(size_t) i].x, fr.points[(size_t) i].y).getDistanceFrom (p) < 8.0f) return i;
    return -1;
}

void WaveCanvas::editHarmonic (juce::Point<float> pos, bool phase)
{
    auto& doc = proc.getWave (osc);
    const auto pl = plot();
    const auto bars = pl.withTrimmedBottom (16.0f);
    const int h = juce::jlimit (1, harmonicCount, 1 + (int) ((pos.x - bars.getX()) / bars.getWidth() * (float) harmonicCount));
    const float y01 = juce::jlimit (0.0f, 1.0f, (bars.getBottom() - pos.y) / bars.getHeight());
    if ((int) mags.size() < harmonicCount) doc.getHarmonics (doc.getSelectedFrame(), mags, phases, harmonicCount);
    if (phase) phases[(size_t) h - 1] = y01 * juce::MathConstants<float>::twoPi;
    else mags[(size_t) h - 1] = y01 * y01;
    if (allFrames)
        doc.setHarmonic (doc.getSelectedFrame(), h, mags[(size_t) h - 1], phases[(size_t) h - 1], true);
    else
        doc.setHarmonics (doc.getSelectedFrame(), mags, phases);
}

void WaveCanvas::mouseDown (const juce::MouseEvent& e)
{
    if (view != View::Wave2D && view != View::Spectrum)
    {
        // 3D: click selects the nearest frame by depth
        auto& doc = proc.getWave (osc);
        const auto pl = plot();
        const float t = juce::jlimit (0.0f, 1.0f, (pl.getBottom() - e.position.y) / pl.getHeight());
        doc.setSelectedFrame (juce::roundToInt (t * (float) (doc.getNumFrames() - 1)));
        if (onEdited) onEdited();
        repaint();
        return;
    }
    if (view == View::Spectrum) return;
    auto& doc = proc.getWave (osc);
    const int f = doc.getSelectedFrame();

    if (mode == Mode::Harmonics)
    {
        doc.beginGesture();
        mags.clear();
        editHarmonic (e.position, e.mods.isAltDown());
        if (onEdited) onEdited();
        repaint();
        return;
    }
    if (mode == Mode::Points)
    {
        if (! doc.getFrame (f).pointMode)
        {
            doc.convertToPoints (f, 16);
            if (onEdited) onEdited();
            repaint();
            return;
        }
        doc.beginGesture();
        dragPoint = hitPoint (e.position);
        curveSeg = -1;
        if (e.mods.isPopupMenu())
        {
            auto pts = doc.getFrame (f).points;
            if (dragPoint >= 0 && pts.size() > 2)
            {
                pts.erase (pts.begin() + dragPoint);
                doc.setPoints (f, pts);
            }
            dragPoint = -1;
            if (onEdited) onEdited();
            repaint();
            return;
        }
        if (dragPoint < 0 && e.mods.isAltDown())
        {
            auto pts = doc.getFrame (f).points;
            std::sort (pts.begin(), pts.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
            const float x = toWave (e.position).x;
            int seg = (int) pts.size() - 1;
            for (int i = 0; i < (int) pts.size(); ++i) if (pts[(size_t) i].x <= x) seg = i;
            doc.setPoints (f, pts);
            curveSeg = seg;
            curveStart = pts[(size_t) seg].curve;
            dragStartY = e.position.y;
        }
        repaint();
        return;
    }
    // draw
    doc.beginGesture();
    last = toWave (e.position);
    doc.drawSegment (f, last.x, last.y, last.x, last.y, allFrames);
    if (onEdited) onEdited();
    repaint();
}

void WaveCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (view != View::Wave2D) return;
    auto& doc = proc.getWave (osc);
    const int f = doc.getSelectedFrame();
    hoverWave = toWave (e.position);
    if (mode == Mode::Harmonics)
    {
        editHarmonic (e.position, e.mods.isAltDown());
    }
    else if (mode == Mode::Points)
    {
        auto pts = doc.getFrame (f).points;
        if (curveSeg >= 0 && curveSeg < (int) pts.size())
        {
            pts[(size_t) curveSeg].curve = juce::jlimit (-1.0f, 1.0f, curveStart + (dragStartY - e.position.y) / 90.0f);
            doc.setPoints (f, pts);
        }
        else if (dragPoint >= 0 && dragPoint < (int) pts.size())
        {
            auto w = toWave (e.position);
            const float lo = dragPoint > 0 ? pts[(size_t) dragPoint - 1].x + 0.001f : 0.0f;
            const float hi = dragPoint < (int) pts.size() - 1 ? pts[(size_t) dragPoint + 1].x - 0.001f : 0.999f;
            pts[(size_t) dragPoint].x = juce::jlimit (lo, juce::jmax (lo, hi), w.x);
            pts[(size_t) dragPoint].y = juce::jlimit (-2.0f, 2.0f, w.y);
            doc.setPoints (f, pts);
        }
    }
    else
    {
        const auto cur = toWave (e.position);
        doc.drawSegment (f, last.x, last.y, cur.x, cur.y, allFrames);
        last = cur;
    }
    if (onEdited) onEdited();
    repaint();
}

void WaveCanvas::mouseUp (const juce::MouseEvent&)
{
    curveSeg = -1;
    mags.clear();
}

void WaveCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (mode != Mode::Points || view != View::Wave2D) return;
    auto& doc = proc.getWave (osc);
    const int f = doc.getSelectedFrame();
    auto pts = doc.getFrame (f).points;
    if (hitPoint (e.position) >= 0 || pts.size() >= 128) return;
    const auto w = toWave (e.position);
    pts.push_back ({ w.x, w.y, 0.0f });
    std::sort (pts.begin(), pts.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
    doc.setPoints (f, pts);
    if (onEdited) onEdited();
    repaint();
}

void WaveCanvas::mouseMove (const juce::MouseEvent& e)
{
    if (view == View::Wave2D && mode != Mode::Harmonics)
    {
        hoverWave = toWave (e.position);
        repaint (getLocalBounds().removeFromBottom (24));
    }
}

// ======================================================================================= frame strip
void FrameStrip::paint (juce::Graphics& g)
{
    auto& doc = proc.getWave (osc);
    const int nf = doc.getNumFrames();
    const int cw = cellW();
    const int first = (int) (scroll / (float) cw);
    for (int f = first; f < nf; ++f)
    {
        const float x = (float) (f * cw) - scroll;
        if (x > (float) getWidth()) break;
        auto cell = juce::Rectangle<float> (x + 2.0f, 2.0f, (float) cw - 4.0f, (float) getHeight() - 4.0f);
        const bool sel = f == doc.getSelectedFrame();
        g.setColour (Colours::raised);
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (sel ? Colours::text : Colours::border);
        g.drawRoundedRectangle (cell, 3.0f, sel ? 1.4f : 1.0f);
        const auto& s = doc.getSamples (f);
        juce::Path p;
        auto plot = cell.reduced (4.0f, 8.0f).withTrimmedBottom (6.0f);
        for (int i = 0; i <= 40; ++i)
        {
            const float px = plot.getX() + plot.getWidth() * (float) i / 40.0f;
            const float py = plot.getCentreY() - juce::jlimit (-1.2f, 1.2f, s[(size_t) juce::jmin (kWaveSize - 1, i * kWaveSize / 40)]) * plot.getHeight() * 0.42f;
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (sel ? Colours::text : Colours::secondary);
        g.strokePath (p, juce::PathStrokeType (1.0f));
        g.setFont (font (9.0f, true));
        g.setColour (sel ? Colours::text : Colours::muted);
        g.drawText (juce::String (f + 1), cell.reduced (3.0f, 1.0f), juce::Justification::bottomLeft, false);
        if (f == markA)
        {
            g.setColour (Colours::accent);
            juce::Path tri;
            tri.addTriangle (cell.getRight() - 10.0f, cell.getY() + 3.0f, cell.getRight() - 3.0f, cell.getY() + 3.0f, cell.getRight() - 3.0f, cell.getY() + 10.0f);
            g.fillPath (tri);
        }
    }
}

void FrameStrip::mouseDown (const juce::MouseEvent& e)
{
    auto& doc = proc.getWave (osc);
    const int f = (int) (((float) e.x + scroll) / (float) cellW());
    if (f >= 0 && f < doc.getNumFrames())
    {
        doc.setSelectedFrame (f);
        if (onSelect) onSelect();
        repaint();
    }
}

void FrameStrip::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float maxScroll = juce::jmax (0.0f, (float) (proc.getWave (osc).getNumFrames() * cellW() - getWidth()));
    scroll = juce::jlimit (0.0f, maxScroll, scroll - (w.deltaY + w.deltaX) * 240.0f);
    repaint();
}

// ===================================================================================== import panel
ImportPanel::ImportPanel (DeadwaveProcessor& p) : Panel ("IMPORT AUDIO"), proc (p)
{
    for (int n : { 4, 8, 16, 32, 64, 128, 256 }) frames.addItem (juce::String (n) + " FRAMES", n);
    frames.setSelectedId (32, juce::dontSendNotification);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &frames, &loop, &preview, &stop, &createBtn, &cancel })
        addAndMakeVisible (c);
    mode.onChange = [this] (int) { analyse(); repaint(); };
    preview.onClick = [this]
    {
        auto clip = std::make_unique<PreviewClip>();
        const int n = juce::jmax (1, selEnd - selStart);
        clip->l.resize ((size_t) n);
        clip->r.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            clip->l[(size_t) i] = audio.buffer.getSample (0, selStart + i);
            clip->r[(size_t) i] = audio.buffer.getSample (juce::jmin (1, audio.buffer.getNumChannels() - 1), selStart + i);
        }
        clip->rate = audio.rate;
        proc.getEngine().playPreview (std::move (clip));
    };
    stop.onClick = [this] { proc.getEngine().stopPreview(); };
    createBtn.onClick = [this] { create(); };
    cancel.onClick = [this] { proc.getEngine().stopPreview(); if (onClose) onClose(); };
}

void ImportPanel::setAudio (int o, sampleimport::Audio a)
{
    osc = o;
    audio = std::move (a);
    selStart = 0;
    selEnd = audio.length();
    analyse();
    repaint();
}

juce::Rectangle<int> ImportPanel::overviewArea() const
{
    return getLocalBounds().reduced (14).withTrimmedTop (24).withHeight (getHeight() - 150);
}

void ImportPanel::analyse()
{
    const int len = selEnd - selStart;
    period = len > 256 ? sampleimport::detectPeriod (audio, selStart, juce::jmin (len, (int) (audio.rate * 0.5))) : 0.0f;
    const double secs = (double) len / audio.rate;
    info = audio.name + "   " + juce::String (secs, 2) + " s selected   ";
    if (period > 0.0f)
    {
        const float note = sampleimport::periodToNote (period, audio.rate);
        info += "detected " + juce::MidiMessage::getMidiNoteName (juce::roundToInt (note), true, true, 4) + " ("
              + juce::String (audio.rate / period, 1) + " Hz)";
    }
    else
        info += "no stable pitch detected - SLICES recommended";
}

void ImportPanel::create()
{
    auto& doc = proc.getWave (osc);
    const int nf = frames.getSelectedId();
    const int m = mode.getSelected();
    if (m == 0)
    {
        doc.setFrames (sampleimport::extractCycles (audio, selStart, selEnd, nf, period), audio.name);
        proc.setValue (P::osc (osc, OscP::Type), 0.0f);
    }
    else if (m == 1)
    {
        doc.setFrames (sampleimport::sliceFixed (audio, selStart, selEnd, nf), audio.name);
        proc.setValue (P::osc (osc, OscP::Type), 0.0f);
    }
    else
    {
        const float root = period > 0.0f ? sampleimport::periodToNote (period, audio.rate) : 60.0f;
        proc.setSample (osc, sampleimport::toSample (audio, selStart, selEnd, root, loop.getToggleState()));
        proc.setValue (P::osc (osc, OscP::Type), 2.0f);
        proc.setValue (P::osc (osc, OscP::WtPos), 0.0f);
    }
    proc.getEngine().stopPreview();
    if (onClose) onClose();
}

void ImportPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    auto ov = overviewArea().toFloat();
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (ov, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (ov, Metrics::controlRadius, 1.0f);
    const int len = audio.length();
    if (len > 0)
    {
        const float sx = ov.getX() + ov.getWidth() * (float) selStart / (float) len;
        const float ex = ov.getX() + ov.getWidth() * (float) selEnd / (float) len;
        g.setColour (Colours::accent.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (sx, ov.getY(), ex, ov.getBottom()));
        const int W = (int) ov.getWidth();
        g.setColour (Colours::text.withAlpha (0.8f));
        for (int x = 0; x < W; ++x)
        {
            const int a = (int) ((int64_t) len * x / W), b = juce::jmax (a + 1, (int) ((int64_t) len * (x + 1) / W));
            float lo = 0.0f, hi = 0.0f;
            for (int i = a; i < b; i += juce::jmax (1, (b - a) / 64))
            {
                const float v = audio.buffer.getSample (0, i);
                lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
            }
            g.drawVerticalLine ((int) ov.getX() + x, ov.getCentreY() - hi * ov.getHeight() * 0.45f, ov.getCentreY() - lo * ov.getHeight() * 0.45f + 1.0f);
        }
        g.setColour (Colours::accent);
        g.drawVerticalLine (juce::roundToInt (sx), ov.getY(), ov.getBottom());
        g.drawVerticalLine (juce::roundToInt (ex), ov.getY(), ov.getBottom());
    }
    g.setColour (Colours::value);
    g.setFont (font (12.0f));
    g.drawText (info, juce::Rectangle<int> (14, (int) ov.getBottom() + 8, getWidth() - 28, 18), juce::Justification::centredLeft, true);
    g.setColour (Colours::dim);
    g.setFont (font (11.5f));
    const char* help[] = { "CYCLES: single cycles are extracted across the selection (zero-crossing aligned) - best for pitched sounds.",
                           "SLICES: the selection is cut into equal windows, one per frame - best for textures and noises.",
                           "SAMPLE OSC: the selection is played as a pitched sample by this oscillator (root note from pitch detection)." };
    g.drawText (help[juce::jlimit (0, 2, mode.getSelected())], juce::Rectangle<int> (14, (int) ov.getBottom() + 28, getWidth() - 28, 18),
                juce::Justification::centredLeft, true);
    g.drawText ("Drag on the waveform to select a region.", juce::Rectangle<int> (14, 6, getWidth() - 28, 18), juce::Justification::centredRight, false);
}

void ImportPanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto bottom = r.removeFromBottom (28);
    mode.setBounds (bottom.removeFromLeft (300));
    bottom.removeFromLeft (8);
    frames.setBounds (bottom.removeFromLeft (130));
    bottom.removeFromLeft (8);
    loop.setBounds (bottom.removeFromLeft (90));
    cancel.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (6);
    createBtn.setBounds (bottom.removeFromRight (100));
    bottom.removeFromRight (16);
    stop.setBounds (bottom.removeFromRight (70));
    bottom.removeFromRight (6);
    preview.setBounds (bottom.removeFromRight (90));
}

void ImportPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto ov = overviewArea();
    if (! ov.contains (e.getPosition()) || audio.length() == 0) return;
    dragAnchor = (int) ((int64_t) audio.length() * (e.x - ov.getX()) / juce::jmax (1, ov.getWidth()));
    selStart = selEnd = juce::jlimit (0, audio.length(), dragAnchor);
}

void ImportPanel::mouseDrag (const juce::MouseEvent& e)
{
    const auto ov = overviewArea();
    if (audio.length() == 0) return;
    const int pos = juce::jlimit (0, audio.length(), (int) ((int64_t) audio.length() * (e.x - ov.getX()) / juce::jmax (1, ov.getWidth())));
    selStart = juce::jmin (dragAnchor, pos);
    selEnd = juce::jmax (dragAnchor, pos);
    if (selEnd - selStart < 64) selEnd = juce::jmin (audio.length(), selStart + 64);
    analyse();
    repaint();
}

// ======================================================================================= wave editor
WaveEditor::WaveEditor (DeadwaveProcessor& p) : proc (p), canvas (p), strip (p), importPanel (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &oscSel, &undoBtn, &redoBtn, &closeBtn, &tableName, &toolsPanel, &canvasPanel, &framePanel, &rightPanel })
        addAndMakeVisible (c);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &modeSel, &gridSel, &allFrames, &gridCap, &amtCap, &transCap, &amount })
        toolsPanel.addAndMakeVisible (c);
    canvasPanel.addAndMakeVisible (viewSel);
    canvasPanel.addAndMakeVisible (canvas);
    canvasPanel.addAndMakeVisible (hint);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &strip, &addBtn, &dupBtn, &delBtn, &leftBtn, &rightBtn,
                                                                        &markBtn, &interpBtn, &morphSel, &resampleBtn })
        framePanel.addAndMakeVisible (c);
    rightPanel.addAndMakeVisible (rightTabs);
    addChildComponent (importPanel);

    tableName.setFont (font (13.0f, true));
    tableName.setColour (juce::Label::textColourId, Colours::text);
    tableName.setEditable (false, true);
    tableName.setTooltip ("Double-click to rename the wavetable");
    tableName.onTextChange = [this] { proc.getWave (osc).setName (tableName.getText()); };

    oscSel.onChange = [this] (int i) { setOsc (i); };
    undoBtn.onClick = [this] { proc.getWave (osc).undo(); refresh(); };
    redoBtn.onClick = [this] { proc.getWave (osc).redo(); refresh(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };

    modeSel.onChange = [this] (int i)
    {
        canvas.mode = (WaveCanvas::Mode) i;
        if (canvas.view != WaveCanvas::View::Wave2D) { canvas.view = WaveCanvas::View::Wave2D; viewSel.setSelected (0, false); }
        updateInfo();
        canvas.repaint();
    };
    gridSel.onChange = [this] (int i) { canvas.grid = i == 0 ? 0 : (i == 1 ? 8 : i == 2 ? 16 : 32); };
    allFrames.onClick = [this] { canvas.allFrames = allFrames.getToggleState(); };
    allFrames.setTooltip ("Apply drawing, harmonic edits and transforms to every frame");
    amount.setSliderStyle (juce::Slider::LinearBar);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setRange (0.0, 1.0);
    amount.setValue (0.5);
    amount.setTooltip ("Strength for SMOOTH, FOLD, QUANTIZE, SHIFT, STRETCH, COMPRESS, SCALE, OFFSET, SHARPEN, RECTIFY");

    using T = WaveDocument::Transform;
    const std::pair<const char*, T> transforms[] = {
        { "INVERT", T::Invert }, { "REVERSE", T::Reverse }, { "MIRROR", T::MirrorSymmetric }, { "NORMALIZE", T::Normalise },
        { "REMOVE DC", T::RemoveDc }, { "SMOOTH", T::Smooth }, { "RECTIFY", T::Rectify }, { "FOLD", T::Fold },
        { "QUANTIZE", T::Quantise }, { "SHIFT", T::ShiftPhase }, { "STRETCH", T::StretchToCentre }, { "COMPRESS", T::CompressToCentre },
        { "SCALE", T::Scale }, { "OFFSET", T::Offset }, { "SHARPEN", T::Sharpen }, { "HALF CYCLE", T::HalfCycle },
        { "DOUBLE", T::DoubleCycle }, { "CLEAR", T::Clear } };
    for (auto& [name, t] : transforms)
    {
        auto b = std::make_unique<FlatButton> (name, false, 10.0f);
        const auto tt = t;
        b->onClick = [this, tt] { applyTransform (tt); };
        toolsPanel.addAndMakeVisible (*b);
        transformButtons.push_back (std::move (b));
        transformIds.push_back (t);
    }
    transformButtons[12]->setTooltip ("SCALE: multiplies the drawn waveform (amount 0.5 = unchanged). Baked into the table, unlike the AMP knob.");
    transformButtons[13]->setTooltip ("OFFSET: shifts the drawn waveform vertically (amount 0.5 = none). Baked into the table, unlike the BIAS knob.");

    viewSel.onChange = [this] (int i) { canvas.view = (WaveCanvas::View) i; updateInfo(); canvas.repaint(); };
    canvas.onEdited = [this] { strip.repaint(); updateInfo(); };
    hint.setFont (font (11.0f));
    hint.setColour (juce::Label::textColourId, Colours::dim);
    hint.setJustificationType (juce::Justification::centredRight);

    strip.onSelect = [this] { canvas.repaint(); updateInfo(); };
    addBtn.onClick = [this] { proc.getWave (osc).addFrame (false); refresh(); };
    dupBtn.onClick = [this] { proc.getWave (osc).addFrame (true); refresh(); };
    delBtn.onClick = [this] { proc.getWave (osc).deleteFrame (proc.getWave (osc).getSelectedFrame()); refresh(); };
    leftBtn.onClick = [this] { auto& d = proc.getWave (osc); d.moveFrame (d.getSelectedFrame(), d.getSelectedFrame() - 1); refresh(); };
    rightBtn.onClick = [this] { auto& d = proc.getWave (osc); d.moveFrame (d.getSelectedFrame(), d.getSelectedFrame() + 1); refresh(); };
    markBtn.onClick = [this] { strip.markA = proc.getWave (osc).getSelectedFrame(); strip.repaint(); };
    markBtn.setTooltip ("Mark the selected frame as keyframe A");
    interpBtn.setTooltip ("Rebuilds every frame between keyframe A and the selected frame by crossfade or spectral morph");
    interpBtn.onClick = [this]
    {
        if (strip.markA < 0) return;
        proc.getWave (osc).interpolateBetween (strip.markA, proc.getWave (osc).getSelectedFrame(), morphSel.getSelected() == 1);
        refresh();
    };
    resampleBtn.setTooltip ("Change the number of frames (in-betweens are morphed)");
    resampleBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int n : { 1, 2, 4, 8, 16, 32, 64, 128, 256 }) m.addItem (n, juce::String (n) + " frames");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&resampleBtn), [this] (int n)
        {
            if (n > 0) { proc.getWave (osc).resampleFrameCount (n, morphSel.getSelected() == 1); refresh(); }
        });
    };
    morphSel.setSelected (1, false);
    morphSel.fontHeight = 9.5f;

    // right panel
    rightTabs.onChange = [this] (int) { resized(); };
    shapeInfo.setFont (font (11.5f));
    shapeInfo.setColour (juce::Label::textColourId, Colours::dim);
    shapeInfo.setJustificationType (juce::Justification::topLeft);
    shapeInfo.setText ("These controls act on the oscillator output in real time and are independent of each other: "
                       "BIAS moves the waveform relative to its centre line, AMP scales it around that line, PHASE "
                       "rotates the cycle, SHAPE warps it (mode below) and WT POS morphs between frames. The amber "
                       "curve shows the result. DC: removes DC contained in the drawn shape only - BIAS is kept.",
                       juce::dontSendNotification);
    rightPanel.addAndMakeVisible (shapeInfo);

    const auto& names = wavegen::factoryTableNames();
    factoryBox.addItemList (names, 1);
    factoryBox.setSelectedId (1, juce::dontSendNotification);
    loadFactoryBtn.onClick = [this] { proc.getWave (osc).loadFactory (factoryBox.getSelectedId() - 1); refresh(); };
    expr.setMultiLine (true);
    expr.setReturnKeyStartsNewLine (false);
    expr.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain)));
    expr.setText ("sin(tau*x) + f*0.5*sin(3*tau*x)");
    expr.onReturnKey = [this] { generateMath(); };
    for (int n : { 1, 8, 16, 32, 64, 128, 256 }) exprFrames.addItem (juce::String (n) + " FR", n);
    exprFrames.setSelectedId (32, juce::dontSendNotification);
    const char* examples[] = { "sin(tau*x)", "saw(x)", "tri(x)*(1-f) + sqr(x)*f", "sin(tau*x + f*4*sin(tau*x))",
                               "tanh((1+f*12)*sin(tau*x))", "pulse(x, 0.5-0.45*f)", "fold(sin(tau*x)*(1+f*6))",
                               "sin(tau*x)*exp(-x*f*8)", "sign(sin(tau*x))*abs(sin(tau*x))^(1-f*0.9)",
                               "lerp(sin(tau*x), noise(x*(4+f*60)), f*0.5)" };
    exprExamples.addItem ("EXAMPLES", 1000);
    for (int i = 0; i < 10; ++i) exprExamples.addItem (examples[i], i + 1);
    exprExamples.setSelectedId (1000, juce::dontSendNotification);
    exprExamples.onChange = [this]
    {
        if (exprExamples.getSelectedId() > 0 && exprExamples.getSelectedId() < 1000)
            expr.setText (exprExamples.getText());
        exprExamples.setSelectedId (1000, juce::dontSendNotification);
    };
    mathBtn.onClick = [this] { generateMath(); };
    mathError.setFont (font (11.0f));
    mathError.setColour (juce::Label::textColourId, Colours::danger);
    randStyle.addItemList (wavegen::randomStyleNames(), 1);
    randStyle.setSelectedId (1, juce::dontSendNotification);
    for (int n : { 1, 8, 16, 32, 64 }) randFrames.addItem (juce::String (n) + " FR", n);
    randFrames.setSelectedId (16, juce::dontSendNotification);
    randComplexity.setSliderStyle (juce::Slider::LinearBar);
    randComplexity.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    randComplexity.setRange (0.0, 1.0);
    randComplexity.setValue (0.5);
    randComplexity.setTooltip ("Complexity of generated waveforms");
    randBtn.onClick = [this] { generateRandom(); };
    importBtn.onClick = [this] { importAudio(); };
    saveBtn.onClick = [this] { saveTable(); };
    loadBtn.onClick = [this] { loadTable(); };
    importBtn.setTooltip ("Import a WAV/AIFF/FLAC: extract cycles, slice, or load as a sample oscillator");
    saveBtn.setTooltip ("Save this wavetable to Documents/DEADWAVE/Wavetables (32-bit WAV, Serum/Vital compatible)");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &factoryBox, &loadFactoryBtn, &expr, &exprFrames, &exprExamples,
             &mathBtn, &mathError, &randStyle, &randFrames, &randComplexity, &randCurrent, &randBtn, &importBtn, &saveBtn,
             &loadBtn, &facCap, &mathCap, &randCap, &fileCap })
        rightPanel.addChildComponent (c);

    importPanel.onClose = [this] { importPanel.setVisible (false); refresh(); };
    setWantsKeyboardFocus (true);
    setOsc (0);
    startTimerHz (30);
}

WaveEditor::~WaveEditor() = default;

void WaveEditor::setOsc (int o)
{
    osc = o;
    oscSel.setSelected (o, false);
    canvas.setOsc (o);
    strip.setOsc (o);
    strip.markA = -1;
    rebuildShapeKnobs();
    refresh();
}

void WaveEditor::rebuildShapeKnobs()
{
    for (auto& k : shapeKnobs) rightPanel.removeChildComponent (k.get());
    shapeKnobs.clear();
    for (int id : { OscP::WtPos, OscP::Bias, OscP::Amp, OscP::Phase, OscP::Shape, OscP::Level })
    {
        shapeKnobs.push_back (std::make_unique<ParamKnob> (proc, P::osc (osc, id)));
        rightPanel.addChildComponent (*shapeKnobs.back());
    }
    if (warpChoice) rightPanel.removeChildComponent (warpChoice.get());
    if (dcToggle) rightPanel.removeChildComponent (dcToggle.get());
    warpChoice = std::make_unique<ParamChoice> (proc, P::osc (osc, OscP::WarpMode));
    dcToggle = std::make_unique<ParamToggle> (proc, P::osc (osc, OscP::DcRemove), "REMOVE SHAPE DC");
    rightPanel.addChildComponent (*warpChoice);
    rightPanel.addChildComponent (*dcToggle);
    resized();
}

void WaveEditor::refresh()
{
    auto& doc = proc.getWave (osc);
    tableName.setText (doc.getName(), juce::dontSendNotification);
    undoBtn.setEnabled (doc.canUndo());
    redoBtn.setEnabled (doc.canRedo());
    lastVersion = doc.getVersion();
    canvas.repaint();
    strip.repaint();
    updateInfo();
}

void WaveEditor::updateInfo()
{
    auto& doc = proc.getWave (osc);
    juce::String t = "FRAME " + juce::String (doc.getSelectedFrame() + 1) + " / " + juce::String (doc.getNumFrames());
    if (canvas.view == WaveCanvas::View::Wave2D)
    {
        if (canvas.mode == WaveCanvas::Mode::Draw) t += "   DRAG TO DRAW";
        else if (canvas.mode == WaveCanvas::Mode::Points)
            t += doc.getFrame (doc.getSelectedFrame()).pointMode ? "   DRAG POINTS - DOUBLE-CLICK ADDS - RIGHT-CLICK DELETES - ALT-DRAG BENDS"
                                                                  : "   CLICK TO CONVERT THIS FRAME TO CONTROL POINTS";
    }
    hint.setText (t, juce::dontSendNotification);
}

void WaveEditor::timerCallback()
{
    auto& doc = proc.getWave (osc);
    if (doc.getVersion() != lastVersion) refresh();
    for (auto& k : shapeKnobs) k->refreshModulation();
    const float wt = proc.getEngine().getWtPosForUi (osc);
    if (std::abs (wt - lastWt) > 0.002f && canvas.view == WaveCanvas::View::Wave3D) { lastWt = wt; canvas.repaint(); }
    if (canvas.view == WaveCanvas::View::Wave2D && canvas.mode != WaveCanvas::Mode::Harmonics)
    {
        // keep the amber output overlay in sync with knob moves
        auto& key = overlayKey;
        float k[5] = { proc.getValue (P::osc (osc, OscP::Bias)), proc.getValue (P::osc (osc, OscP::Amp)),
                       proc.getValue (P::osc (osc, OscP::Phase)), proc.getValue (P::osc (osc, OscP::Shape)),
                       proc.getValue (P::osc (osc, OscP::WarpMode)) + 10.0f * proc.getValue (P::osc (osc, OscP::DcRemove)) };
        bool changed = false;
        for (int i = 0; i < 5; ++i) if (k[i] != key[i]) { key[i] = k[i]; changed = true; }
        if (changed) canvas.repaint();
    }
}

bool WaveEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.getWave (osc).undo(); refresh(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)) { proc.getWave (osc).redo(); refresh(); return true; }
    if (k == juce::KeyPress::leftKey) { auto& d = proc.getWave (osc); d.setSelectedFrame (d.getSelectedFrame() - 1); refresh(); return true; }
    if (k == juce::KeyPress::rightKey) { auto& d = proc.getWave (osc); d.setSelectedFrame (d.getSelectedFrame() + 1); refresh(); return true; }
    return false;
}

void WaveEditor::applyTransform (WaveDocument::Transform t)
{
    proc.getWave (osc).applyTransform (t, (float) amount.getValue(), allFrames.getToggleState());
    refresh();
}

void WaveEditor::generateMath()
{
    juce::String err;
    proc.getWave (osc).generateFromExpression (expr.getText(), exprFrames.getSelectedId(), err);
    mathError.setText (err, juce::dontSendNotification);
    refresh();
}

void WaveEditor::generateRandom()
{
    randSeed = randSeed * 1664525u + 1013904223u + (uint32_t) juce::Time::getMillisecondCounter();
    proc.getWave (osc).generateRandom ((wavegen::RandomStyle) (randStyle.getSelectedId() - 1), randSeed, (float) randComplexity.getValue(),
                                       randFrames.getSelectedId(), randCurrent.getToggleState());
    refresh();
}

void WaveEditor::importAudio()
{
    chooser = std::make_unique<juce::FileChooser> ("Import audio", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        sampleimport::Audio a;
        juce::String err;
        if (! sampleimport::load (f, a, err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", err);
            return;
        }
        importPanel.setAudio (osc, std::move (a));
        importPanel.setVisible (true);
        importPanel.toFront (true);
    });
}

void WaveEditor::saveTable()
{
    const auto dir = PresetManager::userWavetableDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save wavetable", dir.getChildFile (proc.getWave (osc).getName() + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
        if (! stateio::writeWavetableWav (f, proc.getWave (osc).getAllFrames()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", "Could not write " + f.getFullPathName());
    });
}

void WaveEditor::loadTable()
{
    chooser = std::make_unique<juce::FileChooser> ("Load wavetable", PresetManager::userWavetableDirectory(), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        wavegen::Frames frames;
        juce::String err;
        if (! stateio::readWavetableWav (f, frames, err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load failed", err);
            return;
        }
        proc.getWave (osc).setFrames (frames, f.getFileNameWithoutExtension());
        proc.setValue (P::osc (osc, OscP::Type), 0.0f);
        refresh();
    });
}

void WaveEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    drawTracked (g, "WAVE EDITOR", juce::Rectangle<float> (4.0f, 0.0f, 180.0f, 30.0f), font (14.0f, false, true), Colours::text,
                 juce::Justification::centredLeft, 0.25f);
}

void WaveEditor::layoutLeft (juce::Rectangle<int> r)
{
    r = r.reduced (10, 8);
    r.removeFromTop (20);
    modeSel.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    gridCap.setBounds (r.removeFromTop (12));
    gridSel.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    allFrames.setBounds (r.removeFromTop (22));
    r.removeFromTop (10);
    transCap.setBounds (r.removeFromTop (14));
    r.removeFromTop (4);
    const int bw = (r.getWidth() - 4) / 2;
    for (size_t i = 0; i < transformButtons.size(); ++i)
    {
        const int row = (int) i / 2, col = (int) i % 2;
        transformButtons[i]->setBounds (r.getX() + col * (bw + 4), r.getY() + row * 26, bw, 22);
    }
    r.removeFromTop ((int) (transformButtons.size() + 1) / 2 * 26 + 6);
    amtCap.setBounds (r.removeFromTop (12));
    amount.setBounds (r.removeFromTop (18));
}

void WaveEditor::layoutRight (juce::Rectangle<int> r)
{
    r = r.reduced (10, 8);
    rightTabs.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);
    const bool shapeTab = rightTabs.getSelected() == 0;
    for (auto& k : shapeKnobs) k->setVisible (shapeTab);
    if (warpChoice) warpChoice->setVisible (shapeTab);
    if (dcToggle) dcToggle->setVisible (shapeTab);
    shapeInfo.setVisible (shapeTab);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &factoryBox, &loadFactoryBtn, &expr, &exprFrames, &exprExamples,
             &mathBtn, &mathError, &randStyle, &randFrames, &randComplexity, &randCurrent, &randBtn, &importBtn, &saveBtn,
             &loadBtn, &facCap, &mathCap, &randCap, &fileCap })
        c->setVisible (! shapeTab);

    if (shapeTab)
    {
        if (shapeKnobs.size() < 6) return;
        auto row1 = r.removeFromTop (70);
        layoutRow (row1, { shapeKnobs[0].get(), shapeKnobs[1].get(), shapeKnobs[2].get() }, 64);
        r.removeFromTop (4);
        layoutRow (r.removeFromTop (70), { shapeKnobs[3].get(), shapeKnobs[4].get(), shapeKnobs[5].get() }, 64);
        r.removeFromTop (8);
        if (warpChoice) warpChoice->setBounds (r.removeFromTop (24));
        r.removeFromTop (8);
        if (dcToggle) dcToggle->setBounds (r.removeFromTop (22));
        r.removeFromTop (10);
        shapeInfo.setBounds (r.removeFromTop (juce::jmin (r.getHeight(), 200)));
        return;
    }
    facCap.setBounds (r.removeFromTop (14));
    auto fr = r.removeFromTop (24);
    loadFactoryBtn.setBounds (fr.removeFromRight (56));
    fr.removeFromRight (4);
    factoryBox.setBounds (fr);
    r.removeFromTop (10);
    mathCap.setBounds (r.removeFromTop (14));
    expr.setBounds (r.removeFromTop (52));
    r.removeFromTop (4);
    auto mr = r.removeFromTop (24);
    exprFrames.setBounds (mr.removeFromLeft (72));
    mr.removeFromLeft (4);
    exprExamples.setBounds (mr);
    r.removeFromTop (4);
    mathBtn.setBounds (r.removeFromTop (24));
    mathError.setBounds (r.removeFromTop (16));
    r.removeFromTop (4);
    randCap.setBounds (r.removeFromTop (14));
    auto rr = r.removeFromTop (24);
    randStyle.setBounds (rr.removeFromLeft (rr.getWidth() - 76));
    rr.removeFromLeft (4);
    randFrames.setBounds (rr);
    r.removeFromTop (4);
    randComplexity.setBounds (r.removeFromTop (16));
    r.removeFromTop (4);
    auto rb = r.removeFromTop (24);
    randCurrent.setBounds (rb.removeFromLeft (rb.getWidth() / 2));
    randBtn.setBounds (rb);
    r.removeFromTop (10);
    fileCap.setBounds (r.removeFromTop (14));
    importBtn.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    auto fb = r.removeFromTop (24);
    saveBtn.setBounds (fb.removeFromLeft (fb.getWidth() / 2 - 2));
    fb.removeFromLeft (4);
    loadBtn.setBounds (fb);
}

void WaveEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (30);
    head.removeFromLeft (170);
    closeBtn.setBounds (head.removeFromRight (80).reduced (0, 3));
    head.removeFromRight (8);
    redoBtn.setBounds (head.removeFromRight (64).reduced (0, 3));
    head.removeFromRight (4);
    undoBtn.setBounds (head.removeFromRight (64).reduced (0, 3));
    head.removeFromRight (12);
    oscSel.setBounds (head.removeFromLeft (300).reduced (0, 3));
    head.removeFromLeft (16);
    tableName.setBounds (head);
    r.removeFromTop (6);

    auto left = r.removeFromLeft (196);
    r.removeFromLeft (Metrics::gap);
    auto right = r.removeFromRight (266);
    r.removeFromRight (Metrics::gap);
    toolsPanel.setBounds (left);
    layoutLeft (toolsPanel.getLocalBounds());
    rightPanel.setBounds (right);
    layoutRight (rightPanel.getLocalBounds());

    auto frames = r.removeFromBottom (136);
    r.removeFromBottom (Metrics::gap);
    canvasPanel.setBounds (r);
    {
        auto c = canvasPanel.getLocalBounds().reduced (10, 8);
        auto top = c.removeFromTop (24);
        viewSel.setBounds (top.removeFromLeft (240));
        hint.setBounds (top);
        c.removeFromTop (6);
        canvas.setBounds (c);
    }
    framePanel.setBounds (frames);
    {
        auto c = framePanel.getLocalBounds().reduced (10, 8);
        c.removeFromTop (20);
        auto buttons = c.removeFromBottom (24);
        strip.setBounds (c.withTrimmedBottom (6));
        for (auto* b : { &addBtn, &dupBtn, &delBtn, &leftBtn, &rightBtn })
        {
            b->setBounds (buttons.removeFromLeft (b == &leftBtn || b == &rightBtn ? 28 : (b == &dupBtn ? 84 : 58)));
            buttons.removeFromLeft (4);
        }
        buttons.removeFromLeft (12);
        markBtn.setBounds (buttons.removeFromLeft (70));
        buttons.removeFromLeft (4);
        interpBtn.setBounds (buttons.removeFromLeft (140));
        buttons.removeFromLeft (4);
        morphSel.setBounds (buttons.removeFromLeft (124));
        buttons.removeFromLeft (6);
        resampleBtn.setBounds (buttons);
    }
    importPanel.setBounds (getLocalBounds().reduced (40, 50));
}
} // namespace dw::ui

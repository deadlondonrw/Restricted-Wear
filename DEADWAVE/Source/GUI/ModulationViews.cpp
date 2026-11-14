#include "ModulationViews.h"
#include <complex>

namespace dw::ui
{
namespace
{
juce::String groupName (Group g)
{
    switch (g)
    {
        case Group::Osc1: return "OSC 1"; case Group::Osc2: return "OSC 2"; case Group::Osc3: return "OSC 3"; case Group::Osc4: return "OSC 4";
        case Group::Filter1: return "FILTER 1"; case Group::Filter2: return "FILTER 2";
        case Group::Env1: return "ENV 1"; case Group::Env2: return "ENV 2"; case Group::Env3: return "ENV 3"; case Group::Env4: return "ENV 4";
        case Group::Lfo1: return "LFO 1"; case Group::Lfo2: return "LFO 2"; case Group::Lfo3: return "LFO 3"; case Group::Lfo4: return "LFO 4";
        case Group::Voice: return "VOICE"; case Group::Analog: return "ANALOG"; case Group::Global: return "GLOBAL";
        case Group::FxMaster: return "MASTER FX"; case Group::FxOsc1: return "OSC 1 FX"; case Group::FxOsc2: return "OSC 2 FX";
        case Group::FxOsc3: return "OSC 3 FX"; case Group::FxOsc4: return "OSC 4 FX";
        default: return "OTHER";
    }
}
} // namespace

void showDestinationMenu (juce::Component* target, std::function<void (const juce::String&)> onPick)
{
    const auto& reg = Registry::get();
    juce::PopupMenu m;
    std::map<int, juce::PopupMenu> subs;
    for (int idx : reg.modDestinations())
        subs[(int) reg.spec (idx).group].addItem (idx + 1, reg.spec (idx).name);
    for (auto& [g, sub] : subs)
        m.addSubMenu (groupName ((Group) g), sub);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [onPick] (int r)
    {
        if (r > 0) onPick (Registry::get().spec (r - 1).id);
    });
}

// ================================================================================== filter response
void FilterResponse::refresh()
{
    const float cutN = proc.getEngine().getModulatedNormalised (P::filt (f, FiltP::Cutoff));
    float k[6] = { cutN >= 0.0f ? cutN : -1.0f, proc.getValue (P::filt (f, FiltP::Cutoff)), proc.getValue (P::filt (f, FiltP::Reso)),
                   proc.getValue (P::filt (f, FiltP::Type)), proc.getValue (P::filt (f, FiltP::On)), proc.getValue (P::filt (f, FiltP::Mix)) };
    bool changed = false;
    for (int i = 0; i < 6; ++i) if (std::abs (k[i] - key[i]) > 1.0e-4f) changed = true;
    if (changed) { std::copy (k, k + 6, key); repaint(); }
}

void FilterResponse::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    auto plot = r.reduced (4.0f, 6.0f);
    g.setColour (Colours::grid);
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
    }

    const auto& spec = Registry::get().spec (P::filt (f, FiltP::Cutoff));
    const float cutoff = key[0] >= 0.0f ? spec.range().convertFrom0to1 (key[0]) : key[1];
    const float res = key[2];
    const int type = (int) key[3];
    const bool on = key[4] >= 0.5f;
    const float mix = key[5];
    const float kq = 2.0f - 1.97f * res;

    auto mag = [&] (float hz) -> float
    {
        using C = std::complex<double>;
        const C s (0.0, hz / cutoff);
        auto svf = [&] (double k) { return s * s + k * s + 1.0; };
        C h;
        switch (type)
        {
            case 0: h = 1.0 / svf (kq); break;
            case 1: h = 1.0 / (svf (kq) * svf (1.4142)); break;
            case 2: h = s * s / svf (kq); break;
            case 3: h = (s * s / svf (kq)) * (s * s / svf (1.4142)); break;
            case 4: h = (double) kq * s / svf (kq); break;
            case 5: h = ((double) kq * s / svf (kq)) * (1.4142 * s / svf (1.4142)); break;
            case 6: h = (s * s + 1.0) / svf (kq); break;
            case 7: h = (1.0 - s * s) / svf (kq); break;
            case 8: h = 1.0; break;
            case 9: { const C a = (1.0 + s); h = 1.0 / (a * a * a * a + (double) res * 3.96); h *= (1.0 + 0.5 * res * 3.96); break; }
            case 10: { const C a = (1.0 + s); h = (s * s * s * s) / (a * a * a * a + (double) res * 3.96); break; }
            case 11: case 12:
            {
                const double fb = (type == 11 ? 1.0 : -1.0) * (0.3 + 0.69 * res);
                const double w = 2.0 * juce::MathConstants<double>::pi * hz / cutoff;
                h = (1.0 - std::abs (fb) * 0.5) / (1.0 - fb * std::exp (C (0.0, -w)));
                break;
            }
            default:
            {
                double acc = 0.0;
                for (double fc : { 700.0, 1200.0, 2600.0 })
                {
                    const C ss (0.0, hz / fc);
                    acc += std::abs (0.5 * ss / (ss * ss + 0.5 * ss + 1.0));
                }
                return (float) acc;
            }
        }
        const C total = 1.0 + (h - 1.0) * (double) mix;
        return (float) std::abs (total);
    };

    juce::Path p;
    const int N = juce::jmax (32, (int) plot.getWidth());
    for (int i = 0; i <= N; ++i)
    {
        const float t = (float) i / (float) N;
        const float hz = 20.0f * std::pow (1000.0f, t);
        const float m = on ? mag (hz) : 1.0f;
        const float db = juce::jlimit (-36.0f, 24.0f, 20.0f * std::log10 (juce::jmax (1.0e-5f, m)));
        const float y = plot.getY() + plot.getHeight() * (1.0f - (db + 36.0f) / 60.0f);
        const float x = plot.getX() + t * plot.getWidth();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    auto fill = p;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setColour (Colours::text.withAlpha (on ? 0.06f : 0.02f));
    g.fillPath (fill);
    g.setColour (on ? Colours::text : Colours::muted);
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

// ===================================================================================== filter panel
FilterPanel::FilterPanel (DeadwaveProcessor& p, int filter)
    : proc (p), f (filter), on (p, P::filt (filter, FiltP::On)), type (p, P::filt (filter, FiltP::Type)), response (p, filter),
      cutoff (p, P::filt (filter, FiltP::Cutoff)), reso (p, P::filt (filter, FiltP::Reso)), drive (p, P::filt (filter, FiltP::Drive)),
      mix (p, P::filt (filter, FiltP::Mix)), key (p, P::filt (filter, FiltP::KeyTrack)), env (p, P::filt (filter, FiltP::EnvAmt)),
      spread (p, P::filt (filter, FiltP::Spread))
{
    for (auto* c : std::initializer_list<juce::Component*> { &on, &type, &response, &cutoff, &reso, &drive, &mix, &key, &env, &spread })
        addAndMakeVisible (c);
    for (auto* k : { &key, &env, &spread }) k->setCompact (true);
    env.setTooltip ("Envelope 2 amount (+/- 8 octaves)");
    if (filter == 1)
    {
        routing = std::make_unique<Segmented> (p, P::global (GlobalP::FilterRouting), juce::StringArray { "SERIAL", "PARALLEL" });
        routing->fontHeight = 9.0f;
        addAndMakeVisible (*routing);
    }
}

void FilterPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    drawTracked (g, "FILTER " + juce::String (f + 1), juce::Rectangle<float> (50.0f, 8.0f, 90.0f, 18.0f), font (12.5f, true),
                 proc.getValue (P::filt (f, FiltP::On)) >= 0.5f ? Colours::text : Colours::secondary, juce::Justification::centredLeft, 0.14f);
}

void FilterPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto head = r.removeFromTop (20);
    on.setBounds (head.removeFromLeft (36));
    type.setBounds (head.removeFromRight (98));
    r.removeFromTop (6);
    if (routing)
    {
        routing->setBounds (r.removeFromBottom (20));
        r.removeFromBottom (4);
    }
    response.setBounds (r.removeFromTop (64));
    r.removeFromTop (4);
    auto big = r.removeFromTop (juce::jmin (78, r.getHeight() / 2 + 6));
    layoutRow (big, { &cutoff, &reso, &drive, &mix }, juce::jmin (64, big.getWidth() / 4));
    layoutRow (r.removeFromTop (48), { &key, &env, &spread }, 46);
}

void FilterPanel::refresh()
{
    response.refresh();
    for (auto* k : { &cutoff, &reso, &drive, &mix, &key, &env, &spread }) k->refreshModulation();
    repaint (0, 0, getWidth(), 28);
}

// ======================================================================================== env graph
void EnvGraph::refresh()
{
    float k[10];
    for (int i = 0; i < 9; ++i) k[i] = proc.getValue (P::env (env, i));
    k[9] = proc.getEngine().getModSourceValueForUi ((ModSrc) ((int) ModSrc::Env1 + env));
    bool changed = false;
    for (int i = 0; i < 10; ++i) if (std::abs (k[i] - key[i]) > 1.0e-4f) changed = true;
    if (changed) { std::copy (k, k + 10, key); repaint(); }
}

void EnvGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    auto plot = r.reduced (8.0f, 8.0f);

    const float dly = key[EnvP::Delay], a = key[EnvP::Attack], h = key[EnvP::Hold], d = key[EnvP::Decay];
    const float s = key[EnvP::Sustain], rel = key[EnvP::Release];
    // Display time warping: sqrt keeps short and long segments readable.
    auto tw = [] (float t) { return std::sqrt (juce::jmax (0.0f, t)) + 0.04f; };
    const float sustainW = 0.35f;
    const float total = tw (dly) + tw (a) + tw (h) + tw (d) + sustainW + tw (rel);
    const float W = plot.getWidth() / total;
    juce::Path p;
    float x = plot.getX();
    const float y0 = plot.getBottom(), y1 = plot.getY();
    auto Y = [&] (float v) { return y0 - (y0 - y1) * v; };
    p.startNewSubPath (x, Y (0.0f));
    x += tw (dly) * W;
    p.lineTo (x, Y (0.0f));
    auto seg = [&] (float from, float to, float len, float curve)
    {
        const int n = 24;
        const float sx = x;
        for (int i = 1; i <= n; ++i)
        {
            const float u = (float) i / n;
            p.lineTo (sx + len * u, Y (from + (to - from) * Envelope::curve (u, curve)));
        }
        x = sx + len;
    };
    seg (0.0f, 1.0f, tw (a) * W, key[EnvP::AttackCurve]);
    p.lineTo (x + tw (h) * W, Y (1.0f));
    x += tw (h) * W;
    seg (1.0f, s, tw (d) * W, key[EnvP::DecayCurve]);
    p.lineTo (x + sustainW * W, Y (s));
    x += sustainW * W;
    seg (s, 0.0f, tw (rel) * W, key[EnvP::ReleaseCurve]);

    auto fill = p;
    fill.lineTo (x, y0);
    fill.lineTo (plot.getX(), y0);
    fill.closeSubPath();
    g.setColour (Colours::text.withAlpha (0.05f));
    g.fillPath (fill);
    g.setColour (Colours::text);
    g.strokePath (p, juce::PathStrokeType (1.6f));

    const float live = key[9];
    if (live > 0.001f)
    {
        g.setColour (Colours::accent);
        g.fillRect (plot.getRight() - 3.0f, Y (live), 3.0f, y0 - Y (live));
    }
}

// ======================================================================================== env panel
EnvPanel::EnvPanel (DeadwaveProcessor& p, bool isLarge) : Panel ("ENVELOPES"), proc (p), large (isLarge), graph (p)
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (graph);
    tabs.fontHeight = 9.5f;
    tabs.onChange = [this] (int i) { select (i); };
    for (int e = 0; e < kNumEnvs; ++e)
    {
        std::vector<int> ids = { EnvP::Delay, EnvP::Attack, EnvP::Hold, EnvP::Decay, EnvP::Sustain, EnvP::Release };
        if (large) { ids.push_back (EnvP::AttackCurve); ids.push_back (EnvP::DecayCurve); ids.push_back (EnvP::ReleaseCurve); }
        for (int id : ids)
        {
            knobs[e].push_back (std::make_unique<ParamKnob> (p, P::env (e, id)));
            knobs[e].back()->setCompact (! large);
            addChildComponent (*knobs[e].back());
        }
    }
    select (0);
}

void EnvPanel::select (int e)
{
    current = e;
    tabs.setSelected (e, false);
    graph.setEnv (e);
    for (int i = 0; i < kNumEnvs; ++i)
        for (auto& k : knobs[i]) k->setVisible (i == e);
    resized();
}

void EnvPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (20);
    tabs.setBounds (r.removeFromTop (22).withTrimmedLeft (large ? 0 : 0));
    r.removeFromTop (6);
    auto& ks = knobs[current];
    if (large)
    {
        auto bottom = r.removeFromBottom (130);
        graph.setBounds (r.reduced (0, 2));
        auto row1 = bottom.removeFromTop (64);
        layoutRow (row1, { ks[0].get(), ks[1].get(), ks[2].get(), ks[3].get(), ks[4].get(), ks[5].get() }, 58);
        bottom.removeFromTop (4);
        layoutRow (bottom.removeFromTop (58).reduced (bottom.getWidth() / 5, 0), { ks[6].get(), ks[7].get(), ks[8].get() }, 54);
    }
    else
    {
        graph.setBounds (r.removeFromTop (r.getHeight() - 56));
        r.removeFromTop (4);
        layoutRow (r, { ks[0].get(), ks[1].get(), ks[2].get(), ks[3].get(), ks[4].get(), ks[5].get() }, 40);
    }
}

void EnvPanel::refresh()
{
    graph.refresh();
    for (auto& k : knobs[current]) k->refreshModulation();
}

// ===================================================================================== LFO editor
int LfoShapeEditor::hitPoint (juce::Point<float> p) const
{
    const auto& pts = proc.getLfoPoints (lfo);
    const auto pl = plot();
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const juce::Point<float> c (pl.getX() + pts[(size_t) i].x * pl.getWidth(), pl.getBottom() - pts[(size_t) i].y * pl.getHeight());
        if (c.getDistanceFrom (p) < 7.0f) return i;
    }
    return -1;
}

void LfoShapeEditor::commit (stateio::LfoPoints pts)
{
    std::sort (pts.begin(), pts.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
    proc.setLfoPoints (lfo, pts);
    if ((int) proc.getValue (P::lfo (lfo, LfoP::Shape)) != 7)
        proc.setValue (P::lfo (lfo, LfoP::Shape), 7.0f); // editing selects the Custom shape
    repaint();
}

void LfoShapeEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! editable) return;
    auto pts = proc.getLfoPoints (lfo);
    selected = hitPoint (e.position);
    curveSeg = -1;
    if (e.mods.isPopupMenu())
    {
        if (selected >= 0 && pts.size() > 2)
        {
            pts.erase (pts.begin() + selected);
            selected = -1;
            commit (pts);
        }
        return;
    }
    if (selected < 0 && e.mods.isAltDown())
    {
        // alt-drag a segment to bend its curve
        const auto pl = plot();
        const float x = (e.position.x - pl.getX()) / pl.getWidth();
        auto sorted = pts;
        std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
        int seg = (int) sorted.size() - 1;
        for (int i = 0; i < (int) sorted.size(); ++i) if (sorted[(size_t) i].x <= x) seg = i;
        proc.setLfoPoints (lfo, sorted);
        curveSeg = seg;
        curveStart = sorted[(size_t) seg].curve;
        dragStartY = e.position.y;
    }
}

void LfoShapeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! editable) return;
    auto pts = proc.getLfoPoints (lfo);
    const auto pl = plot();
    if (curveSeg >= 0 && curveSeg < (int) pts.size())
    {
        pts[(size_t) curveSeg].curve = juce::jlimit (-1.0f, 1.0f, curveStart + (dragStartY - e.position.y) / 80.0f);
        proc.setLfoPoints (lfo, pts);
        if ((int) proc.getValue (P::lfo (lfo, LfoP::Shape)) != 7) proc.setValue (P::lfo (lfo, LfoP::Shape), 7.0f);
        repaint();
        return;
    }
    if (selected < 0 || selected >= (int) pts.size()) return;
    float x = juce::jlimit (0.0f, 1.0f, (e.position.x - pl.getX()) / pl.getWidth());
    float y = juce::jlimit (0.0f, 1.0f, (pl.getBottom() - e.position.y) / pl.getHeight());
    if (e.mods.isShiftDown() && gridDivisions > 0)
    {
        x = std::round (x * gridDivisions) / gridDivisions;
        y = std::round (y * 8.0f) / 8.0f;
    }
    pts[(size_t) selected].x = x;
    pts[(size_t) selected].y = y;
    proc.setLfoPoints (lfo, pts);
    if ((int) proc.getValue (P::lfo (lfo, LfoP::Shape)) != 7) proc.setValue (P::lfo (lfo, LfoP::Shape), 7.0f);
    repaint();
}

void LfoShapeEditor::mouseUp (const juce::MouseEvent&)
{
    if (selected >= 0)
    {
        // re-sort and keep the dragged point selected
        auto pts = proc.getLfoPoints (lfo);
        const auto moved = pts[(size_t) selected];
        std::sort (pts.begin(), pts.end(), [] (const auto& a, const auto& b) { return a.x < b.x; });
        proc.setLfoPoints (lfo, pts);
        for (int i = 0; i < (int) pts.size(); ++i)
            if (pts[(size_t) i].x == moved.x && pts[(size_t) i].y == moved.y) selected = i;
    }
    curveSeg = -1;
}

void LfoShapeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! editable) return;
    auto pts = proc.getLfoPoints (lfo);
    if (hitPoint (e.position) >= 0 || pts.size() >= 64) return;
    const auto pl = plot();
    pts.push_back ({ juce::jlimit (0.0f, 1.0f, (e.position.x - pl.getX()) / pl.getWidth()),
                     juce::jlimit (0.0f, 1.0f, (pl.getBottom() - e.position.y) / pl.getHeight()), 0.0f });
    commit (pts);
}

void LfoShapeEditor::refresh()
{
    const float ph = (float) proc.getEngine().getLfoPhaseForUi (lfo);
    if (std::abs (ph - lastPhase) > 0.002f) { lastPhase = ph; repaint(); }
}

void LfoShapeEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    const auto pl = plot();
    g.setColour (Colours::grid);
    for (int i = 1; i < gridDivisions; ++i)
        g.drawVerticalLine (juce::roundToInt (pl.getX() + pl.getWidth() * (float) i / (float) gridDivisions), pl.getY(), pl.getBottom());
    g.drawHorizontalLine (juce::roundToInt (pl.getCentreY()), pl.getX(), pl.getRight());

    const int shape = (int) proc.getValue (P::lfo (lfo, LfoP::Shape));
    LfoSettings s;
    s.shape = shape;
    auto table = stateio::renderLfo (proc.getLfoPoints (lfo));
    s.custom = table.get();
    Lfo tmp;
    juce::Path p;
    const int N = juce::jmax (64, (int) pl.getWidth());
    for (int i = 0; i <= N; ++i)
    {
        const float x = (float) i / (float) N;
        float v;
        if (shape == 5 || shape == 6) // random shapes: show an illustrative pattern
        {
            Rng rng (77u + (uint32_t) (x * 8.0f));
            const float a = rng.uni();
            Rng rng2 (77u + (uint32_t) (x * 8.0f) + 1u);
            const float b = rng2.uni();
            const float u = x * 8.0f - std::floor (x * 8.0f);
            v = shape == 5 ? a : a + (b - a) * (0.5f - 0.5f * std::cos (kPi * u));
        }
        else
            v = tmp.shapeValue (s, x);
        const float px = pl.getX() + x * pl.getWidth(), py = pl.getBottom() - v * pl.getHeight();
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (Colours::text);
    g.strokePath (p, juce::PathStrokeType (1.6f));

    if (shape == 7 && editable)
    {
        const auto& pts = proc.getLfoPoints (lfo);
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const float px = pl.getX() + pts[(size_t) i].x * pl.getWidth(), py = pl.getBottom() - pts[(size_t) i].y * pl.getHeight();
            g.setColour (i == selected ? Colours::accent : Colours::bg);
            g.fillEllipse (px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour (i == selected ? Colours::accent : Colours::text);
            g.drawEllipse (px - 4.0f, py - 4.0f, 8.0f, 8.0f, 1.3f);
        }
    }
    if (lastPhase >= 0.0f && (int) proc.getValue (P::lfo (lfo, LfoP::Mode)) == 0)
    {
        const float px = pl.getX() + lastPhase * pl.getWidth();
        g.setColour (Colours::accent.withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (px), pl.getY(), pl.getBottom());
    }
    if (shape != 7 && editable && getHeight() > 120)
    {
        g.setColour (Colours::muted);
        g.setFont (font (10.5f));
        g.drawText ("Double-click to add points (switches to CUSTOM). Right-click deletes, Alt-drag bends, Shift snaps.",
                    r.reduced (8.0f, 4.0f), juce::Justification::bottomLeft, true);
    }
}

// ======================================================================================== LFO panel
LfoPanel::LfoPanel (DeadwaveProcessor& p, bool isLarge) : Panel ("LFO"), proc (p), large (isLarge), editor (p)
{
    addAndMakeVisible (tabs);
    addAndMakeVisible (editor);
    tabs.fontHeight = 9.5f;
    tabs.onChange = [this] (int i) { select (i); };
    editor.editable = large;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& s = sets[l];
        s.shape = std::make_unique<ParamChoice> (p, P::lfo (l, LfoP::Shape));
        s.div = std::make_unique<ParamChoice> (p, P::lfo (l, LfoP::Div));
        s.mode = std::make_unique<Segmented> (p, P::lfo (l, LfoP::Mode), juce::StringArray { "FREE", "RETRIG", "ONCE" });
        s.mode->fontHeight = 9.0f;
        s.sync = std::make_unique<ParamToggle> (p, P::lfo (l, LfoP::Sync), "SYNC");
        s.bipolar = std::make_unique<ParamToggle> (p, P::lfo (l, LfoP::Bipolar), "BIPOLAR");
        s.rate = std::make_unique<ParamKnob> (p, P::lfo (l, LfoP::Rate));
        s.phase = std::make_unique<ParamKnob> (p, P::lfo (l, LfoP::Phase));
        s.smooth = std::make_unique<ParamKnob> (p, P::lfo (l, LfoP::Smooth));
        s.rise = std::make_unique<ParamKnob> (p, P::lfo (l, LfoP::Rise));
        for (juce::Component* c : std::initializer_list<juce::Component*> { s.shape.get(), s.div.get(), s.mode.get(), s.sync.get(),
                                                                            s.bipolar.get(), s.rate.get(), s.phase.get(), s.smooth.get(), s.rise.get() })
            addChildComponent (c);
        for (auto* k : { s.rate.get(), s.phase.get(), s.smooth.get(), s.rise.get() }) k->setCompact (! large);
    }
    if (large)
    {
        presetButton = std::make_unique<FlatButton> ("SHAPES");
        presetButton->setTooltip ("Load a starting shape into the custom LFO editor");
        addAndMakeVisible (*presetButton);
        presetButton->onClick = [this]
        {
            juce::PopupMenu m;
            const char* names[] = { "Triangle", "Ramp Up", "Ramp Down", "Square", "Steps 8", "Bounce", "Pluck", "Gate 3:1" };
            for (int i = 0; i < 8; ++i) m.addItem (i + 1, names[i]);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton.get()), [this] (int r)
            {
                if (r <= 0) return;
                stateio::LfoPoints pts;
                switch (r)
                {
                    case 1: pts = { { 0, 0, 0 }, { 0.5f, 1, 0 } }; break;
                    case 2: pts = { { 0, 0, 0 }, { 0.999f, 1, 0 } }; break;
                    case 3: pts = { { 0, 1, 0 }, { 0.999f, 0, 0 } }; break;
                    case 4: pts = { { 0, 1, 0 }, { 0.499f, 1, 0 }, { 0.5f, 0, 0 }, { 0.999f, 0, 0 } }; break;
                    case 5: for (int k = 0; k < 8; ++k) { pts.push_back ({ k / 8.0f, k / 7.0f, 0 }); pts.push_back ({ (k + 1) / 8.0f - 0.001f, k / 7.0f, 0 }); } break;
                    case 6: pts = { { 0, 0, 0.8f }, { 0.25f, 1, -0.8f }, { 0.5f, 0, 0.8f }, { 0.75f, 0.6f, -0.8f } }; break;
                    case 7: pts = { { 0, 1, 0.85f }, { 0.999f, 0, 0 } }; break;
                    default: pts = { { 0, 1, 0 }, { 0.74f, 1, 0 }, { 0.75f, 0, 0 }, { 0.999f, 0, 0 } }; break;
                }
                proc.setLfoPoints (current, pts);
                proc.setValue (P::lfo (current, LfoP::Shape), 7.0f);
                editor.repaint();
            });
        };
    }
    select (0);
}

void LfoPanel::select (int l)
{
    current = l;
    tabs.setSelected (l, false);
    editor.setLfo (l);
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& s = sets[i];
        for (juce::Component* c : std::initializer_list<juce::Component*> { s.shape.get(), s.div.get(), s.mode.get(), s.sync.get(),
                                                                            s.bipolar.get(), s.rate.get(), s.phase.get(), s.smooth.get(), s.rise.get() })
            c->setVisible (i == l);
    }
    refresh();
    resized();
}

void LfoPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (20);
    auto tabRow = r.removeFromTop (22);
    if (presetButton) presetButton->setBounds (tabRow.removeFromRight (70));
    tabs.setBounds (tabRow);
    r.removeFromTop (6);
    auto& s = sets[current];
    if (large)
    {
        auto bottom = r.removeFromBottom (120);
        editor.setBounds (r.reduced (0, 2));
        auto row = bottom.removeFromTop (24);
        s.shape->setBounds (row.removeFromLeft (120));
        row.removeFromLeft (8);
        s.mode->setBounds (row.removeFromLeft (180));
        row.removeFromLeft (8);
        s.sync->setBounds (row.removeFromLeft (84));
        row.removeFromLeft (6);
        s.bipolar->setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        s.div->setBounds (row.removeFromLeft (90));
        bottom.removeFromTop (8);
        layoutRow (bottom.removeFromTop (70).withTrimmedRight (bottom.getWidth() / 3), { s.rate.get(), s.phase.get(), s.smooth.get(), s.rise.get() }, 62);
    }
    else
    {
        auto controls = r.removeFromBottom (72);
        editor.setBounds (r);
        auto row = controls.removeFromTop (22);
        s.shape->setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        row.removeFromLeft (4);
        s.mode->setBounds (row);
        controls.removeFromTop (4);
        auto k = controls;
        auto left = k.removeFromLeft (78);
        s.sync->setBounds (left.removeFromTop (22));
        s.div->setBounds (left.removeFromTop (22).withWidth (74));
        layoutRow (k, { s.rate.get(), s.phase.get(), s.smooth.get() }, 42);
    }
}

void LfoPanel::refresh()
{
    editor.refresh();
    auto& s = sets[current];
    const bool sync = proc.getValue (P::lfo (current, LfoP::Sync)) >= 0.5f;
    s.div->setEnabled (sync);
    s.rate->setEnabled (! sync);
    for (auto* k : { s.rate.get(), s.phase.get(), s.smooth.get(), s.rise.get() }) k->refreshModulation();
}

// ====================================================================================== voice panel
VoicePanel::VoicePanel (DeadwaveProcessor& p)
    : Panel ("VOICE & GLIDE"), proc (p),
      mode (p, P::voice (VoiceP::Mode), { "POLY", "MONO", "LEGATO" }),
      glide (p, P::voice (VoiceP::GlideMode), { "OFF", "ALWAYS", "OVERLAP" }),
      voices (p, P::voice (VoiceP::Polyphony)), time (p, P::voice (VoiceP::GlideTime)), curve (p, P::voice (VoiceP::GlideCurve)),
      bendUp (p, P::voice (VoiceP::BendUp)), bendDown (p, P::voice (VoiceP::BendDown)), vel (p, P::voice (VoiceP::VelSens)),
      priority (p, P::voice (VoiceP::Priority)), constRate (p, P::voice (VoiceP::GlideConstRate), "RATE"),
      hard (p, P::voice (VoiceP::Retrigger), "HARD"), ret (p, P::voice (VoiceP::ReturnToHeld), "RETURN")
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &glide, &voices, &time, &curve, &bendUp, &bendDown,
                                                                        &vel, &priority, &constRate, &hard, &ret, &prioCap })
        addAndMakeVisible (c);
    for (auto* k : { &voices, &time, &curve, &bendUp, &bendDown, &vel }) k->setCompact (true);
    mode.fontHeight = glide.fontHeight = 9.0f;
    constRate.setTooltip ("Constant rate: glide time is per octave instead of per note change");
    hard.setTooltip ("Hard retrigger: mono envelopes restart from zero (punchier, may click)");
    ret.setTooltip ("Return to held note: releasing a note glides back to a key that is still held");
    glide.setTooltip ("OVERLAP glides only when notes overlap - overlap notes in the FL Studio piano roll to slide");
}

void VoicePanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (22);
    mode.setBounds (r.removeFromTop (22));
    r.removeFromTop (5);
    glide.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    layoutRow (r.removeFromTop (50), { &voices, &time, &curve, &vel }, 44);
    r.removeFromTop (2);
    auto row = r.removeFromTop (20).withSizeKeepingCentre (r.getWidth(), 16);
    constRate.setBounds (row.removeFromLeft (64));
    hard.setBounds (row.removeFromLeft (66));
    ret.setBounds (row);
    r.removeFromTop (4);
    auto last = r.removeFromTop (48);
    auto pc = last.removeFromLeft (88);
    prioCap.setBounds (pc.removeFromTop (12));
    priority.setBounds (pc.removeFromTop (20));
    layoutRow (last, { &bendUp, &bendDown }, 44);
}

void VoicePanel::refresh()
{
    for (auto* k : { &voices, &time, &curve, &bendUp, &bendDown, &vel }) k->refreshModulation();
    const bool poly = (int) proc.getValue (P::voice (VoiceP::Mode)) == 0;
    voices.setEnabled (poly);
    priority.setEnabled (! poly);
}

// ===================================================================================== macro panel
MacroPanel::MacroPanel (DeadwaveProcessor& p) : Panel ("MACROS")
{
    for (int i = 0; i < kNumMacros; ++i)
    {
        knobs.push_back (std::make_unique<ParamKnob> (p, P::macro (i)));
        chips.push_back (std::make_unique<ModSourceChip> (p, (ModSrc) ((int) ModSrc::Macro1 + i)));
        addAndMakeVisible (*knobs.back());
        addAndMakeVisible (*chips.back());
    }
}

void MacroPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (22);
    const int w = r.getWidth() / kNumMacros;
    for (int i = 0; i < kNumMacros; ++i)
    {
        auto c = r.withX (r.getX() + i * w).withWidth (w).reduced (6, 0);
        chips[(size_t) i]->setBounds (c.removeFromBottom (24));
        c.removeFromBottom (4);
        knobs[(size_t) i]->setBounds (c);
    }
}

void MacroPanel::refresh()
{
    for (auto& c : chips) c->refresh();
}

// ==================================================================================== analog panel
AnalogPanel::AnalogPanel (DeadwaveProcessor& p) : Panel ("ANALOG CHARACTER")
{
    for (int i = 0; i < AnalogP::Count; ++i)
    {
        knobs.push_back (std::make_unique<ParamKnob> (p, P::analog (i)));
        addAndMakeVisible (*knobs.back());
    }
    knobs[0]->setTooltip ("Slow random pitch drift per voice and oscillator");
    knobs[1]->setTooltip ("Per-note random detune of each oscillator");
    knobs[2]->setTooltip ("Voice saturation after the filters");
    knobs[3]->setTooltip ("Filter resonance saturation (analog-style nonlinear feedback)");
    knobs[4]->setTooltip ("Random stereo placement per unison voice and note");
}

void AnalogPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (22);
    layoutRow (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 76)),
               { knobs[0].get(), knobs[1].get(), knobs[2].get(), knobs[3].get(), knobs[4].get() }, 62);
}

void AnalogPanel::refresh()
{
    for (auto& k : knobs) k->refreshModulation();
}

// ===================================================================================== matrix view
MatrixView::Row::Row (DeadwaveProcessor& p, int s) : proc (p), slot (s)
{
    for (int i = 0; i < (int) ModSrc::NumSources; ++i)
        src.addItem (i == 0 ? juce::String ("-") : modSourceName ((ModSrc) i), i + 1);
    src.onChange = [this]
    {
        const auto chosen = (ModSrc) (src.getSelectedId() - 1);
        const auto& r = proc.getRouting (slot);
        if (chosen == ModSrc::None) proc.clearRouting (slot);
        else proc.setRouting (slot, chosen, r.dstId);
    };
    dst.onClick = [this]
    {
        showDestinationMenu (&dst, [this] (const juce::String& id)
        {
            auto s2 = proc.getRouting (slot).src;
            if (s2 == ModSrc::None) s2 = ModSrc::Lfo1;
            proc.setRouting (slot, s2, id);
            if (std::abs (proc.getValue (P::modAmt (slot))) < 1.0e-4f) proc.setValue (P::modAmt (slot), 0.3f);
        });
    };
    amount.setSliderStyle (juce::Slider::LinearBar);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.getProperties().set ("bipolar", true);
    att = std::make_unique<juce::SliderParameterAttachment> (*proc.param (P::modAmt (slot)), amount);
    remove.onClick = [this] { proc.clearRouting (slot); };
    remove.setTooltip ("Remove this routing");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &src, &dst, &amount, &remove }) addAndMakeVisible (c);
    update();
}

void MatrixView::Row::update()
{
    const auto& r = proc.getRouting (slot);
    src.setSelectedId ((int) r.src + 1, juce::dontSendNotification);
    const int idx = Registry::get().indexOf (r.dstId);
    dst.setButtonText (idx >= 0 ? Registry::get().spec (idx).name : juce::String ("Choose destination"));
    repaint();
}

void MatrixView::Row::resized()
{
    auto r = getLocalBounds().reduced (4, 4);
    r.removeFromLeft (34);
    src.setBounds (r.removeFromLeft (150));
    r.removeFromLeft (28);
    dst.setBounds (r.removeFromLeft (300));
    r.removeFromLeft (16);
    remove.setBounds (r.removeFromRight (28));
    r.removeFromRight (70);
    amount.setBounds (r);
}

void MatrixView::Row::paint (juce::Graphics& g)
{
    const auto& r = proc.getRouting (slot);
    g.setColour (r.isUsed() ? Colours::raised.withAlpha (0.5f) : Colours::panel);
    g.fillRect (getLocalBounds().reduced (0, 1));
    drawTracked (g, juce::String (slot + 1).paddedLeft ('0', 2), juce::Rectangle<float> (8.0f, 0.0f, 26.0f, (float) getHeight()),
                 font (11.0f, true), r.isUsed() ? Colours::secondary : Colours::muted, juce::Justification::centredLeft, 0.05f);
    g.setColour (r.isUsed() ? Colours::accent : Colours::tick);
    const float y = (float) getHeight() * 0.5f;
    const float x0 = (float) src.getRight() + 6.0f, x1 = (float) dst.getX() - 6.0f;
    g.drawLine (x0, y, x1, y, 1.2f);
    juce::Path arrow;
    arrow.addTriangle (x1, y, x1 - 5.0f, y - 3.5f, x1 - 5.0f, y + 3.5f);
    g.fillPath (arrow);
    const float a = proc.getValue (P::modAmt (slot));
    g.setColour (r.isUsed() ? Colours::value : Colours::muted);
    g.setFont (font (12.0f));
    g.drawText ((a > 0 ? "+" : "") + juce::String (juce::roundToInt (a * 100.0f)) + "%",
                juce::Rectangle<int> (amount.getRight() + 4, 0, 62, getHeight()), juce::Justification::centredRight, false);
}

MatrixView::MatrixView (DeadwaveProcessor& p) : proc (p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    for (int s = 0; s < kNumModSlots; ++s)
    {
        rows.push_back (std::make_unique<Row> (p, s));
        list.addAndMakeVisible (*rows.back());
    }
    proc.addChangeListener (this);
}

MatrixView::~MatrixView() { proc.removeChangeListener (this); }

void MatrixView::rebuild()
{
    for (auto& r : rows) r->update();
}

void MatrixView::refresh()
{
    for (auto& r : rows) r->repaint();
}

void MatrixView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, Metrics::panelRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::panelRadius, 1.0f);
    drawTracked (g, "MODULATION MATRIX", { 14.0f, 10.0f, 300.0f, 14.0f }, font (11.0f, true), Colours::secondary, juce::Justification::centredLeft);
    drawTracked (g, "SOURCE", { 52.0f, 30.0f, 100.0f, 12.0f }, font (9.5f, true), Colours::muted, juce::Justification::centredLeft);
    drawTracked (g, "DESTINATION", { 232.0f, 30.0f, 120.0f, 12.0f }, font (9.5f, true), Colours::muted, juce::Justification::centredLeft);
    drawTracked (g, "AMOUNT", { 550.0f, 30.0f, 100.0f, 12.0f }, font (9.5f, true), Colours::muted, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (font (11.5f));
    g.drawText ("Tip: drag any source chip from the bottom bar onto a knob. Alt-drag a modulated knob to change depth.",
                getLocalBounds().reduced (14, 8), juce::Justification::topRight, true);
}

void MatrixView::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (38);
    viewport.setBounds (r);
    const int rowH = 32;
    list.setSize (r.getWidth() - 10, rowH * kNumModSlots);
    for (int i = 0; i < (int) rows.size(); ++i)
        rows[(size_t) i]->setBounds (0, i * rowH, list.getWidth(), rowH);
}

// ========================================================================================= mod page
ModPage::ModPage (DeadwaveProcessor& p) : lfo (p, true), env (p, true), macros (p), analog (p)
{
    addAndMakeVisible (lfo);
    addAndMakeVisible (env);
    addAndMakeVisible (macros);
    addAndMakeVisible (analog);
}

void ModPage::resized()
{
    auto r = getLocalBounds();
    auto bottom = r.removeFromBottom (150);
    r.removeFromBottom (Metrics::gap);
    lfo.setBounds (r.removeFromLeft (r.getWidth() * 55 / 100));
    r.removeFromLeft (Metrics::gap);
    env.setBounds (r);
    macros.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - Metrics::gap / 2));
    bottom.removeFromLeft (Metrics::gap);
    analog.setBounds (bottom);
}

void ModPage::refresh()
{
    lfo.refresh();
    env.refresh();
    macros.refresh();
    analog.refresh();
}
} // namespace dw::ui

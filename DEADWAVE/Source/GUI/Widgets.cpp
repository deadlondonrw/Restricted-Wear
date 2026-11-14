#include "Widgets.h"

namespace dw::ui
{
// ============================================================================================ panel
void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, Metrics::panelRadius);
    g.setColour (highlighted ? Colours::muted : Colours::border);
    g.drawRoundedRectangle (r, Metrics::panelRadius, 1.0f);
    if (title.isNotEmpty())
        drawTracked (g, title, juce::Rectangle<float> (12.0f, 9.0f, r.getWidth() - 24.0f, 14.0f), font (10.5f, true),
                     Colours::secondary, juce::Justification::centredLeft, 0.16f);
}

// ======================================================================================= param knob
ParamKnob::ParamKnob (DeadwaveProcessor& p, int paramIndex, juce::String labelOverride)
    : proc (p), index (paramIndex), param (*p.param (paramIndex)),
      attachment (param, [this] (float v) { norm = param.convertTo0to1 (v); repaint(); }, nullptr)
{
    const auto& spec = Registry::get().spec (index);
    label = labelOverride.isNotEmpty() ? labelOverride : spec.shortName;
    bipolar = spec.min < 0.0f && spec.max > 0.0f;
    attachment.sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
    setTooltip (spec.name + (spec.modulatable ? "  -  drag a modulation source here; Alt-drag sets modulation depth" : ""));
}

ParamKnob::~ParamKnob() = default;

void ParamKnob::paint (juce::Graphics& g)
{
    const auto& spec = Registry::get().spec (index);
    auto r = getLocalBounds().toFloat();
    const float labelH = showLabel ? (compact ? 12.0f : 15.0f) : 0.0f;
    auto labelArea = r.removeFromTop (labelH);
    if (showLabel)
    {
        const bool showValue = hover || dragging || modDrag;
        juce::String t = showValue ? spec.format (param.convertFrom0to1 (norm)) : label;
        if (modDrag && modDragSlot >= 0)
            t = "MOD " + juce::String (juce::roundToInt (proc.getValue (P::modAmt (modDragSlot)) * 100.0f)) + "%";
        drawTracked (g, showValue ? t : t.toUpperCase(), labelArea, font (compact ? 9.5f : 10.5f, true),
                     dropHover ? Colours::accent : (showValue ? Colours::text : (isEnabled() ? Colours::secondary : Colours::muted)), juce::Justification::centred,
                     showValue ? 0.02f : (compact ? 0.06f : 0.12f));
    }

    const float size = juce::jmin (r.getWidth(), r.getHeight());
    auto knob = r.withSizeKeepingCentre (size, size);
    DeadLookAndFeel::paintKnob (g, knob, norm, bipolar, isEnabled(), modLo, modHi, modCur);

    if (dropHover)
    {
        g.setColour (Colours::accent);
        g.drawEllipse (knob.reduced (0.5f), 1.2f);
    }
}

void ParamKnob::setNormalised (float v)
{
    norm = juce::jlimit (0.0f, 1.0f, v);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (norm));
}

void ParamKnob::mouseDown (const juce::MouseEvent& e)
{
    if (textEntry != nullptr) textEntry.reset();
    if (e.mods.isPopupMenu()) { showMenu(); return; }

    lastDragPos = e.position;
    const auto& spec = Registry::get().spec (index);
    if (e.mods.isAltDown() && spec.modulatable)
    {
        const auto slots = proc.getRoutingsForParam (spec.id);
        if (! slots.isEmpty())
        {
            modDrag = true;
            modDragSlot = slots.getLast();
            dragStartAmount = proc.getValue (P::modAmt (modDragSlot));
            repaint();
            return;
        }
    }
    dragging = true;
    dragStartNorm = norm;
    attachment.beginGesture();
    repaint();
}

void ParamKnob::mouseDrag (const juce::MouseEvent& e)
{
    const float dy = (float) (lastDragPos.y - e.position.y) + (float) (e.position.x - lastDragPos.x) * 0.25f;
    lastDragPos = e.position;
    const float sens = e.mods.isShiftDown() ? 1.0f / 1400.0f : 1.0f / 240.0f;

    if (modDrag && modDragSlot >= 0)
    {
        const float a = juce::jlimit (-1.0f, 1.0f, proc.getValue (P::modAmt (modDragSlot)) + dy * sens * 1.2f);
        proc.setValue (P::modAmt (modDragSlot), a);
        refreshModulation();
        repaint();
        return;
    }
    if (! dragging) return;
    setNormalised (norm + dy * sens);
}

void ParamKnob::mouseUp (const juce::MouseEvent&)
{
    if (dragging) attachment.endGesture();
    dragging = false;
    modDrag = false;
    modDragSlot = -1;
    repaint();
}

void ParamKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    attachment.setValueAsCompleteGesture (Registry::get().spec (index).def);
}

void ParamKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float step = (e.mods.isShiftDown() ? 0.005f : 0.03f) * (w.isReversed ? -1.0f : 1.0f);
    const float n = juce::jlimit (0.0f, 1.0f, norm + (w.deltaY > 0 ? step : -step));
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (n));
}

bool ParamKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("modsrc:") && Registry::get().spec (index).modulatable;
}

void ParamKnob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const auto src = modSourceFromId (d.description.toString().fromFirstOccurrenceOf ("modsrc:", false, false));
    const int slot = proc.addModulation (src, Registry::get().spec (index).id, 0.3f);
    if (slot < 0)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Modulation matrix full",
                                                "All 32 modulation slots are in use. Remove a routing in the MATRIX page first.");
    refreshModulation();
    repaint();
}

void ParamKnob::refreshModulation()
{
    const auto& spec = Registry::get().spec (index);
    float lo = -1.0f, hi = -1.0f, cur = -1.0f;
    if (spec.modulatable)
    {
        const auto slots = proc.getRoutingsForParam (spec.id);
        if (! slots.isEmpty())
        {
            float down = 0.0f, up = 0.0f;
            for (int s : slots)
            {
                const float a = proc.getValue (P::modAmt (s));
                const auto src = proc.getRouting (s).src;
                bool bip = modSourceIsBipolar (src);
                if (src >= ModSrc::Lfo1 && src <= ModSrc::Lfo4)
                    bip = proc.getValue (P::lfo ((int) src - (int) ModSrc::Lfo1, LfoP::Bipolar)) >= 0.5f;
                if (bip) { down -= std::abs (a); up += std::abs (a); }
                else if (a >= 0.0f) up += a;
                else down += a;
            }
            lo = juce::jlimit (0.0f, 1.0f, norm + down);
            hi = juce::jlimit (0.0f, 1.0f, norm + up);
            cur = proc.getEngine().getModulatedNormalised (index);
        }
    }
    if (lo != modLo || hi != modHi || std::abs (cur - modCur) > 0.002f)
    {
        modLo = lo; modHi = hi; modCur = cur;
        repaint();
    }
}

void ParamKnob::showMenu()
{
    const auto& spec = Registry::get().spec (index);
    juce::PopupMenu m;
    m.addSectionHeader (spec.name + "   " + spec.format (param.convertFrom0to1 (norm)));
    m.addItem (1, "Reset to default");
    m.addItem (2, "Enter value...");

    if (spec.modulatable)
    {
        const auto slots = proc.getRoutingsForParam (spec.id);
        if (! slots.isEmpty())
        {
            m.addSeparator();
            for (int s : slots)
            {
                juce::PopupMenu sub;
                const float a = proc.getValue (P::modAmt (s));
                sub.addItem (1000 + s * 10 + 0, "Remove");
                sub.addItem (1000 + s * 10 + 1, "Invert");
                sub.addItem (1000 + s * 10 + 2, "Depth 25%");
                sub.addItem (1000 + s * 10 + 3, "Depth 50%");
                sub.addItem (1000 + s * 10 + 4, "Depth 100%");
                m.addSubMenu (modSourceName (proc.getRouting (s).src) + "   " + juce::String (juce::roundToInt (a * 100.0f)) + "%", sub);
            }
        }
        juce::PopupMenu add;
        for (int s = 1; s < (int) ModSrc::NumSources; ++s)
            add.addItem (500 + s, modSourceName ((ModSrc) s));
        m.addSubMenu ("Add modulation", add);
    }

    juce::Component::SafePointer<ParamKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& k = *safe;
        const auto& sp = Registry::get().spec (k.index);
        if (r == 1) k.attachment.setValueAsCompleteGesture (sp.def);
        else if (r == 2) k.editValueAsText();
        else if (r >= 1000)
        {
            const int slot = (r - 1000) / 10, act = (r - 1000) % 10;
            const float a = k.proc.getValue (P::modAmt (slot));
            if (act == 0) k.proc.clearRouting (slot);
            else if (act == 1) k.proc.setValue (P::modAmt (slot), -a);
            else k.proc.setValue (P::modAmt (slot), (a < 0 ? -1.0f : 1.0f) * (act == 2 ? 0.25f : act == 3 ? 0.5f : 1.0f));
        }
        else if (r > 500)
            k.proc.addModulation ((ModSrc) (r - 500), sp.id, 0.3f);
        k.refreshModulation();
        k.repaint();
    });
}

void ParamKnob::editValueAsText()
{
    textEntry = std::make_unique<juce::TextEditor>();
    auto* t = textEntry.get();
    addAndMakeVisible (t);
    t->setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmax (60, getWidth()), 22));
    t->setJustification (juce::Justification::centred);
    t->setFont (font (12.0f));
    t->setText (param.getCurrentValueAsText(), false);
    t->selectAll();
    t->grabKeyboardFocus();
    juce::Component::SafePointer<ParamKnob> safe (this);
    t->onReturnKey = [safe]
    {
        if (safe == nullptr) return;
        const float n = safe->param.getValueForText (safe->textEntry->getText());
        safe->attachment.setValueAsCompleteGesture (safe->param.convertFrom0to1 (n));
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->textEntry.reset(); });
    };
    t->onEscapeKey = t->onFocusLost = [safe]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->textEntry.reset(); });
    };
}

// ===================================================================================== param toggle
ParamToggle::ParamToggle (DeadwaveProcessor& p, int paramIndex, const juce::String& text)
    : juce::ToggleButton (text), attachment (*p.param (paramIndex), *this)
{
    setTooltip (Registry::get().spec (paramIndex).name);
}

// ======================================================================================= segmented
Segmented::Segmented (juce::StringArray i) : items (std::move (i)) {}

Segmented::Segmented (DeadwaveProcessor& p, int paramIndex, juce::StringArray labelsOverride)
{
    const auto& spec = Registry::get().spec (paramIndex);
    items = labelsOverride.isEmpty() ? spec.choices : labelsOverride;
    auto* prm = p.param (paramIndex);
    attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
    {
        selected = juce::roundToInt (v);
        repaint();
    }, nullptr);
    attachment->sendInitialUpdate();
    onChange = [this] (int i) { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
}

Segmented::~Segmented() = default;

void Segmented::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    if (items.isEmpty()) return;
    const float w = (r.getWidth() - 4.0f) / (float) items.size();
    for (int i = 0; i < items.size(); ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + 2.0f + w * (float) i, r.getY() + 2.0f, w, r.getHeight() - 4.0f);
        const bool on = i == selected;
        if (on)
        {
            g.setColour (Colours::text);
            g.fillRoundedRectangle (cell, 3.0f);
        }
        drawTracked (g, items[i].toUpperCase(), cell, font (fontHeight, true), on ? Colours::bg : (isEnabled() ? Colours::secondary : Colours::muted),
                     juce::Justification::centred, 0.08f);
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (items.isEmpty()) return;
    const int i = juce::jlimit (0, items.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) items.size()));
    setSelected (i, true);
}

void Segmented::setSelected (int i, bool notify)
{
    if (i == selected && ! notify) return;
    selected = i;
    repaint();
    if (notify && onChange) onChange (i);
}

// ====================================================================================== param choice
ParamChoice::ParamChoice (DeadwaveProcessor& p, int paramIndex)
{
    const auto& spec = Registry::get().spec (paramIndex);
    addItemList (spec.choices, 1);
    attachment = std::make_unique<juce::ComboBoxParameterAttachment> (*p.param (paramIndex), *this);
    setTooltip (spec.name);
}

// =================================================================================== mod source chip
ModSourceChip::ModSourceChip (DeadwaveProcessor& p, ModSrc s) : proc (p), src (s)
{
    setTooltip ("Drag " + modSourceName (s) + " onto any knob to modulate it");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModSourceChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (hover ? juce::Colour (0xff202024) : Colours::raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (hover ? Colours::accent.withAlpha (0.6f) : Colours::border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    auto bar = r.removeFromBottom (3.0f).reduced (4.0f, 0.0f).translated (0.0f, -2.0f);
    g.setColour (Colours::track);
    g.fillRect (bar);
    g.setColour (Colours::accent);
    const float v = juce::jlimit (-1.0f, 1.0f, value);
    if (modSourceIsBipolar (src) || (src >= ModSrc::Lfo1 && src <= ModSrc::Lfo4))
    {
        const float c = bar.getCentreX();
        const float x = c + v * bar.getWidth() * 0.5f;
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (c, x), bar.getY(), juce::jmax (c, x), bar.getBottom()));
    }
    else
        g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)));
 juce::String nm = modSourceName (src);
    if (src == ModSrc::ModWheel) nm = "WHEEL";
    else if (src == ModSrc::Aftertouch) nm = "PRESS";
    else if (src == ModSrc::PitchBend) nm = "BEND";
    else if (src == ModSrc::Velocity) nm = "VEL";
    drawTracked (g, nm, r.withTrimmedBottom (2.0f), font (10.5f, true), Colours::text, juce::Justification::centred, 0.08f);
}

void ModSourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! dnd->isDragAndDropActive())
            dnd->startDragging ("modsrc:" + juce::String (modSourceId (src)), this, juce::ScaledImage (createComponentSnapshot (getLocalBounds())));
}

void ModSourceChip::refresh()
{
    const float v = proc.getEngine().getModSourceValueForUi (src);
    if (std::abs (v - value) > 0.004f) { value = v; repaint(); }
}

// ========================================================================================== layout
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, int itemWidth)
{
    const int n = (int) comps.size();
    if (n == 0) return;
    const int w = itemWidth > 0 ? itemWidth : area.getWidth() / n;
    const float step = n > 1 ? (float) (area.getWidth() - w) / (float) (n - 1) : 0.0f;
    int i = 0;
    for (auto* c : comps)
    {
        if (c != nullptr)
            c->setBounds (area.getX() + juce::roundToInt (step * (float) i), area.getY(), w, area.getHeight());
        ++i;
    }
}
} // namespace dw::ui

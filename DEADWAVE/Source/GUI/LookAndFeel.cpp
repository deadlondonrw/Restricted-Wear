#include "LookAndFeel.h"

namespace dw::ui
{
DeadLookAndFeel::DeadLookAndFeel()
{
    using namespace Colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ComboBox::backgroundColourId, raised);
    setColour (juce::ComboBox::outlineColourId, border);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, secondary);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, raised);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextEditor::backgroundColourId, raised);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, muted);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, text);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ScrollBar::thumbColourId, track);
    setColour (juce::Slider::textBoxTextColourId, value);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, border);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, text);
}

juce::Typeface::Ptr DeadLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

// ============================================================================================ knobs
void DeadLookAndFeel::paintKnob (juce::Graphics& g, juce::Rectangle<float> b, float pos, bool bipolar, bool enabled,
                                 float modLo, float modHi, float modCur)
{
    using namespace Colours;
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = size * 0.5f - 2.0f;
    if (r < 4.0f) return;

    const float start = juce::MathConstants<float>::pi * 1.25f;
    const float end = juce::MathConstants<float>::pi * 2.75f;
    const float stroke = juce::jlimit (1.6f, 3.4f, r * 0.095f);
    const float arcR = r - stroke * 0.5f;

    juce::Path trackArc;
    trackArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (track);
    g.strokePath (trackArc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    const float angle = start + pos * (end - start);
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (enabled ? text : secondary);
        g.strokePath (valueArc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // Modulation range (amber) just inside the value arc.
    if (modLo >= 0.0f && modHi >= 0.0f && std::abs (modHi - modLo) > 0.001f)
    {
        const float mr = arcR - stroke * 1.4f;
        juce::Path modArc;
        const float a0 = start + juce::jlimit (0.0f, 1.0f, juce::jmin (modLo, modHi)) * (end - start);
        const float a1 = start + juce::jlimit (0.0f, 1.0f, juce::jmax (modLo, modHi)) * (end - start);
        modArc.addCentredArc (c.x, c.y, mr, mr, 0.0f, a0, a1, true);
        g.setColour (accent.withAlpha (enabled ? 0.9f : 0.4f));
        g.strokePath (modArc, juce::PathStrokeType (juce::jmax (1.2f, stroke * 0.6f), juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const float bodyR = r * 0.70f;
    g.setColour (raised);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (knobRim);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    const float shownAngle = modCur >= 0.0f ? start + juce::jlimit (0.0f, 1.0f, modCur) * (end - start) : angle;
    if (modCur >= 0.0f)
    {
        const auto dot = c.getPointOnCircumference (arcR, angle);
        juce::ignoreUnused (dot);
        const auto md = c.getPointOnCircumference (arcR - stroke * 1.4f, shownAngle);
        g.setColour (accent);
        g.fillEllipse (md.x - 2.2f, md.y - 2.2f, 4.4f, 4.4f);
    }

    const auto p0 = c.getPointOnCircumference (bodyR * 0.42f, angle);
    const auto p1 = c.getPointOnCircumference (bodyR * 0.86f, angle);
    g.setColour (enabled ? text : secondary);
    g.drawLine ({ p0, p1 }, juce::jlimit (1.5f, 2.8f, r * 0.075f));
}

void DeadLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float lo = (float) s.getProperties().getWithDefault ("modLo", -1.0f);
    const float hi = (float) s.getProperties().getWithDefault ("modHi", -1.0f);
    const float cur = (float) s.getProperties().getWithDefault ("modCur", -1.0f);
    paintKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, bipolar, s.isEnabled(), lo, hi, cur);
}

void DeadLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace Colours;
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        g.setColour (raised);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        auto fill = r.reduced (2.0f);
        if (vertical)
        {
            const float zero = bipolar ? fill.getCentreY() : fill.getBottom();
            const float yy = juce::jlimit (fill.getY(), fill.getBottom(), pos);
            g.setColour (s.isEnabled() ? text.withAlpha (0.85f) : secondary);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (fill.getX(), juce::jmin (zero, yy), fill.getRight(), juce::jmax (zero, yy)));
        }
        else
        {
            const float zero = bipolar ? fill.getCentreX() : fill.getX();
            const float xx = juce::jlimit (fill.getX(), fill.getRight(), pos);
            g.setColour (s.isEnabled() ? text.withAlpha (0.85f) : secondary);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, xx), fill.getY(), juce::jmax (zero, xx), fill.getBottom()));
        }
        return;
    }

    const float t = 3.0f;
    juce::Rectangle<float> tr = vertical ? r.withSizeKeepingCentre (t, r.getHeight() - 8.0f)
                                         : r.withSizeKeepingCentre (r.getWidth() - 8.0f, t);
    g.setColour (track);
    g.fillRoundedRectangle (tr, 1.5f);
    g.setColour (s.isEnabled() ? text : secondary);
    if (vertical)
    {
        const float zero = bipolar ? tr.getCentreY() : tr.getBottom();
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (tr.getX(), juce::jmin (zero, pos), tr.getRight(), juce::jmax (zero, pos)), 1.5f);
        g.fillEllipse (tr.getCentreX() - 5.0f, pos - 5.0f, 10.0f, 10.0f);
    }
    else
    {
        const float zero = bipolar ? tr.getCentreX() : tr.getX();
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, pos), tr.getY(), juce::jmax (zero, pos), tr.getBottom()), 1.5f);
        g.fillEllipse (pos - 5.0f, tr.getCentreY() - 5.0f, 10.0f, 10.0f);
    }
}

juce::Slider::SliderLayout DeadLookAndFeel::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout l;
    l.sliderBounds = s.getLocalBounds();
    return l;
}

// ========================================================================================== buttons
void DeadLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace Colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState() && (bool) b.getProperties().getWithDefault ("lightWhenOn", true);
    const bool flat = (bool) b.getProperties().getWithDefault ("flat", false);
    if (flat && ! on)
    {
        if (over) { g.setColour (raised); g.fillRoundedRectangle (r, Metrics::controlRadius); }
        return;
    }
    g.setColour (on ? text : (down ? track : (over ? juce::Colour (0xff202024) : raised)));
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    if (! on)
    {
        g.setColour (border);
        g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    }
}

void DeadLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace Colours;
    const bool on = b.getToggleState() && (bool) b.getProperties().getWithDefault ("lightWhenOn", true);
    const float h = (float) b.getProperties().getWithDefault ("fontHeight", 11.5f);
    auto c = on ? bg : (b.isEnabled() ? text : muted);
    if ((bool) b.getProperties().getWithDefault ("dimText", false) && ! on) c = secondary;
    drawTracked (g, b.getButtonText().toUpperCase(), b.getLocalBounds().toFloat(), font (h, true), c,
                 juce::Justification::centred, 0.1f);
}

void DeadLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace Colours;
    // DEADNOISE pill switch: track #2C2C31, knob #8E8E93 (off) / #F2F2F2 (on)
    auto r = b.getLocalBounds().toFloat();
    const float h = juce::jmin (r.getHeight(), 18.0f);
    auto pill = juce::Rectangle<float> (r.getX() + 1.0f, r.getCentreY() - h * 0.5f, h * 1.9f, h);
    const bool on = b.getToggleState();
    g.setColour (on ? juce::Colour (0xff4a4a50) : track);
    g.fillRoundedRectangle (pill, h * 0.5f);
    const float kd = h - 4.0f;
    const float kx = on ? pill.getRight() - kd - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? text : (over ? juce::Colour (0xffa0a0a5) : secondary));
    g.fillEllipse (kx, pill.getY() + 2.0f, kd, kd);

    if (b.getButtonText().isNotEmpty())
    {
        auto tr = r.withLeft (pill.getRight() + 6.0f);
        drawTracked (g, b.getButtonText().toUpperCase(), tr, font (h >= 18.0f ? 12.0f : 10.5f, true), b.isEnabled() ? text : muted,
                     juce::Justification::centredLeft, 0.08f);
    }
}

// ============================================================================================ combos
void DeadLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace Colours;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (raised);
    g.fillRoundedRectangle (r, Metrics::controlRadius);
    g.setColour (box.hasKeyboardFocus (true) ? muted : border);
    g.drawRoundedRectangle (r, Metrics::controlRadius, 1.0f);
    const float cx = (float) w - 11.0f, cy = (float) h * 0.5f;
    juce::Path chev;
    chev.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chev.lineTo (cx, cy + 2.0f);
    chev.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (secondary);
    g.strokePath (chev, juce::PathStrokeType (1.4f));
}

juce::Font DeadLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (12.0f, true); }

void DeadLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void DeadLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Colours::panel);
    g.setColour (Colours::border);
    g.drawRect (0, 0, w, h, 1);
}

void DeadLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                         bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& txt,
                                         const juce::String& shortcut, const juce::Drawable*, const juce::Colour* textColour)
{
    using namespace Colours;
    if (isSeparator)
    {
        g.setColour (border);
        g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (raised);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
    }
    const auto c = textColour != nullptr ? *textColour : (isActive ? text : muted);
    g.setColour (c);
    g.setFont (font (13.0f, isTicked));
    auto tr = r.reduced (10, 0);
    if (isTicked)
    {
        g.setColour (accent);
        g.fillEllipse ((float) r.getX() + 3.0f, (float) r.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setColour (c);
    }
    g.drawText (txt, tr, juce::Justification::centredLeft, true);
    if (shortcut.isNotEmpty())
    {
        g.setColour (muted);
        g.drawText (shortcut, tr, juce::Justification::centredRight, true);
    }
    if (hasSubMenu)
    {
        juce::Path p;
        const float x = (float) r.getRight() - 10.0f, y = (float) r.getCentreY();
        p.startNewSubPath (x - 2.0f, y - 3.5f);
        p.lineTo (x + 1.5f, y);
        p.lineTo (x - 2.0f, y + 3.5f);
        g.setColour (secondary);
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }
}

juce::Font DeadLookAndFeel::getPopupMenuFont() { return font (13.0f); }

void DeadLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                     int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
    g.setColour (down ? Colours::secondary : (over ? Colours::muted : Colours::track));
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

void DeadLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (Colours::raised);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, Metrics::controlRadius);
}

void DeadLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? Colours::muted : Colours::border);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, Metrics::controlRadius, 1.0f);
}

void DeadLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h)
{
    g.fillAll (Colours::panel);
    g.setColour (Colours::border);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (Colours::text);
    g.setFont (font (12.5f));
    g.drawFittedText (t, juce::Rectangle<int> (0, 0, w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> DeadLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = juce::jmin (320, (int) juce::GlyphArrangement::getStringWidth (font (12.5f), tipText) + 20);
    const int lines = 1 + (int) juce::GlyphArrangement::getStringWidth (font (12.5f), tipText) / 300;
    const int h = 10 + lines * 16;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parent);
}

juce::Font DeadLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}
} // namespace dw::ui

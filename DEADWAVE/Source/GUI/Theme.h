#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dw::ui
{
/**
    DEADWAVE design tokens.

    These values were measured from a render of the DEADNOISE 1.0.1 editor (see
    docs/design/deadnoise-reference.png) so both products share one visual family.
*/
namespace Colours
{
const juce::Colour bg          { 0xff0a0a0b }; // window background
const juce::Colour panel       { 0xff131315 }; // panel / card fill
const juce::Colour border      { 0xff26262a }; // 1 px panel + control outline
const juce::Colour raised      { 0xff1a1a1d }; // buttons, fields, knob body, meter track
const juce::Colour knobRim     { 0xff212125 }; // knob body outline
const juce::Colour grid        { 0xff1e1e21 }; // display grid lines
const juce::Colour gridStrong  { 0xff212125 };
const juce::Colour track       { 0xff2c2c31 }; // knob arc track, toggle track, progress bars
const juce::Colour tick        { 0xff3c3c40 }; // scale ticks, axis numbers
const juce::Colour muted       { 0xff5a5a60 }; // control labels, inactive dots
const juce::Colour dim         { 0xff6c6c70 }; // body copy / hints
const juce::Colour secondary   { 0xff8e8e93 }; // section titles, inactive segment text, toggle knob
const juce::Colour value       { 0xffd5d5d5 }; // numeric read-outs
const juce::Colour text        { 0xfff2f2f2 }; // primary text, active controls, main curves
const juce::Colour accent      { 0xffd8a657 }; // secondary data (DEADNOISE: noise estimate) -> modulation
const juce::Colour danger      { 0xffd8665a };
} // namespace Colours

namespace Metrics
{
constexpr float panelRadius = 6.0f;
constexpr float controlRadius = 4.0f;
constexpr int gap = 8;
constexpr int baseWidth = 1280;
constexpr int baseHeight = 820;
} // namespace Metrics

/** Segoe UI family (Windows system font, as in DEADNOISE). Falls back to the platform sans. */
inline juce::Font font (float height, bool semibold = false, bool bold = false)
{
    if (bold)
        return juce::Font (juce::FontOptions ("Segoe UI", height, juce::Font::bold));
    if (semibold)
        return juce::Font (juce::FontOptions ("Segoe UI Semibold", height, juce::Font::plain));
    return juce::Font (juce::FontOptions ("Segoe UI", height, juce::Font::plain));
}

/** Tracked uppercase label text (DEADNOISE section-title style). */
inline void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& f,
                         juce::Colour c, juce::Justification just, float tracking = 0.12f)
{
    g.setColour (c);
    g.setFont (f.withExtraKerningFactor (tracking));
    g.drawText (text, area, just, false);
}
} // namespace dw::ui

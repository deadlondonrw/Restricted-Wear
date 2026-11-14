#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dw::ui
{
/**
    DEADWAVE design tokens.

    Taken from the DEADNOISE source (src/plugin/PluginEditor.h, namespace dnui::colours) and
    cross-checked against a render of the DEADNOISE 1.0.1 editor (docs/design/deadnoise-reference.png).
    Comments give the DEADNOISE name of each value.
*/
namespace Colours
{
const juce::Colour bg          { 0xff0a0a0b }; // background
const juce::Colour panel       { 0xff131315 }; // panel
const juce::Colour border      { 0xff26262a }; // border
const juce::Colour raised      { 0xff1a1a1d }; // panelHi: buttons, fields, knob body
const juce::Colour knobRim     { 0xff26262a }; // knob body outline (= border)
const juce::Colour grid        { 0xff1e1e21 }; // display grid lines (measured)
const juce::Colour gridStrong  { 0xff2a2a2f }; // spectrum grid (DEADNOISE spectrum view)
const juce::Colour track       { 0xff2c2c31 }; // track: arcs, toggles, progress
const juce::Colour tick        { 0xff3c3c40 }; // scale ticks (measured)
const juce::Colour muted       { 0xff5a5a60 }; // textFaint: disabled, subtitles, chip labels
const juce::Colour dim         { 0xff6c6c70 }; // body copy / hints (measured)
const juce::Colour secondary   { 0xff8e8e93 }; // textDim: knob captions, panel titles
const juce::Colour value       { 0xffd9d9dc }; // accent: numeric read-outs
const juce::Colour text        { 0xfff2f2f2 }; // text: primary text, active controls, curves
const juce::Colour accent      { 0xffd8a657 }; // warn: secondary data (noise estimate) -> modulation here
const juce::Colour danger      { 0xffe5484d }; // clip
} // namespace Colours

namespace Metrics
{
constexpr float panelRadius = 6.0f;
constexpr float controlRadius = 4.0f;
constexpr int gap = 8;
constexpr int baseWidth = 1280;
constexpr int baseHeight = 820;
} // namespace Metrics

/** DEADNOISE's uiFont(): Segoe UI / Segoe UI Semibold on Windows, platform sans elsewhere. */
inline juce::Font uiFont (float height, bool bold = false, float tracking = 0.0f)
{
    juce::FontOptions o;
   #if JUCE_WINDOWS
    o = o.withName (bold ? "Segoe UI Semibold" : "Segoe UI");
   #endif
    o = o.withHeight (height).withStyle (bold ? "Bold" : "Regular").withKerningFactor (tracking);
    return juce::Font (o);
}

/** Convenience used across DEADWAVE: semibold/bold both map to DEADNOISE's bold face. */
inline juce::Font font (float height, bool semibold = false, bool bold = false)
{
    return uiFont (height, semibold || bold);
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

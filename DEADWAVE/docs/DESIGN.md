# DEADWAVE – design reference (DEADNOISE family)

DEADWAVE uses DEADNOISE's design system directly. The values below are taken from the DEADNOISE
source (`DEADNOISE/src/plugin/PluginEditor.*`, namespace `dnui`, branch
`claude/modest-lovelace-49w2ye`) and were cross-checked against a render of the DEADNOISE 1.0.1
editor ([design/deadnoise-reference.png](design/deadnoise-reference.png)).

## Colour

| Token (DEADNOISE name) | Value | Use |
|---|---|---|
| background | `#0A0A0B` | window |
| panel | `#131315` | cards / panels |
| panelHi | `#1A1A1D` | buttons, fields, knob bodies, displays |
| border | `#26262A` | 1 px outlines, knob body rim |
| track | `#2C2C31` | arc tracks, switches off, progress |
| textFaint | `#5A5A60` | disabled, subtitles, chip labels |
| textDim | `#8E8E93` | knob captions, panel titles, inactive segments |
| accent | `#D9D9DC` | numeric read-outs |
| text | `#F2F2F2` | text, active controls, curves, switch on |
| warn | `#D8A657` | DEADNOISE: noise estimate (secondary data). DEADWAVE: modulation (rings, chips, LFO playhead) |
| clip | `#E5484D` | errors / clipping |

## Type

`uiFont (height, bold, tracking)`: Segoe UI / Segoe UI Semibold ("Bold" style) on Windows, the
platform sans elsewhere. Title 26 pt bold, tracking 0.32; subtitle 9 pt bold, 0.22, textFaint;
panel titles 10.5 pt bold, 0.16, textDim; knob captions 10.5 pt bold, 0.12, textDim; buttons and
switch labels 12 pt bold, 0.08; status chips 9.5 pt label (0.14) + 10.5 pt value (0.06).

## Controls

* Panels: radius 6, 1 px border. Buttons/fields: radius 4.
* Knob: 270° arc (rounded caps, 3 px, 4 px when ≥ 64 px), value arc in text colour (bipolar from
  the centre), body `panelHi` radius = arc − 2.2 × stroke with a `border` rim, pointer from 0.35 to
  0.85 of the body radius. DEADWAVE adds the modulation range in amber between arc and body.
* Switch: 34 × 18 pill; off = track + textDim knob, on = text + background knob.
* Segmented control: panelHi field, selected segment = text-coloured pill (radius 3, inset 2).
* Status chips: panel colour, radius 4, dot + label + value.

## Layout (1280 × 820 base, scalable 60–200 %, fixed aspect)

Header (logo, preset bar, VOICES / OUTPUT chips, quality, master) · page tabs ·
page area (OSCILLATORS: 4 oscillator cards + Filter 1, Filter 2, Envelopes, LFO, Voice & Glide;
MODULATION; MATRIX; EFFECTS; ARP / SEQ; GENERATE) · modulation-source strip (always visible,
drag sources onto knobs). The Wave Editor and preset browser open over the page area.

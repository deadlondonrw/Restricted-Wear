# DEADWAVE

Wavetable synthesizer, Windows x64 **VST3 instrument** for FL Studio, in the same
product family as DEADNOISE (same design language, palette and typography, same
build and test pipeline).

Four oscillators with a deep wave editor, two filters, four envelopes, four LFOs,
a 32-slot modulation matrix with drag-and-drop, eleven effects with per-oscillator
FX lanes and LINK FX, glide/legato for FL Studio, arpeggiator and step sequencer,
228 factory presets and a local sound generator.

> Status: first development build (0.1.0). It builds and passes automated tests on
> Windows and Linux, but it has **not yet been played in FL Studio** — see
> [docs/STATUS.md](docs/STATUS.md) for exactly what is verified and what is not.

---

## 1. Install (FL Studio, Windows)

The Windows CI publishes the deliverables to [`bin/`](bin) (after a successful run):

* `DEADWAVE-FL-STUDIO-WINDOWS-x64.zip` – VST3 bundle `DEADWAVE.vst3/`
* `DEADWAVE-FL-STUDIO-WINDOWS-x64-SINGLE-FILE.zip` – same binary as one `.vst3` file
* `INSTALLATION.txt`, `TEST_REPORT.txt`, `SHA256SUMS.txt`, `support/` scripts

Short version: unblock the ZIP (Properties → Unblock), extract into `D:\Plugins`
so that `D:\Plugins\DEADWAVE.vst3\Contents\x86_64-win\DEADWAVE.vst3` exists, then
FL Studio → Options → Manage plugins → *Find installed plugins* (Verify ticked).
If FL does not list it, `support\LINK-DEADWAVE-FROM-D-PLUGINS.bat` links it into the
standard VST3 folder and `support\DEADWAVE-diagnose.ps1` reports what Windows sees.

Built with MSVC (static runtime): no Visual C++ redistributable needed.

## 2. Glide and slides in FL Studio

* **VOICE & GLIDE → LEGATO** (or MONO) and **GLIDE → OVERLAP**: overlapping notes in
  the piano roll glide; separated notes retrigger. **ALWAYS** glides every note.
* **TIME**, **CURVE** (linear ↔ fast-start ↔ slow-start) and **RATE** (constant time vs.
  time per octave) shape the slide. All are automatable; TIME is also a modulation target.
* **PRIORITY** (last / low / high), **RETURN** (glide back to a still-held key on release)
  and **HARD** (retrigger envelopes from zero) define overlap behaviour.
* **BEND + / BEND −** set the pitch-bend range (0–48 semitones).
* FL Studio's piano-roll *slide notes* are an FL feature for its native instruments.
  DEADWAVE does not claim to receive them; use overlap glide (and pitch bend) instead.

## 3. What is in it

| Area | Implemented |
|---|---|
| Oscillators | 4 × (wavetable / noise / sample). Per osc: level, pan, octave, semitone, fine, continuous **PITCH** (±48 st, modulatable), key track, **WT POS**, **BIAS** (vertical offset), **AMP**, **PHASE**, phase random, **SHAPE** + 8 shape modes (bend, squeeze, pulse width, sync, formant, mirror, quantize, phase-FM), unison 1–16 with detune / stereo spread / blend, FM (sine at ratio, or from a lower-numbered oscillator), drive (5 modes), crush, filter route, FX lane, LINK |
| Wavetables | Band-limited mipmaps (20 half-octave levels), Hermite interpolation, frame morphing, 26 procedural factory tables, up to 256 frames |
| Wave Editor | Freehand drawing, control points with curvature, harmonic editor (magnitude + phase), 2D / 3D / spectrum views, 18 transforms (invert, reverse, mirror, normalise, remove DC, smooth, rectify, fold, quantise, shift, stretch, compress, scale, offset, sharpen, half cycle, double, clear) on one or all frames, frame strip, add/duplicate/delete/move, keyframe interpolation (crossfade or spectral), frame-count resampling, math expressions, random generation (5 styles), undo/redo, live BIAS/AMP/PHASE/SHAPE/WT POS controls with an amber output overlay |
| Import | WAV/AIFF/FLAC/OGG(/MP3 where supported): region selection, preview, pitch detection, cycle extraction, fixed slicing, or sample oscillator; save/load user wavetables (32-bit WAV with a Serum/Vital-compatible `clm` chunk) in `Documents\DEADWAVE\Wavetables` |
| Filters | 2 × (LP/HP/BP 12 & 24, notch, peak, all-pass, nonlinear ladder LP/HP, comb ±, formant), cutoff / resonance / drive / mix / key track / envelope / stereo spread, serial or parallel |
| Modulation | 4 DAHDSR envelopes with curves, 4 LFOs (7 shapes + editable custom shape, sync, free / retrigger / one-shot, phase, smooth, rise), velocity, note, mod wheel, aftertouch, pitch bend, random, 4 macros; 32-slot matrix; drag a source chip onto any knob; Alt-drag a knob to set depth |
| Effects | Distortion (6 modes, 2× oversampled), saturation, chorus, phaser, flanger, delay (sync, ping-pong, filters), reverb (8-line FDN), compressor, EQ, stereo imager, bitcrusher; reorderable; master rack + one lane per oscillator; LINK FX |
| Voices | Poly up to 32 (stealing with short fades), mono, legato, glide, sustain pedal, analog drift / instability / saturation / filter nonlinearity / stereo variation |
| Quality | STANDARD and ULTRA (voices rendered at 2× with a linear-phase half-band decimator) |
| MIDI FX | Arpeggiator (7 modes, octaves, gate, swing, velocity, hold, cycle length + transpose) and 16-step sequencer (pitch, velocity, gate per step), host-synced or free-running |
| Presets | 228 factory presets in 20 categories; search, categories, favourites, recent, user save / rename / delete / import (`Documents\DEADWAVE\Presets`) |
| Generator | Local and rule-based (no AI service): text description → traits → complete preset; parameter-based design; partial / full randomisation with group locks and intensity; undo |

## 4. Build

Requirements: CMake ≥ 3.22, a C++20 compiler (Visual Studio 2022 on Windows), Git.
JUCE 8.0.15 is fetched automatically (or pass `-DDEADWAVE_JUCE_PATH=<JUCE checkout>`).

```bash
cmake -S DEADWAVE -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
```

```bash
cmake --build build --config Release --target DEADWAVE_VST3 DEADWAVE_Standalone
```

Output: `build/DEADWAVE_artefacts/Release/VST3/DEADWAVE.vst3`.
Tests: `DeadwaveTests` (engine, presets, real-time safety) and `DeadwaveHostTest <path to .vst3>`
(loads the built plug-in through JUCE's VST3 hosting). `DeadwaveTests --snapshot <dir>` renders the UI.

Linux (development): `cmake -S DEADWAVE -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` with the
JUCE Linux dependencies installed (ALSA, X11, FreeType, fontconfig, GL).

## 5. Project layout

```
Source/Core      parameter registry (stable IDs), math utilities, expression compiler
Source/DSP       wavetables, oscillator, filters, envelopes/LFOs, voice, engine, effects, FX racks
Source/MIDI      arpeggiator / step sequencer
Source/Wave      wave document (editing model), audio import
Source/Presets   state I/O, preset manager, factory library, sound generator
Source/GUI       DEADNOISE-family look and feel, widgets, pages, wave editor
Tests            engine tests, VST3 host test
packaging        bundle verification, test report, installation guide
support          FL Studio link / unlink / diagnostics scripts
docs             status, architecture, design reference
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/DESIGN.md](docs/DESIGN.md).

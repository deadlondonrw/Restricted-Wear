# DEADWAVE – architecture

## Threads

| Thread | Owns / does |
|---|---|
| Audio | `SynthEngine::process` – voices, modulation, MIDI FX, FX racks. Never allocates, locks or does I/O. |
| Message | Parameters UI, wave documents, presets, samples, LFO shapes, mod routing, 30 Hz timer. |
| Table builder | Builds band-limited wavetables from wave documents (`TableBuilder`, background thread). |

Assets cross to the audio thread through `ObjectExchange<T>` (Core/DspUtil.h): the producer
publishes a heap object, the audio thread swaps it in at the start of a block, retired objects
go back through a fixed ring and are freed on the message thread. Mod routings are atomics.
Hosts that restore state off the message thread get parameters applied immediately and
documents/assets applied on the message thread (`AsyncUpdater`), with `getStateInformation`
returning the pending state until then.

## Parameters

`Core/Params.*` is the single registry of ~720 parameters: stable string IDs (persisted by hosts
and presets), ranges, display formatting, groups (for randomiser locks) and whether a parameter is
a modulation destination (406 are). Runtime indices (`P::osc(i, OscP::WtPos)` …) are not persisted.

## Signal flow

```
per voice (rate = host rate, or 2x in ULTRA):
  OSC 1..4  (wavetable | noise | sample)
     phase -> shape warp -> band-limited table read (frame morph) -> amp * (x - shapeDC) + BIAS
     -> unison (detune, pan, blend) -> drive -> crush -> level
     -> route: Filter 1 / Filter 2 / Filter 1+2 / Direct
  Filter 1 -> Filter 2 (serial)   or   Filter 1 + Filter 2 (parallel)
  -> amp envelope x velocity -> analog saturation -> DC blocker -> main bus
  oscillators with FX LANE on: own copy of the filters -> amp env -> DC blocker -> lane bus n

per block (host rate):
  buses decimated 2:1 in ULTRA (79-tap Kaiser half-band)
  lane n -> FX rack n (LINK: parameters of the lowest linked lane) -> master bus or direct out
  master bus -> master FX rack -> master volume -> safety (NaN reset, +18 dBFS ceiling)
```

Control rate: modulation, envelopes and LFOs update every 32 samples; continuous values are
ramped per sample between ticks (pitch, level, wave position, bias, amplitude, phase, shape,
filter coefficients).

## Wavetables

2048-sample source cycles (the editable truth) → per frame, 20 levels spaced half an octave apart,
each keeping harmonics 1..H and stored at ≥ 4 (large) or ≥ 8 (small) samples per period of its top
harmonic, with guard samples for 4-point Hermite interpolation. The level is chosen so the top
harmonic (including unison detune, warp slope and FM bandwidth) stays below Nyquist of the process
rate; level changes crossfade within a block. Each frame's DC is stored separately so the shape's
own DC can be removed in real time while the explicit Vertical Bias stays untouched.

## Modules

* `DSP/Oscillator` – wavetable/noise/sample rendering, warps, unison, FM, inserts.
* `DSP/Filter` – ZDF SVF family, nonlinear ladder, comb, formant; coefficient ramps.
* `DSP/Modulators` – DAHDSR envelope with curves, LFO with custom tables.
* `DSP/Voice`, `DSP/SynthEngine` – voice allocation (poly/mono/legato), glide, MIDI, modulation
  resolution, buses, oversampling, racks.
* `DSP/Effects`, `DSP/FxRack` – the eleven effects and their configurable order.
* `MIDI/MidiFx` – arpeggiator and sequencer (sample-accurate, host-synced or free clock).
* `Wave/WaveDocument` – editing model with undo; `Wave/SampleImport` – audio import.
* `Presets/*` – state I/O, preset manager, factory library (designed in code with a small DSL),
  rule-based sound generator.
* `GUI/*` – DEADNOISE-family look and feel and all pages.

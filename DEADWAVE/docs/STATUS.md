# DEADWAVE – development status

Version 0.1.0 (first development build). This file separates what is **verified**,
what is **implemented but not yet verified in FL Studio**, and what is **not done**.

## Verified (automated, reproducible)

Linux (local + CI) — `DeadwaveTests` 76 checks, `DeadwaveHostTest` 10 checks, all passing:

* Pitch accuracy (A4 = 440.00 Hz), pitch bend range, legato glide (pitch measured
  between and at the target), mono return-to-held note.
* Band-limiting: saw at 3.7 kHz, worst non-harmonic component 92.5 dB below the
  fundamental in STANDARD and ULTRA.
* Vertical Bias changes the real waveform: through drive, the 2nd harmonic of a sine rises
  from −127 dB to −8 dB with bias 0.4; output DC stays at zero (voice DC blocker).
* Polyphony, release to silence, voice stealing.
* Modulation matrix (LFO → cutoff audibly modulates), arpeggiator step count/order,
  sequencer transposition with the free-running clock.
* No heap allocation in `processBlock` during 1 s of heavy processing (16-voice unison,
  FX lanes, ULTRA, arpeggiator, modulation) — counted with an instrumented allocator.
* All 11 effects at extreme settings stay finite (safety ceiling holds).
* Sample rates 44.1–192 kHz and block sizes 1–2048 (incl. odd sizes) render finite audio.
* State save/restore round-trip is identical, including restore from a background thread.
* All 228 factory presets render finite, audible audio below +8 dBFS for a 4-note chord;
  no two presets are near-duplicates by audio features (closest z-distance 0.16).
* The built VST3 loads through JUCE's VST3 hosting: 1 class, instrument, plays MIDI,
  host-side state round-trip identical, editor opens at 1280 × 820 and closes cleanly.
* CPU (one Linux core, 48 kHz): 512 simultaneously running oscillators
  (16 voices × 4 osc × 8 unison) ≈ 45 % STANDARD, ≈ 75 % ULTRA. Factory presets with a
  4-note chord: ≤ 6 %.

Windows — the CI pipeline (`.github/workflows/deadwave.yml`, same as DEADNOISE) builds with
MSVC and the static runtime, verifies the ZIP extracted to `D:\Plugins`, runs the engine
tests, Steinberg's validator, pluginval (strictness 10), the host test, Carla discovery and
renders the UI, then repeats the plug-in tests on a second clean machine. The result of each
run is written to `bin/TEST_REPORT.txt`; only that report states what passed on Windows.

## Implemented, not yet verified in FL Studio

* Scanning and loading in FL Studio, preset browsing, editor scaling under FL's DPI handling.
* Glide workflow with overlapping piano-roll notes; pitch bend from FL (VST3 delivers it
  through JUCE's MIDI-CC parameter mapping).
* Arpeggiator/sequencer sync with FL's transport (tested against synthetic host positions).
* Sample import / wavetable save dialogs (native file choosers).

## Not done yet / known gaps

* FL Studio piano-roll *slide notes* are not supported for VST3 instruments (FL feature);
  DEADWAVE offers overlap glide and pitch bend instead. Not claimed otherwise.
* Factory library: 228 presets (target 200–250). Every preset is checked automatically for
  audibility, level and near-duplicates; musical judgement still needs your ears.
* Per-effect parameters are host-automatable and modulatable at control rate (32 samples);
  there is no per-sample FX modulation.
* FM sources are limited to lower-numbered oscillators (and the internal sine) to keep the
  signal flow causal.
* No MPE / per-note expression beyond poly aftertouch.
* macOS / AU: the code is portable JUCE and the CMake project is ready for `FORMATS AU`,
  but no macOS build has been made.
* The plug-in is not code-signed (same as DEADNOISE).
* The host sees ~2,800 parameters: ~720 DEADWAVE parameters plus JUCE's VST3 MIDI-CC proxy
  parameters (needed so FL Studio can deliver pitch bend / mod wheel / aftertouch).

## Next steps

1. Run the Windows pipeline to green and install the ZIP in FL Studio (user test).
2. Fix anything found in FL Studio (scan, DPI, glide feel, CPU on the user's machine).
3. Refine factory presets after listening in FL Studio; add per-preset tags.
4. Optimise the oscillator inner loop (SIMD across unison voices).
5. Optional: wavetable drag-and-drop from the browser, MPE, AU build.

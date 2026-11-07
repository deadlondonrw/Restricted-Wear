# DEADNOISE test report

Version 1.0.0. Every number in this report comes from an automated run; the raw
logs are in [`ci/`](ci) (Windows) and [`test-results/`](test-results) (Linux).

## 1. Environments

| | Windows (target) | Linux (development) |
|---|---|---|
| Machine | GitHub Actions `windows-2022` runner (Windows Server 2022, x64) | Ubuntu 24.04 container, 4 cores |
| Compiler | MSVC / Visual Studio 2022, `/MT` static runtime | GCC 13 |
| Plugin | `DEADNOISE.vst3`, exactly the file in `bin/` | Linux VST3 build of the same sources |
| Logs | `docs/ci/*.txt`, `docs/ci/build-info.txt` | `docs/test-results/*.txt` |

FL Studio itself was **not** available in either environment.

## 2. Summary

| Test | Windows | Linux |
|---|---|---|
| DSP test-suite, 69 checks (`tests/dsp_tests.cpp`) | **passed** (`ci/dsp-tests-windows.txt`) | **passed** |
| Steinberg VST3 validator, extensive mode (`-e`) | **537 / 537 passed** (`ci/vst3-validator-windows.txt`) | **537 / 537 passed** |
| pluginval, strictness level 10 | **passed** (`ci/pluginval-windows.txt`) | not run |
| Host-level test, 47 checks (`tools/host_test.cpp`) | **passed** (`ci/host-test-windows.txt`) | **passed** |
| Editor render | `ci/deadnoise-ui-windows.png` | |
| Objective audio evaluation, 23 scenarios × 5 settings | | section 5 |

pluginval at level 10 covers: cold/warm open, plugin info, programs, editor,
editor open while processing, audio processing at several rates and block
sizes, non-releasing processing, state save/restore, automation, editor
automation, parameter thread safety, background-thread state, the VST3
validator, bus layouts and parameter fuzzing.

## 3. DSP test-suite

| Area | Check | Result |
|---|---|---|
| FFT | against a direct DFT, round trip (64 … 2048) | error ≤ 4e-7 |
| Transparency | 0 % reduction = pure delay, 44.1/48/96 kHz, mono/stereo, random blocks | max error ≤ 9e-8 |
| Latency | impulse with suppression active | peak exactly at the reported 1024 samples; level −0.66 dB |
| Silence | digital silence in | exact digital silence out |
| Invalid input | NaN / ±Inf burst | output finite, suppression continues |
| Clipping | 0 dBFS speech + hiss | output peak −0.11 dBFS vs input −0.08 dBFS (gain never > 1) |
| Buffer sizes | random host blocks 1 … 4096 vs fixed 512 | bit-identical |
| Real-time safety | heap allocations inside `process()` incl. learning and mode/monitor/voice changes | **0** |
| Learn: accuracy | clean hiss, worst ⅓-octave band error vs a reference | ≤ 0.5 dB, 98.5 % of frames accepted |
| Learn: partial speech | voice in 37 % of the 4 s window | profile error ≤ 0.4 dB (voice frames rejected) |
| Learn: continuous talking | two overlapping syllabic voices | refused ("too much sound") |
| Learn: muted input | digital silence | refused ("no input signal") |
| Profile storage | serialise / deserialise, corrupted data | exact round trip, corruption rejected |
| Suppression depth | HF hiss and white noise, Natural/Maximum at 50/80/100 % | −16 / −25.6 / −32 dB and −30 / −48 / −60 dB (targets met within 0.4 dB) |
| Adaptive mode | no profile | −25.6 dB |
| Input gain | +12 dB after learning | still −31.9 dB (profile follows the input gain) |
| Noise drift | hiss +6 dB after learning | −24.9 dB, 3 s later |
| Speech | synthetic voiced speech at 20 dB SNR | level change 0.00 dB, SDR 20 → 34 dB (both modes) |
| Stereo | identical L/R → identical output; L-only → R silent | yes / yes |
| Monitoring | Original = latency-aligned input; Removed = Original − Processed | errors 1.5e-8 / 1.4e-8 |
| Gate | extra attenuation in pauses / effect on speech / click risk | −30 dB / −0.001 dB / max gain step 0.005 per sample |
| Sample rates | 48 kHz profile used at 96 kHz; re-prepare 44.1 → 96 → 48 kHz | −32 dB; ok |
| Stress | random automation of all parameters every block, 30 s | finite, bounded |
| CPU | stereo, 256-sample blocks, Windows MSVC build | 1.3–2.0 % (48 kHz), 2.6–4.0 % (96 kHz) of one core, depending on the CI machine |

<details><summary>Full Linux output (dsp-tests-linux.txt)</summary>

```
DEADNOISE DSP test suite
========================

[FFT correctness]
    n=64     max |DFT err|/sqrt(n) = 2.03e-07   round trip = 4.77e-07
    n=512    max |DFT err|/sqrt(n) = 3.51e-07   round trip = 5.96e-07
    n=1024   max |DFT err|/sqrt(n) = 3.53e-07   round trip = 7.15e-07
    n=2048   max |DFT err|/sqrt(n) = 3.88e-07   round trip = 6.56e-07

[Transparency at 0 % reduction, latency, sample rates, channel layouts]
     44100 Hz  1 ch  latency 1024 smp (23.22 ms)  max |out - delayed in| = 5.96e-08
     44100 Hz  2 ch  latency 1024 smp (23.22 ms)  max |out - delayed in| = 8.94e-08
     48000 Hz  1 ch  latency 1024 smp (21.33 ms)  max |out - delayed in| = 5.96e-08
     48000 Hz  2 ch  latency 1024 smp (21.33 ms)  max |out - delayed in| = 8.94e-08
     96000 Hz  1 ch  latency 2048 smp (21.33 ms)  max |out - delayed in| = 8.94e-08
     96000 Hz  2 ch  latency 2048 smp (21.33 ms)  max |out - delayed in| = 8.94e-08
    impulse peak offset with suppression active (samples)   1024.000 
    impulse peak level preserved (dB)                        -0.694 

[Silence, NaN / Inf input, extreme levels]
    digital silence -> exact digital silence: yes
    NaN/Inf burst: output finite = yes, still suppressing afterwards (-99.9 dB)
    loud input: peak in (dBFS)                               -0.083 
    loud input: peak out (dBFS)                              -0.107 

[Host buffer-size independence (random block sizes 1..4096)]
    max difference fixed 512 vs random blocks                 0.000 

[Real-time safety: heap allocations inside process()]
    operator new calls during process() (incl. learning)      0.000 

[Learn Background: accuracy and speech rejection]
    clean hiss: accepted frames (%)                          98.533 
    clean hiss: worst band error vs reference (dB)            0.416 
    speech in 37 % of window: accepted frames (%)            57.733 
    speech in 37 % of window: worst band error (dB)           0.381 
    continuous talking during learning -> rejected (correct)
    muted input during learning -> 'no signal' (correct)
    profile serialise/deserialise round trip ok, truncated data rejected

[Noise suppression depth (stationary hiss, profile learned)]
    HF hiss   Natural   50 %   attenuation  -16.0 dB  (target -16.0 dB)
    HF hiss   Natural   80 %   attenuation  -25.5 dB  (target -25.6 dB)
    HF hiss   Natural  100 %   attenuation  -31.8 dB  (target -32.0 dB)
    HF hiss   Maximum   50 %   attenuation  -30.0 dB  (target -30.0 dB)
    HF hiss   Maximum   80 %   attenuation  -47.9 dB  (target -48.0 dB)
    HF hiss   Maximum  100 %   attenuation  -59.8 dB  (target -60.0 dB)
    white     Natural   50 %   attenuation  -16.0 dB  (target -16.0 dB)
    white     Natural   80 %   attenuation  -25.6 dB  (target -25.6 dB)
    white     Natural  100 %   attenuation  -32.0 dB  (target -32.0 dB)
    white     Maximum   50 %   attenuation  -30.0 dB  (target -30.0 dB)
    white     Maximum   80 %   attenuation  -48.0 dB  (target -48.0 dB)
    white     Maximum  100 %   attenuation  -60.0 dB  (target -60.0 dB)
    adaptive tracking (no profile) attenuation (dB)         -25.552 dB
    +12 dB input gain after learning: attenuation (dB)      -31.770 dB
    hiss +6 dB after learning (3 s later) attenuation (dB)    -24.876 dB

[Speech preservation (synthetic voiced speech + hiss, 20 dB SNR)]
    Natural  speech level change  -0.00 dB   SDR in  20.0 dB -> out  34.4 dB
    Maximum  speech level change  -0.00 dB   SDR in  20.0 dB -> out  34.3 dB

[Stereo linking, monitor modes, gate]
    identical L/R -> identical output: yes;  L-only input -> R stays silent: yes
    'Original' = latency-aligned input (err 1.5e-08);  'Removed' = original - processed (err 1.5e-08)
    gate: extra attenuation in pauses (dB)                  -29.996 dB
    gate: level change during speech (dB)                    -0.001 dB
    gate: max per-sample gain step                            0.005 

[Profile reuse across sample rates / FFT sizes]
    48 kHz profile used at 96 kHz: attenuation (dB)         -31.972 dB
    re-prepare 44.1 -> 96 -> 48 kHz with profile: ok

[Stress: random parameter automation every block (30 s, stereo)]
    output finite: yes, peak 71.37 (input peak x gains bounded)

[CPU cost]
     44100 Hz stereo, 256-sample blocks: 3.11 % of one CPU core (real-time factor 0.0311)
     48000 Hz stereo, 256-sample blocks: 3.66 % of one CPU core (real-time factor 0.0366)
     96000 Hz stereo, 256-sample blocks: 6.92 % of one CPU core (real-time factor 0.0692)

69 checks, 0 failures
ALL TESTS PASSED
```
</details>

## 4. Host-level test (loads the .vst3 like a DAW)

`tools/host_test.cpp` uses JUCE's VST3 *hosting* code, i.e. the same VST3
interfaces FL Studio uses, on the real binary:

* scan: 1 plugin, name *DEADNOISE*, category *Fx|Restoration*, not an instrument;
* 6 configurations (44.1/48/96 kHz, blocks 64 … 4096 including 333, mono and
  stereo layouts): reported latency exactly 1024 / 2048, 0 % reduction is a
  perfect delay (error ≈ 1e-8), default settings remove stationary hiss by 25 dB;
* a learned profile injected through the VST3 component state (the data a DAW
  stores in the project) is used after loading: −31.9 dB at 100 %;
* save → load into a new instance: parameters and profile restored, output
  identical (difference 0.0);
* editor opened and closed 12 times while audio is being processed.

## 5. Objective audio evaluation

**Material (synthetic noise, real recorded speech).** Three LibriSpeech excerpts
(16 kHz source, so no speech content above 8 kHz) and one full-band 48 kHz
speech recording. Noise: synthetic stationary *USB-microphone hiss* (flat below
1 kHz, +4 dB/octave up to 9 kHz, flat to 18 kHz), white noise, and one recorded
noise file. Every file has 5 s of noise only (Learn is pressed at 0.5 s), then
speech, then 2 s of noise. Regenerate with `tools/make_testset.py`, evaluate
with `tools/evaluate.py`, build the tables with `tools/summarize.py`; the raw
results are in `test-results/objective-evaluation.json`.

**Settings.** `natural` = default (Natural, 80 %, Voice Preservation 50 %);
`natural100` = Natural 100 %; `max` = Maximum 100 %; `natural_vp1` = Natural
80 % with Voice Preservation 100 %; `adaptive` = default settings without
learning a profile.

**Metrics.** PESQ wide-band (ITU-T P.862.2, at 16 kHz) and STOI against the
clean speech; noise-floor attenuation measured in the last 1.5 s (noise only);
*spike* = 99.9th percentile of the time-frequency gain above its median in that
region (musical-noise indicator: 0 means a perfectly constant residual);
band changes = mean level of speech-active frames relative to the clean speech.

### Speech quality (PESQ-WB / STOI)

| case | noisy | natural | natural100 | max | natural_vp1 | adaptive |
|---|---|---|---|---|---|---|
| libri1_usb_snr30 | 3.86 / 0.994 | 4.31 / 0.992 | 4.24 / 0.991 | 4.09 / 0.989 | 4.34 / 0.992 | 3.79 / 0.982 |
| libri1_usb_snr20 | 2.55 / 0.979 | 3.77 / 0.977 | 3.61 / 0.976 | 3.17 / 0.971 | 3.72 / 0.977 | 3.42 / 0.970 |
| libri1_usb_snr10 | 1.49 / 0.942 | 2.71 / 0.943 | 2.55 / 0.941 | 1.90 / 0.937 | 2.66 / 0.943 | 2.45 / 0.938 |
| libri2_usb_snr30 | 2.81 / 1.000 | 4.02 / 0.999 | 4.01 / 0.999 | 3.86 / 0.999 | 3.92 / 0.999 | 3.42 / 0.988 |
| libri2_usb_snr20 | 1.80 / 0.996 | 3.22 / 0.994 | 3.25 / 0.993 | 2.99 / 0.992 | 3.10 / 0.994 | 2.83 / 0.985 |
| libri2_usb_snr10 | 1.22 / 0.975 | 2.37 / 0.967 | 2.37 / 0.965 | 2.15 / 0.958 | 2.20 / 0.969 | 2.17 / 0.958 |
| libri3_usb_snr30 | 2.58 / 0.998 | 4.16 / 0.997 | 4.12 / 0.996 | 3.95 / 0.995 | 4.06 / 0.997 | 3.66 / 0.991 |
| libri3_usb_snr20 | 1.62 / 0.984 | 3.32 / 0.980 | 3.27 / 0.979 | 2.89 / 0.973 | 3.12 / 0.981 | 2.95 / 0.974 |
| libri3_usb_snr10 | 1.14 / 0.942 | 2.13 / 0.932 | 2.12 / 0.928 | 1.72 / 0.915 | 2.01 / 0.931 | 1.96 / 0.924 |
| fullband_usb_snr30 | 3.27 / 0.999 | 4.18 / 0.999 | 4.17 / 0.999 | 4.05 / 0.999 | 4.12 / 0.999 | 4.09 / 0.997 |
| fullband_usb_snr20 | 2.14 / 0.995 | 3.47 / 0.993 | 3.46 / 0.993 | 3.22 / 0.992 | 3.36 / 0.994 | 3.46 / 0.991 |
| fullband_usb_snr10 | 1.27 / 0.973 | 2.60 / 0.974 | 2.46 / 0.972 | 2.08 / 0.967 | 2.50 / 0.971 | 2.60 / 0.971 |
| fullband_usb_snr5 | 1.11 / 0.949 | 2.19 / 0.947 | 2.05 / 0.944 | 1.62 / 0.937 | 2.07 / 0.945 | 2.18 / 0.946 |
| fullband_white_snr20 | 1.85 / 0.984 | 3.06 / 0.983 | 2.97 / 0.982 | 2.65 / 0.980 | 3.02 / 0.984 | 3.07 / 0.981 |
| libri1_realnoise_snr20 | 3.87 / 0.991 | 3.47 / 0.980 | 3.39 / 0.978 | 3.21 / 0.974 | 3.62 / 0.982 | 3.47 / 0.980 |
| quiet_speech | 1.61 / 0.988 | 3.04 / 0.986 | 2.96 / 0.985 | 2.60 / 0.982 | 2.98 / 0.987 | 3.00 / 0.984 |
| loud_speech | 3.71 / 1.000 | 4.36 / 1.000 | 4.33 / 1.000 | 4.27 / 1.000 | 4.31 / 1.000 | 4.22 / 0.998 |
| breathing | 2.45 / 0.999 | 3.63 / 0.998 | 3.66 / 0.998 | 3.46 / 0.998 | 3.58 / 0.998 | 3.15 / 0.988 |
| noise_jump_up | 1.64 / 0.986 | 3.19 / 0.986 | 3.23 / 0.985 | 3.07 / 0.983 | 2.66 / 0.986 | 2.98 / 0.976 |
| noise_jump_down | 2.50 / 0.997 | 3.95 / 0.995 | 3.88 / 0.994 | 3.69 / 0.992 | 3.90 / 0.995 | 3.57 / 0.990 |

### Noise-floor attenuation in pauses (dB) and gain spikes (dB, musical-noise indicator)

| case | natural | natural100 | max | natural_vp1 | adaptive |
|---|---|---|---|---|---|
| libri1_usb_snr30 | -25.6 / 0.6 | -32.0 / 1.0 | -60.0 / 0.2 | -25.6 / 1.0 | -25.5 / 2.6 |
| libri1_usb_snr20 | -25.6 / 0.8 | -31.9 / 1.2 | -59.9 / 0.3 | -25.6 / 1.0 | -25.6 / 1.8 |
| libri1_usb_snr10 | -25.6 / 0.8 | -32.0 / 1.3 | -59.9 / 0.3 | -25.6 / 1.1 | -25.5 / 1.6 |
| libri2_usb_snr30 | -25.5 / 2.7 | -31.8 / 4.6 | -59.9 / 0.6 | -25.5 / 3.9 | -25.5 / 2.8 |
| libri2_usb_snr20 | -25.6 / 1.1 | -31.9 / 1.6 | -59.9 / 0.3 | -25.6 / 1.4 | -25.6 / 1.6 |
| libri2_usb_snr10 | -25.5 / 1.1 | -31.9 / 1.7 | -59.9 / 0.3 | -25.5 / 1.6 | -25.5 / 0.7 |
| libri3_usb_snr30 | -25.4 / 4.4 | -31.5 / 7.9 | -59.4 / 2.7 | -25.4 / 6.8 | -25.4 / 9.5 |
| libri3_usb_snr20 | -25.6 / 0.6 | -32.0 / 1.0 | -59.9 / 0.2 | -25.6 / 0.9 | -25.5 / 1.0 |
| libri3_usb_snr10 | -25.5 / 2.6 | -31.8 / 4.1 | -59.9 / 0.5 | -25.5 / 3.7 | -25.5 / 2.9 |
| fullband_usb_snr30 | -25.6 / 0.3 | -31.9 / 0.5 | -60.0 / 0.2 | -25.6 / 0.4 | -25.6 / 2.0 |
| fullband_usb_snr20 | -25.6 / 1.1 | -32.0 / 1.5 | -59.9 / 0.3 | -25.6 / 1.4 | -25.6 / 1.5 |
| fullband_usb_snr10 | -25.5 / 1.7 | -31.9 / 3.2 | -59.9 / 0.5 | -25.5 / 2.8 | -25.5 / 1.8 |
| fullband_usb_snr5 | -25.6 / 0.5 | -32.0 / 0.7 | -60.0 / 0.2 | -25.6 / 0.6 | -25.5 / 3.3 |
| fullband_white_snr20 | -25.5 / 0.9 | -31.8 / 1.4 | -59.9 / 0.3 | -25.5 / 1.3 | -25.5 / 2.4 |
| libri1_realnoise_snr20 | -8.1 / 31.2 | -8.4 / 36.9 | -9.0 / 58.3 | -8.1 / 24.3 | -8.1 / 31.2 |
| quiet_speech | -25.6 / 0.9 | -32.0 / 1.3 | -60.0 / 0.3 | -25.6 / 1.1 | -25.5 / 2.8 |
| loud_speech | -25.6 / 0.1 | -32.0 / 0.2 | -60.0 / 0.1 | -25.6 / 0.1 | -25.6 / 0.9 |
| breathing | -25.6 / 0.5 | -32.0 / 0.7 | -60.0 / 0.2 | -25.6 / 0.6 | -25.6 / 1.5 |
| quiet_consonants | -25.6 / 1.8 | -31.9 / 3.7 | -59.8 / 5.0 | -25.5 / 2.4 | -25.5 / 3.6 |
| noise_jump_up | -25.5 / 4.6 | -31.8 / 7.2 | -59.9 / 1.5 | -25.5 / 5.9 | -25.5 / 1.1 |
| noise_jump_down | -25.6 / 0.1 | -32.0 / 0.1 | -60.0 / 0.1 | -25.6 / 0.1 | -25.5 / 2.4 |
| hiss_only | -25.6 / 0.3 | -32.0 / 0.4 | -60.0 / 0.1 | -25.6 / 0.5 | -25.6 / 0.9 |
| white_only | -25.5 / 1.1 | -31.8 / 1.8 | -59.7 / 0.2 | -25.5 / 1.7 | -25.5 / 1.2 |

### Speech preservation: level change of speech-active frames vs clean (dB): 0.08-1 / 1-4 / 4-8 / 8-16 kHz, onset

| case | natural | natural100 | max | natural_vp1 | adaptive |
|---|---|---|---|---|---|
| libri1_usb_snr30 | -0.0 / -0.1 / -0.9 / -4.0, on -0.7 | -0.0 / -0.2 / -1.0 / -6.0, on -0.8 | -0.0 / -0.2 / -1.3 / -13.0, on -0.8 | -0.0 / -0.1 / -0.7 / -0.9, on -0.7 | -0.4 / -0.4 / -1.6 / -4.7, on -0.7 |
| libri1_usb_snr20 | -0.0 / -0.6 / -2.5 / -1.1, on -0.6 | -0.0 / -0.7 / -2.9 / -5.4, on -0.7 | -0.1 / -0.9 / -4.2 / -25.4, on -1.7 | -0.0 / -0.4 / -1.5 / 3.8, on -0.6 | -0.4 / -0.9 / -3.7 / -1.2, on -0.6 |
| libri1_usb_snr10 | -0.2 / -1.5 / -4.8 / 6.7, on -0.1 | -0.2 / -1.9 / -6.4 / 1.3, on -0.6 | -0.2 / -2.8 / -12.3 / -24.5, on -15.1 | -0.1 / -0.9 / -2.2 / 12.2, on -0.1 | -0.5 / -2.1 / -5.8 / 7.0, on -0.1 |
| libri2_usb_snr30 | -0.0 / -0.2 / -3.1 / 1.7, on -0.1 | -0.0 / -0.2 / -3.8 / -2.3, on -0.1 | -0.0 / -0.3 / -5.9 / -21.7, on -0.2 | -0.0 / -0.2 / -1.9 / 6.0, on -0.1 | -1.1 / -0.6 / -4.0 / 1.8, on -0.2 |
| libri2_usb_snr20 | -0.0 / -0.6 / -4.7 / 8.8, on -0.5 | -0.0 / -0.8 / -6.8 / 3.8, on -0.6 | -0.0 / -1.0 / -15.2 / -20.5, on -0.8 | -0.0 / -0.5 / -1.4 / 13.9, on -0.5 | -1.0 / -1.1 / -5.7 / 9.0, on -0.5 |
| libri2_usb_snr10 | -0.1 / -1.6 / -2.8 / 17.8, on -1.8 | -0.1 / -2.0 / -7.1 / 12.3, on -2.1 | -0.2 / -3.0 / -26.1 / -14.3, on -2.9 | -0.1 / -0.9 / 1.7 / 23.0, on -1.6 | -1.0 / -2.3 / -3.1 / 18.1, on -1.5 |
| libri3_usb_snr30 | -0.0 / -0.2 / -2.2 / 6.9, on -0.5 | -0.0 / -0.2 / -2.6 / 1.7, on -0.5 | -0.0 / -0.2 / -3.6 / -23.4, on -0.5 | -0.0 / -0.1 / -1.5 / 12.2, on -0.5 | -1.1 / -0.5 / -3.3 / 7.2, on -0.5 |
| libri3_usb_snr20 | -0.1 / -0.5 / -5.5 / 16.2, on -0.3 | -0.1 / -0.6 / -7.5 / 10.7, on -0.4 | -0.1 / -0.8 / -16.9 / -16.2, on -0.5 | -0.1 / -0.4 / -2.4 / 21.7, on -0.2 | -1.1 / -1.0 / -6.9 / 16.6, on -0.3 |
| libri3_usb_snr10 | -0.5 / -1.4 / -4.6 / 26.2, on -1.2 | -0.6 / -1.7 / -9.2 / 20.7, on -1.7 | -0.9 / -2.8 / -31.4 / -7.0, on -2.7 | -0.4 / -0.7 / 1.0 / 32.2, on -1.1 | -1.3 / -2.3 / -4.6 / 26.6, on -1.2 |
| fullband_usb_snr30 | -0.0 / -0.2 / -0.7 / -2.9, on -0.1 | -0.0 / -0.2 / -0.9 / -3.4, on -0.1 | -0.0 / -0.3 / -1.1 / -4.9, on -0.1 | -0.0 / -0.2 / -0.6 / -1.9, on -0.1 | -0.2 / -0.3 / -1.0 / -3.5, on -0.1 |
| fullband_usb_snr20 | -0.0 / -0.8 / -1.7 / -4.3, on -0.2 | -0.0 / -0.9 / -2.1 / -5.8, on -0.2 | -0.0 / -1.2 / -3.4 / -11.2, on -0.2 | -0.0 / -0.5 / -0.8 / -1.6, on -0.1 | -0.2 / -1.4 / -2.4 / -5.2, on -0.2 |
| fullband_usb_snr10 | -0.1 / -2.3 / -3.2 / -3.9, on -0.3 | -0.1 / -2.8 / -4.6 / -7.2, on -0.4 | -0.1 / -4.7 / -9.6 / -22.0, on -0.6 | -0.1 / -1.3 / -0.9 / 0.2, on -0.3 | -0.2 / -2.8 / -4.1 / -4.3, on -0.2 |
| fullband_usb_snr5 | -0.1 / -2.9 / -3.6 / -1.8, on -0.8 | -0.2 / -3.9 / -5.9 / -5.8, on -1.2 | -0.2 / -7.0 / -15.0 / -24.9, on -1.6 | -0.1 / -1.5 / -0.5 / 3.0, on -0.7 | -0.2 / -3.3 / -4.1 / -1.8, on -0.8 |
| fullband_white_snr20 | -0.1 / -1.5 / -2.0 / -4.2, on -0.6 | -0.1 / -1.8 / -2.5 / -5.5, on -0.7 | -0.1 / -2.6 / -4.0 / -10.5, on -0.8 | -0.1 / -1.0 / -1.1 / -2.1, on -0.6 | -0.2 / -2.3 / -2.7 / -5.1, on -0.6 |
| libri1_realnoise_snr20 | -0.4 / -0.3 / -0.2 / 1.8, on 2.6 | -0.5 / -0.3 / -0.2 / 1.8, on 2.3 | -0.5 / -0.4 / -0.3 / 1.6, on 2.1 | -0.4 / -0.2 / -0.1 / 1.9, on 2.6 | -0.4 / -0.3 / -0.2 / 1.8, on 2.6 |
| quiet_speech | -0.0 / -1.6 / -2.8 / -4.5, on -0.3 | -0.0 / -1.9 / -3.6 / -6.8, on -0.3 | -0.1 / -2.6 / -6.6 / -16.1, on -0.4 | -0.0 / -1.0 / -1.1 / -1.0, on -0.2 | -0.2 / -2.1 / -3.7 / -5.3, on -0.2 |
| loud_speech | -0.0 / -0.1 / -0.4 / -1.7, on -0.0 | -0.0 / -0.1 / -0.5 / -1.9, on -0.0 | -0.0 / -0.1 / -0.6 / -2.5, on -0.0 | -0.0 / -0.1 / -0.3 / -1.3, on -0.0 | -0.2 / -0.2 / -0.5 / -2.0, on -0.1 |
| breathing | -0.0 / -0.3 / -4.1 / 0.1, on -0.9 | -0.0 / -0.4 / -5.2 / -3.6, on -1.1 | -0.0 / -0.5 / -8.5 / -20.0, on -1.3 | -0.0 / -0.3 / -2.7 / 3.9, on -0.9 | -1.0 / -0.8 / -4.5 / -0.8, on -0.9 |
| quiet_consonants | -0.3 / -1.0 / -0.7 / -1.9, on -1.2 | -3.6 / -1.3 / -0.9 / -2.8, on -1.4 | -18.1 / -1.9 / -1.4 / -5.5, on -1.9 | 1.8 / -0.8 / -0.6 / -1.3, on -1.2 | -0.3 / -1.1 / -0.7 / -2.0, on -1.2 |
| noise_jump_up | -0.0 / -0.1 / -1.8 / 20.5, on -0.5 | -0.0 / -0.2 / -2.9 / 17.2, on -0.5 | -0.1 / -0.3 / -8.2 / -2.5, on -0.6 | -0.0 / 0.0 / 1.1 / 26.8, on -0.5 | -1.1 / -1.0 / -6.5 / 16.5, on -0.5 |
| noise_jump_down | -0.0 / -0.3 / -3.6 / 6.3, on -0.5 | -0.1 / -0.3 / -4.2 / 0.3, on -0.5 | -0.1 / -0.4 / -6.3 / -26.2, on -0.5 | -0.0 / -0.2 / -2.7 / 7.4, on -0.4 | -1.1 / -0.5 / -3.5 / 7.6, on -0.5 |

Non-finite samples in any output: none. Largest output-minus-input peak: +0.0281 (linear). Learn results: failed-activity, not-requested, succeeded. Real-time factor of the CLI runs: 0.022 - 0.028.

### Reading the results

* **Hiss removal.** With a learned profile the noise floor in pauses drops by
  the configured depth: 25.6 dB (default), 32 dB (Natural 100 %), 60 dB
  (Maximum). PESQ rises in every stationary-noise case, e.g. full-band speech
  at 20 dB SNR 2.14 → 3.47, at 10 dB SNR 1.27 → 2.60, quiet speech 1.61 → 3.04.
* **No musical noise.** Gain spikes in noise-only regions are about 0.1–2 dB in
  almost all cases (a constant, attenuated residual). The largest values among
  the stationary cases are 4–8 dB in one LibriSpeech file and right after the
  +6 dB noise jump.
* **Speech preservation.** Below 4 kHz the speech level changes by less than
  1 dB at ≥ 20 dB SNR. At 4–8 kHz, speech energy that lies below the hiss is
  partly removed (Natural −0.7 … −5.5 dB, more in Maximum). Voice Preservation
  100 % keeps the 4–8 kHz band within about ±2.5 dB. Onsets (first 30 ms) stay
  within −0.1 … −1.8 dB; the −15.1 dB in `libri1_usb_snr10` / max is a word
  start that lies 16 dB *below* the noise floor.
* **The 8–16 kHz column for LibriSpeech is not meaningful.** Those recordings
  have no speech above 8 kHz, so any remaining hiss shows up as a large
  positive number. Use the full-band rows for that band.
* **Breathing and quiet consonants** are kept (breath onsets −0.9 dB, isolated
  fricatives −0.7 … −1.9 dB in Natural).
* **Loud speech** (peaks −0.3 dBFS): no clipping. The largest output-minus-input
  peak over the whole evaluation is +0.028 (linear); the gains themselves
  never exceed 1.
* **Noise level changes.** A +6 dB jump during speech is followed within a few
  seconds (broadband changes are recognised, and the editor asks to relearn).
  −6 dB is no problem.
* **Limitation shown by `libri1_realnoise_snr20`.** This recorded noise is not a
  background noise but a 5 s sequence of events (50 ms levels from −95 to
  −9 dBFS). Learning correctly refuses it ("too much sound"), and the adaptive
  tracker cannot remove such transients: attenuation only ~8 dB, PESQ drops
  (3.87 → 3.47). DEADNOISE is built for stationary noise such as the hiss of a
  USB microphone.

## 6. Neural engine evaluation

DeepFilterNet 3 was built and measured on the same material; see
[NEURAL_ENGINE_EVALUATION.md](NEURAL_ENGINE_EVALUATION.md). It is not included.

## 7. What was synthetic, and what still needs your microphone

| Done (automated) | Still to do by listening, with your microphone in FL Studio |
|---|---|
| Synthetic stationary hiss shaped like USB-mic noise, white noise | Your mic's actual hiss (spectrum, stability, tonal whine if any) |
| Real speech recordings (LibriSpeech, one full-band speaker) mixed with that hiss | Your own voice: sibilance, breaths, quiet word endings |
| Synthetic breaths and isolated fricatives | Real breaths, lip noise, plosives at your speaking distance |
| VST3 validator, pluginval and a VST3 host test on Windows | Scanning, inserting, saving and reloading a project in FL Studio |
| CPU measured on a CI machine | CPU and latency on your PC with your audio driver |

**Suggested listening test.** Learn the profile while silent, record 30 s of
speech with Monitoring = *Processed*, then listen to the same take with
*Removed*. Only hiss in *Removed* means the settings are right. Words or "s"
sounds in *Removed*: raise Voice Preservation or lower Noise Reduction.
Remaining hiss in *Processed*: Noise Reduction 100 % or Maximum.

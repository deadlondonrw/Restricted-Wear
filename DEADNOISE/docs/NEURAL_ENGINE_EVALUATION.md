# Neural speech enhancement: evaluation and decision

**Decision: no neural network engine ships in DEADNOISE 1.0.** The classical
spectral engine with a learned noise profile performs better on this project's
stated priorities (constant hiss, natural voice, no artifacts, low latency,
low CPU). This document records what was evaluated and measured.

## DeepFilterNet 3 (measured)

* Source: <https://github.com/Rikorose/DeepFilterNet> (commit `7b14b89`,
  2026-11-06), MIT / Apache-2.0. Model `DeepFilterNet3_onnx.tar.gz`.
* Runtime: the project's own Rust inference library (`libDF`, using `tract`;
  no ONNX Runtime needed). Built natively with
  `cargo build --release -p deep_filter --bin deep-filter --features "bin,tract,wav-utils,transforms,default-model"`
  and run as `deep-filter -D` (delay compensated) on the same 48 kHz test
  files as DEADNOISE.
* Integration path considered: Rust static library with a panic-safe C API,
  embedded model, inference on a worker thread with lock-free FIFOs (tract
  allocates per frame, which is not allowed on the audio thread), and
  resampling for 44.1 / 96 kHz (the model only runs at 48 kHz). This is
  technically feasible on Windows (MSVC + rustup on the CI runner).

### Cost

| | DEADNOISE spectral | DeepFilterNet 3 |
|---|---|---|
| CPU, one 48 kHz channel (same Linux machine, single core) | ~1.7 % | **~11.5 %** (RTF 0.112–0.118) |
| Algorithmic latency | 21.3 ms | ~40 ms (20 ms window + 2 frames lookahead), plus worker-thread buffering (≥ 1 host block + 10 ms) and resampler delay at 44.1 kHz |
| Sample rates | any | 48 kHz only (resampling required) |

### Quality (same files, same metrics as docs/TEST_REPORT.md)

The DEADNOISE column was measured with the build that was current at the time
of the comparison. The final 1.0 build differs by at most ~0.05 PESQ and
~1 dB spike (see TEST_REPORT.md), which does not change any conclusion.

`spike` = 99.9th-percentile T-F gain above the median gain in noise-only
regions (dB). Large values = a fluctuating, "watery/bubbling" residual.
Bands: level change of speech vs the clean reference (dB).

| case | metric | DEADNOISE Natural 80 % | DF3 (full) | DF3, 25 dB limit | Natural → DF3 (12 dB) |
|---|---|---|---|---|---|
| hiss only | attenuation / spike | −25.6 / **0.5** | −37.9 / 29.6 | −25.6 / 11.3 | −36.6 / 21.7 |
| white noise only | attenuation / spike | −25.5 / **1.1** | −19.8 / 27.8 | −25.5 / 8.6 | −34.2 / 17.2 |
| full-band speech, 20 dB SNR | PESQ | 3.46 | **4.04** | 3.71 | 3.62 |
| full-band speech, 10 dB SNR | PESQ | 2.60 | **3.55** | 3.07 | 2.54 |
| quiet speech (−40 dBFS) | PESQ / onset | 3.03 / **−0.3 dB** | 3.24 / −5.1 dB | 3.22 / −3.0 dB | 3.05 / −3.3 dB |
| isolated quiet consonants | 1–4 / 4–8 / 8–16 kHz | **−1.0 / −0.7 / −1.9** | −5.8 / −7.5 / −7.9 | −23.8 / −23.9 / −19.7 | −11.4 / −11.0 / −12.5 |
| speech + breaths | PESQ / breath onset | **3.63 / −1.0 dB** | 3.32 / −37.7 dB | 3.15 / −5.0 dB | 3.76 / −3.4 dB |

### Findings

* DF3 gets higher PESQ at moderate/low SNR because it also removes the
  residual hiss *inside* speech more aggressively. This is the case it was
  designed and trained for (dense, non-stationary noise).
* On this project's priorities it is worse:
  1. **Quiet consonants**: isolated fricatives lose 6–8 dB (full model) and up
     to 24 dB with an attenuation limit. The brief explicitly forbids
     aggressive cutting of quiet consonants.
  2. **Breaths** are removed (−38 dB), which changes the character of a voice
     recording.
  3. **Residual artifacts**: in noise-only regions the residual fluctuates by
     28–33 dB (gain spikes), i.e. watery/bubbling residual noise. DEADNOISE
     keeps a constant floor (≤ 1–2 dB). With an attenuation limit or as a
     post-stage, DF3 still fluctuates by 9–22 dB.
  4. **Latency** roughly doubles to triples (≥ 50 ms in a plugin), **CPU**
     roughly ×7.
* For a constant hiss whose spectrum can be measured (Learn Background), the
  statistically optimal classical estimator already reaches 25–60 dB
  attenuation without musical noise. The learned profile is the information
  a general-purpose network does not have.

DeepFilterNet would be the right tool for a noisy room (keyboard, traffic,
other voices). If that becomes a need, the integration path above is
feasible; the measurements say it should then be an optional mode, not the
default.

## RNNoise (not measured)

* <https://github.com/xiph/rnnoise>, BSD-3. Current releases download the model
  weights from `media.xiph.org` at build time, and that host was not reachable
  from the build environment (blocked by the network policy). Building and
  running the older v0.1.1 (weights in the source tree) was not permitted in
  this session, so **no measurements were made**.
* On design grounds alone, RNNoise computes 22 band gains (Bark-like, very wide
  bands at high frequencies) plus a pitch filter. For a hiss concentrated at
  high frequencies, wide-band gains cannot remove noise between speech
  components without dulling the voice. Its strength is very low CPU on
  non-stationary noise.

## Reproduce

```bash
python3 tools/make_testset.py /tmp/testset /path/to/deps
```

```bash
deep-filter -D -o /tmp/df_out /tmp/testset/*_noisy.wav
```

Then compute the metrics with the functions in `tools/evaluate.py`
(`speech_metrics`, `gain_spikes`, `band_changes`, `onset_level`).

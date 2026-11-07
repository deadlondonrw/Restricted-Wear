#!/usr/bin/env python3
"""Objective evaluation of the DEADNOISE DSP chain on the generated test set.

usage: evaluate.py CLI_BINARY TESTSET_DIR OUT_DIR [config-name ...]

For each case the noisy file is processed by deadnoise_cli (the exact plugin
chain, latency-compensated), with "Learn Background" pressed at 0.5 s while
only noise is present. Metrics:
  pesq      ITU-T P.862.2 wide-band PESQ (16 kHz), noisy vs processed
  stoi      short-time objective intelligibility
  sisdr     scale-invariant SDR over the speech region (dB)
  atten     noise-floor attenuation in the trailing noise-only region (dB)
  mnk       log kurtosis ratio of the residual (musical-noise indicator; ~0 = none)
  spike     99.9th-percentile T-F gain above the median gain in the noise-only
            region (dB): how far isolated noise peaks open (musical noise)
  onset     output vs clean level in the first 30 ms of each speech onset (dB)
  band_*    mean level change of speech-active frames vs the clean reference (dB)
            per band (negative = speech attenuated / dulled)
"""
import json, os, subprocess, sys
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly, stft
from pesq import pesq
from pystoi import stoi

SR = 48000

CONFIGS = {
    'natural':      ['--reduction', '0.8', '--mode', 'natural', '--voice', '0.5'],
    'natural100':   ['--reduction', '1.0', '--mode', 'natural', '--voice', '0.5'],
    'max':          ['--reduction', '1.0', '--mode', 'max', '--voice', '0.5'],
    'max_vp0':      ['--reduction', '1.0', '--mode', 'max', '--voice', '0.0'],
    'natural_vp1':  ['--reduction', '0.8', '--mode', 'natural', '--voice', '1.0'],
    'adaptive':     ['--reduction', '0.8', '--mode', 'natural', '--voice', '0.5', '--no-learn'],
}


def run_cli(cli, inp, out, args):
    learn = '--no-learn' not in args
    a = [x for x in args if x != '--no-learn']
    cmd = [cli, '--in', inp, '--out', out, '--compensate'] + a + (['--learn-at', '0.5'] if learn else [])
    r = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return json.loads(r.stdout.strip().splitlines()[-1])


def rms_db(x):
    return 10 * np.log10(np.mean(x ** 2) + 1e-30)


def musical_noise_kurtosis(ref, out):
    """log kurtosis ratio of whitened power spectra (Uemura et al.)."""
    def kurt(x):
        _, _, Z = stft(x, fs=SR, nperseg=1024, noverlap=768)
        P = np.abs(Z[5:400]) ** 2
        P = P / (P.mean(axis=1, keepdims=True) + 1e-30)
        P = P.ravel()
        m = P.mean()
        return np.mean((P - m) ** 4) / (np.var(P) ** 2 + 1e-30)
    return float(np.log(kurt(out) / kurt(ref)))


def gain_spikes(ref, out):
    """99.9th percentile of the time-frequency gain, relative to its median (dB).
    In a noise-only region a well-behaved suppressor applies an almost constant
    gain; isolated openings (= musical noise / blips) raise this number."""
    _, _, X = stft(ref, fs=SR, nperseg=1024, noverlap=768)
    _, _, Y = stft(out, fs=SR, nperseg=1024, noverlap=768)
    G = 20 * np.log10(np.abs(Y[5:480]) / (np.abs(X[5:480]) + 1e-20) + 1e-20)
    return float(np.percentile(G, 99.9) - np.median(G))


def onset_level(clean, out):
    """Mean level of the output vs the clean reference during the first 30 ms
    after each speech onset (dB, ~0 = onsets intact, negative = clipped attacks)."""
    hop = int(0.005 * SR)
    e = np.array([np.mean(clean[i:i + hop] ** 2) for i in range(0, len(clean) - hop, hop)])
    thr = e.max() * 10 ** (-40 / 10)
    act = e > thr
    vals = []
    i = 20
    while i < len(act) - 6:
        if act[i] and not act[i - 20:i].any():          # >= 100 ms of silence before
            a, b = i * hop, (i + 6) * hop
            pc = np.sum(clean[a:b] ** 2); po = np.sum(out[a:b] ** 2)
            if pc > 0:
                vals.append(10 * np.log10((po + 1e-30) / pc))
            i += 20
        i += 1
    return float(np.mean(vals)) if vals else float('nan')


def band_changes(clean, out):
    f, _, C = stft(clean, fs=SR, nperseg=2048, noverlap=1536)
    _, _, O = stft(out, fs=SR, nperseg=2048, noverlap=1536)
    Pc, Po = np.abs(C) ** 2, np.abs(O) ** 2
    e = Pc.sum(axis=0)
    active = e > e.max() * 10 ** (-35 / 10)
    res = {}
    for name, lo, hi in [('lo', 80, 1000), ('mid', 1000, 4000), ('hi', 4000, 8000), ('air', 8000, 16000)]:
        b = (f >= lo) & (f < hi)
        pc = Pc[b][:, active].sum(axis=0)
        po = Po[b][:, active].sum(axis=0)
        ok = pc > pc.max() * 10 ** (-40 / 10) if pc.max() > 0 else np.zeros_like(pc, bool)
        res['band_' + name] = float(np.mean(10 * np.log10((po[ok] + 1e-30) / (pc[ok] + 1e-30)))) if ok.any() else float('nan')
    return res


def speech_metrics(clean, test, s0, s1):
    i0, i1 = int(max(0, s0 - 0.3) * SR), int(min(len(clean) / SR, s1 + 0.3) * SR)
    c, t = clean[i0:i1], test[i0:i1]
    c16 = resample_poly(c, 1, 3); t16 = resample_poly(t, 1, 3)
    try:
        pq = float(pesq(16000, c16, t16, 'wb'))
    except Exception:
        pq = float('nan')
    st = float(stoi(c, t, SR, extended=False))
    alpha = np.dot(t, c) / (np.dot(c, c) + 1e-30)
    tgt = alpha * c
    sisdr = float(10 * np.log10(np.sum(tgt ** 2) / (np.sum((t - tgt) ** 2) + 1e-30)))
    return pq, st, sisdr


def evaluate(cli, ts, out_dir, configs):
    cases = json.load(open(os.path.join(ts, 'cases.json')))
    os.makedirs(out_dir, exist_ok=True)
    results = []
    for case in cases:
        name = case['name']
        clean, _ = sf.read(os.path.join(ts, name + '_clean.wav'), dtype='float64')
        noisy, _ = sf.read(os.path.join(ts, name + '_noisy.wav'), dtype='float64')
        has_speech = np.max(np.abs(clean)) > 0
        s0, s1 = case['speech_start'], case['speech_end']
        tail0 = int((len(noisy) / SR - 1.5) * SR) if has_speech else int(6.0 * SR)
        base = {'case': name}
        if has_speech and name != 'quiet_consonants':
            pq, st, sd = speech_metrics(clean, noisy, s0, s1)
            base.update({'noisy_pesq': pq, 'noisy_stoi': st, 'noisy_sisdr': sd})
        for cfg in configs:
            out = os.path.join(out_dir, f'{name}__{cfg}.wav')
            info = run_cli(cli, os.path.join(ts, name + '_noisy.wav'), out, CONFIGS[cfg])
            y, _ = sf.read(out, dtype='float64')
            r = dict(base)
            r.update({'config': cfg, 'learn': info['learn'], 'latency': info['latency'], 'rtf': info['rtf'],
                      'nan': bool(not np.all(np.isfinite(y))), 'peak_in': float(np.max(np.abs(noisy))),
                      'peak_out': float(np.max(np.abs(y)))})
            r['atten'] = float(rms_db(y[tail0:]) - rms_db(noisy[tail0:]))
            r['mnk'] = musical_noise_kurtosis(noisy[tail0:], y[tail0:])
            r['spike'] = gain_spikes(noisy[tail0:], y[tail0:])
            if has_speech:
                r.update(band_changes(clean, y))
                r['onset'] = onset_level(clean, y)
                if name != 'quiet_consonants':
                    pq, st, sd = speech_metrics(clean, y, s0, s1)
                    r.update({'pesq': pq, 'stoi': st, 'sisdr': sd})
                else:
                    # consonant survival: energy of the bursts in output vs clean (4-14 kHz)
                    _, _, C = stft(clean, fs=SR, nperseg=1024, noverlap=768)
                    _, _, O = stft(y, fs=SR, nperseg=1024, noverlap=768)
                    e = (np.abs(C) ** 2).sum(axis=0)
                    act = e > e.max() * 10 ** (-10 / 10)
                    r['consonant_level'] = float(10 * np.log10((np.abs(O[:, act]) ** 2).sum() / ((np.abs(C[:, act]) ** 2).sum() + 1e-30)))
            results.append(r)
            print(json.dumps({k: (round(v, 3) if isinstance(v, float) else v) for k, v in r.items()}), flush=True)
    return results


if __name__ == '__main__':
    cli, ts, out_dir = sys.argv[1:4]
    configs = sys.argv[4:] or ['natural', 'max', 'adaptive']
    res = evaluate(cli, ts, out_dir, configs)
    with open(os.path.join(out_dir, 'results.json'), 'w') as f:
        json.dump(res, f, indent=1)

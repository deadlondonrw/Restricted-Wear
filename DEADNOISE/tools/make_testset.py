#!/usr/bin/env python3
"""Generate the DEADNOISE evaluation set (48 kHz).

Every file is: 5 s noise only (the user presses Learn at 0.5 s), then speech,
then 2 s of noise. A clean reference (same timing, no noise) is written too.

Speech sources (not redistributed with the project):
  * LibriSpeech excerpts from librosa/data (CC-BY 4.0, 16 kHz -> 48 kHz)
  * DeepFilterNet assets/clean_freesound_33711.wav (48 kHz full-band, CC BY-NC,
    used only for local testing)
Noise: synthetic "USB microphone hiss" (stationary, rising high-frequency
broadband noise), plain white noise, and a real recorded CC0 noise.
"""
import json, os, sys
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly, butter, sosfilt

SR = 48000
rng = np.random.default_rng(20261107)


def load(path):
    x, sr = sf.read(path, dtype='float64', always_2d=True)
    x = x.mean(axis=1)
    if sr != SR:
        g = np.gcd(int(sr), SR)
        x = resample_poly(x, SR // g, int(sr) // g)
    return x


def active_rms(x, sr=SR):
    """RMS over active frames (ITU-P.56-like, simplified: frames within 30 dB of max)."""
    hop = int(0.02 * sr)
    fr = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2) + 1e-20) for i in range(0, len(x) - hop, hop)])
    thr = fr.max() * 10 ** (-30 / 20)
    a = fr[fr > thr]
    return np.sqrt(np.mean(a ** 2))


def shaped_noise(n, shape='usb'):
    """Stationary noise with a given spectral shape (frequency-domain shaping)."""
    w = rng.standard_normal(n + 4096)
    X = np.fft.rfft(w)
    f = np.fft.rfftfreq(len(w), 1 / SR)
    if shape == 'usb':
        # flat-ish low end, +4 dB/oct rise from 1 kHz to 9 kHz, flat to 18 kHz, gentle roll-off
        g_db = np.where(f < 1000, 0.0, 4.0 * np.log2(np.maximum(f, 1000) / 1000))
        g_db = np.minimum(g_db, 4.0 * np.log2(9.0))
        g_db -= np.where(f > 18000, (f - 18000) / 1000 * 3.0, 0.0)
        g_db += 2.0 * np.exp(-((f - 50) / 30) ** 2)   # a little mains-ish low hum region
    elif shape == 'white':
        g_db = np.zeros_like(f)
    elif shape == 'pink':
        g_db = -3.0 * np.log2(np.maximum(f, 20) / 1000)
    X *= 10 ** (g_db / 20)
    y = np.fft.irfft(X, len(w))[:n]
    return y / np.sqrt(np.mean(y ** 2))


def breath(n_len):
    """Synthetic breath: band-passed noise (300 Hz - 5 kHz) with a smooth envelope."""
    sos = butter(2, [300, 5000], btype='band', fs=SR, output='sos')
    b = sosfilt(sos, rng.standard_normal(n_len))
    env = np.sin(np.linspace(0, np.pi, n_len)) ** 2
    b *= env
    return b / (np.sqrt(np.mean(b ** 2)) + 1e-12)


def fricatives(total_len, level_rms):
    """Quiet unvoiced consonants: 60-140 ms high-band noise bursts (s / sh / f like)."""
    out = np.zeros(total_len)
    t = int(0.3 * SR)
    kinds = [(4000, 11000), (2500, 7000), (1500, 9000), (5000, 14000)]
    while t < total_len - int(0.3 * SR):
        lo, hi = kinds[rng.integers(len(kinds))]
        L = int(rng.uniform(0.06, 0.14) * SR)
        sos = butter(4, [lo, hi], btype='band', fs=SR, output='sos')
        b = sosfilt(sos, rng.standard_normal(L))
        env = np.sin(np.linspace(0, np.pi, L)) ** 1.5
        b = b * env
        b *= level_rms / (np.sqrt(np.mean(b ** 2)) + 1e-12)
        out[t:t + L] += b
        t += L + int(rng.uniform(0.25, 0.5) * SR)
    return out


def assemble(speech, noise_shape='usb', speech_db=-22.0, snr_db=30.0, lead=5.0, tail=2.0,
             extra=None, noise_change=None, peak_norm=None):
    s = speech / active_rms(speech) * 10 ** (speech_db / 20)
    if peak_norm is not None:
        s = s / np.max(np.abs(s)) * 10 ** (peak_norm / 20)
        speech_db = 20 * np.log10(active_rms(s))
    total = int((lead + tail) * SR) + len(s)
    clean = np.zeros(total)
    clean[int(lead * SR):int(lead * SR) + len(s)] = s
    if extra is not None:
        clean += extra[:total] if len(extra) >= total else np.pad(extra, (0, total - len(extra)))
    if noise_shape == 'real':
        nz = load(os.path.join(DF, 'assets', 'noise_freesound_573577.wav'))
        reps = int(np.ceil(total / len(nz)))
        nz = np.tile(nz, reps)[:total]
        nz = nz / np.sqrt(np.mean(nz ** 2))
    else:
        nz = shaped_noise(total, noise_shape)
    noise_db = speech_db - snr_db
    nz *= 10 ** (noise_db / 20)
    if noise_change is not None:
        at, delta_db = noise_change
        i = int(at * SR)
        ramp = np.ones(total)
        ramp[i:] = 10 ** (delta_db / 20)
        nz *= ramp
    noisy = clean + nz
    return clean, noisy, nz, noise_db


if __name__ == '__main__':
    out_dir = sys.argv[1]
    DEPS = sys.argv[2] if len(sys.argv) > 2 else '/home/user/deps'
    DF = os.path.join(DEPS, 'DeepFilterNet')
    LR = os.path.join(DEPS, 'librosa-data', 'audio')
    os.makedirs(out_dir, exist_ok=True)

    libri = [load(os.path.join(LR, f)) for f in ['198-209-0000.hq.ogg', '3436-172162-0000.hq.ogg', '5703-47212-0000.hq.ogg']]
    fullband = load(os.path.join(DF, 'assets', 'clean_freesound_33711.wav'))

    cases = []

    def add(name, clean, noisy, noise, noise_db, desc, speech_start=5.0):
        sf.write(os.path.join(out_dir, name + '_clean.wav'), clean.astype(np.float32), SR, subtype='FLOAT')
        sf.write(os.path.join(out_dir, name + '_noisy.wav'), noisy.astype(np.float32), SR, subtype='FLOAT')
        sf.write(os.path.join(out_dir, name + '_noise.wav'), noise.astype(np.float32), SR, subtype='FLOAT')
        cases.append({'name': name, 'desc': desc, 'noise_db': noise_db, 'speech_start': speech_start,
                      'speech_end': speech_start + (len(clean) / SR - speech_start - 2.0)})

    for i, sp in enumerate(libri):
        for snr in (30, 20, 10):
            c, n, nz, nd = assemble(sp, 'usb', -22, snr)
            add(f'libri{i+1}_usb_snr{snr}', c, n, nz, nd, f'LibriSpeech {i+1}, USB hiss, SNR {snr} dB')
    for snr in (30, 20, 10, 5):
        c, n, nz, nd = assemble(fullband, 'usb', -22, snr)
        add(f'fullband_usb_snr{snr}', c, n, nz, nd, f'Full-band speech, USB hiss, SNR {snr} dB')
    c, n, nz, nd = assemble(fullband, 'white', -22, 20)
    add('fullband_white_snr20', c, n, nz, nd, 'Full-band speech, white noise, SNR 20 dB')
    c, n, nz, nd = assemble(libri[0], 'real', -22, 20)
    add('libri1_realnoise_snr20', c, n, nz, nd, 'LibriSpeech 1, recorded CC0 noise, SNR 20 dB')
    # quiet speech: -40 dBFS active level, hiss 15 dB below
    c, n, nz, nd = assemble(fullband, 'usb', -40, 15)
    add('quiet_speech', c, n, nz, nd, 'Quiet speech (-40 dBFS), USB hiss, SNR 15 dB')
    # loud speech, peaks at -0.3 dBFS
    c, n, nz, nd = assemble(fullband, 'usb', -22, 35, peak_norm=-0.3)
    add('loud_speech', c, n, nz, nd, 'Loud speech (peak -0.3 dBFS), USB hiss, SNR 35 dB')
    # breathing between phrases (and no speech for the first 5 s)
    sp = libri[1]
    total = int(7.0 * SR) + len(sp)
    br = np.zeros(total)
    for t in np.arange(5.0, total / SR - 2.5, 2.2):
        L = int(0.45 * SR)
        i = int(t * SR)
        br[i:i + L] += breath(L) * 10 ** ((-22 - 18) / 20)   # breaths ~18 dB below speech
    c, n, nz, nd = assemble(sp, 'usb', -22, 25, extra=br)
    add('breathing', c, n, nz, nd, 'Speech + breaths (18 dB below speech), USB hiss, SNR 25 dB')
    # quiet consonants alone (worst case for a hiss remover): fricative bursts 12 dB above the hiss
    fr = fricatives(int(10 * SR), 10 ** ((-55 + 12) / 20))
    total = int(5 * SR) + len(fr) + int(2 * SR)
    ext = np.zeros(total); ext[int(5 * SR):int(5 * SR) + len(fr)] = fr
    nz = shaped_noise(total, 'usb') * 10 ** (-55 / 20)
    add('quiet_consonants', ext, ext + nz, nz, -55.0, 'Isolated quiet fricatives (12 dB above hiss), USB hiss -55 dBFS')
    # sudden noise changes (hiss +6 dB at 9 s), and a drop (-6 dB at 9 s)
    c, n, nz, nd = assemble(libri[2], 'usb', -22, 25, noise_change=(9.0, 6.0))
    add('noise_jump_up', c, n, nz, nd, 'Speech, hiss jumps +6 dB at 9 s (profile from before)')
    c, n, nz, nd = assemble(libri[2], 'usb', -22, 25, noise_change=(9.0, -6.0))
    add('noise_jump_down', c, n, nz, nd, 'Speech, hiss drops -6 dB at 9 s')
    # hiss only
    total = int(12 * SR)
    nz = shaped_noise(total, 'usb') * 10 ** (-50 / 20)
    add('hiss_only', np.zeros(total), nz, nz, -50.0, 'Constant USB hiss only, -50 dBFS', speech_start=12.0)
    nz = shaped_noise(total, 'white') * 10 ** (-50 / 20)
    add('white_only', np.zeros(total), nz, nz, -50.0, 'Constant white noise only, -50 dBFS', speech_start=12.0)

    with open(os.path.join(out_dir, 'cases.json'), 'w') as f:
        json.dump(cases, f, indent=1)
    print(f'{len(cases)} cases written to {out_dir}')

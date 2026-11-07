#!/usr/bin/env python3
"""Turn evaluate.py results.json into markdown tables (docs/TEST_REPORT.md)."""
import json, sys
res = json.load(open(sys.argv[1]))
cfgs = []
for r in res:
    if r['config'] not in cfgs: cfgs.append(r['config'])
by = {}
for r in res: by.setdefault(r['case'], {})[r['config']] = r
f = lambda v, d=2: '' if v is None or v != v else f'{v:.{d}f}'
print('### Speech quality (PESQ-WB / STOI)\n')
print('| case | noisy | ' + ' | '.join(cfgs) + ' |')
print('|---|---|' + '---|' * len(cfgs))
for case, d in by.items():
    first = next(iter(d.values()))
    if 'noisy_pesq' not in first: continue
    row = f"| {case} | {f(first['noisy_pesq'])} / {f(first['noisy_stoi'],3)} | "
    row += ' | '.join(f"{f(d[c].get('pesq'))} / {f(d[c].get('stoi'),3)}" for c in cfgs if c in d)
    print(row + ' |')
print('\n### Noise-floor attenuation in pauses (dB) and gain spikes (dB, musical-noise indicator)\n')
print('| case | ' + ' | '.join(cfgs) + ' |')
print('|---|' + '---|' * len(cfgs))
for case, d in by.items():
    print(f"| {case} | " + ' | '.join(f"{f(d[c]['atten'],1)} / {f(d[c]['spike'],1)}" for c in cfgs if c in d) + ' |')
print('\n### Speech preservation: level change of speech-active frames vs clean (dB): 0.08-1 / 1-4 / 4-8 / 8-16 kHz, onset\n')
print('| case | ' + ' | '.join(cfgs) + ' |')
print('|---|' + '---|' * len(cfgs))
for case, d in by.items():
    first = next(iter(d.values()))
    if 'band_lo' not in first: continue
    cells = []
    for c in cfgs:
        if c not in d: continue
        r = d[c]
        cells.append(f"{f(r['band_lo'],1)} / {f(r['band_mid'],1)} / {f(r['band_hi'],1)} / {f(r['band_air'],1)}, on {f(r.get('onset'),1)}")
    print(f"| {case} | " + ' | '.join(cells) + ' |')
peak = max((r['peak_out'] - r['peak_in']) for r in res)
nan = any(r['nan'] for r in res)
learn = {r['learn'] for r in res}
print(f"\nNon-finite samples in any output: {'YES' if nan else 'none'}. "
      f"Largest output-minus-input peak: {peak:+.4f} (linear). Learn results: {', '.join(sorted(learn))}. "
      f"Real-time factor of the CLI runs: {min(r['rtf'] for r in res):.3f} - {max(r['rtf'] for r in res):.3f}.")

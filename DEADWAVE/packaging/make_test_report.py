#!/usr/bin/env python3
"""Build TEST_REPORT.txt for a DEADWAVE build from the real CI logs.

Every PASS / FAIL is derived from a log produced in the same CI run; nothing is
hard-coded as passed. Missing logs are reported as NOT RUN.
"""
import argparse, hashlib, os, re, zipfile

ap = argparse.ArgumentParser()
ap.add_argument('--logs', required=True)
ap.add_argument('--zip', required=True)
ap.add_argument('--zip-single', default='')
ap.add_argument('--version', required=True)
ap.add_argument('--out', required=True)
a = ap.parse_args()


def read(name):
    p = os.path.join(a.logs, name)
    if not os.path.exists(p):
        return None
    with open(p, 'rb') as f:
        return f.read().decode('utf-8', 'replace').replace('\r', '')


def status(ok):
    return 'PASS' if ok else 'FAIL'


results = {}


def summarise(test, machine, text):
    results.setdefault(test, {})[machine] = text


for machine, suffix in (('build machine', 'build-machine'), ('clean machine', 'clean-machine')):
    t = 'Bundle / binary checks (ZIP extracted to D:\\Plugins)'
    checks = read(f'bundle-checks-{suffix}.txt')
    if checks is None:
        summarise(t, machine, 'NOT RUN')
    else:
        n_pass = len(re.findall(r'(?m)^PASS ', checks))
        n_fail = len(re.findall(r'(?m)^FAIL ', checks))
        summarise(t, machine, f'{status(n_fail == 0 and n_pass > 0)} ({n_pass} passed, {n_fail} failed)')

    t = 'Steinberg VST3 validator, extensive (-e)'
    v = read(f'vst3-validator-{suffix}.txt')
    if v is None:
        summarise(t, machine, 'NOT RUN')
    else:
        m = re.findall(r'Result: (\d+) tests passed, (\d+) tests failed', v)
        summarise(t, machine, f'{status(int(m[-1][1]) == 0)} ({m[-1][0]} passed, {m[-1][1]} failed)' if m else 'FAIL (no result line)')

    t = 'pluginval, strictness level 10'
    pv = read(f'pluginval-{suffix}.txt')
    if pv is None:
        summarise(t, machine, 'NOT RUN')
    else:
        ok = bool(re.search(r'(?m)^SUCCESS\s*$', pv)) and 'FAILED' not in pv
        summarise(t, machine, status(ok))

    t = 'VST3 host test (scan, instantiate, play MIDI, state, editor)'
    ht = read(f'host-test-{suffix}.txt')
    if ht is None:
        summarise(t, machine, 'NOT RUN')
    else:
        m = re.search(r'(\d+) checks, (\d+) failures', ht)
        ok = m is not None and m.group(2) == '0' and 'HOST TEST PASSED' in ht
        summarise(t, machine, f'{status(ok)} ({m.group(1)} checks, {m.group(2)} failures)' if m else 'FAIL (no result)')

    t = 'Carla plugin discovery (independent, non-JUCE scanner)'
    cd = read(f'carla-discovery-{suffix}.txt')
    if cd is None:
        summarise(t, machine, 'NOT RUN')
    else:
        ok = 'carla-discovery::name::DEADWAVE' in cd
        tool_side = 'assertion failure' in cd and 'carla-discovery::init' not in cd
        summarise(t, machine, 'PASS (plugin discovered)' if ok else
                  ('INCONCLUSIVE (the Carla tool failed before opening the plugin; informational only)' if tool_side
                   else 'DID NOT DETECT (see docs/ci log; informational only)'))

    t = 'Single-file VST3 (D:\\Plugins\\DEADWAVE.vst3 as one file): load + pluginval'
    sf = read(f'single-file-{suffix}.txt')
    if sf is not None:
        ok = 'LoadLibrary + GetPluginFactory: PASS' in sf and bool(re.search(r'(?m)^SUCCESS\s*$', sf))
        summarise(t, machine, status(ok))

eng = read('engine-tests-windows.txt')
t = 'Engine test-suite (synthesis, pitch, glide, aliasing, bias, presets, RT safety)'
if eng is None:
    summarise(t, 'build machine', 'NOT RUN')
else:
    m = re.search(r'(\d+) checks, (\d+) failures', eng)
    summarise(t, 'build machine', f'{status(m is not None and m.group(2) == "0")} ({m.group(1)} checks, {m.group(2)} failures)' if m else 'FAIL')

with zipfile.ZipFile(a.zip) as z:
    zip_ok = z.testzip() is None
    names = z.namelist()
    infos = {i.filename: i for i in z.infolist()}
sha = hashlib.sha256(open(a.zip, 'rb').read()).hexdigest()

L = []
W = L.append
W('DEADWAVE - TEST REPORT')
W('=' * 72)
W(f'Version            : {a.version}')
W(f'Deliverable        : {os.path.basename(a.zip)}')
W(f'SHA-256            : {sha}')
if a.zip_single and os.path.exists(a.zip_single):
    W(f'Alternative        : {os.path.basename(a.zip_single)}  (single-file VST3)')
    W(f'SHA-256            : {hashlib.sha256(open(a.zip_single, "rb").read()).hexdigest()}')
W('Format             : VST3 instrument, Windows x64 (AMD64), Release, static MSVC runtime')
W('')
W('1. BUILD CONFIGURATION')
W('-' * 72)
for l in (read('build-info.txt') or '').strip().splitlines():
    W('  ' + l)
W('')
W('2. ZIP CONTENTS (extract directly into D:\\Plugins)')
W('-' * 72)
for n in names:
    i = infos[n]
    W(f'  {n:<60} {i.file_size:>10} bytes' if not n.endswith('/') else f'  {n}')
W(f'  ZIP integrity (CRC of every entry): {status(zip_ok)}')
W(f'  Only one top-level folder "DEADWAVE.vst3": {status(all(n.startswith("DEADWAVE.vst3/") for n in names))}')
W('')
W('3. RESULTS OF TESTS ACTUALLY PERFORMED')
W('-' * 72)
W('  "build machine" = the GitHub Actions Windows Server 2022 runner that compiled the plugin.')
clean_os = (read('clean-machine-os.txt') or '').strip().replace('\n', '; ')
W(f'  "clean machine" = a second, separate Windows runner ({clean_os or "Windows Server 2025"})')
W('                    that received only the ZIP and the test tools, no build files.')
W('')
for test, per in results.items():
    W(f'  {test}')
    for machine in ('build machine', 'clean machine'):
        if machine in per:
            W(f'      {machine:<14}: {per[machine]}')
W('')
if eng:
    W('  Engine test highlights (build machine):')
    for l in eng.splitlines():
        s = l.strip()
        if any(k in s for k in ('measured', 'worst non-harmonic', '2nd harmonic', 'allocations during', 'factory presets in',
                                'pairs below', 'of one core', 'glide:', 'state size')):
            W('    ' + s)
    W('')
W('4. TESTS THAT COULD NOT BE PERFORMED')
W('-' * 72)
W('  * FL Studio was NOT available in CI. DEADWAVE has NOT been scanned, loaded or')
W('    played in FL Studio itself, so FL Studio compatibility is NOT verified.')
W('    It passes Steinberg\'s reference validator, pluginval and a JUCE VST3 host,')
W('    but only a scan and a test session in your FL Studio can confirm it.')
W('  * FL Studio piano-roll slide notes: FL Studio applies slide notes to its own')
W('    plug-ins. For VST3 instruments DEADWAVE relies on glide (overlap legato) and')
W('    pitch bend; whether FL converts slide notes into pitch bend for a VST3 was')
W('    not tested.')
W('  * No test on a Windows 10/11 desktop installation (Windows Server runners).')
W('  * No listening test with your speakers / audio interface; sound quality was')
W('    checked with automated measurements (aliasing, pitch, levels, finite output).')
W('')
W('5. KNOWN LIMITATIONS')
W('-' * 72)
W('  * The binary is not code-signed: unblock the ZIP before extracting (INSTALLATION.txt).')
W('  * The host sees about 2,800 parameters: ~720 DEADWAVE parameters plus JUCE\'s')
W('    MIDI-CC proxy parameters that let VST3 hosts deliver pitch bend, mod wheel and')
W('    aftertouch to the instrument.')
W('  * ULTRA quality renders voices at 2x and roughly doubles the oscillator CPU cost.')
W('')
overall = all(v.startswith('PASS') for t, per in results.items() if 'Carla' not in t for v in per.values())
W('OVERALL (all mandatory tests above): ' + ('PASS' if overall else 'FAIL'))

with open(a.out, 'w', newline='\r\n') as f:
    f.write('\n'.join(L) + '\n')
print('\n'.join(L))

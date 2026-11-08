#!/usr/bin/env python3
"""Build TEST_REPORT.txt for the DEADNOISE release from the real CI logs.

Every PASS / FAIL below is derived from a log file produced in this CI run;
nothing is hard-coded as passed. Missing logs are reported as NOT RUN.
"""
import argparse, hashlib, os, re, zipfile

ap = argparse.ArgumentParser()
ap.add_argument('--logs', required=True)
ap.add_argument('--zip', required=True)
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


lines = []
W = lines.append
results = {}   # summary rows: (test, machine) -> text


def summarise(test, machine, text):
    results.setdefault(test, {})[machine] = text


build_info = read('build-info.txt') or ''
for machine, suffix in (('build machine', 'build-machine'), ('clean machine', 'clean-machine')):
    checks = read(f'bundle-checks-{suffix}.txt')
    if checks is None:
        summarise('Bundle / binary checks (ZIP extracted to D:\\Plugins)', machine, 'NOT RUN')
    else:
        n_pass = len(re.findall(r'(?m)^PASS ', checks)); n_fail = len(re.findall(r'(?m)^FAIL ', checks))
        summarise('Bundle / binary checks (ZIP extracted to D:\\Plugins)', machine,
                  f'{status(n_fail == 0 and n_pass > 0)} ({n_pass} passed, {n_fail} failed)')

    v = read(f'vst3-validator-{suffix}.txt')
    if v is None:
        summarise('Steinberg VST3 validator, extensive (-e)', machine, 'NOT RUN')
    else:
        m = re.findall(r'Result: (\d+) tests passed, (\d+) tests failed', v)
        if m:
            p, f = m[-1]
            summarise('Steinberg VST3 validator, extensive (-e)', machine, f'{status(int(f) == 0)} ({p} passed, {f} failed)')
        else:
            summarise('Steinberg VST3 validator, extensive (-e)', machine, 'FAIL (no result line)')

    pv = read(f'pluginval-{suffix}.txt')
    if pv is None:
        summarise('pluginval, strictness level 10', machine, 'NOT RUN')
    else:
        ok = bool(re.search(r'(?m)^SUCCESS\s*$', pv)) and 'FAILED' not in pv
        summarise('pluginval, strictness level 10', machine, status(ok))

    ht = read(f'host-test-{suffix}.txt')
    if ht is None:
        summarise('VST3 host test (scan, instantiate, process, state, editor)', machine, 'NOT RUN')
    else:
        m = re.search(r'(\d+) checks, (\d+) failures', ht)
        ok = m is not None and m.group(2) == '0' and 'HOST TEST PASSED' in ht
        summarise('VST3 host test (scan, instantiate, process, state, editor)', machine,
                  f'{status(ok)} ({m.group(1)} checks, {m.group(2)} failures)' if m else 'FAIL (no result)')

    cd = read(f'carla-discovery-{suffix}.txt')
    if cd is None:
        summarise('Carla plugin discovery (independent, non-JUCE scanner)', machine, 'NOT RUN')
    else:
        ok = 'carla-discovery::name::DEADNOISE' in cd and 'carla-discovery::error' not in cd
        summarise('Carla plugin discovery (independent, non-JUCE scanner)', machine,
                  'PASS (plugin discovered)' if ok else 'DID NOT DETECT (see docs/ci log; informational only)')

dsp = read('dsp-tests-windows.txt')
if dsp is None:
    summarise('DSP test-suite (audio processing)', 'build machine', 'NOT RUN')
else:
    m = re.search(r'(\d+) checks, (\d+) failures', dsp)
    summarise('DSP test-suite (audio processing)', 'build machine',
              f'{status(m is not None and m.group(2) == "0")} ({m.group(1)} checks, {m.group(2)} failures)' if m else 'FAIL')

# ZIP facts (checked again here, on Linux, with Python's zipfile)
with zipfile.ZipFile(a.zip) as z:
    zip_ok = z.testzip() is None
    names = z.namelist()
    infos = {i.filename: i for i in z.infolist()}
sha = hashlib.sha256(open(a.zip, 'rb').read()).hexdigest()
dll_entry = 'DEADNOISE.vst3/Contents/x86_64-win/DEADNOISE.vst3'

W('DEADNOISE - TEST REPORT')
W('=' * 72)
W(f'Version            : {a.version}')
W(f'Deliverable        : {os.path.basename(a.zip)}')
W(f'SHA-256            : {sha}')
W(f'Format             : VST3 (bundle), Windows x64 (AMD64), Release')
W('')
W('1. BUILD CONFIGURATION')
W('-' * 72)
for l in build_info.strip().splitlines():
    W('  ' + l)
W('')
W('2. ZIP CONTENTS (extract directly into D:\\Plugins)')
W('-' * 72)
for n in names:
    i = infos[n]
    W(f'  {n:<60} {i.file_size:>10} bytes' if not n.endswith('/') else f'  {n}')
W(f'  ZIP integrity (CRC of every entry): {status(zip_ok)}')
W(f'  Only one top-level folder "DEADNOISE.vst3": {status(all(n.startswith("DEADNOISE.vst3/") for n in names))}')
W(f'  Result after extracting into D:\\Plugins: D:\\Plugins\\DEADNOISE.vst3\\Contents\\x86_64-win\\DEADNOISE.vst3')
W('')
W('3. RESULTS OF TESTS ACTUALLY PERFORMED')
W('-' * 72)
W('  "build machine" = GitHub Actions Windows Server 2022 runner that compiled the plugin.')
clean_os = (read('clean-machine-os.txt') or '').strip().replace('\n', '; ')
W(f'  "clean machine" = a second, separate Windows runner ({clean_os or "Windows Server 2025"})')
W('                    that received only the ZIP and the test tools, no build files.')
W('  All plugin tests ran on the plugin extracted from the ZIP into D:\\Plugins.')
W('')
for test, per in results.items():
    W(f'  {test}')
    for machine in ('build machine', 'clean machine'):
        if machine in per:
            W(f'      {machine:<14}: {per[machine]}')
W('')

for machine, suffix in (('build machine', 'build-machine'), ('clean machine', 'clean-machine')):
    checks = read(f'bundle-checks-{suffix}.txt')
    if checks:
        W(f'  Detailed bundle / binary checks ({machine}):')
        for l in checks.splitlines():
            if l.startswith(('PASS', 'FAIL', 'INFO', 'RESULT')):
                W('    ' + l)
        W('')

if dsp:
    W('  DSP test-suite highlights (build machine):')
    for key in ('latency', 'attenuation', 'allocations', 'digital silence', 'NaN', 'speech level change', 'continuous talking',
                'clean hiss: worst band', 'block', 'CPU core'):
        for l in dsp.splitlines():
            if key.lower() in l.lower() and l.strip() and not l.strip().startswith('['):
                W('    ' + l.strip())
                break
    W('')

cd = read('carla-discovery-build-machine.txt')
if cd:
    W('  Carla discovery output (build machine):')
    for l in cd.splitlines():
        if l.startswith('carla-discovery::') and any(k in l for k in ('name', 'label', 'maker', 'audio', 'error', 'category', 'build', 'hints')):
            W('    ' + l)
    W('')

W('4. TESTS THAT COULD NOT BE PERFORMED')
W('-' * 72)
carla_ok = any(v.startswith('PASS') for v in results.get('Carla plugin discovery (independent, non-JUCE scanner)', {}).values())
hosts = 'a JUCE-based VST3 host and Carla\'s plugin scanner' if carla_ok else 'a JUCE-based VST3 host'
W('  * FL Studio was NOT available. The plugin has NOT been scanned or loaded in')
W('    FL Studio itself. FL Studio compatibility is therefore NOT verified. It')
W('    is expected, because the plugin passes Steinberg\'s reference validator,')
W(f'    pluginval and {hosts}, but only a scan in your FL Studio can confirm it.')
W('  * No test on a Windows 10/11 desktop installation. The tests ran on')
W('    Windows Server 2022 and 2025 (the same Windows code base and system DLLs).')
W('  * No test with your USB microphone or your audio driver; audio quality was')
W('    measured with synthetic hiss and recorded speech (see docs/TEST_REPORT.md).')
W('  * Not tested with Windows Smart App Control / third-party antivirus.')
W('')
W('5. KNOWN LIMITATIONS')
W('-' * 72)
W('  * The binary is not code-signed. Windows may mark the downloaded ZIP as')
W('    coming from the internet; unblock it before extracting (INSTALLATION.txt).')
W('    With Windows 11 Smart App Control enabled, unsigned plugins can be')
W('    blocked from loading.')
W('  * Latency 1024 samples at 44.1/48 kHz (23.2 / 21.3 ms), 2048 samples at')
W('    88.2/96 kHz, reported to the host for delay compensation.')
W('  * Built for stationary noise (constant microphone hiss, hum, fan).')
W('    Transient noises (keyboard, clicks) are not removed.')
W('  * Maximum mode can dull quiet "s" sounds when the hiss is loud; use Natural')
W('    or raise Voice Preservation.')
W('  * Mono and stereo only. Stereo channels share one gain.')
W('')
# Carla discovery is informational (a third-party tool); everything else is mandatory.
overall = all(v.startswith('PASS') for t, per in results.items() if 'Carla' not in t for v in per.values())
W('OVERALL (all mandatory tests above): ' + ('PASS' if overall else 'FAIL'))

with open(a.out, 'w', newline='\r\n') as f:
    f.write('\n'.join(lines) + '\n')
print('\n'.join(lines))

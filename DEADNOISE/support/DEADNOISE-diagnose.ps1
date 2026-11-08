# DEADNOISE - diagnostics for "FL Studio does not find DEADNOISE".
# READ-ONLY: this script changes nothing on your computer.
#
# Run it (PowerShell, no admin needed):
#   powershell -ExecutionPolicy Bypass -File "%USERPROFILE%\Downloads\DEADNOISE-diagnose.ps1"
# It prints a report and saves it as DEADNOISE-diagnose.txt on your Desktop.
param([string] $PluginRoot = 'D:\Plugins')

$expectedDllSha = 'DACAB882F85EE14865A1B91CC47EA1A163034D98981B9A6D5B02A627AF8090FB'
$out = New-Object System.Collections.Generic.List[string]
function Say([string] $s) { $out.Add($s); Write-Host $s }

Say '=== DEADNOISE diagnostics ==='
Say ("Date: {0}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm'))
try { $os = Get-CimInstance Win32_OperatingSystem; Say ("Windows: {0} {1} build {2}, {3}" -f $os.Caption, $os.Version, $os.BuildNumber, $os.OSArchitecture) } catch { Say 'Windows: (unknown)' }
Say ("PowerShell 64-bit process: {0}" -f [Environment]::Is64BitProcess)
Say ("CPU: {0}" -f $env:PROCESSOR_ARCHITECTURE)
Say ''

# ---------------------------------------------------------------- 1. where is it?
Say ("--- 1. Searching for DEADNOISE under {0}" -f $PluginRoot)
if (-not (Test-Path $PluginRoot)) { Say "  $PluginRoot does NOT exist" }
$found = @()
if (Test-Path $PluginRoot) {
    $found = @(Get-ChildItem -Path $PluginRoot -Recurse -Force -ErrorAction SilentlyContinue -Filter '*DEADNOISE*')
}
foreach ($extra in "$env:CommonProgramFiles\VST3", "$env:USERPROFILE\Downloads") {
    if (Test-Path $extra) { $found += @(Get-ChildItem -Path $extra -Recurse -Force -ErrorAction SilentlyContinue -Filter '*DEADNOISE*' | Where-Object { $_.FullName -notlike "$PluginRoot*" }) }
}
if ($found.Count -eq 0) { Say '  nothing named DEADNOISE found' }
foreach ($f in $found) {
    $kind = 'file'; if ($f.PSIsContainer) { $kind = 'FOLDER' }
    $size = ''; if (-not $f.PSIsContainer) { $size = ("{0} bytes" -f $f.Length) }
    Say ("  {0,-6} {1}  {2}" -f $kind, $f.FullName, $size)
}
Say ''

$bundle = Join-Path $PluginRoot 'DEADNOISE.vst3'
$dll = Join-Path $bundle 'Contents\x86_64-win\DEADNOISE.vst3'
Say '--- 2. Expected layout'
Say ("  {0}  bundle folder {1}" -f $(if (Test-Path $bundle -PathType Container) { 'OK  ' } else { 'MISSING' }), $bundle)
Say ("  {0}  binary        {1}" -f $(if (Test-Path $dll -PathType Leaf) { 'OK  ' } else { 'MISSING' }), $dll)
$mi = Join-Path $bundle 'Contents\Resources\moduleinfo.json'
Say ("  {0}  moduleinfo    {1}" -f $(if (Test-Path $mi -PathType Leaf) { 'OK  ' } else { 'MISSING' }), $mi)

# if not at the expected place, test the first binary we can find
if (-not (Test-Path $dll -PathType Leaf)) {
    $cand = $found | Where-Object { -not $_.PSIsContainer -and $_.Name -eq 'DEADNOISE.vst3' -and $_.Length -gt 1000000 } | Select-Object -First 1
    if ($cand) { $dll = $cand.FullName; Say "  -> testing the binary found at $dll instead" }
}
Say ''

if (Test-Path $dll -PathType Leaf) {
    # ------------------------------------------------------------ 3. file identity / blocking
    Say '--- 3. Binary'
    $h = (Get-FileHash -Algorithm SHA256 $dll).Hash
    Say ("  SHA-256: {0}  {1}" -f $h, $(if ($h -eq $expectedDllSha) { '(matches the released 1.0.1 build)' } else { '(DIFFERENT from the released 1.0.1 build)' }))
    $zone = Get-Item -Path $dll -Stream Zone.Identifier -ErrorAction SilentlyContinue
    if ($zone) { Say '  Marked as downloaded from the internet (Zone.Identifier present) -> file is BLOCKED-marked' }
    else       { Say '  Not marked as downloaded from the internet (unblocked)' }
    $sig = Get-AuthenticodeSignature $dll
    Say ("  Code signature: {0}" -f $sig.Status)

    # ------------------------------------------------------------ 4. can Windows load it?
    Say ''
    Say '--- 4. Loading the plugin DLL (what FL Studio does when scanning)'
    if (-not [Environment]::Is64BitProcess) {
        Say '  SKIPPED: this PowerShell is 32-bit; run "Windows PowerShell" (not x86) to test a 64-bit plugin.'
    } else {
        Add-Type -Namespace DnDiag -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr module, string name);
'@
        $handle = [DnDiag.Native]::LoadLibraryExW($dll, [IntPtr]::Zero, 0)
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if ($handle -ne [IntPtr]::Zero) {
            $fac = [DnDiag.Native]::GetProcAddress($handle, 'GetPluginFactory')
            Say ("  LoadLibrary: OK   GetPluginFactory: {0}" -f $(if ($fac -ne [IntPtr]::Zero) { 'found' } else { 'MISSING' }))
        } else {
            $msg = (New-Object ComponentModel.Win32Exception $err).Message
            Say ("  LoadLibrary: FAILED, Windows error {0}: {1}" -f $err, $msg)
            switch ($err) {
                4551 { Say '  -> Blocked by Windows code integrity policy (Smart App Control / application control).' }
                225  { Say '  -> Blocked by antivirus (Windows Defender or another product).' }
                5    { Say '  -> Access denied (antivirus or file permissions).' }
                126  { Say '  -> A required Windows component was not found.' }
                193  { Say '  -> Not a valid 64-bit Windows DLL (damaged file).' }
            }
        }
    }
}
Say ''

# ---------------------------------------------------------------- 5. Smart App Control / Defender
Say '--- 5. Windows protection'
try {
    $sac = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' -Name VerifiedAndReputablePolicyState -ErrorAction Stop).VerifiedAndReputablePolicyState
    $sacText = switch ($sac) { 0 { 'Off' } 1 { 'ON (enforcing)' } 2 { 'Evaluation' } default { "value $sac" } }
    Say "  Smart App Control: $sacText"
} catch { Say '  Smart App Control: not present (Windows 10 or not configured)' }
try {
    $det = @(Get-MpThreatDetection -ErrorAction Stop | Where-Object { ($_.Resources -join ' ') -match 'DEADNOISE' })
    if ($det.Count) { Say ("  Windows Defender detections mentioning DEADNOISE: {0}" -f $det.Count) } else { Say '  Windows Defender: no detections mentioning DEADNOISE' }
} catch { Say '  Windows Defender history: not readable without admin rights (not an error)' }
Say ''

# ---------------------------------------------------------------- 6. FL Studio's view
Say '--- 6. FL Studio plugin database'
$docs = [Environment]::GetFolderPath('MyDocuments')
$db = Join-Path $docs 'Image-Line\FL Studio\Presets\Plugin database'
if (Test-Path $db) {
    $hits = @(Get-ChildItem -Path $db -Recurse -Force -ErrorAction SilentlyContinue -Filter '*DEADNOISE*')
    if ($hits.Count) { foreach ($x in $hits) { Say ("  found: {0}" -f $x.FullName) } } else { Say "  no DEADNOISE entry in $db" }
} else { Say "  database folder not found at $db" }
try {
    $paths = @()
    Get-ChildItem 'HKCU:\Software\Image-Line' -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $key = $_
        foreach ($n in $key.GetValueNames()) {
            $v = [string] $key.GetValue($n)
            if ($v -match '(?i)[a-z]:\\[^;]*plugin') { $paths += ("  {0} : {1} = {2}" -f $key.Name.Replace('HKEY_CURRENT_USER\', ''), $n, $v) }
        }
    }
    if ($paths.Count) { Say '  FL Studio settings mentioning plugin folders:'; $paths | Select-Object -First 20 | ForEach-Object { Say $_ } }
} catch { }

$file = Join-Path ([Environment]::GetFolderPath('Desktop')) 'DEADNOISE-diagnose.txt'
$out | Set-Content -Path $file -Encoding UTF8
Write-Host ''
Write-Host "Saved to $file - please send this file (or its text)."

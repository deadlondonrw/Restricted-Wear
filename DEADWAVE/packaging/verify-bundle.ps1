# DEADWAVE - verify the delivered ZIP exactly as the user will use it.
#   pwsh verify-bundle.ps1 -Zip DEADWAVE-FL-STUDIO-WINDOWS-x64.zip -Log checks.txt
# Extracts into D:\Plugins (the user's plugin folder), then checks the ZIP
# layout, the bundle, the PE binary, DLL loading, exports, moduleinfo.json and
# (when Visual Studio's dumpbin is available) the DLL dependencies.
# Exit code = number of failed checks.
param(
    [Parameter(Mandatory = $true)] [string] $Zip,
    [string] $Dest = 'D:\Plugins',
    [Parameter(Mandatory = $true)] [string] $Log,
    [string] $ExpectedVersion = ''
)
$ErrorActionPreference = 'Stop'
$script:failures = 0
Set-Content -Path $Log -Value ("DEADWAVE bundle checks on {0} ({1})" -f $env:COMPUTERNAME, [Environment]::OSVersion.VersionString)

function Check([string] $Name, [bool] $Ok, [string] $Detail = '') {
    if ($Ok) { $state = 'PASS' } else { $state = 'FAIL'; $script:failures++ }
    $line = "{0}  {1}" -f $state, $Name
    if ($Detail) { $line += "  [" + $Detail + "]" }
    Write-Output $line
    Add-Content -Path $Log -Value $line
}

# ---------------------------------------------------------------- ZIP layout
Add-Type -AssemblyName System.IO.Compression.FileSystem
$za = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $Zip))
$entries = @($za.Entries | ForEach-Object { [pscustomobject]@{ Name = $_.FullName; Length = $_.Length } })
$za.Dispose()
$names = @($entries | ForEach-Object { $_.Name })
$outside = @($names | Where-Object { -not $_.StartsWith('DEADWAVE.vst3/') })
Check 'zip: single top-level folder DEADWAVE.vst3 (no extra nesting)' ($outside.Count -eq 0) ("entries: " + ($names -join ', '))
Check 'zip: forward-slash entry paths' (@($names | Where-Object { $_.Contains('\') }).Count -eq 0)
foreach ($req in 'DEADWAVE.vst3/Contents/x86_64-win/DEADWAVE.vst3', 'DEADWAVE.vst3/Contents/Resources/moduleinfo.json') {
    Check "zip: contains $req" ($names -contains $req)
}
$zipDllLen = ($entries | Where-Object { $_.Name -eq 'DEADWAVE.vst3/Contents/x86_64-win/DEADWAVE.vst3' }).Length

# ---------------------------------------------------------------- extract (two independent extractors)
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
$target = Join-Path $Dest 'DEADWAVE.vst3'
if (Test-Path $target) { Remove-Item -Recurse -Force $target }
Expand-Archive -Path $Zip -DestinationPath $Dest -Force
$dll = Join-Path $target 'Contents\x86_64-win\DEADWAVE.vst3'
$mi  = Join-Path $target 'Contents\Resources\moduleinfo.json'
Check "extract (Expand-Archive) into $Dest -> bundle folder $target" (Test-Path $target -PathType Container)
Check 'bundle: Contents\x86_64-win\DEADWAVE.vst3 is a file' (Test-Path $dll -PathType Leaf)
Check 'bundle: Contents\Resources\moduleinfo.json is a file' (Test-Path $mi -PathType Leaf)
Check 'bundle: binary size matches the ZIP entry' ((Get-Item $dll).Length -eq $zipDllLen) ("{0} bytes" -f (Get-Item $dll).Length)

$tarDir = Join-Path ([IO.Path]::GetTempPath()) ('dw-tar-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tarDir | Out-Null
& tar.exe -xf $Zip -C $tarDir
Check 'extract with Windows tar.exe (second extractor)' (($LASTEXITCODE -eq 0) -and (Test-Path (Join-Path $tarDir 'DEADWAVE.vst3\Contents\x86_64-win\DEADWAVE.vst3')))

# ---------------------------------------------------------------- PE header
$b = [IO.File]::ReadAllBytes($dll)
$pe = [BitConverter]::ToInt32($b, 0x3C)
$machine = [BitConverter]::ToUInt16($b, $pe + 4)
$chars = [BitConverter]::ToUInt16($b, $pe + 22)
$magic = [BitConverter]::ToUInt16($b, $pe + 24)
Check 'PE: MZ header' (($b[0] -eq 0x4D) -and ($b[1] -eq 0x5A))
Check 'PE: PE signature' ([BitConverter]::ToUInt32($b, $pe) -eq 0x00004550)
Check 'PE: machine = x64 (AMD64, 0x8664)' ($machine -eq 0x8664) ('0x{0:X4}' -f $machine)
Check 'PE: optional header = PE32+ (64-bit)' ($magic -eq 0x20B) ('0x{0:X3}' -f $magic)
Check 'PE: image is a DLL' (($chars -band 0x2000) -ne 0) ('characteristics 0x{0:X4}' -f $chars)
Check 'PE: not an empty/stub binary (> 1 MB)' ($b.Length -gt 1MB) ("{0} bytes" -f $b.Length)

# ---------------------------------------------------------------- load the DLL like a host does
Add-Type -Namespace DwW32 -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
[DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr module, string name);
'@
$h = [DwW32.Native]::LoadLibraryExW($dll, [IntPtr]::Zero, 0)
$err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
Check 'LoadLibrary succeeds (every DLL dependency resolves on this machine)' ($h -ne [IntPtr]::Zero) ("win32 error $err")
if ($h -ne [IntPtr]::Zero) {
    foreach ($fn in 'GetPluginFactory', 'InitDll', 'ExitDll') {
        Check "export $fn" ([DwW32.Native]::GetProcAddress($h, $fn) -ne [IntPtr]::Zero)
    }
}

# ---------------------------------------------------------------- moduleinfo.json (Steinberg JSON5: trailing commas)
$raw = Get-Content -Raw $mi
$json = [regex]::Replace($raw, ',(\s*[}\]])', '$1') | ConvertFrom-Json
$classes = @($json.Classes)
$proc = $classes | Where-Object { $_.Category -eq 'Audio Module Class' }
$ctrl = $classes | Where-Object { $_.Category -eq 'Component Controller Class' }
Check 'moduleinfo: parses' ($null -ne $json)
Check 'moduleinfo: module name DEADWAVE' ($json.Name -eq 'DEADWAVE') $json.Name
if ($ExpectedVersion) { Check "moduleinfo: version $ExpectedVersion" ($json.Version -eq $ExpectedVersion) $json.Version }
Check 'moduleinfo: audio processor class registered' ($null -ne $proc) $proc.CID
Check 'moduleinfo: edit controller class registered' ($null -ne $ctrl) $ctrl.CID
Check 'moduleinfo: processor and controller have distinct 128-bit IDs' (($proc.CID -match '^[0-9A-F]{32}$') -and ($ctrl.CID -match '^[0-9A-F]{32}$') -and ($proc.CID -ne $ctrl.CID))
Check 'moduleinfo: instrument category (Instrument)' (@($proc.'Sub Categories') -contains 'Instrument') (@($proc.'Sub Categories') -join '|')

# ---------------------------------------------------------------- dumpbin: dependencies / release build
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$dumpbin = $null
if (Test-Path $vswhere) { $dumpbin = & $vswhere -latest -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe' | Select-Object -First 1 }
if ($dumpbin) {
    $hdr = & $dumpbin /nologo /headers $dll | Out-String
    $deps = & $dumpbin /nologo /dependents $dll | Out-String
    Add-Content -Path $Log -Value "`n--- dumpbin /dependents ---`n$deps"
    Check 'dumpbin: machine (x64)' ($hdr -match '8664 machine \(x64\)')
    Check 'dumpbin: DLL' ($hdr -match '\bDLL\b')
    $dllNames = @([regex]::Matches($deps, '(?m)^\s+(\S+\.dll)\s*$') | ForEach-Object { $_.Groups[1].Value.ToLowerInvariant() })
    $system = 'advapi32.dll','comctl32.dll','comdlg32.dll','gdi32.dll','imm32.dll','kernel32.dll','oleaut32.dll','shell32.dll',
              'shlwapi.dll','user32.dll','wininet.dll','winmm.dll','ws2_32.dll','api-ms-win-shcore-scaling-l1-1-1.dll','d2d1.dll',
              'd3d11.dll','dcomp.dll','dwmapi.dll','dxgi.dll','ole32.dll','version.dll','crypt32.dll','dwrite.dll','uxtheme.dll',
              'windowscodecs.dll','shcore.dll','setupapi.dll','iphlpapi.dll','msimg32.dll','opengl32.dll','ntdll.dll'
    $nonSystem = @($dllNames | Where-Object { $system -notcontains $_ })
    Check 'dumpbin: only Windows system DLLs imported (no VC++ runtime / no extra DLLs needed)' ($nonSystem.Count -eq 0) ("imports: " + ($dllNames -join ', ') + $(if ($nonSystem.Count) { '; NOT SYSTEM: ' + ($nonSystem -join ', ') } else { '' }))
    Check 'dumpbin: no debug CRT (Release build)' (-not ($deps -match '(?i)ucrtbased|vcruntime140d|msvcp140d'))
} else {
    Add-Content -Path $Log -Value 'INFO  dumpbin not available on this machine (dependency check covered by LoadLibrary)'
}

Add-Content -Path $Log -Value ("`nRESULT: {0} failed check(s)" -f $script:failures)
Write-Output ("RESULT: {0} failed check(s)" -f $script:failures)
exit $script:failures

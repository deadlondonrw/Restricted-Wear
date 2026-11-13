@echo off
setlocal
rem ---------------------------------------------------------------------------
rem DEADNOISE - make FL Studio see the plugin while it stays on D:\Plugins.
rem
rem Creates a directory junction (a 0-byte pointer, no files copied to C:)
rem   C:\Program Files\Common Files\VST3\DEADNOISE.vst3  ->  D:\Plugins\DEADNOISE.vst3
rem Windows asks for administrator permission once (needed to write into
rem Program Files). To undo: run UNLINK-DEADNOISE.bat (your files on D: stay).
rem ---------------------------------------------------------------------------

rem re-launch with administrator rights if needed
net session >nul 2>&1
if errorlevel 1 (
    echo Asking Windows for administrator permission...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set "SRC=D:\Plugins\DEADNOISE.vst3"
set "VST3DIR=%ProgramW6432%\Common Files\VST3"
if "%ProgramW6432%"=="" set "VST3DIR=%ProgramFiles%\Common Files\VST3"
set "DST=%VST3DIR%\DEADNOISE.vst3"

echo Plugin on D:      %SRC%
echo Link in VST3 dir: %DST%
echo.

if not exist "%SRC%\Contents\x86_64-win\DEADNOISE.vst3" (
    echo ERROR: DEADNOISE was not found at %SRC%
    echo Extract DEADNOISE-FL-STUDIO-WINDOWS-x64.zip into D:\Plugins first, so that
    echo %SRC%\Contents\x86_64-win\DEADNOISE.vst3 exists.
    goto :done
)

if not exist "%VST3DIR%" mkdir "%VST3DIR%"

if exist "%DST%" (
    echo ERROR: %DST% already exists.
    echo If it is an old copy of DEADNOISE, delete it, then run this script again.
    goto :done
)

mklink /J "%DST%" "%SRC%"
if errorlevel 1 (
    echo ERROR: could not create the link.
    goto :done
)

if exist "%DST%\Contents\x86_64-win\DEADNOISE.vst3" (
    echo.
    echo DONE. FL Studio: Options - Manage plugins - Find installed plugins.
) else (
    echo ERROR: the link was created but the plugin is not reachable through it.
)

:done
echo.
if not defined DN_NOPAUSE pause
endlocal

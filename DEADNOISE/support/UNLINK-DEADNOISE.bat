@echo off
setlocal
rem Removes ONLY the link C:\Program Files\Common Files\VST3\DEADNOISE.vst3.
rem The plugin files in D:\Plugins\DEADNOISE.vst3 are not touched.
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
set "VST3DIR=%ProgramW6432%\Common Files\VST3"
if "%ProgramW6432%"=="" set "VST3DIR=%ProgramFiles%\Common Files\VST3"
set "DST=%VST3DIR%\DEADNOISE.vst3"

fsutil reparsepoint query "%DST%" >nul 2>&1
if errorlevel 1 (
    echo %DST% is not a link - nothing removed.
) else (
    rmdir "%DST%"
    echo Link removed. Your files in D:\Plugins are unchanged.
)
echo.
if not defined DN_NOPAUSE pause
endlocal

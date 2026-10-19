@echo off
REM Builds and installs Canopy Pads on Windows: VST3 (Ableton, Studio One, Cubase, Reaper, FL Studio...) and the standalone app.
REM Right-click this file and choose "Run as administrator" (needed to copy the VST3 into Program Files).
REM First run takes 10-20 minutes.
setlocal
cd /d "%~dp0"

echo ============================================================
echo   Canopy Pads - Windows build
echo ============================================================

REM 1. Visual Studio 2022 Build Tools (free). Installs the C++ compiler + CMake if missing.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if "%VSPATH%"=="" (
    echo Installing Visual Studio 2022 Build Tools with the C++ workload. Accept the prompts, wait for it to finish,
    echo then run this file again.
    powershell -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile '%TEMP%\vs_BuildTools.exe'"
    "%TEMP%\vs_BuildTools.exe" --passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.CMake.Project
    echo Build Tools installed. Please run this file again.
    pause
    exit /b 0
)
echo Using Visual Studio at %VSPATH%
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
where cmake >nul 2>nul || set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where cmake >nul 2>nul || ( echo CMake not found. Install it from https://cmake.org/download/ and run again. & pause & exit /b 1 )

REM 2. JUCE framework, downloaded once into .\JUCE
if not exist "JUCE\CMakeLists.txt" (
    echo Downloading JUCE 8.0.4 ^(about 100 MB^)...
    if exist JUCE rmdir /s /q JUCE
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip' -OutFile '%TEMP%\juce.zip'"
    powershell -Command "Expand-Archive -Force '%TEMP%\juce.zip' '%TEMP%\juce-unzip'"
    move "%TEMP%\juce-unzip\JUCE-8.0.4" JUCE >nul
    rmdir /s /q "%TEMP%\juce-unzip"
    del "%TEMP%\juce.zip"
)

REM 3. Build (Release, 64-bit)
echo Building... this is the slow part.
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 ( echo CMake configuration failed. & pause & exit /b 1 )
cmake --build build --config Release --target CanopyPads_VST3 CanopyPads_Standalone
if errorlevel 1 ( echo Build failed - scroll up for the error. & pause & exit /b 1 )

REM 4. Install. The build already copied the VST3 to "C:\Program Files\Common Files\VST3" (if run as administrator).
set "APPDIR=%LOCALAPPDATA%\Programs\Canopy Pads"
mkdir "%APPDIR%" 2>nul
copy /y "build\CanopyPads_artefacts\Release\Standalone\Canopy Pads.exe" "%APPDIR%\" >nul
if not exist "%ProgramFiles%\Common Files\VST3\Canopy Pads.vst3" (
    echo Copying VST3 to Program Files...
    xcopy /e /i /y "build\CanopyPads_artefacts\Release\VST3\Canopy Pads.vst3" "%ProgramFiles%\Common Files\VST3\Canopy Pads.vst3" >nul || echo Could not copy the VST3 - run this file as administrator, or copy build\CanopyPads_artefacts\Release\VST3\Canopy Pads.vst3 to "C:\Program Files\Common Files\VST3" yourself.
)
powershell -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('Desktop')+'\Canopy Pads.lnk'); $s.TargetPath='%APPDIR%\Canopy Pads.exe'; $s.Save()"

echo.
echo ============================================================
echo   Done!
echo   App:   %APPDIR%\Canopy Pads.exe   (shortcut on your Desktop)
echo   VST3:  C:\Program Files\Common Files\VST3\Canopy Pads.vst3
echo.
echo   In your DAW, rescan plugins; Canopy Pads appears under Tagbor Audio.
echo ============================================================
pause

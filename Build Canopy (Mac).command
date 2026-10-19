#!/bin/bash
# Builds and installs Canopy Pads on macOS: AU (for MainStage/Logic/GarageBand), VST3, and the standalone app.
# Double-click this file in Finder. First run takes 10-20 minutes; later runs are faster.
set -e
cd "$(dirname "$0")"

echo "============================================================"
echo "  Canopy Pads - Mac build"
echo "============================================================"

# 1. Apple's command-line compiler tools (free). This pops up an installer if missing.
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Installing Apple Command Line Tools... accept the dialog, let it finish, then run this file again."
    xcode-select --install || true
    exit 0
fi

# 2. CMake. Use Homebrew if present, otherwise download the official CMake app.
if ! command -v cmake >/dev/null 2>&1; then
    if [ -x /Applications/CMake.app/Contents/bin/cmake ]; then
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    elif command -v brew >/dev/null 2>&1; then
        echo "Installing CMake with Homebrew..."
        brew install cmake
    else
        echo "Downloading CMake..."
        curl -L -o /tmp/cmake.dmg "https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-macos-universal.dmg"
        hdiutil attach /tmp/cmake.dmg -nobrowse -mountpoint /tmp/cmake-mount
        cp -R /tmp/cmake-mount/CMake.app /Applications/
        hdiutil detach /tmp/cmake-mount
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    fi
fi
echo "Using $(cmake --version | head -1)"

# 3. JUCE framework (the audio toolkit this is built on). Downloaded once into ./JUCE.
if [ ! -f JUCE/CMakeLists.txt ]; then
    echo "Downloading JUCE 8.0.4 (about 100 MB)..."
    rm -rf JUCE
    curl -L -o /tmp/juce.zip "https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip"
    unzip -q /tmp/juce.zip -d /tmp/juce-unzip
    mv /tmp/juce-unzip/JUCE-8.0.4 JUCE
    rm -rf /tmp/juce-unzip /tmp/juce.zip
fi

# 4. Build. Universal binary (Apple Silicon + Intel), Release.
echo "Building... (this is the slow part)"
cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --config Release --target CanopyPads_AU CanopyPads_VST3 CanopyPads_Standalone -j "$(sysctl -n hw.ncpu)" 2>&1 | grep -E "error|Built target|Linking" || true
[ -d "build/CanopyPads_artefacts/Release/Standalone/Canopy Pads.app" ] || { echo "Build failed - scroll up for the error."; read -n 1 -s -r -p "Press any key to close."; exit 1; }

# 5. Install. The build already copied the AU and VST3 into ~/Library/Audio/Plug-Ins. Put the app in Applications too.
APP="build/CanopyPads_artefacts/Release/Standalone/Canopy Pads.app"
mkdir -p ~/Applications
rm -rf ~/Applications/"Canopy Pads.app"
cp -R "$APP" ~/Applications/
# Make macOS forget the "unidentified developer" quarantine on something you built yourself.
xattr -dr com.apple.quarantine ~/Applications/"Canopy Pads.app" 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Canopy Pads.component" 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Canopy Pads.vst3" 2>/dev/null || true
# Ask macOS to rescan Audio Units so MainStage/Logic see it immediately.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "============================================================"
echo "  Done!"
echo "  App:   ~/Applications/Canopy Pads.app"
echo "  AU:    ~/Library/Audio/Plug-Ins/Components/Canopy Pads.component   (MainStage / Logic)"
echo "  VST3:  ~/Library/Audio/Plug-Ins/VST3/Canopy Pads.vst3              (Ableton etc.)"
echo ""
echo "  In MainStage: add a Software Instrument channel strip, then"
echo "  Instrument > AU Instruments > Tagbor Audio > Canopy Pads."
echo "  If it doesn't appear, quit MainStage, log out and back in once."
echo "============================================================"
read -n 1 -s -r -p "Press any key to close."
echo

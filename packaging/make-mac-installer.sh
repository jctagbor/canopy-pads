#!/bin/bash
# Builds "Canopy Pads <version>.dmg" containing a one-click .pkg installer that puts:
#   Canopy Pads.app        -> /Applications
#   Canopy Pads.component  -> /Library/Audio/Plug-Ins/Components   (AU: MainStage, Logic, GarageBand)
#   Canopy Pads.vst3       -> /Library/Audio/Plug-Ins/VST3         (Ableton, Studio One, Reaper...)
# Run from the repo root after a Release build. Usage: packaging/make-mac-installer.sh 1.0.0
set -euo pipefail
VERSION="${1:-1.0.0}"
ART="build/CanopyPads_artefacts/Release"
[ -d "$ART/Standalone/Canopy Pads.app" ] || { echo "Build first (no $ART/Standalone/Canopy Pads.app)"; exit 1; }

rm -rf dist pkgroot pkgs
mkdir -p dist pkgs pkgroot/Applications "pkgroot/Library/Audio/Plug-Ins/Components" "pkgroot/Library/Audio/Plug-Ins/VST3"
cp -R "$ART/Standalone/Canopy Pads.app" pkgroot/Applications/
cp -R "$ART/AU/Canopy Pads.component" "pkgroot/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/Canopy Pads.vst3" "pkgroot/Library/Audio/Plug-Ins/VST3/"

# Ad-hoc sign everything so the binaries are at least internally consistent (no Developer ID needed).
for b in "pkgroot/Applications/Canopy Pads.app" "pkgroot/Library/Audio/Plug-Ins/Components/Canopy Pads.component" "pkgroot/Library/Audio/Plug-Ins/VST3/Canopy Pads.vst3"; do
    codesign --force --deep --sign - "$b"
done

pkgbuild --root pkgroot --identifier com.tagboraudio.canopypads --version "$VERSION" --install-location / pkgs/canopy.pkg

cat > pkgs/distribution.xml <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Canopy Pads $VERSION</title>
    <welcome file="welcome.html"/>
    <conclusion file="conclusion.html"/>
    <options customize="never" require-scripts="false" rootVolumeOnly="true"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <choices-outline><line choice="default"><line choice="canopy"/></line></choices-outline>
    <choice id="default"/>
    <choice id="canopy" visible="false"><pkg-ref id="com.tagboraudio.canopypads"/></choice>
    <pkg-ref id="com.tagboraudio.canopypads" version="$VERSION" onConclusion="none">canopy.pkg</pkg-ref>
</installer-gui-script>
EOF
mkdir -p pkgs/res
cat > pkgs/res/welcome.html <<'EOF'
<html><body style="font-family:-apple-system;font-size:13px">
<h2>Canopy Pads</h2>
<p>Ambient drone pads for church musicians. This installs:</p>
<ul>
<li><b>Canopy Pads.app</b> in Applications (standalone)</li>
<li>the <b>Audio Unit</b> for MainStage, Logic and GarageBand</li>
<li>the <b>VST3</b> for Ableton Live, Studio One, Reaper and others</li>
</ul>
<p>After installing, restart your music software. In MainStage add a Software Instrument and choose <b>AU Instruments &gt; Tagbor Audio &gt; Canopy Pads</b>.</p>
</body></html>
EOF
cat > pkgs/res/conclusion.html <<'EOF'
<html><body style="font-family:-apple-system;font-size:13px">
<h2>You're set.</h2>
<p>Open <b>Canopy Pads</b> from Applications, or load it inside MainStage / Logic / Ableton.</p>
<p><b>First launch:</b> if macOS says it can't verify the developer, right-click the app and choose <b>Open</b>, then <b>Open</b> again. You only do this once.</p>
<p>Quick start: play a note in the bottom octave of your keyboard (C1-B1) - the pad starts and stays. Play another to change key. Set your own drone zone in the MIDI &amp; Keys tab.</p>
</body></html>
EOF
productbuild --distribution pkgs/distribution.xml --resources pkgs/res --package-path pkgs "dist/Canopy Pads Installer.pkg"

# DMG: the installer plus a short read-me.
rm -rf dmgroot; mkdir dmgroot
cp "dist/Canopy Pads Installer.pkg" dmgroot/
cat > "dmgroot/Read me first.txt" <<'EOF'
CANOPY PADS - install

1. Double-click "Canopy Pads Installer.pkg".
   If macOS blocks it ("cannot be opened because it is from an unidentified developer"):
   right-click the .pkg > Open > Open. This is a free, unsigned community build; that prompt appears once.
2. Click through the installer. It puts the app in Applications and the AU + VST3 plugins where MainStage, Logic and Ableton look for them.
3. Restart your music software.

MainStage: Software Instrument strip > Instrument slot > AU Instruments > Tagbor Audio > Canopy Pads.
If it doesn't show up, log out and back in once (macOS caches the plugin list).

Full guide + videos: https://github.com/jctagbor/canopy-pads
EOF
hdiutil create -volname "Canopy Pads $VERSION" -srcfolder dmgroot -ov -format UDZO "dist/Canopy-Pads-$VERSION-mac.dmg"
rm -f "dist/Canopy Pads Installer.pkg"
rm -rf dmgroot pkgroot pkgs
ls -la dist

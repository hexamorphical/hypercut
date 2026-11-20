#!/usr/bin/env bash
# Builds a macOS .pkg (all formats) and a .dmg with the standalone.
# Usage: scripts/package-macos.sh <version>
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:?usage: $0 <version>}"
PKG_VERSION="$(bash scripts/pkg-version.sh "$VERSION")"

[ -d bin/HyperCut.vst3 ] && [ -d bin/HyperCut.app ] || {
    echo "error: build the plugins first (make macos-universal-10.15)" >&2
    exit 1
}

# Give the standalone bundle an icon, then refresh its ad-hoc signature.
if [ -f packaging/hypercut.icns ]; then
    mkdir -p bin/HyperCut.app/Contents/Resources
    cp packaging/hypercut.icns bin/HyperCut.app/Contents/Resources/hypercut.icns
    /usr/libexec/PlistBuddy -c 'Delete :CFBundleIconFile' bin/HyperCut.app/Contents/Info.plist 2>/dev/null || true
    /usr/libexec/PlistBuddy -c 'Add :CFBundleIconFile string hypercut.icns' bin/HyperCut.app/Contents/Info.plist
    codesign --force --deep --sign - bin/HyperCut.app
fi

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
stage="$root/stage"
mkdir -p "$stage"

# component <stage name> <source in bin/> <install location> <identifier>
component() {
    local name="$1" src="$2" loc="$3" id="$4"
    cp -R "$src" "$stage/$name"
    pkgbuild --root "$stage/$name" --install-location "$loc" \
             --identifier "$id" --version "$PKG_VERSION" "$root/$name.pkg"
}

component vst3 bin/HyperCut.vst3 /Library/Audio/Plug-Ins/VST3 com.hypercut.plugin.vst3
if [ -e bin/HyperCut.clap ]; then
    mkdir -p "$stage/clap"
    cp -R bin/HyperCut.clap "$stage/clap/HyperCut.clap"
    pkgbuild --root "$stage/clap" --install-location /Library/Audio/Plug-Ins/CLAP \
             --identifier com.hypercut.plugin.clap --version "$PKG_VERSION" "$root/clap.pkg"
fi
if [ -d bin/HyperCut.lv2 ]; then component lv2 bin/HyperCut.lv2 /Library/Audio/Plug-Ins/LV2 com.hypercut.plugin.lv2; fi
if [ -d bin/HyperCut.vst ]; then component vst bin/HyperCut.vst /Library/Audio/Plug-Ins/VST com.hypercut.plugin.vst; fi
component app bin/HyperCut.app /Applications com.hypercut.standalone

pkgs=()
for p in vst3 clap lv2 vst app; do
    if [ -f "$root/$p.pkg" ]; then pkgs+=("$root/$p.pkg"); fi
done
args=()
for p in "${pkgs[@]}"; do args+=(--package "$p"); done

mkdir -p dist
outpkg="dist/HyperCut-$VERSION-macos-universal.pkg"
rm -f "$outpkg"
productbuild "${args[@]}" --title "HyperCut $VERSION" "$outpkg"
ls -l "$outpkg"

# The dmg is a drag-to-Applications standalone, with a short install note.
dmgstage="$root/dmg"
mkdir -p "$dmgstage"
cp -R bin/HyperCut.app "$dmgstage/HyperCut.app"
cat > "$dmgstage/Install plugins.txt" <<EOF
HyperCut $VERSION

Drag HyperCut.app to Applications to use the standalone.

The .pkg installer from the same release installs the VST3, CLAP, LV2 and VST2
plugins into /Library/Audio/Plug-Ins/ and the standalone into /Applications.
The plugins are ad-hoc signed but not notarized: after installing, clear the
quarantine once in Terminal, e.g.

  xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/HyperCut.vst3
EOF
ln -s /Applications "$dmgstage/Applications"
outdmg="dist/HyperCut-$VERSION-macos-universal.dmg"
rm -f "$outdmg"
hdiutil create -volname "HyperCut $VERSION" -srcfolder "$dmgstage" -ov -format UDZO "$outdmg"
ls -l "$outdmg"

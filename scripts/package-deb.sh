#!/usr/bin/env bash
# Builds a Debian/Ubuntu .deb that installs every HyperCut format system-wide.
# Usage: scripts/package-deb.sh <version> [architecture]     (e.g. v0.2.2 amd64)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:?usage: $0 <version> [architecture]}"
ARCH="${2:-amd64}"
PKG_VERSION="$(bash scripts/pkg-version.sh "$VERSION")"

[ -d bin/HyperCut.vst3 ] && [ -f bin/HyperCut.clap ] && [ -d bin/HyperCut.lv2 ] && [ -f bin/HyperCut ] || {
    echo "error: build the plugins first (make)" >&2
    exit 1
}

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
mkdir -p "$root/DEBIAN" \
         "$root/usr/lib/vst3/HyperCut.vst3" \
         "$root/usr/lib/clap" \
         "$root/usr/lib/lv2/HyperCut.lv2" \
         "$root/usr/lib/vst" \
         "$root/usr/bin" \
         "$root/usr/share/applications" \
         "$root/usr/share/icons/hicolor/256x256/apps"

cp -R bin/HyperCut.vst3/. "$root/usr/lib/vst3/HyperCut.vst3/"
cp bin/HyperCut.clap "$root/usr/lib/clap/HyperCut.clap"
cp -R bin/HyperCut.lv2/. "$root/usr/lib/lv2/HyperCut.lv2/"
if [ -f bin/HyperCut-vst.so ]; then
    cp bin/HyperCut-vst.so "$root/usr/lib/vst/HyperCut-vst.so"
fi
cp bin/HyperCut "$root/usr/bin/HyperCut"
cp packaging/hypercut.desktop "$root/usr/share/applications/hypercut.desktop"
cp packaging/hypercut.png "$root/usr/share/icons/hicolor/256x256/apps/hypercut.png"

installed_kb="$(du -sk "$root/usr" | cut -f1)"
cat > "$root/DEBIAN/control" <<EOF
Package: hypercut
Version: $PKG_VERSION
Section: sound
Priority: optional
Architecture: $ARCH
Installed-Size: $installed_kb
Depends: libc6 (>= 2.35), libstdc++6, libgl1, libx11-6, libxext6, libxcursor1, libxrandr2
Maintainer: eventual-recluse <noreply@github.com>
Homepage: https://github.com/hexamorphical/hypercut
Description: HyperCut - beat slicer plugin and standalone
 HyperCut slices loops and beats in real time, with filter, crush and delay
 effects and live visualisation. Installs the VST3, CLAP, LV2 and VST2
 plugins plus the JACK standalone application.
EOF

mkdir -p dist
out="dist/HyperCut-$VERSION-linux-x86_64.deb"
rm -f "$out"
dpkg-deb --build --root-owner-group "$root" "$out"
ls -l "$out"

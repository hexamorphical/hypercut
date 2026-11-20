#!/usr/bin/env bash
# Builds a portable AppImage with the JACK standalone. Plugin formats stay in the zip / .deb.
# Usage: scripts/package-appimage.sh <version>       (APPIMAGETOOL_VERSION overrides the tool tag)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:?usage: $0 <version>}"
TOOL_VERSION="${APPIMAGETOOL_VERSION:-v13}"

[ -f bin/HyperCut ] || { echo "error: build the standalone first (make)" >&2; exit 1; }

root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
appdir="$root/HyperCut.AppDir"
mkdir -p "$appdir/usr/bin" \
         "$appdir/usr/share/applications" \
         "$appdir/usr/share/icons/hicolor/256x256/apps"

cp bin/HyperCut "$appdir/usr/bin/HyperCut"
# AppImages resolve their desktop entry via AppRun, while the .deb entry launches the binary directly.
sed 's/^Exec=.*/Exec=AppRun/' packaging/hypercut.desktop > "$appdir/usr/share/applications/hypercut.desktop"
cp packaging/hypercut.png "$appdir/usr/share/icons/hicolor/256x256/apps/hypercut.png"
cp packaging/hypercut.png "$appdir/hypercut.png"
ln -s hypercut.png "$appdir/.DirIcon"

cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/HyperCut" "$@"
EOF
chmod +x "$appdir/AppRun"

tool="$root/appimagetool.AppImage"
curl -fL --retry 3 -o "$tool" "https://github.com/AppImage/AppImageKit/releases/download/$TOOL_VERSION/appimagetool-x86_64.AppImage"
chmod +x "$tool"

mkdir -p dist
out="$(pwd)/dist/HyperCut-$VERSION-linux-x86_64.AppImage"
rm -f "$out"
# --appimage-extract-and-run avoids needing FUSE on the build runner
ARCH=x86_64 "$tool" --appimage-extract-and-run "$appdir" "$out"
chmod +x "$out"
ls -l "$out"

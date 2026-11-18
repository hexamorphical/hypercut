#!/usr/bin/env bash
# Packages the build in bin/ into dist/HyperCut-<platform>.zip with every format, inside a folder that names the version.
# The zip name has no version, so https://github.com/hexamorphical/hypercut/releases/latest/download/HyperCut-<platform>.zip
# always points at the newest release.
# Works for the Linux, Windows (MSYS2) and macOS builds; the file names differ per platform.
# Usage: scripts/package.sh <version> <platform>     e.g. scripts/package.sh v0.1-beta windows-x64
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:?usage: $0 <version> <platform>}"
PLATFORM="${2:?usage: $0 <version> <platform>}"
base="HyperCut-$VERSION-$PLATFORM"   # folder inside the zip
zipname="HyperCut-$PLATFORM.zip"

# zip, or bsdtar where zip is not installed
mkzip() { local dst="$1"; shift; if command -v zip >/dev/null; then zip -qry "$dst" "$@"; else bsdtar --format zip -cf "$dst" "$@"; fi; }
# the first of the given names that exists in bin/
first() { local f; for f in "$@"; do if [ -e "bin/$f" ]; then echo "$f"; return 0; fi; done; return 0; }

mkdir -p dist
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/$base"
for fmt in vst3 clap lv2 vst2 standalone; do
    case $fmt in
        vst3)       f=$(first HyperCut.vst3) ;;
        clap)       f=$(first HyperCut.clap) ;;
        lv2)        f=$(first HyperCut.lv2) ;;
        vst2)       f=$(first HyperCut.vst HyperCut-vst.dll HyperCut-vst.so) ;;
        standalone) f=$(first HyperCut.app HyperCut.exe HyperCut) ;;
    esac
    if [ -z "$f" ]; then echo "error: no $fmt build in bin/" >&2; exit 1; fi
    cp -R "bin/$f" "$stage/$base/"
done
rm -f "dist/$zipname"
out="$(pwd)/dist/$zipname"
(cd "$stage" && mkzip "$out" "$base")
ls -l dist

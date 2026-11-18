#!/usr/bin/env bash
# Builds the Linux x86_64 release binaries so they also run on older distributions (glibc 2.35+), then packages
# every format into dist/. Usage: scripts/build-linux-release.sh <version>   e.g. scripts/build-linux-release.sh v0.1-beta
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:?usage: $0 <version>}"
COMPAT="$(pwd)/scripts/glibc_compat.h"

make clean >/dev/null
CFLAGS="-include $COMPAT" CXXFLAGS="-include $COMPAT" make -j"$(nproc)"

# refuse to package binaries that need a newer glibc than 2.35
max=$(objdump -T bin/HyperCut.clap bin/HyperCut bin/HyperCut-vst.so bin/HyperCut.lv2/HyperCut.so \
        bin/HyperCut.vst3/Contents/x86_64-linux/HyperCut.so | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
echo "highest glibc symbol: $max"
if [ "$(printf '%s\n' "$max" GLIBC_2.35 | sort -V | tail -1)" != GLIBC_2.35 ]; then
    echo "error: binaries need $max" >&2
    exit 1
fi

# zip, or bsdtar where zip is not installed
mkzip() { local dst="$1"; shift; if command -v zip >/dev/null; then zip -qr "$dst" "$@"; else bsdtar --format zip -cf "$dst" "$@"; fi; }

out="dist"
base="HyperCut-$VERSION-linux-x86_64"
rm -rf "$out" && mkdir -p "$out"
(cd bin && mkzip "../$out/$base-vst3.zip" HyperCut.vst3)
(cd bin && mkzip "../$out/$base-clap.zip" HyperCut.clap)
(cd bin && mkzip "../$out/$base-lv2.zip" HyperCut.lv2)
(cd bin && mkzip "../$out/$base-vst2.zip" HyperCut-vst.so)
(cd bin && mkzip "../$out/$base-standalone.zip" HyperCut)
(cd bin && mkzip "../$out/$base-all.zip" HyperCut.vst3 HyperCut.clap HyperCut.lv2 HyperCut-vst.so HyperCut)
(cd "$out" && sha256sum ./*.zip > SHA256SUMS.txt)
ls -la "$out"

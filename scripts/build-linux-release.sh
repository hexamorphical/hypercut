#!/usr/bin/env bash
# Builds the Linux x86_64 release binaries so they also run on older distributions (glibc 2.35+), then packages
# every format into dist/ (the GitHub Actions workflow builds all platforms the same way). Usage: scripts/build-linux-release.sh <version>   e.g. scripts/build-linux-release.sh v0.1-beta
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

rm -rf dist
scripts/package.sh "$VERSION" linux-x86_64
(cd dist && sha256sum ./*.zip > SHA256SUMS.txt)

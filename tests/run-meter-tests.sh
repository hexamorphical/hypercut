#!/usr/bin/env bash
set -euo pipefail
source_dir="${1:?Usage: bash tests/run_tests.sh /path/to/patched/hypercut}"
test_dir="$(cd "$(dirname "$0")" && pwd)"
probe_dir="$(mktemp -d "${TMPDIR:-/tmp}/hypercut-meter-tests.XXXXXX")"
trap 'rm -rf "$probe_dir"' EXIT
"${CXX:-c++}" -std=c++14 -O2 -Wno-deprecated-declarations -DDISTRHO_PLUGIN_TARGET_VST3 \
 -I"$source_dir/plugins/LiveCut" -I"$source_dir/plugins/LiveCut/lib" \
 -I"$source_dir/dpf/distrho" -I"$source_dir/dpf/distrho/src" \
 "$test_dir/meter_regression.cpp" "$source_dir/plugins/LiveCut/LiveCut.cpp" \
 "$source_dir/plugins/LiveCut/lib/BBCutter.cpp" "$source_dir/plugins/LiveCut/lib/BitCrusher.cpp" \
 "$source_dir/plugins/LiveCut/lib/Comb.cpp" "$source_dir/plugins/LiveCut/lib/DelayLine.cpp" \
 "$source_dir/plugins/LiveCut/lib/FirstOrderLowpass.cpp" "$source_dir/dpf/distrho/src/DistrhoPlugin.cpp" \
 -o "$probe_dir/meter-regression"
"$probe_dir/meter-regression"

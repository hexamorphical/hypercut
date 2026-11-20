#!/usr/bin/env bash
# Prints a plain numeric version suitable for .exe / .deb / .pkg installers.
# Release tags such as v0.2.2-delay-route become 0.2.2; dev builds become 0.0.0.
# Usage: scripts/pkg-version.sh [version]   (falls back to $VERSION)
set -euo pipefail
v="${1:-${VERSION:-}}"
v="${v#v}"
case "$v" in
    ''|dev-*) v='0.0.0' ;;
esac
if [[ "$v" =~ ^([0-9]+(\.[0-9]+){0,2}) ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
else
    printf '%s\n' '0.0.0'
fi

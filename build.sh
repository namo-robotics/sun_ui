#!/usr/bin/env bash
# Build and test the core, optionally including the GTK adapter and acceptance scenarios.
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# -gt 1 || ( $# -eq 1 && $1 != --gtk ) ]]; then
  echo 'Usage: ./build.sh [--gtk]' >&2
  exit 1
fi
./scripts/bootstrap-sun.sh
"${SUN_EXECUTABLE:-sun}" tools/build.sun -- build
"${SUN_EXECUTABLE:-sun}" tools/build.sun -- test | tee build/test-results.log
if [[ ${1:-} == --gtk ]]; then
  "${SUN_EXECUTABLE:-sun}" tools/build.sun -- gtk
  "${SUN_EXECUTABLE:-sun}" tools/build.sun -- test-gtk | tee build/gtk-test-results.log
fi

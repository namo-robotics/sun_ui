#!/usr/bin/env bash
# Build and test the core, optionally including the GTK adapter and acceptance scenarios.
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# -gt 1 || ( $# -eq 1 && $1 != --gtk && $1 != --plot-gtk && $1 != --graphics ) ]]; then
  echo 'Usage: ./build.sh [--gtk|--plot-gtk|--graphics]' >&2
  exit 1
fi
./scripts/bootstrap-sun.sh
build_root=$("${SUN_EXECUTABLE:-sun}" describe --config sun-config.json --field build_root)
"${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- build
"${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- test | tee "$build_root/test-results.log"
if [[ ${1:-} != --gtk ]]; then
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- markup | tee "$build_root/markup-test-results.log"
fi
if [[ ${1:-} == --plot-gtk ]]; then
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- plot-gtk
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- test-plot-gtk | tee "$build_root/plot-gtk-test-results.log"
fi
if [[ ${1:-} == --gtk ]]; then
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- gtk
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- test-gtk | tee "$build_root/gtk-test-results.log"
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- markup-gtk | tee "$build_root/markup-test-results.log"
fi

if [[ ${1:-} == --graphics ]]; then
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- graphics
  "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- test-graphics | tee "$build_root/graphics-test-results.log"
fi

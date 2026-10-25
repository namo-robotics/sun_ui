#!/usr/bin/env bash
# Build and test native Apple Silicon UI bundles against the checked GTK dependency.
set -euo pipefail
cd "$(dirname "$0")/.."
[[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] || { echo 'Requires Apple Silicon macOS' >&2; exit 1; }
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
export PATH="$(brew --prefix coreutils)/libexec/gnubin:$PATH"
export SUN_GOBJECT_BUILD="${SUN_GOBJECT_BUILD:-$PWD/src/sun_gobject/build}"
compiler=${SUN_EXECUTABLE:-sun}
if [[ ${1:-} == --bundles ]]; then
  mkdir -p build
  "$compiler" --dynamic --emit-moon -o build/sun_ui.moon src/ui.sun
  "$compiler" --dynamic --lib-path build --lib-path "$SUN_GOBJECT_BUILD" --emit-moon -o build/sun_ui_gtk.moon src/adapters/gtk/renderer.sun
  "$compiler" --dynamic --lib-path build -c -o build/sun-viewc src/tools/viewc.sun
  exit 0
fi
./build.sh --gtk
mkdir -p build/macos/lib/sun build/macos/bin build/macos/share/sun_ui
cp build/sun_ui.moon build/sun_plot.moon build/sun_ui_gtk.moon build/sun_ui_gtk_drawing.moon build/macos/lib/sun/
cp build/sun-viewc build/macos/bin/
cp LICENSE build/sun-version.txt build/macos/share/sun_ui/
tar -C build/macos -czf build/sun_ui-arm64-apple-darwin.tar.gz .

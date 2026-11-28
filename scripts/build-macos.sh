#!/usr/bin/env bash
# Build and test native Apple Silicon UI bundles against the checked GTK dependency.
set -euo pipefail
cd "$(dirname "$0")/.."
[[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] || { echo 'Requires Apple Silicon macOS' >&2; exit 1; }
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
export PATH="$(brew --prefix coreutils)/libexec/gnubin:$PATH"
compiler=${SUN_EXECUTABLE:-sun}
build_root=$("$compiler" describe --config sun-config.json --field build_root)
if [[ ${1:-} == --bundles ]]; then
  mkdir -p "$build_root"
  "$compiler" --dynamic --emit-moon -o "$build_root/sun_ui.moon" src/ui.sun
  "$compiler" --dynamic --lib-path "$build_root" --emit-moon -o "$build_root/sun_ui_gtk.moon" src/adapters/gtk/renderer.sun
  "$compiler" --dynamic --lib-path "$build_root" -c -o "$build_root/sun_viewc" src/tools/viewc.sun
  exit 0
fi
./build.sh --gtk
mkdir -p "$build_root/macos/lib/sun" "$build_root/macos/bin" "$build_root/macos/share/sun_ui"
cp "$build_root/sun_ui.moon" "$build_root/sun_plot.moon" "$build_root/sun_ui_gtk.moon" "$build_root/sun_ui_gtk_drawing.moon" "$build_root/macos/lib/sun/"
cp "$build_root/sun_viewc" "$build_root/macos/bin/"
cp LICENSE "$build_root/sun-version.txt" "$build_root/macos/share/sun_ui/"
tar -C "$build_root/macos" -czf "$build_root/sun_ui-arm64-apple-darwin.tar.gz" .

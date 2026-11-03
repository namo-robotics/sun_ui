#!/usr/bin/env bash
# Collect compiler-generated packages and platform-qualified downloads after tests.
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${SUN_EXECUTABLE:-sun}
export SUN_DEPENDENCY_CACHE="${SUN_DEPENDENCY_CACHE:-$PWD/build/dependencies/cache}"
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64) platform=x86_64-linux-gnu ;;
  Darwin-arm64) platform=arm64-apple-darwin ;;
  *) echo 'Release assets require Linux x86_64 or macOS arm64.' >&2; exit 1 ;;
esac
"$compiler" build --config sun-config.json --no-test
build_root=$("$compiler" describe --config sun-config.json --field build_root)
package="$build_root/packages/sun_ui"
mkdir -p build/release-assets
for moon in "$package"/lib/*.moon; do
  install -m 644 "$moon" "build/release-assets/$(basename "${moon%.moon}")-$platform.moon"
done
install -m 755 "$package/bin/sun_viewc" "build/release-assets/sun_viewc-$platform"
install -m 644 "$package.tar.gz" "build/release-assets/sun_ui-$platform.sun.tar.gz"

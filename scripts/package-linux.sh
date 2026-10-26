#!/usr/bin/env bash
# Package the tested Linux UI bundles and XML compiler for release.
set -euo pipefail
cd "$(dirname "$0")/.."
"${SUN_EXECUTABLE:-sun}" src/tools/package.sun

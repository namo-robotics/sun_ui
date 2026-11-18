#!/usr/bin/env bash
# Record the selected tool paths before compiling the configured markup tests.
set -euo pipefail
: "${SUN_EXECUTABLE:?Run through sun build}"
: "${SUN_BUILD:?Missing build directory}"
env -u SUN_BUILD_CONTEXT "$SUN_EXECUTABLE" src/tools/build.sun -- configure-markup-tests "$SUN_BUILD"
"$SUN_EXECUTABLE" build-compile

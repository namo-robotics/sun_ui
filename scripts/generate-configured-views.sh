#!/usr/bin/env bash
# Generate XML fixtures and examples before compiling their configured entrypoints.
set -euo pipefail
compiler=$1
output=$2
for name in counter tasks forms; do
  example=$name
  if [[ $name != forms ]]; then example=$name-xml; fi
  "$compiler" "examples/$example/$name.view.xml" "$output/generated/examples/$example/$name.view.sun"
done
for name in scoped actions; do
  "$compiler" "tests/markup/$name.view.xml" "$output/generated/tests/markup/$name.view.sun"
done
"$compiler" tests/markup/reusable.view.xml "$output/generated/tests/markup/reusable.view.sun" \
  --registry tests/markup/reusable.registry.xml

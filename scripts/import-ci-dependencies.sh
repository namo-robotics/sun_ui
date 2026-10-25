#!/usr/bin/env bash
# Import only the toolkit's GTK dependency from the shared development manifest.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -c 'import yaml; data = yaml.safe_load(open("deps.repos")); print(yaml.safe_dump({"repositories": {"src/sun_gobject": data["repositories"]["src/sun_gobject"]}}))' | vcs import .

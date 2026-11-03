#!/usr/bin/env python3
"""Package existing native GTK build outputs for Sun's dependency resolver."""

import hashlib
import json
import pathlib
import platform
import shutil
import sys


def stage(build):
    """Stage the GTK bundles and native archive with a validated file inventory."""
    machine = platform.machine()
    architecture = {"arm64": "aarch64"}.get(machine, machine)
    system = {"Darwin": "darwin", "Linux": "linux-gnu"}[platform.system()]
    target = f"{architecture}-unknown-{system}"
    root = pathlib.Path("build/dependencies/sun_gobject/package")
    temporary = root.with_name("package.new")
    artifacts = []
    files = []
    sources = {
        "lib/sun_gobject.moon": build / "sun_gobject.moon",
        "lib/sun_gtk.moon": build / "sun_gtk.moon",
        "lib/native/libsun_gobject_native.a": build / "native/libsun_gobject_native.a",
    }
    for source in sources.values():
        if not source.is_file():
            raise FileNotFoundError(f"Build sun_gobject first: missing {source}")
    if temporary.exists():
        shutil.rmtree(temporary)
    for destination, source in sources.items():
        output = temporary / destination
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, output)
        output.chmod(0o644)
        files.append({"path": destination,
                      "hash": hashlib.sha256(output.read_bytes()).hexdigest(),
                      "executable": False})
        if source.suffix == ".moon":
            artifacts.append({"name": source.stem, "kind": "library", "path": destination})
    (temporary / "package.json").write_text(json.dumps({
        "format_version": 1,
        "name": "sun_gobject",
        "package_id": "namo-robotics/sun_gobject",
        "target": target,
        "artifacts": artifacts,
        "files": files,
    }, indent=2) + "\n")
    if root.exists():
        shutil.rmtree(root)
    temporary.rename(root)


if __name__ == "__main__":
    stage(pathlib.Path(sys.argv[1]))

#!/usr/bin/env python3
# garlic-player: SBOM (CycloneDX 1.5 JSON) of a build, for the release.
#
#   scripts/sbom/create-sbom.py <build-dir> [output]      default output: garlic-player.cdx.json
#
# C++ has no package manager, so the third party components are listed in scripts/sbom/components.json and their
# versions are read from the build directory, as they were really used:
#   cmake_package  <name>_DIR in CMakeCache.txt -> <name>ConfigVersion.cmake (Qt, vcpkg packages)
#   fetchcontent   _deps/<name>-src exists -> GIT_TAG of FetchContent_Declare(<name> in the CMakeLists
#   pkgconfig      pkg-config --modversion, with lib/pkgconfig of vcpkg first when the build used vcpkg
# A component behind a CMake option that is OFF is left out, as is an optional one that is not found; a required
# one that is not found stops the script. The version of the player is src/player-ui/VERSION.
import json
import os
import re
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read_cache(build_dir: Path) -> dict[str, str]:
    cache = {}
    for line in (build_dir / "CMakeCache.txt").read_text(errors="replace").splitlines():
        match = re.match(r"^([A-Za-z0-9_.+-]+):[A-Z]+=(.*)$", line)
        if match:
            cache[match.group(1)] = match.group(2)
    return cache


def from_cmake_package(name: str, cache: dict[str, str]) -> str | None:
    directory = cache.get(name + "_DIR", "")
    if not directory or "pkgRedirects" in directory:  # FetchContent, not a real package
        return None
    # Qt keeps the version in <name>ConfigVersionImpl.cmake
    for file in (f"{name}ConfigVersionImpl.cmake", f"{name}ConfigVersion.cmake", f"{name.lower()}-config-version.cmake"):
        path = Path(directory) / file
        if path.is_file():
            match = re.search(r'set\(PACKAGE_VERSION\s+"?([0-9][^")\s]*)"?\)', path.read_text(errors="replace"))
            if match:
                return match.group(1)
    return None


def from_fetchcontent(name: str, file: str, build_dir: Path) -> str | None:
    if not (build_dir / "_deps" / f"{name.lower()}-src").is_dir():
        return None
    text = (ROOT / file).read_text()
    match = re.search(r"FetchContent_Declare\(\s*" + re.escape(name) + r"\b.*?GIT_TAG\s+(\S+)", text, re.S)
    return match.group(1) if match else None


def from_pkgconfig(name: str, env: dict[str, str]) -> str | None:
    try:
        result = subprocess.run(["pkg-config", "--modversion", name], capture_output=True, text=True, env=env)
    except FileNotFoundError:
        return None
    return result.stdout.strip() if result.returncode == 0 and result.stdout.strip() else None


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__ or "usage: create-sbom.py <build-dir> [output]", file=sys.stderr)
        return 2
    build_dir = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2] if len(sys.argv) > 2 else "garlic-player.cdx.json")
    cache = read_cache(build_dir)

    env = dict(os.environ)
    vcpkg = cache.get("VCPKG_INSTALLED_DIR", "")
    triplet = cache.get("VCPKG_TARGET_TRIPLET", "")
    if vcpkg and triplet:
        env["PKG_CONFIG_PATH"] = os.pathsep.join(filter(None, [str(Path(vcpkg) / triplet / "lib" / "pkgconfig"), env.get("PKG_CONFIG_PATH", "")]))

    manifest = json.loads((ROOT / "scripts" / "sbom" / "components.json").read_text())
    player_version = (ROOT / "src" / "player-ui" / "VERSION").read_text().splitlines()[0].strip()
    root_ref = f"garlic-player@{player_version}"

    components, refs, missing = [], [], []
    for entry in manifest["components"]:
        option = entry.get("option")
        if option and cache.get(option, "OFF").upper() not in ("ON", "TRUE", "1", "YES"):
            continue
        version = None
        for source in entry["sources"]:
            if "cmake_package" in source:
                version = from_cmake_package(source["cmake_package"], cache)
            elif "fetchcontent" in source:
                version = from_fetchcontent(source["fetchcontent"], source["file"], build_dir)
            elif "pkgconfig" in source:
                version = from_pkgconfig(source["pkgconfig"], env)
            if version:
                break
        if not version:
            if entry.get("required"):
                missing.append(entry["name"])
            continue
        ref = f"{entry['name']}@{version}"
        component = {"bom-ref": ref, "type": "library", "name": entry["name"], "version": version}
        license_id = entry["license"]
        component["licenses"] = [{"expression": license_id}] if " OR " in license_id else [{"license": {"id": license_id}}]
        component["purl"] = entry["purl"].replace("{version}", version)
        components.append(component)
        refs.append(ref)

    if missing:
        print("not found in the build: " + ", ".join(missing), file=sys.stderr)
        return 1

    sbom = {
        "$schema": "http://cyclonedx.org/schema/bom-1.5.schema.json",
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
        "version": 1,
        "metadata": {
            "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "tools": [{"name": "garlic-player scripts/sbom/create-sbom.py"}],
            "component": {
                "bom-ref": root_ref, "type": "application", "name": "garlic-player", "version": player_version,
                "licenses": [{"license": {"id": "AGPL-3.0-only"}}],
                "purl": f"pkg:github/garlic-signage/garlic-player@{player_version}",
            },
        },
        "components": components,
        "dependencies": [{"ref": root_ref, "dependsOn": refs}] + [{"ref": ref} for ref in refs],
    }
    output.write_text(json.dumps(sbom, indent=4) + "\n")
    print(f"{len(components)} components in {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

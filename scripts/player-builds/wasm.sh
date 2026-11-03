#!/usr/bin/env bash
# Builds the JS player with the WASM core: build/wasm-<type>/ holds everything to deploy.
#
#   scripts/player-builds/wasm.sh                  release
#   scripts/player-builds/wasm.sh debug            debug
#   scripts/player-builds/wasm.sh release <hub>    and copies it as preview into <hub>/public/player-preview/ (garlic-hub)
set -euo pipefail

type=${1:-release}
hub=${2:-}
if [[ $type != release && $type != debug ]]; then
    echo "usage: $0 [release|debug] [garlic-hub directory]" >&2
    exit 1
fi

cd "$(dirname "$0")/../../src/player-ui/js"

# Emscripten and vcpkg: from /etc/profile.d (Arch packages) if the shell does not have them yet; these scripts read
# unset variables, so without -u
set +u
if ! command -v emcc >/dev/null && [[ -f /etc/profile.d/emscripten.sh ]]; then
    source /etc/profile.d/emscripten.sh
fi
if [[ -z ${VCPKG_ROOT:-} && -f /etc/profile.d/vcpkg.sh ]]; then
    source /etc/profile.d/vcpkg.sh
fi
set -u
if ! command -v emcc >/dev/null; then
    echo "emcc not found: install Emscripten or source emsdk_env.sh" >&2
    exit 1
fi
if [[ -z ${VCPKG_ROOT:-} || ! -f $VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake ]]; then
    echo "VCPKG_ROOT does not point to a vcpkg clone" >&2
    exit 1
fi

cmake --preset "wasm-$type"
cmake --build --preset "wasm-$type"

output=build/wasm-$type
files=(index.html player.css player.js stage.js dialogs.js storage.js sw.js garlic-core.js garlic-core.wasm)
echo "built: $output"

if [[ -n $hub ]]; then
    target=$hub/public/player-preview
    mkdir -p "$target"
    for file in "${files[@]}"; do
        cp "$output/$file" "$target/"
    done
    echo "copied to: $target"
fi

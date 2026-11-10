#!/usr/bin/env bash
# Mutation tests of the SMIL module with Mull: build the smil tests with the Mull plugin (LLVM 19), change the code of
# src/core/smil/ in small steps (mutants) and check whether a test fails. Runs in debian:trixie like the CI, only
# Docker is needed on the host. The Mull packages come from the GitHub release (the Cloudsmith repository answered
# "402 Payment Required").
#
#   scripts/mull_smil.sh
#
# Output in build/mull/:
#   report/smil.html   all mutants, to open in the browser
#   report/smil.txt    the surviving mutants as file:line, the test gaps (and equivalent mutants)
#   report/smil.json   Mutation Testing Elements format
set -euo pipefail

mull_version=0.37.0
mull_debs=(
    "Mull-${mull_version}-debian-amd64-13.deb"                 # mull-runner, mull-reporter
    "Mull-19-${mull_version}-LLVM-19.1.7-debian-amd64-13.deb" # plugin for clang 19 (Trixie)
)

# inside the container: build and run
if [[ ${1:-} == --container ]]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends ca-certificates git cmake ninja-build clang-19 pkg-config \
        libcurl4-openssl-dev libmicrohttpd-dev libsqlite3-dev libxml2-dev nlohmann-json3-dev doctest-dev >/dev/null
    apt-get install -y -qq /out/debs/*.deb >/dev/null

    # only the code of smil/ is mutated, not the tests or libraries
    cat >/out/mull.yml <<'EOF'
mutators:
  - cxx_all
includePaths:
  - .*/src/core/smil/.*
EOF
    export MULL_CONFIG=/out/mull.yml
    CC=clang-19 CXX=clang++-19 cmake -S /repo/src/core -B /out/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DGARLIC_CORE_BUILD_TESTS=ON \
        "-DCMAKE_CXX_FLAGS=-O0 -g -grecord-command-line -fpass-plugin=/usr/lib/mull-ir-frontend-19" >/dev/null
    cmake --build /out/build --target garlic_core_smil_tests

    rm -rf /out/report
    mull-runner-19 --reporters=Elements --reporters=IDE --report-dir=/out/report --report-name=smil \
        /out/build/tests/smil/garlic_core_smil_tests 2>&1 | grep -v '^ *\[[-#]*\] [0-9]*/[0-9]*\. [0-9.]*m\?s$'
    exit 0
fi

# on the host
if ! command -v docker >/dev/null; then
    echo "docker not found" >&2
    exit 1
fi

repo=$(cd "$(dirname "$0")/.." && pwd)
out=$repo/build/mull
mkdir -p "$out/debs"

for deb in "${mull_debs[@]}"; do
    if [[ ! -f $out/debs/$deb ]]; then
        echo "download $deb"
        curl -fsSL -o "$out/debs/$deb.part" \
            "https://github.com/mull-project/mull/releases/download/${mull_version}/${deb}"
        mv "$out/debs/$deb.part" "$out/debs/$deb"
    fi
done

# the source read-only; the files in build/mull belong to the user afterwards, not to root
docker run --rm \
    -v "$repo:/repo:ro" \
    -v "$out:/out" \
    debian:trixie \
    bash -c "/repo/scripts/mull_smil.sh --container; status=\$?; chown -R $(id -u):$(id -g) /out; exit \$status"

echo
echo "report: $out/report/smil.html"
echo "surviving mutants: $out/report/smil.txt"

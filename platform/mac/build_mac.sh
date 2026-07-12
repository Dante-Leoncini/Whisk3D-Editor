#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" # el script vive en platform/mac/ -> raiz 2 niveles arriba
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/platform/mac/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Error: build_mac.sh must be run on macOS." >&2
    exit 1
fi

for command in git cmake; do
    if ! command -v "${command}" >/dev/null 2>&1; then
        echo "Error: required command '${command}' was not found." >&2
        exit 1
    fi
done

git -C "${ROOT_DIR}" submodule update --init --recursive

cmake \
    -S "${ROOT_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel

echo
echo "Whisk3D was built successfully:"
echo "${BUILD_DIR}/Whisk3D.app"

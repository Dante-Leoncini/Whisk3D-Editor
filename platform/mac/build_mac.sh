#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" # el script vive en platform/mac/ -> raiz 2 niveles arriba
BUILD_TYPE="${BUILD_TYPE:-Release}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "Error: build_mac.sh must be run on macOS." >&2
    exit 1
fi

# En Apple Silicon una terminal bajo Rosetta puede reportar x86_64. Detectar el
# hardware ARM permite que el build nativo siga siendo arm64; ARCH permite
# seleccionar explicitamente cualquiera de las dos arquitecturas soportadas.
HOST_ARCH="$(uname -m)"
if [[ "${HOST_ARCH}" == "x86_64" ]] && [[ "$(sysctl -in hw.optional.arm64 2>/dev/null || true)" == "1" ]]; then
    HOST_ARCH="arm64"
fi
ARCH="${ARCH:-${HOST_ARCH}}"

case "${ARCH}" in
    x86_64|arm64)
        ;;
    *)
        echo "Error: unsupported macOS architecture '${ARCH}'. Use ARCH=x86_64 or ARCH=arm64." >&2
        exit 1
        ;;
esac

# Mantener caches separadas: CMake/SDL2 hacen pruebas de CPU distintas para
# Intel y Apple Silicon y no deben compartir el mismo directorio de build.
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/platform/mac/build-${ARCH}}"

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
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}"

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel

echo
echo "Whisk3D was built successfully for ${ARCH}:"
echo "${BUILD_DIR}/Whisk3D.app"

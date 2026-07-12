#!/usr/bin/env bash
# ============================================================================
#  build_linux.sh - compila Whisk3D para GNU/Linux (SDL2 + OpenGL de escritorio)
#  Requiere las dependencias del sistema (ver platform/linux/README.md):
#    git build-essential cmake libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev libsdl2-dev
#  Genera: platform/linux/build/whisk3d
#
#  Uso:  platform/linux/build_linux.sh            (Release)
#        BUILD_TYPE=Debug platform/linux/build_linux.sh
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/../.." # el script vive en platform/linux/ -> raiz del repo 2 niveles arriba

BUILD_TYPE="${BUILD_TYPE:-Release}"

for cmd in git cmake; do
    command -v "$cmd" >/dev/null 2>&1 || { echo "ERROR: falta '$cmd' en el PATH." >&2; exit 1; }
done

git submodule update --init --recursive

cmake -B platform/linux/build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build platform/linux/build --parallel

echo
echo "Whisk3D compilado -> platform/linux/build/whisk3d"
echo "(Instaladores .deb/.rpm/AppImage:  cd platform/linux/build && cpack   -- ver platform/linux/README.md)"

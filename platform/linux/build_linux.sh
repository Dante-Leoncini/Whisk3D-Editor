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

# -j ACOTADO, NUNCA "--parallel" pelado: el editor son ~200 unidades de compilacion a
# -std=c++17 y con paralelismo ilimitado el OOM killer cierra la sesion entera en una
# maquina de 15 GB (paso tres veces). La mitad de los nucleos, minimo 1, tope 8; se
# puede pisar con W3D_JOBS=n. Misma regla que W3dCompilarJobs (main/io/CompilarJuego.h).
if [ -z "${W3D_JOBS:-}" ]; then
    W3D_NPROC="$(nproc 2>/dev/null || echo 2)"
    W3D_JOBS=$(( W3D_NPROC / 2 ))
    if [ "${W3D_JOBS}" -lt 1 ]; then W3D_JOBS=1; fi
    if [ "${W3D_JOBS}" -gt 8 ]; then W3D_JOBS=8; fi
fi
echo "Compilando con -j${W3D_JOBS} (W3D_JOBS para cambiarlo)."

cmake -B platform/linux/build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build platform/linux/build -j"${W3D_JOBS}"

# empaquetado: .deb (cpack) + AppImage, todo en una corrida. Si el empaquetado
# falla, el binario igual queda listo (no se aborta el build).
( cd platform/linux/build && cpack -G DEB ) || echo "AVISO: cpack fallo (el .deb no se genero)."
platform/linux/build_appimage.sh || echo "AVISO: no se pudo generar el AppImage."

echo
echo "Whisk3D compilado -> platform/linux/build/whisk3d"
echo ".deb / AppImage   -> platform/linux/build/  (ls whisk3d-*-Linux.deb Whisk3D-*.AppImage)"

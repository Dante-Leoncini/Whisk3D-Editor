#!/usr/bin/env bash
# ---------------------------------------------------------------------------------------------------------------
#  preparar_mingw.sh -- el kit para compilar JUEGOS de Windows desde Linux ("Compilar juego" -> Windows .exe).
#
#  Baja a ~/windows (afuera del proyecto, sin root):
#    - llvm-mingw: clang + lld con las bibliotecas de mingw-w64, que genera ejecutables de Windows desde Linux.
#    - SDL2 para mingw: la biblioteca estatica (el .exe del juego no lleva DLLs al lado).
#  Las dos son libres. Las versiones van fijas, para que dos maquinas armen el mismo .exe.
#
#  (El EDITOR de Windows se compila en Windows con Visual Studio: build_windows.bat.)
#
#  Uso: bash platform/windows/preparar_mingw.sh
# ---------------------------------------------------------------------------------------------------------------
set -euo pipefail
DESTINO="${RAIZWINDOWS:-$HOME/windows}"
LLVM_MINGW_VER="20261020"
SDL2_VER="2.32.10"
LLVM_TAR="llvm-mingw-${LLVM_MINGW_VER}-ucrt-ubuntu-22.04-x86_64.tar.xz"
SDL2_TAR="SDL2-devel-${SDL2_VER}-mingw.tar.gz"

mkdir -p "$DESTINO/descargas"
cd "$DESTINO"
if [ ! -x llvm-mingw/bin/x86_64-w64-mingw32-clang++ ]; then
    echo "== llvm-mingw $LLVM_MINGW_VER"
    [ -f "descargas/$LLVM_TAR" ] || curl -fL -o "descargas/$LLVM_TAR" \
        "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VER/$LLVM_TAR"
    rm -rf llvm-mingw "${LLVM_TAR%.tar.xz}"
    tar xf "descargas/$LLVM_TAR"
    mv "${LLVM_TAR%.tar.xz}" llvm-mingw
fi
if [ ! -f sdl2-mingw/x86_64-w64-mingw32/lib/libSDL2.a ]; then
    echo "== SDL2 $SDL2_VER para mingw"
    [ -f "descargas/$SDL2_TAR" ] || curl -fL -o "descargas/$SDL2_TAR" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/$SDL2_TAR"
    rm -rf sdl2-mingw "SDL2-$SDL2_VER"
    tar xzf "descargas/$SDL2_TAR"
    mv "SDL2-$SDL2_VER" sdl2-mingw
fi
"$DESTINO/llvm-mingw/bin/x86_64-w64-mingw32-clang++" --version | head -1
echo "listo: $DESTINO/llvm-mingw y $DESTINO/sdl2-mingw"

#!/usr/bin/env bash
# ---------------------------------------------------------------------------------------------------------------
#  preparar_shaderc.sh -- el compilador de shaders del backend VULKAN (GLSL -> SPIR-V), para los JUEGOS.
#
#  El backend Vulkan (libs/Whisk3DCore/graphics/vulkan) compila sus shaders mientras corre, con shaderc. El editor lo abre
#  de una biblioteca suelta (shaderc_shared); un juego lo lleva ADENTRO (libshaderc_combined.a, enlazada estatica):
#  sin nada que instalar ni DLLs al lado.
#
#  Baja shaderc (una version fija) con lo que necesita (glslang, SPIRV-Tools, SPIRV-Headers: las versiones que fija
#  su archivo DEPS) y la compila para cada plataforma en ~/shaderc/<plataforma>/lib/libshaderc_combined.a.
#  Afuera del proyecto y sin root.
#
#  Uso: bash tools/preparar_shaderc.sh [linux] [windows] [android]     (sin nada: las tres)
#    windows: con llvm-mingw (platform/windows/preparar_mingw.sh)
#    android: con el NDK (ANDROID_NDK, o el mas nuevo de ~/Android/Sdk/ndk), para arm64-v8a y armeabi-v7a
# ---------------------------------------------------------------------------------------------------------------
set -euo pipefail
DESTINO="${RAIZSHADERC:-$HOME/shaderc}"
VERSION="v2026.4"
JOBS="${JOBS:-$(( $(nproc) > 2 ? $(nproc) - 1 : 1 ))}"
PLATAFORMAS="${*:-linux windows android}"
FUENTE="$DESTINO/fuente-$VERSION"

# ---- las fuentes: shaderc en su version y sus dependencias en los commits de su DEPS ----
traer() {   # traer <repo> <commit|tag> <carpeta>
    [ -d "$3/.git" ] && return 0
    mkdir -p "$3"
    git -C "$3" init -q
    git -C "$3" fetch -q --depth 1 "$1" "$2"
    git -C "$3" checkout -q FETCH_HEAD
}
mkdir -p "$DESTINO"
if [ ! -f "$FUENTE/.listo" ]; then
    echo "== shaderc $VERSION"
    traer https://github.com/google/shaderc.git "$VERSION" "$FUENTE"
    rev() { grep -o "'$1': '[0-9a-f]*'" "$FUENTE/DEPS" | grep -o "[0-9a-f]\{40\}"; }
    traer https://github.com/KhronosGroup/glslang.git "$(rev glslang_revision)" "$FUENTE/third_party/glslang"
    traer https://github.com/KhronosGroup/SPIRV-Tools.git "$(rev spirv_tools_revision)" "$FUENTE/third_party/spirv-tools"
    traer https://github.com/KhronosGroup/SPIRV-Headers.git "$(rev spirv_headers_revision)" "$FUENTE/third_party/spirv-headers"
    touch "$FUENTE/.listo"
fi

OPCIONES=(-DCMAKE_BUILD_TYPE=Release -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON
          -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSHADERC_SKIP_INSTALL=ON -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_SKIP_TESTS=ON
          -DSPIRV_WERROR=OFF -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_CTEST=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

compilar() {   # compilar <nombre> [opciones de cmake...]
    local nombre="$1"; shift
    local salida="$DESTINO/$nombre"
    if [ -f "$salida/lib/libshaderc_combined.a" ]; then echo "== $nombre: ya esta"; return 0; fi
    echo "== $nombre"
    local obra="$DESTINO/obra/$nombre"
    cmake -S "$FUENTE" -B "$obra" "${OPCIONES[@]}" "$@" > "$DESTINO/obra-$nombre.log" 2>&1 \
        || { tail -20 "$DESTINO/obra-$nombre.log"; exit 1; }
    cmake --build "$obra" --target shaderc_combined -j "$JOBS" >> "$DESTINO/obra-$nombre.log" 2>&1 \
        || { tail -20 "$DESTINO/obra-$nombre.log"; exit 1; }
    mkdir -p "$salida/lib" "$salida/include"
    cp "$obra/libshaderc/libshaderc_combined.a" "$salida/lib/"
    cp -r "$FUENTE/libshaderc/include/shaderc" "$salida/include/"
    rm -rf "$obra"
    echo "   $salida/lib/libshaderc_combined.a"
}

for p in $PLATAFORMAS; do
    case "$p" in
        linux) compilar linux ;;
        windows)
            LM="${LLVM_MINGW:-$HOME/windows/llvm-mingw}"
            [ -x "$LM/bin/x86_64-w64-mingw32-clang++" ] || { echo "falta llvm-mingw: platform/windows/preparar_mingw.sh"; exit 1; }
            compilar windows -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
                -DCMAKE_C_COMPILER="$LM/bin/x86_64-w64-mingw32-clang" -DCMAKE_CXX_COMPILER="$LM/bin/x86_64-w64-mingw32-clang++" \
                -DCMAKE_RC_COMPILER="$LM/bin/x86_64-w64-mingw32-windres" -DCMAKE_AR="$LM/bin/llvm-ar" \
                -DCMAKE_RANLIB="$LM/bin/llvm-ranlib" ;;
        android)
            NDK="${ANDROID_NDK:-$(ls -d "$HOME"/Android/Sdk/ndk/* 2>/dev/null | sort -V | tail -1)}"
            [ -f "$NDK/build/cmake/android.toolchain.cmake" ] || { echo "falta el NDK (ANDROID_NDK)"; exit 1; }
            for abi in arm64-v8a armeabi-v7a; do
                compilar "android-$abi" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
                    -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static
            done ;;
        *) echo "plataforma desconocida: $p (linux, windows o android)"; exit 1 ;;
    esac
done
echo "listo: $DESTINO"

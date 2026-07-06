#!/usr/bin/env bash
# ============================================================================
#  build_web.sh - compila el EDITOR Whisk3D para WebGL (Emscripten)
# ----------------------------------------------------------------------------
#  Requiere emsdk activado:  source /ruta/emsdk/emsdk_env.sh   (tener 'emcc' en el PATH)
#  Genera:  web/whisk3d.html + .js + .wasm + .data (res/ empaquetado adentro)
#  El .wasm NO carga por file:// -> hay que servirlo por http:
#      emrun web/whisk3d.html
#    o  python -m http.server --directory web      (y abrir http://localhost:8000/whisk3d.html)
#
#  El backend de graficos es gles2/w3dGraphicsGLES2.cpp (ES2/WebGL), NO el
#  w3dGraphics.cpp de escritorio. Todo el editor dibuja por w3dEngine.
# ============================================================================
set -e
cd "$(dirname "$0")"

CORE=libs/Whisk3DCore
OUT=web
mkdir -p "$OUT"

# --- fuentes: mismo set que el CMakeLists, pero con el backend ES2 en vez del de desktop ---
SRC=$(find main "$CORE/objects" "$CORE/animation" libs/WhiskUI -name '*.cpp')
SRC="$SRC $CORE/w3dFilesystem.cpp $CORE/w3dTexture.cpp $CORE/w3dlog.cpp"
SRC="$SRC $CORE/math/Vector3.cpp $CORE/math/Quaternion.cpp $CORE/math/Matrix4.cpp"
SRC="$SRC $CORE/gles2/w3dGraphicsGLES2.cpp"

# --- includes: los mismos del CMakeLists + platform/web (shim de <GL/gl.h> -> ES2) ---
INC="-Iplatform/web -I. -Imain -I$CORE -I$CORE/thirdparty -Ilibs -Ithirdparty"

# --- defines: solo el backend WebGL. OJO: NO va -DW3D_STB_IMPL: en el editor la
#     implementacion de stb_image ya la instancia constructor.cpp (una sola vez). Con
#     W3D_STB_IMPL w3dTexture.cpp la instanciaria de nuevo -> "duplicate symbol stbi_*". ---
DEF="-DW3D_WEBGL"

# --- flags de Emscripten ---
#   -sUSE_SDL=2            ventana + eventos (mismo SDL2 que los ejemplos web)
#   -sINITIAL_MEMORY=256MB  memoria FIJA. OJO: NO usar -sALLOW_MEMORY_GROWTH: con growth la
#                           memoria WASM es un ArrayBuffer "resizable" y el Chrome/Firefox nuevos
#                           rechazan TextDecoder.decode() sobre eso -> crash al arrancar (pantalla
#                           negra + cuelgue). Con memoria fija es un buffer normal y anda.
#   --preload-file res@/res  empaqueta res/ y lo monta en /res (= GetResDir() en web)
#   --shell-file           HTML propio (canvas fullscreen + pantalla de carga)
EMFLAGS="-sUSE_SDL=2 -sINITIAL_MEMORY=268435456"
EMFLAGS="$EMFLAGS --preload-file res@/res --shell-file platform/web/shell.html"

echo "Compilando editor Whisk3D para WebGL ($(echo $SRC | wc -w) fuentes)..."
emcc -std=c++17 -O2 $DEF $INC $SRC $EMFLAGS -o "$OUT/whisk3d.html"

echo ""
echo "Listo -> $OUT/whisk3d.html"
echo "Servir por http:  emrun $OUT/whisk3d.html   (o)   python -m http.server --directory $OUT"

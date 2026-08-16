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
cd "$(dirname "$0")/../.." # el script vive en platform/web/ -> raiz del repo 2 niveles arriba

CORE=libs/Whisk3DCore
OUT=platform/web/build
mkdir -p "$OUT"

# --- fuentes: mismo set que el CMakeLists, pero con el backend ES2 en vez del de desktop ---
SRC=$(find main "$CORE/objects" "$CORE/animation" libs/WhiskUI -name '*.cpp')
SRC="$SRC $CORE/io/w3dFilesystem.cpp $CORE/gfx/w3dTexture.cpp $CORE/io/w3dCompress.cpp $CORE/io/W3dZip.cpp"
# OJO: los .cpp del Core de io/ NO entran por el find de arriba (que solo barre main/, objects/,
# animation/ y WhiskUI) -> van A MANO, igual que en el CMakeLists. Estos tres son el formato v4
# (el .w3d contenedor): W3dTexto = round-trip exacto de floats, W3dMalla = el .w3dm, W3dAlmacen =
# el montaje del zip. Sin ellos el LINK falla con "undefined symbol: AlmacenCarpeta::Leer" y
# "W3dEscribirFloat", y como el .wasm viejo queda en su lugar es facil creer que compilo.
# Si se agrega otro .cpp al Core, comparar contra el CMakeLists antes de dar por bueno el build.
SRC="$SRC $CORE/io/W3dTexto.cpp $CORE/io/W3dMalla.cpp $CORE/io/W3dAlmacen.cpp"
SRC="$SRC $CORE/io/W3dRecursos.cpp"
SRC="$SRC $CORE/base/w3dlog.cpp $CORE/base/W3dInteractionState.cpp $CORE/base/W3dConfig.cpp"
SRC="$SRC $CORE/math/Vector3.cpp $CORE/math/Quaternion.cpp $CORE/math/Matrix4.cpp"
SRC="$SRC $CORE/gles2/w3dGraphicsGLES2.cpp"
# scripts lua: el modulo del Core + el interprete vendorizado. Los .c de lua NO
# aceptan -std=c++17 -> se compilan APARTE a una lib estatica y se linkea al final.
SRC="$SRC $CORE/script/W3dScript.cpp"
# fisica minima del Core (velocidad + rebotes AABB): va SIEMPRE junto a W3dScript.cpp (registra sus binds)
SRC="$SRC $CORE/physics/W3dFisica.cpp"
# audio: el beep() de los juegos (efectos estilo WhiskPaddle). SDL2 audio anda en emscripten (-sUSE_SDL=2).
# Sin estas fuentes el link falla: W3dScript/main referencian W3dSoundBeep/W3dSoundPlay/W3dAudioInit.
SRC="$SRC $CORE/audio/W3dAudio.cpp $CORE/audio/W3dAudioSDL.cpp"
# portapapeles del sistema (Core): backend SDL (mapea al del navegador en emscripten)
SRC="$SRC $CORE/base/W3dClipboardSDL.cpp"
LUA_A="$OUT/libw3dlua.a"
if [[ ! -f "$LUA_A" || thirdparty/lua/src -nt "$LUA_A" ]]; then
    echo "compilando lua (C) -> libw3dlua.a"
    rm -f "$OUT"/lua_*.o
    for c in $(ls thirdparty/lua/src/*.c | grep -vE '/lua\.c|/luac\.c'); do
        emcc -O2 -Ithirdparty/lua/src -c "$c" -o "$OUT/lua_$(basename "$c" .c).o"
    done
    emar rcs "$LUA_A" "$OUT"/lua_*.o
fi

# --- includes: los mismos del CMakeLists + platform/web (shim de <GL/gl.h> -> ES2) ---
INC="-Iplatform/web -I. -Imain -Imain/app -Imain/config -Imain/io -Imain/undo"
INC="$INC -Imain/ui -Imain/ui/ViewPorts -Imain/ui/GeometriaUI"
INC="$INC -I$CORE -I$CORE/base -I$CORE/gfx -I$CORE/io -I$CORE/thirdparty"
INC="$INC -Ilibs -Ilibs/WhiskUI/widgets -Ilibs/WhiskUI/text -Ilibs/WhiskUI/draw"
INC="$INC -Ilibs/WhiskUI/theme -Ilibs/WhiskUI/core -Ithirdparty -Ithirdparty/lua/src"

# --- defines: solo el backend WebGL. OJO: NO va -DW3D_STB_IMPL: en el editor la
#     implementacion de stb_image ya la instancia constructor.cpp (una sola vez). Con
#     W3D_STB_IMPL w3dTexture.cpp la instanciaria de nuevo -> "duplicate symbol stbi_*". ---
DEF="-DW3D_WEBGL -DW3D_ENABLE_AUDIO"

# --- flags de Emscripten ---
#   -sUSE_SDL=2            ventana + eventos (mismo SDL2 que los ejemplos web)
#   -sINITIAL_MEMORY=128MB  memoria FIJA. OJO: NO usar -sALLOW_MEMORY_GROWTH: con growth la
#                           memoria WASM es un ArrayBuffer "resizable" y el Chrome/Firefox nuevos
#                           rechazan TextDecoder.decode() sobre eso -> crash al arrancar (pantalla
#                           negra + cuelgue). Con memoria fija es un buffer normal y anda.
#                           128MB (antes 256): iOS Safari fallaba al alocar el bloque fijo de 256MB
#                           -> "no carga" (pantalla en blanco). 128 alcanza para el editor + un modelo
#                           y es mucho mas facil de alocar en iOS. Sigue FIJA (no rompe Chrome/Firefox).
#   --preload-file res@/res  empaqueta res/ y lo monta en /res (= GetResDir() en web)
#   --shell-file           HTML propio (canvas fullscreen + pantalla de carga)
EMFLAGS="-sUSE_SDL=2 -sINITIAL_MEMORY=134217728"
#   -sMIN_SAFARI_VERSION=140100  Safari 14.1+: el emcc 6.x por defecto emite features wasm
#                           (bulk-memory, sign-ext, non-trapping fptoint, etc.) que Safari viejo
#                           no soporta -> "no carga". Con este flag emcc baja el target y el
#                           wasm vuelve a andar en Safari/iOS 14.1 en adelante.
EMFLAGS="$EMFLAGS -sMIN_SAFARI_VERSION=140100"
#   -sEXPORTED_RUNTIME_METHODS=ccall,FS,UTF8ToString  el picker (EM_JS) usa ccall (llamar a C) y FS
#                                        (leer/escribir archivos); la descarga usa UTF8ToString
EMFLAGS="$EMFLAGS -sEXPORTED_RUNTIME_METHODS=ccall,FS,UTF8ToString"
EMFLAGS="$EMFLAGS --preload-file res@/res --shell-file platform/web/shell.html"

echo "Compilando editor Whisk3D para WebGL ($(echo $SRC | wc -w) fuentes)..."
emcc -std=c++17 -O2 $DEF $INC $SRC "$LUA_A" $EMFLAGS -o "$OUT/whisk3d.html"

echo ""
echo "Listo -> $OUT/whisk3d.html"
echo "Servir por http:  emrun $OUT/whisk3d.html   (o)   python -m http.server --directory $OUT"

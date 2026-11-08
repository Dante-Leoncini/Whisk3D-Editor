#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  preparar_vita.sh -- deja listo lo que hace falta FUERA del proyecto para compilar y probar Whisk3D para la PS Vita
#  (en ~/vita, o RAIZVITA). Todo con el kit LIBRE (VitaSDK: nada de Sony) y sin root. Si algo ya esta, lo saltea:
#
#    1. VitaSDK: el gcc arm-vita-eabi y las herramientas que arman el .vpk, con su gestor de paquetes (vdpm).
#    2. SDL2 de la Vita con vitaGL (la ventana GL, el tactil, los botones, el sonido). Trae vitaGL: OpenGL sobre la GPU
#       de la Vita con el pipeline fijo emulado (el backend fijo de Whisk3D anda encima, como en la Dreamcast con GLdc).
#    3. El emulador Vita3K en ~/vita/emu (desarmado: asi corre sin FUSE). Su configuracion y su memoria de la consola
#       quedan en ~/vita/emu/home, no en la del usuario.
#    4. Para probar sin ventana (captura_vita3k.sh): Xvfb y xdotool, bajados con apt download (sin root).
#
#  Lo que NO baja (ver platform/vita/README.md): el compilador de shaders de la Vita (libshacccg.suprx), que es de Sony
#  y vitaGL lo necesita para dibujar.
#  Uso: bash platform/vita/preparar_vita.sh
# ---------------------------------------------------------------------------------------------------------------
set -e
RAIZ="${RAIZVITA:-$HOME/vita}"
export VITASDK="$RAIZ/vitasdk"
export PATH="$VITASDK/bin:$PATH"
mkdir -p "$RAIZ/emu"
cd "$RAIZ"

# ---- 1. VitaSDK ----
if [ ! -x "$VITASDK/bin/arm-vita-eabi-gcc" ]; then
    echo "== VitaSDK"
    [ -d vdpm ] || git clone https://github.com/vitasdk/vdpm
    ./vdpm/bootstrap-vitasdk.sh --install-dir "$VITASDK"
fi

# ---- 2. SDL2 con vitaGL (vdpm trae vitaGL y lo que pide) ----
if [ ! -f "$VITASDK/arm-vita-eabi/lib/libvitaGL.a" ] || [ ! -f "$VITASDK/arm-vita-eabi/lib/libSDL2.a" ]; then
    echo "== SDL2 y vitaGL"
    VDPM_NONINTERACTIVE=1 vdpm install sdl2_vitagl
fi

# ---- 3. el emulador ----
EMU="$RAIZ/emu"
if [ ! -x "$EMU/Vita3K-x86_64.AppImage" ]; then
    echo "== Vita3K (emulador)"
    curl -sSfL -o "$EMU/Vita3K-x86_64.AppImage.parcial" \
        https://github.com/Vita3K/Vita3K/releases/download/continuous/Vita3K-x86_64.AppImage
    mv "$EMU/Vita3K-x86_64.AppImage.parcial" "$EMU/Vita3K-x86_64.AppImage"
    chmod +x "$EMU/Vita3K-x86_64.AppImage"
fi
if [ ! -x "$EMU/squashfs-root/AppRun" ]; then
    echo "== Vita3K: desarmando el AppImage"
    (cd "$EMU" && ./Vita3K-x86_64.AppImage --appimage-extract > /dev/null)
fi
# (vita3k.sh: abre el emulador con su carpeta propia; para usarlo con ventana, a mano)
cat > "$EMU/vita3k.sh" <<'FIN'
#!/bin/bash
# Vita3K con su configuracion y su memoria de la consola en ~/vita/emu/home (lo deja preparar_vita.sh)
EMU="$(cd "$(dirname "$0")" && pwd)"
export XDG_DATA_HOME="$EMU/home/data" XDG_CONFIG_HOME="$EMU/home/config" XDG_CACHE_HOME="$EMU/home/cache"
exec "$EMU/squashfs-root/AppRun" "$@"
FIN
chmod +x "$EMU/vita3k.sh"

# ---- 4. para probar sin ventana: un X virtual y xdotool ----
if [ ! -x "$RAIZ/host/pkg/usr/bin/Xvfb" ] || [ ! -x "$RAIZ/host/pkg/usr/bin/xdotool" ]; then
    # (los paquetes del sistema bajados con apt download y desarmados en host/pkg: sin root ni instalar nada)
    echo "== Xvfb y xdotool"
    mkdir -p "$RAIZ/host/deb" && cd "$RAIZ/host/deb"
    apt download xvfb xdotool libxdo3 > /dev/null 2>&1 || echo "   no se pudieron bajar: captura_vita3k.sh necesita Xvfb y xdotool"
    for d in *.deb; do [ -f "$d" ] && dpkg -x "$d" "$RAIZ/host/pkg"; done
    cd "$RAIZ"
fi

echo "listo: $RAIZ (VITASDK=$VITASDK)"
[ -f "$EMU/home/data/Vita3K/Vita3K/ur0/data/external/libshacccg.suprx" ] || \
    echo "falta el compilador de shaders en el emulador: ver platform/vita/README.md (libshacccg.suprx)"

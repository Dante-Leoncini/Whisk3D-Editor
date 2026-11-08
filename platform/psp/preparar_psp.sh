#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  preparar_psp.sh -- deja listo lo que hace falta FUERA del proyecto para compilar y probar Whisk3D para la PSP (en
#  ~/psp, o RAIZPSP). Todo con el kit LIBRE (pspdev: nada de Sony) y sin root. Si algo ya esta, lo saltea:
#
#    1. pspdev: el gcc psp-gcc (MIPS Allegrex), el PSPSDK (sceGu, sceCtrl...) y sus bibliotecas (psp-packages), el
#       paquete ya compilado de su pagina de releases.
#    2. SDL2 de la PSP (los eventos, los botones, el sonido): si el paquete no la trae, con su gestor (psp-pacman).
#    3. El emulador PPSSPP en ~/psp/emu (el AppImage oficial, desarmado: asi corre sin FUSE). Su configuracion y su
#       Memory Stick quedan en ~/psp/emu/home, no en la del usuario.
#    4. Para probar sin ventana (captura_ppsspp.sh): Xvfb y xdotool, bajados con apt download (sin root).
#
#  Uso: bash platform/psp/preparar_psp.sh
# ---------------------------------------------------------------------------------------------------------------
set -e
RAIZ="${RAIZPSP:-$HOME/psp}"
export PSPDEV="$RAIZ/pspdev"
export PATH="$PSPDEV/bin:$PATH"
PPSSPP_VERSION="${PPSSPP_VERSION:-v1.20.4}"
mkdir -p "$RAIZ/emu"
cd "$RAIZ"

# ---- 1. pspdev ----
if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
    echo "== pspdev"
    curl -sSfL -o pspdev.tar.gz.parcial \
        https://github.com/pspdev/pspdev/releases/latest/download/pspdev-ubuntu-latest-x86_64.tar.gz
    mv pspdev.tar.gz.parcial pspdev.tar.gz
    tar -xzf pspdev.tar.gz   # (se desarma en pspdev/)
    rm -f pspdev.tar.gz
fi

# ---- 2. SDL2 de la PSP ----
if [ ! -f "$PSPDEV/psp/lib/libSDL2.a" ]; then
    echo "== SDL2 (psp-pacman)"
    psp-pacman -Sy --noconfirm sdl2
fi

# ---- 3. el emulador ----
EMU="$RAIZ/emu"
if [ ! -x "$EMU/PPSSPP-x86_64.AppImage" ]; then
    echo "== PPSSPP $PPSSPP_VERSION (emulador)"
    curl -sSfL -o "$EMU/PPSSPP-x86_64.AppImage.parcial" \
        "https://github.com/hrydgard/ppsspp/releases/download/$PPSSPP_VERSION/PPSSPP-$PPSSPP_VERSION-anylinux-x86_64.AppImage"
    mv "$EMU/PPSSPP-x86_64.AppImage.parcial" "$EMU/PPSSPP-x86_64.AppImage"
    chmod +x "$EMU/PPSSPP-x86_64.AppImage"
fi
if [ ! -x "$EMU/squashfs-root/AppRun" ]; then
    echo "== PPSSPP: desarmando el AppImage"
    (cd "$EMU" && ./PPSSPP-x86_64.AppImage --appimage-extract > /dev/null)
fi
# (ppsspp.sh: abre el emulador con su carpeta propia; para usarlo con ventana, a mano)
cat > "$EMU/ppsspp.sh" <<'FIN'
#!/bin/bash
# PPSSPP con su configuracion y su Memory Stick en ~/psp/emu/home (lo deja preparar_psp.sh)
EMU="$(cd "$(dirname "$0")" && pwd)"
export XDG_DATA_HOME="$EMU/home/data" XDG_CONFIG_HOME="$EMU/home/config" XDG_CACHE_HOME="$EMU/home/cache"
exec "$EMU/squashfs-root/AppRun" "$@"
FIN
chmod +x "$EMU/ppsspp.sh"

# ---- 4. para probar sin ventana: un X virtual y xdotool ----
if [ ! -x "$RAIZ/host/pkg/usr/bin/Xvfb" ] || [ ! -x "$RAIZ/host/pkg/usr/bin/xdotool" ]; then
    # (los paquetes del sistema bajados con apt download y desarmados en host/pkg: sin root ni instalar nada)
    echo "== Xvfb y xdotool"
    mkdir -p "$RAIZ/host/deb" && cd "$RAIZ/host/deb"
    apt download xvfb xdotool libxdo3 > /dev/null 2>&1 || echo "   no se pudieron bajar: captura_ppsspp.sh necesita Xvfb y xdotool"
    for d in *.deb; do [ -f "$d" ] && dpkg -x "$d" "$RAIZ/host/pkg"; done
    cd "$RAIZ"
fi

echo "listo: $RAIZ (PSPDEV=$PSPDEV)"

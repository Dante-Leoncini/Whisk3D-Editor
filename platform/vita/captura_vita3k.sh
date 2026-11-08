#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  captura_vita3k.sh -- ver en la PC lo que dibuja Whisk3D en la PS Vita, sin la consola ni una ventana en el
#  escritorio. Usa su propia memoria de la consola (~/vita/emu/home-captura), aparte de la del emulador con ventana.
#
#  Instala build/whisk3d.vpk en Vita3K (el emulador, ~/vita/emu: lo deja preparar_vita.sh) y lo corre adentro de un X
#  virtual de 960x544 (Xvfb, display :78), con el Vulkan por software del sistema (lavapipe: el X virtual no tiene
#  placa de video). Espera a que arranque, opcionalmente toca la pantalla y la captura en <salida>.png. El log del
#  juego queda en build/vita3k.log.
#
#  Uso: bash platform/vita/captura_vita3k.sh [segundos=40] [salida=build/captura]
#  TOQUES="x,y x,y" antes del comando: toques en la pantalla (pixeles de la Vita, 0..959 x 0..543), uno por segundo
#  despues de arrancar. "x,y>x2,y2" arrastra de un punto al otro.
# ---------------------------------------------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"
SEGUNDOS="${1:-40}"
SALIDA="${2:-build/captura}"
RAIZ="${RAIZVITA:-$HOME/vita}"
EMU="$RAIZ/emu"
PKG="$RAIZ/host/pkg"
XDO="$PKG/usr/bin/xdotool"
[ -f build/whisk3d.vpk ] || { echo "falta build/whisk3d.vpk (bash build_vita.sh)"; exit 1; }
[ -x "$EMU/squashfs-root/AppRun" ] && [ -x "$PKG/usr/bin/Xvfb" ] && [ -x "$XDO" ] || { echo "falta el emulador o Xvfb/xdotool (preparar_vita.sh)"; exit 1; }
export DISPLAY=:78 WAYLAND_DISPLAY= QT_QPA_PLATFORM=xcb
# (su propia memoria de la consola, ~/vita/emu/home-captura: asi se puede capturar con el emulador abierto en el
# escritorio, que usa ~/vita/emu/home. El compilador de shaders se copia de ahi la primera vez)
CAP="$EMU/home-captura"
export XDG_DATA_HOME="$CAP/data" XDG_CONFIG_HOME="$CAP/config" XDG_CACHE_HOME="$CAP/cache"
SHACC="Vita3K/Vita3K/ur0/data/external/libshacccg.suprx"
if [ ! -f "$CAP/data/$SHACC" ] && [ -f "$EMU/home/data/$SHACC" ]; then
    mkdir -p "$(dirname "$CAP/data/$SHACC")" && cp "$EMU/home/data/$SHACC" "$CAP/data/$SHACC"
fi
if [ ! -f "$CAP/config/Vita3K/config.yml" ] && [ -f "$EMU/home/config/Vita3K/config.yml" ]; then
    mkdir -p "$CAP/config/Vita3K" && cp "$EMU/home/config/Vita3K/config.yml" "$CAP/config/Vita3K/"
fi
# el Vulkan por software del sistema (el AppImage trae solo los drivers de las placas, y el X virtual no tiene ninguna)
export SHARUN_ALLOW_SYS_VKICD=1 VK_LOADER_DRIVERS_SELECT='*lvp*'
CONF="$XDG_CONFIG_HOME/Vita3K/config.yml"
if [ -f "$CONF" ]; then
    # (pref-path: donde Vita3K tiene la memoria de la consola. Lo anota en la configuracion: la copiada apuntaba a la
    # del emulador del escritorio)
    sed -i 's/^backend-renderer: .*/backend-renderer: Vulkan/; s/^show-welcome: .*/show-welcome: false/; s/^check-for-updates-mode: .*/check-for-updates-mode: 0/' "$CONF"
    sed -i "s|^pref-path: .*|pref-path: $CAP/data/Vita3K/Vita3K|" "$CONF"
fi
LOGAPP="$XDG_DATA_HOME/Vita3K/Vita3K/ux0/data/Whisk3D/whisk3d.log"
rm -f "$LOGAPP"

# (el X virtual mide lo mismo que la pantalla de la Vita: la ventana del emulador la ocupa entera, sin escalar)
LD_LIBRARY_PATH="$PKG/usr/lib/x86_64-linux-gnu" "$PKG/usr/bin/Xvfb" :78 -screen 0 960x544x24 -nolisten tcp > /dev/null 2>&1 &
XVFB=$!
sleep 1
# (con el .vpk como argumento lo instala, o lo actualiza, y lo arranca; --frontend sdl: solo la pantalla de la Vita)
"$EMU/squashfs-root/AppRun" --frontend sdl -B Vulkan "$PWD/build/whisk3d.vpk" > build/vita3k_emu.log 2>&1 &
EMUPID=$!
trap 'kill $EMUPID 2>/dev/null; sleep 1; kill $XVFB 2>/dev/null' EXIT
x() { LD_LIBRARY_PATH="$PKG/usr/lib/x86_64-linux-gnu" "$XDO" "$@"; }

sleep "$SEGUNDOS"
for t in ${TOQUES:-}; do
    if [[ "$t" == *">"* ]]; then
        a="${t%%>*}"; b="${t##*>}"
        ax=${a%%,*}; ay=${a##*,}; bx=${b%%,*}; by=${b##*,}
        x mousemove "$ax" "$ay" mousedown 1
        sleep 0.2
        for i in $(seq 1 20); do
            x mousemove $((ax + (bx - ax) * i / 20)) $((ay + (by - ay) * i / 20))
            sleep 0.05
        done
        sleep 0.2
        x mouseup 1
    else
        # (un toque de verdad dura algo: un click instantaneo puede caer entre dos lecturas del tactil)
        x mousemove "${t%%,*}" "${t##*,}" mousedown 1
        sleep 0.15
        x mouseup 1
    fi
    sleep 1
done
sleep 1
LD_LIBRARY_PATH="$PKG/usr/lib/x86_64-linux-gnu" import -window root "${SALIDA}.png"
# el log: lo que escribio Whisk3D (su archivo en la memoria de la consola: el emulador lo termina de escribir al
# cerrarse) y los errores del emulador
kill $EMUPID 2>/dev/null; sleep 1
{ cat "$LOGAPP" 2>/dev/null; echo "---- emulador ----"; grep -a "|E|\||C|\|Unhandled\|SIGSEGV" build/vita3k_emu.log | grep -v "Missing file\|io_error" | tail -40; } > build/vita3k.log
echo "captura: ${SALIDA}.png (log: build/vita3k.log)"

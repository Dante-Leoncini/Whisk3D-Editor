#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  captura_ppsspp.sh -- ver en la PC lo que dibuja Whisk3D en la PSP, sin la consola ni una ventana en el escritorio.
#  Usa su propia configuracion y su propio Memory Stick (~/psp/emu/home-captura), aparte de los del emulador con
#  ventana.
#
#  Copia build/PSP/GAME/Whisk3D al Memory Stick de PPSSPP (el emulador, ~/psp/emu: lo deja preparar_psp.sh) y lo corre
#  adentro de un X virtual de 480x272 (Xvfb, display :79), con el Vulkan por software del sistema (lavapipe: el X
#  virtual no tiene placa de video). Espera y captura la pantalla en <salida>.png. El log de Whisk3D y los errores del
#  emulador quedan en build/ppsspp.log.
#
#  Uso: bash platform/psp/captura_ppsspp.sh [segundos=25] [salida=build/captura]
#  TOQUES="x,y x,y>x2,y2" antes del comando: clicks y arrastres del cursor (pixeles de la PSP, 0..479 x 0..271), uno
#  por segundo desde los 3 segundos de arrancar. La PSP no tiene pantalla tactil: los hace Whisk3D (make PRUEBA=...),
#  asi que con TOQUES se vuelve a compilar w3d_psp.cpp; sin TOQUES, tambien (para sacarlos).
# ---------------------------------------------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"
SEGUNDOS="${1:-25}"
SALIDA="${2:-build/captura}"
RAIZ="${RAIZPSP:-$HOME/psp}"
EMU="$RAIZ/emu"
PKG="$RAIZ/host/pkg"
[ -x "$EMU/squashfs-root/AppRun" ] && [ -x "$PKG/usr/bin/Xvfb" ] || { echo "falta el emulador o Xvfb (preparar_psp.sh)"; exit 1; }
bash build_psp.sh PRUEBA="${TOQUES:-}" > build/build_captura.log 2>&1 || { tail -20 build/build_captura.log; exit 1; }

export DISPLAY=:79 WAYLAND_DISPLAY= SDL_VIDEODRIVER=x11
# (su propia configuracion y su Memory Stick: asi se puede capturar con el emulador abierto en el escritorio)
CAP="$EMU/home-captura"
export XDG_DATA_HOME="$CAP/data" XDG_CONFIG_HOME="$CAP/config" XDG_CACHE_HOME="$CAP/cache"
MS="$XDG_CONFIG_HOME/ppsspp"   # (el Memory Stick de PPSSPP en Linux: PSP/GAME, PSP/SAVEDATA...)
mkdir -p "$MS/PSP/GAME" "$MS/PSP/SYSTEM"
rm -rf "$MS/PSP/GAME/Whisk3D"
cp -r build/PSP/GAME/Whisk3D "$MS/PSP/GAME/Whisk3D"
# la configuracion: la pantalla a 1x (como la de la PSP) sin estirar ni filtrar, sin las ayudas del primer arranque
cat > "$CAP/ajustes.ini" <<'FIN'
[General]
FirstRun = False
CheckForNewVersion = False
ShowOnScreenMessages = False
[Graphics]
InternalResolution = 1
FullScreen = False
BufferFiltering = 2
DisplayStretch = False
DisplayIntegerScale = True
FrameSkip = 0
VSync = False
[Control]
ShowTouchControls = False
FIN
# el Vulkan por software del sistema (el AppImage trae solo los drivers de las placas, y el X virtual no tiene ninguna)
export SHARUN_ALLOW_SYS_VKICD=1 VK_LOADER_DRIVERS_SELECT='*lvp*'
LOGAPP="$MS/PSP/GAME/Whisk3D/whisk3d.log"

# (el X virtual mide lo mismo que la pantalla de la PSP: la ventana del emulador la ocupa entera, sin escalar)
LD_LIBRARY_PATH="$PKG/usr/lib/x86_64-linux-gnu" "$PKG/usr/bin/Xvfb" :79 -screen 0 480x272x24 -nolisten tcp > /dev/null 2>&1 &
XVFB=$!
sleep 1
rm -f build/ppsspp_emu.log
"$EMU/squashfs-root/AppRun" --windowed --xres 480 --yres 272 --graphics=vulkan --appendconfig="$CAP/ajustes.ini" \
    --log="$PWD/build/ppsspp_emu.log" "$MS/PSP/GAME/Whisk3D/EBOOT.PBP" > build/ppsspp_salida.log 2>&1 &
EMUPID=$!
trap 'kill $EMUPID 2>/dev/null; sleep 1; kill $XVFB 2>/dev/null' EXIT

sleep "$SEGUNDOS"
LD_LIBRARY_PATH="$PKG/usr/lib/x86_64-linux-gnu" import -window root "${SALIDA}.png"
kill $EMUPID 2>/dev/null; sleep 1
# el log: lo que escribio Whisk3D (su archivo al lado del EBOOT.PBP) y los errores del emulador
{ cat "$LOGAPP" 2>/dev/null; echo "---- emulador ----"; grep -a " E\[\| W\[\|Crash\|crash\|Invalid\|Unimpl" build/ppsspp_emu.log 2>/dev/null | \
    grep -v "ISO looks bogus\|relocate non-loaded section\|controller config\|CreateFullPath\|OpenCFile" | tail -40; } > build/ppsspp.log
echo "captura: ${SALIDA}.png (log: build/ppsspp.log)"

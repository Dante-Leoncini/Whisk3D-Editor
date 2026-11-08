#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  build_vita.sh -- compila Whisk3D para la PS Vita (build/whisk3d.vpk). Antes, una vez: preparar_vita.sh.
#  Los argumentos pasan a make (por ejemplo CAPTURA=30, o clean).
# ---------------------------------------------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"
RAIZVITA="${RAIZVITA:-$HOME/vita}"
export VITASDK="${VITASDK:-$RAIZVITA/vitasdk}"
export PATH="$VITASDK/bin:$PATH"
[ -x "$VITASDK/bin/arm-vita-eabi-g++" ] || { echo "falta VitaSDK en $VITASDK (correr preparar_vita.sh)"; exit 1; }
[ -f "$VITASDK/arm-vita-eabi/lib/libvitaGL.a" ] || { echo "falta vitaGL en $VITASDK (correr preparar_vita.sh)"; exit 1; }
[ -f "$VITASDK/arm-vita-eabi/lib/libSDL2.a" ] || { echo "falta SDL2 de la Vita en $VITASDK (correr preparar_vita.sh)"; exit 1; }
command -v convert > /dev/null || { echo "falta ImageMagick (convert): arma el icono"; exit 1; }
make -j"${W3D_JOBS:-$(nproc)}" "$@"

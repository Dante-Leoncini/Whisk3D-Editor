#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  build_psp.sh -- compila Whisk3D para la PSP (build/PSP/GAME/Whisk3D y build/whisk3d-psp.zip). Antes, una vez:
#  preparar_psp.sh. Los argumentos pasan a make (por ejemplo CAPTURA=30, PRUEBA="240,136", o clean).
# ---------------------------------------------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"
RAIZPSP="${RAIZPSP:-$HOME/psp}"
export PSPDEV="${PSPDEV:-$RAIZPSP/pspdev}"
export PATH="$PSPDEV/bin:$PATH"
[ -x "$PSPDEV/bin/psp-g++" ] || { echo "falta pspdev en $PSPDEV (correr preparar_psp.sh)"; exit 1; }
[ -f "$PSPDEV/psp/lib/libSDL2.a" ] || { echo "falta SDL2 de la PSP en $PSPDEV (correr preparar_psp.sh)"; exit 1; }
command -v convert > /dev/null || { echo "falta ImageMagick (convert): arma el icono"; exit 1; }
command -v zip > /dev/null || { echo "falta zip: arma el paquete para bajar"; exit 1; }
make -j"${W3D_JOBS:-$(nproc)}" "$@"

#!/bin/bash
# ==========================================================================
#  EL comando de regresion del motor. Corre TODAS las pruebas .w3s de esta
#  carpeta y devuelve 0 si todas pasan, 1 si alguna falla.
#
#    ./correr_todas.sh              # todas
#    ./correr_todas.sh riel espejo  # solo las que contengan "riel" o "espejo"
#
#  Todas las pruebas de esta carpeta son SINTETICAS: su escena, su geometria y
#  sus texturas viven al lado del .w3s, asi que un clon limpio del motor puede
#  regresionarse sin ningun proyecto externo en disco. Una prueba que necesite
#  contenido de un juego concreto NO va aca: va en el repo de ese proyecto.
#
#  Unica excepcion: el CORPUS DE ORO del formato (Whisk3D/formato/corpus,
#  gitignoreado). Sin esa carpeta, las partes de prueba_formatos.w3s que lo
#  usan se SALTEAN con un aviso "SIN CORPUS:" (la prueba sigue en verde y el
#  resumen final dice cuantas partes no se probaron). Con
#  W3D_CORPUS_OBLIGATORIO=1 la falta del corpus es una FALLA.
#
#  El binario se elige con W3D_BIN; por defecto, el build de linux del repo.
# ==========================================================================
set -u
cd "$(dirname "$0")" || exit 1

W3D_BIN="${W3D_BIN:-../../platform/linux/build/whisk3d}"
# SIN VENTANAS: por defecto las pruebas dibujan en el driver 'offscreen' de SDL (GL por EGL, sin
# abrir ninguna ventana en el escritorio). Para verlas: W3D_VER_VENTANA=1 ./correr_todas.sh
if [ -z "${W3D_VER_VENTANA:-}" ]; then export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-offscreen}" SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"; fi
if [ ! -x "$W3D_BIN" ]; then
    echo "ERROR: no encuentro el binario del motor en '$W3D_BIN'."
    echo "       Compilalo o pasa W3D_BIN=<ruta> ./correr_todas.sh"
    exit 1
fi
TIMEOUT="${W3D_TIMEOUT:-300}"

pruebas=()
for f in prueba_*.w3s; do
    [ -e "$f" ] || continue
    if [ "$#" -eq 0 ]; then
        pruebas+=("$f")
    else
        for pat in "$@"; do
            case "$f" in *"$pat"*) pruebas+=("$f"); break;; esac
        done
    fi
done

ok=0; fallo=0; fallidas=(); sincorpus=0
for f in "${pruebas[@]}"; do
    printf '%-34s ' "$f"
    # -k: SDL atrapa el SIGTERM (lo vuelve un evento de salida que el harness no mira mientras
    # corre un comando), asi que un test colgado no moria nunca y colgaba la suite entera. Si a
    # los 10 s del aviso sigue vivo va SIGKILL, y timeout devuelve 137 en vez de 124.
    t0=$SECONDS
    salida=$(timeout -k 10 "$TIMEOUT" "$W3D_BIN" --script "$f" 2>&1)
    cod=$?
    if [ $cod -eq 0 ]; then
        # partes salteadas porque no esta el corpus de oro (gitignoreado): verde, pero se dice
        sc=$(echo "$salida" | grep -c "SIN CORPUS:")
        if [ "$sc" -gt 0 ]; then echo "OK ($sc parte(s) SIN CORPUS)"; else echo "OK"; fi
        sincorpus=$((sincorpus + sc))
        ok=$((ok + 1))
    else
        if [ $cod -eq 124 ] || { [ $cod -eq 137 ] && [ $((SECONDS - t0)) -ge "$TIMEOUT" ]; }; then
            echo "TIMEOUT (${TIMEOUT}s)"
        else
            echo "FALLA (exit $cod)"
        fi
        # El runner imprime "FALLO [n] cmd" y en la linea siguiente "-> motivo": se muestran las dos.
        motivo=$(echo "$salida" | grep -E -A1 "^(FALLO|FALLA|ERROR|=== TEST)" | tail -6)
        # Sin linea de FALLO (crash, abort, timeout): las ultimas lineas de la salida para ubicar el comando.
        if [ -z "$motivo" ]; then motivo=$(echo "$salida" | tail -5); fi
        echo "$motivo" | sed 's/^/      /'
        fallo=$((fallo + 1)); fallidas+=("$f")
    fi
done

echo "--------------------------------------------------"
echo "$ok OK, $fallo FALLA(S), de $((ok + fallo)) pruebas"
if [ $sincorpus -gt 0 ]; then
    echo "AVISO: $sincorpus parte(s) no se probaron: falta formato/corpus (gitignoreado; copialo o"
    echo "       corre con W3D_CORPUS_OBLIGATORIO=1 para que su falta sea una falla)"
fi
if [ $fallo -gt 0 ]; then
    printf '  fallo: %s\n' "${fallidas[@]}"
    exit 1
fi
exit 0

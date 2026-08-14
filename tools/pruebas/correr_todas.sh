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
#  El binario se elige con W3D_BIN; por defecto, el build de linux del repo.
# ==========================================================================
set -u
cd "$(dirname "$0")" || exit 1

W3D_BIN="${W3D_BIN:-../../platform/linux/build/whisk3d}"
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

ok=0; fallo=0; fallidas=()
for f in "${pruebas[@]}"; do
    printf '%-34s ' "$f"
    salida=$(timeout "$TIMEOUT" "$W3D_BIN" --script "$f" 2>&1)
    cod=$?
    if [ $cod -eq 0 ]; then
        echo "OK"
        ok=$((ok + 1))
    else
        if [ $cod -eq 124 ]; then echo "TIMEOUT (${TIMEOUT}s)"; else echo "FALLA (exit $cod)"; fi
        echo "$salida" | grep -E "^(FALLA|ERROR|=== TEST)" | tail -5 | sed 's/^/      /'
        fallo=$((fallo + 1)); fallidas+=("$f")
    fi
done

echo "--------------------------------------------------"
echo "$ok OK, $fallo FALLA(S), de $((ok + fallo)) pruebas"
if [ $fallo -gt 0 ]; then
    printf '  fallo: %s\n' "${fallidas[@]}"
    exit 1
fi
exit 0

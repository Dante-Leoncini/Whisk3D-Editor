#!/bin/bash
# ---------------------------------------------------------------------------------------------------------------
#  comparar_backends.sh -- dibuja los mismos proyectos con los DOS backends de graficos y compara las capturas:
#    - el FIJO (OpenGL / GLES 1.1: PC y N95)          -> platform/linux/build/whisk3d
#    - el de SHADERS (GLES 2.0 / WebGL: web y Android) -> platform/linux/build-gles2/whisk3d (cmake -DW3D_PC_GLES2=ON)
#  Los dos tienen que verse igual: si el shader se aparta del pipeline fijo (la luz, la textura, la niebla...), aca
#  salta como diferencia de pixeles, sin pasar por el navegador ni el telefono.
#
#  Uso:
#      bash tools/comparar_backends.sh <proyecto.w3d> [<proyecto.w3d> ...]
#  Por cada proyecto: captura al abrir (a) y tras correr el juego 45 cuadros (b). Imprime el brillo medio de cada
#  backend y cuantos pixeles difieren; deja en la carpeta de salida (W3D_CMP_OUT, o una temporal) las capturas y una
#  tira fijo | shaders | diferencia x4. (Al abrir puede diferir el outliner: las miniaturas se generan de a poco.)
# ---------------------------------------------------------------------------------------------------------------
set -e
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
[ $# -ge 1 ] || { echo "uso: bash tools/comparar_backends.sh <proyecto.w3d> [...]"; exit 1; }
JOBS="${W3D_JOBS:-6}"
OUT="${W3D_CMP_OUT:-$(mktemp -d)}"
mkdir -p "$OUT"
OUT="$(realpath "$OUT")"
# las rutas de los proyectos, absolutas ANTES de cambiar de carpeta
PROYECTOS=()
for W3D in "$@"; do
    [ -f "$W3D" ] || { echo "no existe: $W3D"; exit 1; }
    PROYECTOS+=("$(realpath "$W3D")")
done

echo "== compilando los dos backends"
cmake --build "$RAIZ/platform/linux/build" -j"$JOBS" > "$OUT/build_fijo.log" 2>&1 || { echo "fallo el build fijo ($OUT/build_fijo.log)"; exit 1; }
[ -f "$RAIZ/platform/linux/build-gles2/CMakeCache.txt" ] || cmake -B "$RAIZ/platform/linux/build-gles2" -S "$RAIZ" -DW3D_PC_GLES2=ON > "$OUT/cfg_gles2.log" 2>&1
cmake --build "$RAIZ/platform/linux/build-gles2" -j"$JOBS" > "$OUT/build_gles2.log" 2>&1 || { echo "fallo el build de shaders ($OUT/build_gles2.log)"; exit 1; }

cd "$RAIZ/tools/pruebas"   # (de aca corre el harness: res/ y las rutas relativas)
NOMBRES=()
for W3D in "${PROYECTOS[@]}"; do
    N="$(basename "$W3D" .w3d)"
    NOMBRES+=("$N")
    for B in fijo es2; do
        printf 'openw3d %s\nloadtex\nuishot %s/%s_%s_a.png\nsimplay 45\nloadtex\nuishot %s/%s_%s_b.png\n' \
            "$W3D" "$OUT" "$B" "$N" "$OUT" "$B" "$N" > "$OUT/${B}_$N.w3s"
    done
    SDL_VIDEODRIVER=offscreen "$RAIZ/platform/linux/build/whisk3d" --script "$OUT/fijo_$N.w3s" > "$OUT/fijo_$N.log" 2>&1 &
    SDL_VIDEODRIVER=offscreen "$RAIZ/platform/linux/build-gles2/whisk3d" --script "$OUT/es2_$N.w3s" > "$OUT/es2_$N.log" 2>&1 &
    wait
    for B in fijo es2; do grep -q "TEST OK" "$OUT/${B}_$N.log" || echo "OJO: $N no corrio bien con el backend $B ($OUT/${B}_$N.log)"; done
done

python3 - "$OUT" "${NOMBRES[@]}" <<'EOF'
import os, sys
import numpy as np
from PIL import Image
out = sys.argv[1]
for n in sys.argv[2:]:
    for t in "ab":
        f, e = os.path.join(out, "fijo_%s_%s.png" % (n, t)), os.path.join(out, "es2_%s_%s.png" % (n, t))
        if not (os.path.exists(f) and os.path.exists(e)):
            print("%s %s: falta una captura (mirar %s/*_%s.log)" % (n, t, out, n)); continue
        A = np.asarray(Image.open(f).convert("RGB")).astype(np.int16)
        B = np.asarray(Image.open(e).convert("RGB")).astype(np.int16)
        h, w = min(A.shape[0], B.shape[0]), min(A.shape[1], B.shape[1])
        A, B = A[:h, :w], B[:h, :w]
        D = np.abs(A - B)
        d = D.max(axis=2)
        print("%-28s %s  brillo fijo %5.1f  shaders %5.1f  dif media %5.2f  px >16: %5.2f%%  px >48: %5.2f%%" % (
            n, t, A.mean(), B.mean(), d.mean(), 100.0 * (d > 16).mean(), 100.0 * (d > 48).mean()))
        tira = np.concatenate([A, B, np.clip(D * 4, 0, 255)], axis=1).astype(np.uint8)
        Image.fromarray(tira).save(os.path.join(out, "cmp_%s_%s.png" % (n, t)))
EOF
echo "capturas y tiras en: $OUT"

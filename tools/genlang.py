#!/usr/bin/env python3
# ---------------------------------------------------------------------------------------------------------------
#  genlang.py -- (re)genera main/config/W3dLangTabla.h dejando las filas ORDENADAS.
#
#  El diccionario ES la tabla: no hay otra fuente. Este script la lee, la re-ordena y la vuelve a escribir con
#  las MISMAS filas (texto identico, solo cambia el orden) -> el diff de una regeneracion es puro movimiento.
#
#  EL PUNTO IMPORTANTE (y el bug que arreglo): T() bisecta con strcmp sobre el string en RUNTIME, no sobre el
#  literal como se ve en el .h. En el literal `"\" already exists..."` el primer byte es la barra invertida (0x5C);
#  en runtime es la comilla (0x22). Ordenar por el literal ESCAPADO deja esas entradas fuera de lugar para strcmp
#  y la biseccion se pierde (se veia como ~17 traducciones "faltantes"). Por eso el orden se calcula sobre el
#  string DESESCAPADO y en BYTES utf-8, que es exactamente lo que compara strcmp (unsigned char).
#
#  Uso:
#      python3 tools/genlang.py            # reordena y reescribe el .h (si hace falta)
#      python3 tools/genlang.py --check    # no escribe: 0 si ya esta ordenada, 1 si no
# ---------------------------------------------------------------------------------------------------------------

import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLA = os.path.join(RAIZ, "main", "config", "W3dLangTabla.h")

ABRE = "static const W3dLangEntrada W3dLangTabla[] = {"
CIERRA = "};"


def desescapar(lit):
    """El literal C tal cual aparece en el .h (SIN las comillas) -> el string que ve strcmp en runtime."""
    out = []
    i = 0
    simples = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', '\\': '\\', '"': '"', "'": "'"}
    while i < len(lit):
        c = lit[i]
        if c == '\\' and i + 1 < len(lit):
            sig = lit[i + 1]
            if sig in simples:
                out.append(simples[sig])
                i += 2
                continue
        out.append(c)
        i += 1
    return "".join(out)


def primer_literal(linea):
    """Primer literal C de la linea (el ingles = la clave), ya desescapado. None si la linea no tiene ninguno."""
    i = linea.find('"')
    if i < 0:
        return None
    i += 1
    crudo = []
    while i < len(linea):
        c = linea[i]
        if c == '\\' and i + 1 < len(linea):   # \" no cierra el literal
            crudo.append(linea[i:i + 2])
            i += 2
            continue
        if c == '"':
            return desescapar("".join(crudo))
        crudo.append(c)
        i += 1
    return None


def main():
    check = "--check" in sys.argv

    with open(TABLA, "r", encoding="utf-8") as f:
        lineas = f.read().split("\n")

    try:
        ini = next(i for i, l in enumerate(lineas) if l.strip() == ABRE)
    except StopIteration:
        print("ERROR: no encontre el arranque de la tabla en %s" % TABLA)
        return 2
    try:
        fin = next(i for i in range(ini + 1, len(lineas)) if lineas[i].strip() == CIERRA)
    except StopIteration:
        print("ERROR: no encontre el cierre de la tabla en %s" % TABLA)
        return 2

    filas = []          # (clave en RUNTIME como bytes, la linea entera tal cual)
    for l in lineas[ini + 1:fin]:
        if not l.strip():
            continue
        en = primer_literal(l)
        if en is None:
            print("ERROR: fila sin literal ingles: %s" % l)
            return 2
        filas.append((en.encode("utf-8"), l))

    # duplicados: dos filas con la misma clave hacen que una sea inalcanzable
    vistas = {}
    for clave, l in filas:
        if clave in vistas:
            print("ERROR: clave duplicada %r" % clave.decode("utf-8", "replace"))
            return 2
        vistas[clave] = l

    ordenadas = sorted(filas, key=lambda f: f[0])   # bytes = el orden de strcmp (unsigned char)
    ya = [f[1] for f in filas] == [f[1] for f in ordenadas]

    if check:
        print("tabla: %d entradas | ordenada por el string RUNTIME: %s" % (len(filas), "OK" if ya else "MAL"))
        return 0 if ya else 1

    if ya:
        print("tabla: %d entradas, ya estaba ordenada (nada que escribir)" % len(filas))
        return 0

    nuevas = lineas[:ini + 1] + [f[1] for f in ordenadas] + lineas[fin:]
    with open(TABLA, "w", encoding="utf-8") as f:
        f.write("\n".join(nuevas))
    movidas = sum(1 for a, b in zip(filas, ordenadas) if a[1] != b[1])
    print("tabla: %d entradas reescritas (%d filas cambiaron de posicion). Contenido intacto: solo el ORDEN."
          % (len(filas), movidas))
    return 0


if __name__ == "__main__":
    sys.exit(main())

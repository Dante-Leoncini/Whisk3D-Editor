#!/usr/bin/env python3
# ---------------------------------------------------------------------------------------------------------------
#  genlang.py -- las TRADUCCIONES de la interfaz del editor: res/idiomas/<codigo>.txt (es, pt, fr, it, tr, de, pl, ru).
#
#  Estan en disco y no compiladas: el editor carga al arrancar SOLO el archivo del idioma elegido (en el N95 la RAM
#  vale; antes las 1700 frases en tres idiomas vivian siempre en memoria, dentro del ejecutable). El ingles no
#  tiene archivo: es la CLAVE (T("Add") en el codigo) y lo que no esta traducido sale en ingles.
#
#  FORMATO: UTF-8, una frase por linea:   <ingles> TAB <traduccion>
#  Dentro de cada lado: \n = salto de linea, \t = tab, \\ = barra invertida. Sin comentarios (hay claves que
#  empiezan con '#'). Una traduccion vacia = todavia sin traducir (sale en ingles).
#
#  AGREGAR UNA FRASE NUEVA A LA INTERFAZ: T("...") en el codigo con el ingles, y la linea en es.txt y pt.txt (y en
#  los demas idiomas si se puede: lo que falte sale en ingles). Despues correr este script: ordena los archivos y
#  avisa lo que este mal.
#
#  Lo que controla:
#    - el formato (un TAB por linea), claves repetidas
#    - los %s / %d / %.1f... de la traduccion: los MISMOS y en el MISMO orden que en el ingles (printf no reordena)
#    - que dos claves distintas no tengan el mismo HASH: el editor no guarda las claves en memoria, solo dos hashes
#      de 32 bits de cada una (ver W3dLang.cpp). Tienen que coincidir con los de aca.
#    - que todos los caracteres se puedan DIBUJAR con la fuente de la interfaz (font.png + font-ext.png, ver
#      tools/fuente_ext.py). Una letra que la fuente no tiene sale como '?'.
#
#  Uso:
#      python3 tools/genlang.py               valida y ordena todos los idiomas
#      python3 tools/genlang.py --check       solo valida (sale con 1 si hay algo mal)
#      python3 tools/genlang.py --faltan xx   las frases que le faltan al idioma xx
#      python3 tools/genlang.py --migrar      (una vez) pasa main/config/W3dLangTabla.h a es.txt y pt.txt
# ---------------------------------------------------------------------------------------------------------------
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CARPETA = os.path.join(RAIZ, "res", "idiomas")
TABLA_VIEJA = os.path.join(RAIZ, "main", "config", "W3dLangTabla.h")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

PLACEHOLDER = re.compile(r"%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z)?[diouxXeEfgGcsp%]")


# ---- los hashes: IGUALES a los de W3dLang.cpp ---------------------------------------------------------------------
def fnv1a32(b):
    h = 0x811C9DC5
    for c in b:
        h ^= c
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def sdbm32(b):
    h = 0
    for c in b:
        h = (c + (h << 6) + (h << 16) - h) & 0xFFFFFFFF
    return h


def hashes(clave):
    b = clave.encode("utf-8")
    return fnv1a32(b), sdbm32(b)


# ---- leer / escribir ------------------------------------------------------------------------------------------
def desescapar(s):
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s) and s[i + 1] in "nt\\":
            out.append({"n": "\n", "t": "\t", "\\": "\\"}[s[i + 1]])
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def escapar(s):
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")


def leer(ruta):
    """[(clave, traduccion, nro de linea)] y la lista de errores de formato."""
    filas, errores = [], []
    for n, linea in enumerate(open(ruta, encoding="utf-8").read().split("\n"), 1):
        if linea == "":
            continue
        if linea.count("\t") != 1:
            errores.append("%s:%d: tiene que haber UN tab (ingles<TAB>traduccion)" % (os.path.basename(ruta), n))
            continue
        a, b = linea.split("\t")
        filas.append((desescapar(a), desescapar(b), n))
    return filas, errores


def escribir(ruta, filas):
    filas = sorted(filas, key=lambda f: f[0].encode("utf-8"))
    texto = "".join("%s\t%s\n" % (escapar(a), escapar(b)) for a, b, *_ in filas)
    if not os.path.isfile(ruta) or open(ruta, encoding="utf-8").read() != texto:
        open(ruta, "w", encoding="utf-8").write(texto)
        return True
    return False


def idiomas():
    if not os.path.isdir(CARPETA):
        return []
    return sorted(f[:-4] for f in os.listdir(CARPETA) if f.endswith(".txt") and len(f) == 6)


# ---- la tabla compilada vieja (solo para --migrar) --------------------------------------------------------------
def desescapar_c(lit):
    simples = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", '"': '"', "'": "'"}
    out, i = [], 0
    while i < len(lit):
        if lit[i] == "\\" and i + 1 < len(lit) and lit[i + 1] in simples:
            out.append(simples[lit[i + 1]])
            i += 2
        else:
            out.append(lit[i])
            i += 1
    return "".join(out)


def migrar():
    lit = r'"((?:[^"\\]|\\.)*)"'
    fila = re.compile(r"^\s*\{\s*%s\s*,\s*%s\s*,\s*%s\s*\}\s*,?\s*$" % (lit, lit, lit))
    es, pt = [], []
    for linea in open(TABLA_VIEJA, encoding="utf-8"):
        m = fila.match(linea)
        if m:
            en, tes, tpt = (desescapar_c(x) for x in m.groups())
            es.append((en, tes))
            pt.append((en, tpt))
    os.makedirs(CARPETA, exist_ok=True)
    escribir(os.path.join(CARPETA, "es.txt"), es)
    escribir(os.path.join(CARPETA, "pt.txt"), pt)
    print("migradas %d frases a es.txt y pt.txt" % len(es))


# ---- validar ----------------------------------------------------------------------------------------------------
def validar(escribir_ordenado=True):
    import fuente_ext
    soportados = fuente_ext.soportados()
    mal = 0
    todas = {}                       # union de claves -> para los hashes y para --faltan
    por_idioma = {}
    for idi in idiomas():
        ruta = os.path.join(CARPETA, idi + ".txt")
        filas, errores = leer(ruta)
        vistas = {}
        for clave, trad, n in filas:
            if clave in vistas:
                errores.append("%s.txt:%d: clave repetida (tambien en la linea %d): %.60s" % (idi, n, vistas[clave], clave))
            vistas[clave] = n
            todas.setdefault(clave, n)
            if trad:
                if PLACEHOLDER.findall(clave) != PLACEHOLDER.findall(trad):
                    errores.append("%s.txt:%d: los %%s/%%d no coinciden con el ingles (%s vs %s): %.50s"
                                   % (idi, n, PLACEHOLDER.findall(clave), PLACEHOLDER.findall(trad), clave))
                raros = sorted(set(c for c in trad if c not in soportados and c not in "\n\t"))
                if raros:
                    errores.append("%s.txt:%d: la fuente no tiene %s: %.60s" % (idi, n, " ".join(repr(c) for c in raros), trad))
        for e in errores:
            print(e)
        mal += len(errores)
        por_idioma[idi] = filas
        if escribir_ordenado and not errores and escribir(ruta, [(a, b) for a, b, _ in filas]):
            print("%s.txt: ordenado" % idi)
    vistos = {}
    for clave in todas:
        h = hashes(clave)
        if h in vistos:
            print("HASH REPETIDO (cambiar una de las dos frases): %r / %r" % (clave, vistos[h]))
            mal += 1
        vistos[h] = clave
    for idi, filas in por_idioma.items():
        hechas = sum(1 for _, b, _ in filas if b)
        print("%s: %d de %d frases traducidas" % (idi, hechas, len(todas)))
    return mal, todas, por_idioma


def main():
    if "--migrar" in sys.argv:
        migrar()
    if "--faltan" in sys.argv:
        idi = sys.argv[sys.argv.index("--faltan") + 1]
        _, todas, por_idioma = validar(False)
        hechas = set(a for a, b, _ in por_idioma.get(idi, []) if b)
        for clave in sorted(todas, key=lambda c: c.encode("utf-8")):
            if clave not in hechas:
                print(escapar(clave))
        return 0
    mal, _, _ = validar(escribir_ordenado="--check" not in sys.argv)
    if mal:
        print("%d problema(s)" % mal)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

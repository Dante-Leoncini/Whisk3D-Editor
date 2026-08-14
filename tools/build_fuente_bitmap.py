#!/usr/bin/env python3
# ==========================================================================
#  build_fuente_bitmap.py -- generador de FUENTES BITMAP para Whisk3D.
#
#  Escribe el par <nombre>.png + <nombre>.json que consume Texto2D.fuenteBitmap
#  (formato especificado en formato/fuente-bitmap.md). No depende de PIL ni de
#  ninguna libreria externa: trae adentro una tipografia de 5x7 pixeles y arma
#  el atlas con ella.
#
#    python3 build_fuente_bitmap.py salida.png [--escala 2] [--pad 1]
#                                              [--ancho-max 0] [--mayus-si-falta]
#
#  El atlas queda a proposito de tamano NO potencia de dos: el motor tiene que
#  poder subir un atlas NPOT sin redondear ni reescalar (si lo redondeara, las
#  UV de cada glifo se correrian y el texto saldria borroso o desplazado).
#
#  Para fuentes ripeadas de un juego, el productor del formato es el extractor
#  del proyecto; este script es la referencia de que escribe el formato completo
#  (incluidos xoff/yoff, que los glifos con descendente necesitan).
# ==========================================================================
import argparse, json, os, struct, sys, zlib

# --- Tipografia de 5x7 (mas 2 filas de descendente donde hace falta) -------
# '#' = pixel encendido. Cada glifo se dibuja en una celda de 5 de ancho; el
# alto es el de su lista. La linea base esta en la fila 7 (0-based: tras la 6).
BASE = 7
G = {
 " ": ["....."] * 7,
 "!": ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
 '"': [".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."],
 "'": ["..#..", "..#..", ".....", ".....", ".....", ".....", "....."],
 "(": ["...#.", "..#..", "..#..", "..#..", "..#..", "..#..", "...#."],
 ")": [".#...", "..#..", "..#..", "..#..", "..#..", "..#..", ".#..."],
 "+": [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."],
 "-": [".....", ".....", ".....", "#####", ".....", ".....", "....."],
 ".": [".....", ".....", ".....", ".....", ".....", ".....", "..#.."],
 "/": ["....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...."],
 ":": [".....", "..#..", ".....", ".....", ".....", "..#..", "....."],
 "=": [".....", ".....", "#####", ".....", "#####", ".....", "....."],
 "?": [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
 "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
 "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
 "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
 "3": ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
 "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
 "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
 "6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
 "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
 "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
 "9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
 "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
 "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
 "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
 "D": ["###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."],
 "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
 "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
 "G": [".###.", "#...#", "#....", "#..##", "#...#", "#...#", ".####"],
 "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
 "I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
 "J": ["....#", "....#", "....#", "....#", "#...#", "#...#", ".###."],
 "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
 "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
 "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#"],
 "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
 "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
 "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
 "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
 "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
 "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
 "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
 "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
 "V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
 "W": ["#...#", "#...#", "#...#", "#...#", "#.#.#", "##.##", "#...#"],
 "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
 "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
 "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
 # --- minusculas: las cuatro con DESCENDENTE llevan 9 filas -------------
 "a": [".....", ".....", ".###.", "....#", ".####", "#...#", ".####"],
 "e": [".....", ".....", ".###.", "#...#", "#####", "#....", ".###."],
 "g": [".....", ".....", ".####", "#...#", ".####", "....#", ".###.",
       ".....", "....."],  # las 2 ultimas se rellenan abajo
 "j": ["..#..", ".....", "..#..", "..#..", "..#..", "..#..", "..#..",
       "#.#..", ".#..."],
 "n": [".....", ".....", "####.", "#...#", "#...#", "#...#", "#...#"],
 "o": [".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###."],
 "p": [".....", ".....", "####.", "#...#", "#...#", "####.", "#....",
       "#....", "#...."],
 "q": [".....", ".....", ".####", "#...#", "#...#", ".####", "....#",
       "....#", "....#"],
 "y": [".....", ".....", "#...#", "#...#", "#...#", ".####", "....#",
       "....#", ".###."],
 ",": [".....", ".....", ".....", ".....", ".....", ".....", "..##.",
       "..#..", ".#..."],
}
# 'g' con su cola
G["g"][7] = "....#"
G["g"][8] = ".###."


def png_rgba(ancho, alto, pix):
    """PNG RGBA de 8 bits sin dependencias. `pix` = bytearray de ancho*alto*4."""
    crudo = bytearray()
    for y in range(alto):
        crudo.append(0)                                  # filtro None
        crudo += pix[y * ancho * 4:(y + 1) * ancho * 4]

    def chunk(tipo, datos):
        c = struct.pack(">I", len(datos)) + tipo + datos
        return c + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", ancho, alto, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(crudo), 9))
            + chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser(description="Genera una fuente bitmap de Whisk3D (.png + .json)")
    ap.add_argument("salida", help="ruta del .png (el .json se escribe al lado)")
    ap.add_argument("--escala", type=int, default=2, help="pixeles por pixel de la tipografia (default 2)")
    ap.add_argument("--pad", type=int, default=1, help="separacion entre glifos en el atlas (default 1)")
    ap.add_argument("--ancho-max", type=int, default=0,
                    help="ancho maximo del atlas; 0 = elegir uno NPOT automatico")
    ap.add_argument("--mayus-si-falta", action="store_true",
                    help="marca la fuente como 'una minuscula ausente cae en su mayuscula'")
    a = ap.parse_args()

    esc, pad = max(1, a.escala), max(0, a.pad)
    glifos = sorted(G.keys())
    celdaW = 5 * esc
    altoMax = max(len(v) for v in G.values())
    celdaH = altoMax * esc
    altoLinea = BASE * esc                    # la base del renglon, en pixeles del atlas

    # ancho del atlas: NPOT a proposito (ver cabecera)
    porFila = 12
    ancho = a.ancho_max if a.ancho_max > 0 else porFila * (celdaW + pad) + pad + 3
    filas = (len(glifos) + porFila - 1) // porFila
    alto = filas * (celdaH + pad) + pad + 3

    pix = bytearray(ancho * alto * 4)          # todo transparente
    meta = {}
    for i, ch in enumerate(glifos):
        col, fil = i % porFila, i // porFila
        gx = pad + col * (celdaW + pad)
        gy = pad + fil * (celdaH + pad)
        filasG = G[ch]
        gh = len(filasG) * esc
        for ry, linea in enumerate(filasG):
            for rx, c in enumerate(linea):
                if c != "#":
                    continue
                for sy in range(esc):
                    for sx in range(esc):
                        o = ((gy + ry * esc + sy) * ancho + (gx + rx * esc + sx)) * 4
                        pix[o:o + 4] = b"\xff\xff\xff\xff"
        # yoff = cuanto BAJA el glifo respecto del tope del renglon. Con la base
        # en `altoLinea`, un glifo de alto gh sin descendente apoya en
        # altoLinea - gh; los que tienen cola bajan por debajo de la base.
        meta[ch] = {"x": gx, "y": gy, "w": celdaW, "h": gh,
                    "xoff": 0, "yoff": altoLinea - BASE * esc, "avance": celdaW + esc}

    raiz = {
        "textura": os.path.basename(a.salida),
        "alto_linea": altoLinea,
        "origen": "build_fuente_bitmap.py (tipografia 5x7 propia del motor)",
        "nota": ("atlas NPOT a proposito: %dx%d. Los glifos con descendente "
                 "(g j p q y ,) declaran su alto real y apoyan por yoff."
                 % (ancho, alto)),
        "glifos": meta,
    }
    if a.mayus_si_falta:
        raiz["mayusculas_si_falta"] = True

    open(a.salida, "wb").write(png_rgba(ancho, alto, pix))
    open(os.path.splitext(a.salida)[0] + ".json", "w").write(json.dumps(raiz, indent=1) + "\n")
    print("%s  %dx%d  %d glifos  alto_linea=%d" % (a.salida, ancho, alto, len(meta), altoLinea))
    return 0


if __name__ == "__main__":
    sys.exit(main())

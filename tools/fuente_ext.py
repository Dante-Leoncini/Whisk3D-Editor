#!/usr/bin/env python3
# ---------------------------------------------------------------------------------------------------------------
#  fuente_ext.py -- el ATLAS EXTENDIDO de la fuente de la interfaz: lo que no entra en font.png.
#
#  font.png (128x128) tiene el ASCII, la n~ y los iconos: no queda lugar. Lo demas va en un segundo atlas chico,
#  res/Skins/Whisk3D/font-ext.png, que el editor carga RECIEN cuando un texto lo necesita (en el N95 la RAM vale):
#    - las MARCAS de los acentos que no se pueden recortar de font.png (grave, circunflejo, dieresis, punto,
#      breve, cedilla, ogonek). El agudo y la tilde siguen saliendo de font.png (ver font.cpp).
#    - letras que no son "letra + marca": ss, i sin punto, l con barra, oe, ae.
#    - signos: ; +- << >> -- ... ^ ~ `
#    - el CIRILICO (ruso). Las que son iguales a una latina (A B E K M H O P C T X / a e o p c y x) NO se dibujan:
#      el editor usa la latina de font.png.
#  Las letras acentuadas NO estan dibujadas: se arman con la letra base y la marca encima (o abajo).
#
#  Este archivo es la UNICA fuente: genera el png y libs/WhiskUI/text/FuenteExt.h (las tablas que lee font.cpp:
#  que celda tiene cada caracter y como se arma cada letra compuesta). tools/genlang.py lo importa para saber que
#  caracteres se pueden mostrar.
#
#  Cada glifo es de 5x11, como los de font.png: mayusculas en las filas 2 a 8, minusculas de la 4 a la 8,
#  descendentes 9 y 10. Las marcas de arriba: en una minuscula, filas 1-2; en una mayuscula, 0-1.
#
#  Uso:  python3 tools/fuente_ext.py            regenera el png y el .h
#        python3 tools/fuente_ext.py --muestra x.png   ademas, una muestra ampliada (para mirar los dibujos)
# ---------------------------------------------------------------------------------------------------------------
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PNG = os.path.join(RAIZ, "res", "Skins", "Whisk3D", "font-ext.png")
CABECERA = os.path.join(RAIZ, "libs", "WhiskUI", "text", "FuenteExt.h")
FONT_CPP = os.path.join(RAIZ, "libs", "WhiskUI", "text", "font.cpp")

ANCHO, ALTO = 128, 64          # potencia de 2 (el N95 no acepta otra cosa)
COLUMNAS = 21                  # celdas de 6x12 (el glifo de 5x11 + 1 px de aire)


def g(*filas):
    """Un glifo: las filas desde la 0. Lo que falta abajo queda vacio."""
    filas = list(filas) + ["....."] * (11 - len(filas))
    assert len(filas) == 11 and all(len(f) == 5 for f in filas), filas
    return filas


V = "....."
# ---- las marcas (celdas fijas: font.cpp las pide por numero) ----------------------------------------------------
MARCAS = [  # (nombre, glifo)
    ("GraveMin",       g(V, ".#...", "..#..")),
    ("GraveMay",       g(".#...", "..#..")),
    ("CircunflejoMin", g(V, "..#..", ".#.#.")),
    ("CircunflejoMay", g("..#..", ".#.#.")),
    ("DieresisMin",    g(V, V, ".#.#.")),
    ("DieresisMay",    g(".#.#.")),
    ("PuntoMin",       g(V, V, "..#..")),
    ("PuntoMay",       g("..#..")),
    ("BreveMin",       g(V, "#...#", ".###.")),
    ("BreveMay",       g("#...#", ".###.")),
    ("Cedilla",        g(V, V, V, V, V, V, V, V, V, "..#..", ".##..")),
    ("Ogonek",         g(V, V, V, V, V, V, V, V, V, "...#.", "....#")),
]

# ---- letras y signos propios ------------------------------------------------------------------------------------
GLIFOS = {
    # latinas que no son letra + marca
    "ß": g(V, V, ".##..", "#..#.", "#..#.", "#.##.", "#...#", "#...#", "#.##."),
    "ı": g(V, V, V, V, ".##..", "..#..", "..#..", "..#..", "#####"),
    "ł": g(V, V, "##...", ".#...", ".#...", ".##..", "##...", ".#...", "..###"),
    "Ł": g(V, V, "#....", "#....", "#.#..", "##...", "#....", "#....", "#####"),
    "œ": g(V, V, V, V, ".#.#.", "#.#.#", "#.###", "#.#..", ".#.##"),
    "Œ": g(V, V, ".####", "#.#..", "#.#..", "#.###", "#.#..", "#.#..", ".####"),
    "æ": g(V, V, V, V, "##.#.", "..#.#", ".####", "#.#..", ".#.##"),
    "Æ": g(V, V, ".####", "#.#..", "#.#..", "#####", "#.#..", "#.#..", "#.###"),
    # signos
    ";": g(V, V, V, V, V, "..#..", V, V, "..#..", ".#..."),
    "±": g(V, V, V, "..#..", "..#..", "#####", "..#..", V, "#####"),
    "«": g(V, V, V, V, V, "..#.#", ".#.#.", "..#.#"),
    "»": g(V, V, V, V, V, "#.#..", ".#.#.", "#.#.."),
    "—": g(V, V, V, V, V, V, "#####"),
    "–": g(V, V, V, V, V, V, ".###."),
    "…": g(V, V, V, V, V, V, V, V, "#.#.#"),
    "^": g(V, V, "..#..", ".#.#.", "#...#"),
    "~": g(V, V, V, V, ".##.#", "#..#."),
    "`": g(V, V, ".#...", "..#.."),
    # cirilico: MAYUSCULAS (filas 2 a 8; Д Ц Щ bajan a la 9)
    "Б": g(V, V, "#####", "#....", "#....", "####.", "#...#", "#...#", "####."),
    "Г": g(V, V, "#####", "#....", "#....", "#....", "#....", "#....", "#...."),
    "Д": g(V, V, ".###.", ".#.#.", ".#.#.", ".#.#.", ".#.#.", ".#.#.", "#####", "#...#"),
    "Ж": g(V, V, "#.#.#", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", "#.#.#"),
    "З": g(V, V, ".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."),
    "И": g(V, V, "#...#", "#...#", "#..##", "#.#.#", "##..#", "#...#", "#...#"),
    "Л": g(V, V, ".####", ".#..#", ".#..#", ".#..#", ".#..#", ".#..#", "#...#"),
    "П": g(V, V, "#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#"),
    "У": g(V, V, "#...#", "#...#", "#...#", ".####", "....#", "#...#", ".###."),
    "Ф": g(V, V, "..#..", ".###.", "#.#.#", "#.#.#", "#.#.#", ".###.", "..#.."),
    "Ц": g(V, V, "#..#.", "#..#.", "#..#.", "#..#.", "#..#.", "#..#.", "#####", "....#"),
    "Ч": g(V, V, "#...#", "#...#", "#...#", ".####", "....#", "....#", "....#"),
    "Ш": g(V, V, "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####"),
    "Щ": g(V, V, "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....#"),
    "Ъ": g(V, V, "##...", ".#...", ".#...", ".###.", ".#..#", ".#..#", ".###."),
    "Ы": g(V, V, "#...#", "#...#", "#...#", "###.#", "#.#.#", "#.#.#", "###.#"),
    "Ь": g(V, V, "#....", "#....", "#....", "####.", "#...#", "#...#", "####."),
    "Э": g(V, V, ".###.", "#...#", "....#", ".####", "....#", "#...#", ".###."),
    "Ю": g(V, V, "#..#.", "#.#.#", "#.#.#", "###.#", "#.#.#", "#.#.#", "#..#."),
    "Я": g(V, V, ".####", "#...#", "#...#", ".####", "..#.#", ".#..#", "#...#"),
    # cirilico: minusculas (filas 4 a 8; б y ф suben, д ц щ ф bajan)
    "б": g(V, V, "..###", ".#...", "#....", "####.", "#...#", "#...#", ".###."),
    "в": g(V, V, V, V, "####.", "#...#", "####.", "#...#", "####."),
    "г": g(V, V, V, V, "#####", "#....", "#....", "#....", "#...."),
    "д": g(V, V, V, V, "..##.", ".#.#.", ".#.#.", ".#.#.", "#####", "#...#"),
    "ж": g(V, V, V, V, "#.#.#", "#.#.#", ".###.", "#.#.#", "#.#.#"),
    "з": g(V, V, V, V, "####.", "....#", "..##.", "....#", "####."),
    "и": g(V, V, V, V, "#...#", "#..##", "#.#.#", "##..#", "#...#"),
    "к": g(V, V, V, V, "#..#.", "#.#..", "##...", "#.#..", "#..#."),
    "л": g(V, V, V, V, ".####", ".#..#", ".#..#", ".#..#", "#...#"),
    "м": g(V, V, V, V, "#...#", "##.##", "#.#.#", "#...#", "#...#"),
    "н": g(V, V, V, V, "#...#", "#...#", "#####", "#...#", "#...#"),
    "п": g(V, V, V, V, "#####", "#...#", "#...#", "#...#", "#...#"),
    "т": g(V, V, V, V, "#####", "..#..", "..#..", "..#..", "..#.."),
    "ф": g(V, V, "..#..", "..#..", ".###.", "#.#.#", "#.#.#", "#.#.#", ".###.", "..#..", "..#.."),
    "ц": g(V, V, V, V, "#..#.", "#..#.", "#..#.", "#..#.", "#####", "....#"),
    "ч": g(V, V, V, V, "#...#", "#...#", ".####", "....#", "....#"),
    "ш": g(V, V, V, V, "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####"),
    "щ": g(V, V, V, V, "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....#"),
    "ъ": g(V, V, V, V, "##...", ".#...", ".###.", ".#..#", ".###."),
    "ы": g(V, V, V, V, "#...#", "#...#", "###.#", "#.#.#", "###.#"),
    "ь": g(V, V, V, V, "#....", "#....", "####.", "#...#", "####."),
    "э": g(V, V, V, V, "####.", "....#", ".####", "....#", "####."),
    "ю": g(V, V, V, V, "#..#.", "#.#.#", "###.#", "#.#.#", "#..#."),
    "я": g(V, V, V, V, ".####", "#...#", ".####", ".#..#", "#...#"),
}

# ---- cirilicas que se dibujan con la latina de font.png --------------------------------------------------------
COMO_LATINA = {"А": "A", "В": "B", "Е": "E", "К": "K", "М": "M", "Н": "H", "О": "O", "Р": "P", "С": "C", "Т": "T",
               "Х": "X", "а": "a", "е": "e", "о": "o", "р": "p", "с": "c", "у": "y", "х": "x"}

# ---- letras compuestas: letra base + marca ----------------------------------------------------------------------
# La marca: Agudo y Tilde salen de font.png (font.cpp); el resto, de este atlas. 'May' = la version para mayuscula.
# Las i con grave, circunflejo o dieresis van sobre la i SIN punto (si no, el punto y la marca se pisan). La i con
# agudo sigue con la i de font.png: es la del castellano y del portugues, y asi esos idiomas no cargan el extendido.
def _compuestas():
    c = {}
    def put(letras, bases, marca):
        for l, b in zip(letras, bases):
            c[l] = (b, marca)
    put("áéíóúćńśźý", "aeioucnszy", "Agudo")     # (la i de font.png, como siempre: asi el castellano no carga el extendido)
    put("ÁÉÍÓÚĆŃŚŹÝ", "AEIOUCNSZY", "Agudo")
    put("ãõñ", "aon", "Tilde")
    put("ÃÕ", "AO", "Tilde")
    put("àèìòù", "aeıou", "Grave")
    put("ÀÈÌÒÙ", "AEIOU", "Grave")
    put("âêîôû", "aeıou", "Circunflejo")
    put("ÂÊÎÔÛ", "AEIOU", "Circunflejo")
    put("äëïöüÿё", "aeıouye", "Dieresis")
    put("ÄËÏÖÜŸЁ", "AEIOUYЕ", "Dieresis")
    put("żİ", "zI", "Punto")
    put("Ż", "Z", "Punto")
    put("ğй", "gи", "Breve")
    put("ĞЙ", "GИ", "Breve")
    put("çşÇŞ", "csCS", "Cedilla")
    put("ąęĄĘ", "aeAE", "Ogonek")
    return c


COMPUESTAS = _compuestas()
COMPUESTAS.pop("ñ", None)      # la n~ esta dibujada en font.png (con su tilde propia)
COMPUESTAS["Ё"] = ("E", "Dieresis")   # (la Е cirilica es la E latina)
COMPUESTAS["ё"] = ("e", "Dieresis")


def glifos_de_font_png():
    """Los caracteres que font.png tiene dibujados (los addGlyph de font.cpp) + los que font.cpp deriva."""
    src = open(FONT_CPP, encoding="utf-8").read()
    out = set(" \\¿")
    for m in re.finditer(r"addGlyph\((0x[0-9A-Fa-f]+|'(?:\\.|[^'])'|\d+),", src):
        k = m.group(1)
        if k.startswith("'"):
            k = k[1:-1].encode().decode("unicode_escape")
        elif k.startswith("0x"):
            k = chr(int(k, 16))
        else:
            k = chr(int(k))
        out.add(k)
    return out


def soportados():
    """Todo lo que la interfaz puede mostrar (para validar las traducciones)."""
    base = glifos_de_font_png()
    s = set(base) | set(GLIFOS) | set(COMO_LATINA)
    for l, (b, _) in COMPUESTAS.items():
        if b in s:
            s.add(l)
    return s


def celdas():
    """[(nombre o caracter, glifo)] en el orden de las celdas: primero las marcas, despues GLIFOS."""
    return [(n, gl) for n, gl in MARCAS] + [(ch, GLIFOS[ch]) for ch in GLIFOS]


def generar(muestra=None):
    from PIL import Image
    cs = celdas()
    assert len(cs) <= COLUMNAS * (ALTO // 12), "no entra: %d celdas" % len(cs)
    im = Image.new("RGBA", (ANCHO, ALTO), (255, 255, 255, 0))
    for i, (_, gl) in enumerate(cs):
        x0, y0 = 1 + (i % COLUMNAS) * 6, (i // COLUMNAS) * 12
        for y, fila in enumerate(gl):
            for x, c in enumerate(fila):
                if c == "#":
                    im.putpixel((x0 + x, y0 + y), (255, 255, 255, 255))
    os.makedirs(os.path.dirname(PNG), exist_ok=True)
    im.save(PNG, optimize=True)

    marcas = [n for n, _ in MARCAS]
    lineas = [
        "#ifndef W3D_FUENTE_EXT_H",
        "#define W3D_FUENTE_EXT_H",
        "// GENERADO por tools/fuente_ext.py -- no editar a mano: se cambia el script y se lo vuelve a correr.",
        "// El atlas EXTENDIDO de la fuente (res/Skins/<skin>/font-ext.png): marcas de acentos, letras y signos que no",
        "// entran en font.png y el cirilico. Ver font.cpp: se carga recien cuando un texto lo necesita.",
        "",
        "#define W3D_FUENTE_EXT_ANCHO %d" % ANCHO,
        "#define W3D_FUENTE_EXT_ALTO %d" % ALTO,
        "#define W3D_FUENTE_EXT_COLUMNAS %d   // celda i: x = 1 + (i %% columnas) * 6, y = (i / columnas) * 12" % COLUMNAS,
        "",
        "// las marcas de los acentos: su celda (el glifo entero de 5x11, con la marca en su fila)",
        "enum W3dFuenteExtMarca {",
    ]
    for i, n in enumerate(marcas):
        lineas.append("    W3dMarca%s = %d," % (n, i))
    lineas += ["    W3dMarcaCantidad = %d" % len(marcas), "};", ""]
    lineas.append("// los caracteres dibujados en este atlas (ordenados por codepoint): su celda")
    lineas.append("struct W3dFuenteExtGlifo { unsigned short cp; unsigned char celda; };")
    lineas.append("static const W3dFuenteExtGlifo W3dFuenteExtGlifos[] = {")
    filas = sorted((ord(ch), i) for i, (ch, _) in enumerate(cs) if i >= len(marcas))
    for cp, i in filas:
        lineas.append("    { 0x%04X, %d },   // %s" % (cp, i, chr(cp)))
    lineas += ["};", "static const unsigned W3dFuenteExtGlifosN = %d;" % len(filas), ""]
    lineas.append("// cirilicas que son IGUALES a una latina: se dibujan con la de font.png")
    lineas.append("struct W3dFuenteExtIgual { unsigned short cp; unsigned short latina; };")
    lineas.append("static const W3dFuenteExtIgual W3dFuenteExtIguales[] = {")
    for ch, lat in sorted(COMO_LATINA.items(), key=lambda p: ord(p[0])):
        lineas.append("    { 0x%04X, '%s' },   // %s" % (ord(ch), lat, ch))
    lineas += ["};", "static const unsigned W3dFuenteExtIgualesN = %d;" % len(COMO_LATINA), ""]
    lineas.append("// letras COMPUESTAS: la letra base + una marca. marca: 0 = agudo y 1 = tilde (de font.png, ver font.cpp);")
    lineas.append("// 2 + n = la celda n de este atlas (ya elegida: la version de la marca para mayuscula o minuscula)")
    lineas.append("struct W3dFuenteExtCompuesta { unsigned short cp; unsigned short base; unsigned char marca; };")
    lineas.append("static const W3dFuenteExtCompuesta W3dFuenteExtCompuestas[] = {")
    def codigo(m, base):
        if m == "Agudo":
            return 0
        if m == "Tilde":
            return 1
        if m in marcas:                              # una sola version (cedilla, ogonek: van abajo)
            return 2 + marcas.index(m)
        return 2 + marcas.index(m + ("May" if base.isupper() else "Min"))
    for ch, (b, m) in sorted(COMPUESTAS.items(), key=lambda p: ord(p[0])):
        lineas.append("    { 0x%04X, 0x%04X, %d },   // %s = %s + %s" % (ord(ch), ord(b), codigo(m, b), ch, b, m))
    lineas += ["};", "static const unsigned W3dFuenteExtCompuestasN = %d;" % len(COMPUESTAS), "", "#endif", ""]
    open(CABECERA, "w", encoding="utf-8").write("\n".join(lineas))

    if muestra:
        ampliada = Image.new("RGBA", (ANCHO, ALTO), (40, 44, 52, 255))
        ampliada.alpha_composite(im)
        ampliada.resize((ANCHO * 6, ALTO * 6), Image.NEAREST).save(muestra)
    print("font-ext.png: %d celdas (%d marcas, %d glifos) | %s" % (len(cs), len(MARCAS), len(GLIFOS), CABECERA))


if __name__ == "__main__":
    generar(sys.argv[sys.argv.index("--muestra") + 1] if "--muestra" in sys.argv else None)

"""El marco del Nokia N95 para las demos del telefono (ver capturas.py, Guion(n95=True)).

n95.svg es una foto del N95 (apaisado, con las teclas a la derecha) con formas encima, cada una con su id
(se editan con Inkscape):
    pantalla                       donde va la interfaz (320x240)
    verde                          la tecla verde de llamada
    soft-izq, soft-der             las teclas de seleccion
    arriba, abajo, izquierda, derecha, ok    el pad
    c, lapiz                       la C y el lapiz
Las formas no se dibujan: marcan donde esta cada tecla. Al apretar una, se resalta esa zona.
Para sumar una tecla: dibujarla en Inkscape (rectangulo, circulo o trazo, sin transformaciones) y ponerle un id.
"""
import base64, io, os, re

from PIL import Image, ImageDraw

AQUI = os.path.dirname(os.path.abspath(__file__))
SVG = os.path.join(AQUI, "n95.svg")

AMPLIAR = 2                   # la pantalla de 320x240 va al doble, sin suavizar (se lee y en pantallas HiDPI queda nitida)
VERDE = (69, 208, 106)        # el de la interfaz

_cache = {}


def _numeros(txt):
    return [float(n) for n in re.findall(r"-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?", txt)]


def _trazo(d, pasos=12):
    """Un 'd' de SVG (M L H V C Z, absolutos o relativos) -> lista de puntos (las curvas, en tramos rectos)."""
    pts, x, y = [], 0.0, 0.0
    for cmd, args in re.findall(r"([MmLlHhVvCcZz])([^MmLlHhVvCcZz]*)", d):
        n = _numeros(args)
        rel = cmd.islower()
        c = cmd.upper()
        if c == "Z":
            continue
        tam = {"M": 2, "L": 2, "H": 1, "V": 1, "C": 6}[c]
        for i in range(0, len(n), tam):
            a = n[i:i + tam]
            if c in "ML":
                x, y = (x + a[0], y + a[1]) if rel else (a[0], a[1])
                pts.append((x, y))
                if c == "M":
                    c = "L"                    # despues de una M, los pares que siguen son L
            elif c == "H":
                x = x + a[0] if rel else a[0]
                pts.append((x, y))
            elif c == "V":
                y = y + a[0] if rel else a[0]
                pts.append((x, y))
            else:
                x0, y0 = x, y
                p = [(x0 + a[0], y0 + a[1]), (x0 + a[2], y0 + a[3]), (x0 + a[4], y0 + a[5])] if rel else \
                    [(a[0], a[1]), (a[2], a[3]), (a[4], a[5])]
                for k in range(1, pasos + 1):
                    t = k / pasos
                    u = 1 - t
                    pts.append((u * u * u * x0 + 3 * u * u * t * p[0][0] + 3 * u * t * t * p[1][0] + t * t * t * p[2][0],
                                u * u * u * y0 + 3 * u * u * t * p[0][1] + 3 * u * t * t * p[1][1] + t * t * t * p[2][1]))
                x, y = p[2]
    return pts


def _atributo(elem, nombre):
    m = re.search(r'\s%s="([^"]*)"' % re.escape(nombre), elem)
    return m.group(1) if m else None


def cargar():
    """La foto (RGBA) y las formas de las teclas, en pixeles de la foto: {id: ('poli', pts) | ('elipse', caja)}."""
    if _cache:
        return _cache["foto"], _cache["formas"]
    s = open(SVG, encoding="utf-8").read()
    m = re.search(r'xlink:href="data:image/png;base64,([^"]+)"', s)
    foto = Image.open(io.BytesIO(base64.b64decode(re.sub(r"&#10;|\s", "", m.group(1))))).convert("RGBA")
    ancho_svg = float(re.search(r'<svg[^>]*\swidth="([\d.]+)"', s).group(1))
    k = foto.width / ancho_svg                     # unidades del svg -> pixeles de la foto
    formas = {}
    for elem in re.findall(r"<(?:rect|circle|ellipse|path)\b[^>]*>", s):
        ident = _atributo(elem, "id")
        if not ident:
            continue
        if elem.startswith("<rect"):
            x, y, w, h = (float(_atributo(elem, a)) for a in ("x", "y", "width", "height"))
            formas[ident] = ("poli", [(x * k, y * k), ((x + w) * k, y * k), ((x + w) * k, (y + h) * k), (x * k, (y + h) * k)])
        elif elem.startswith("<circle") or elem.startswith("<ellipse"):
            cx, cy = float(_atributo(elem, "cx")), float(_atributo(elem, "cy"))
            rx = float(_atributo(elem, "r") or _atributo(elem, "rx"))
            ry = float(_atributo(elem, "r") or _atributo(elem, "ry"))
            formas[ident] = ("elipse", [(cx - rx) * k, (cy - ry) * k, (cx + rx) * k, (cy + ry) * k])
        else:
            formas[ident] = ("poli", [(px * k, py * k) for px, py in _trazo(_atributo(elem, "d"))])
    if "pantalla" not in formas:
        raise SystemExit("n95.svg: falta la forma 'pantalla' (donde va la interfaz)")
    _cache["foto"], _cache["formas"] = foto, formas
    return foto, formas


def marco():
    """La foto escalada para que la pantalla mida 320x240 * AMPLIAR, recortada al telefono. Devuelve
    (foto, escala, (dx, dy)) donde dx/dy es lo que se recorto (para llevar las formas a la foto final)."""
    if "marco" in _cache:
        return _cache["marco"]
    foto, formas = cargar()
    xs = [p[0] for p in formas["pantalla"][1]]
    esc = 320 * AMPLIAR / (max(xs) - min(xs))
    grande = foto.resize((round(foto.width * esc), round(foto.height * esc)), Image.LANCZOS)
    caja = grande.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
    _cache["marco"] = (grande.crop(caja), esc, (caja[0], caja[1]))
    return _cache["marco"]


def componer(ui, teclas=()):
    """La interfaz (320x240) adentro del N95, con las teclas apretadas resaltadas. Devuelve RGBA."""
    foto, formas = cargar()
    fondo, esc, (dx, dy) = marco()
    def llevar(px, py):
        return px * esc - dx, py * esc - dy
    im = fondo.copy()
    xs = [p[0] for p in formas["pantalla"][1]]
    ys = [p[1] for p in formas["pantalla"][1]]
    x0, y0 = llevar(min(xs), min(ys))
    im.paste(ui.convert("RGB").resize((ui.width * AMPLIAR, ui.height * AMPLIAR), Image.NEAREST), (round(x0), round(y0)))
    if teclas:
        capa = Image.new("RGBA", im.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(capa)
        for t in teclas:
            if t not in formas:
                raise SystemExit("n95.svg: no hay una tecla '%s' (hay: %s)" % (t, ", ".join(sorted(formas))))
            tipo, geo = formas[t]
            if tipo == "elipse":
                a, b = llevar(geo[0], geo[1])
                c, e = llevar(geo[2], geo[3])
                # los circulos del pad son chicos: el resaltado un poco mas grande que la marca
                cx, cy, r = (a + c) / 2, (b + e) / 2, (c - a) / 2 * 1.9
                d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=VERDE + (165,), outline=(255, 255, 255, 235), width=4)
            else:
                pts = [llevar(px, py) for px, py in geo]
                d.polygon(pts, fill=VERDE + (165,))
                d.line(pts + [pts[0]], fill=(255, 255, 255, 235), width=4, joint="curve")
        im = Image.alpha_composite(im, capa)
    return im

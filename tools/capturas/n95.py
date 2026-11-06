"""El marco del Nokia N95 para las demos del telefono (ver capturas.py, Guion(n95=...)).

Hay dos fotos, cada una un .svg de Inkscape con formas encima de la foto, cada forma con su id:
    n95.svg            apaisado, con las teclas a la derecha: pantalla de 320x240     (Guion(n95=True))
    n95-vertical.svg   parado, con el teclado numerico abajo:  pantalla de 240x320     (Guion(n95="vertical"))
Los ids:
    pantalla                                 donde va la interfaz
    verde                                    la tecla verde de llamada
    soft-izq, soft-der                       las teclas de seleccion
    arriba, abajo, izquierda, derecha, ok    el pad
    c, lapiz                                 la C y el lapiz
    num0 ... num9, asterisco, numeral        el teclado numerico (solo en el vertical)
Las formas no se dibujan: marcan donde esta cada tecla. Al apretar una, se resalta esa zona.
Para sumar una tecla: dibujarla en Inkscape (rectangulo, circulo o trazo) y ponerle un id. Se respetan las
transformaciones de Inkscape (grupos movidos, rotados o escalados).
"""
import base64, io, math, os, re
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFilter

AQUI = os.path.dirname(os.path.abspath(__file__))

AMPLIAR = 2                   # la pantalla va al doble, sin suavizar (se lee y en pantallas HiDPI queda nitida)
VERDE = (69, 208, 106)        # el de la interfaz

SVG_NS = "{http://www.w3.org/2000/svg}"
XLINK_HREF = "{http://www.w3.org/1999/xlink}href"


# ---- transformaciones de SVG: matrices (a, b, c, d, e, f) ---------------------------------------------------
def _mul(m, n):
    a, b, c, d, e, f = m
    a2, b2, c2, d2, e2, f2 = n
    return (a * a2 + c * b2, b * a2 + d * b2, a * c2 + c * d2, b * c2 + d * d2, a * e2 + c * f2 + e, b * e2 + d * f2 + f)


def _aplicar(m, x, y):
    a, b, c, d, e, f = m
    return a * x + c * y + e, b * x + d * y + f


IDENTIDAD = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)


def _numeros(txt):
    return [float(n) for n in re.findall(r"-?(?:\d+\.?\d*|\.\d+)(?:[eE]-?\d+)?", txt)]


def _transform(txt):
    m = IDENTIDAD
    for nombre, args in re.findall(r"(\w+)\s*\(([^)]*)\)", txt or ""):
        n = _numeros(args)
        if nombre == "matrix":
            t = tuple(n[:6])
        elif nombre == "translate":
            t = (1, 0, 0, 1, n[0], n[1] if len(n) > 1 else 0)
        elif nombre == "scale":
            t = (n[0], 0, 0, n[1] if len(n) > 1 else n[0], 0, 0)
        elif nombre == "rotate":
            r = math.radians(n[0])
            t = (math.cos(r), math.sin(r), -math.sin(r), math.cos(r), 0, 0)
            if len(n) == 3:      # rotate(a, cx, cy)
                t = _mul(_mul((1, 0, 0, 1, n[1], n[2]), t), (1, 0, 0, 1, -n[1], -n[2]))
        else:
            raise SystemExit("n95: transformacion '%s' no soportada" % nombre)
        m = _mul(m, t)
    return m


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


def _elipse(cx, cy, rx, ry, n=40):
    return [(cx + rx * math.cos(2 * math.pi * k / n), cy + ry * math.sin(2 * math.pi * k / n)) for k in range(n)]


def _sin_fondo(foto):
    """Si la foto trae fondo (blanco y opaco, como la del N95 parado), se vuelve transparente: se rellena desde las
    esquinas hasta el borde del telefono. Asi queda bien sobre el tema oscuro del sitio."""
    w, h = foto.size
    esquinas = [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)]
    if all(foto.getpixel(p)[3] < 250 for p in esquinas):
        return foto                                   # ya viene recortada
    rgb = foto.convert("RGB")
    marca = (255, 0, 255)
    for p in esquinas:
        if rgb.getpixel(p) != marca and sum(rgb.getpixel(p)) > 600:
            ImageDraw.floodfill(rgb, p, marca, thresh=60)
    r, g, b = rgb.split()
    # la marca: r = 255, g = 0, b = 255 -> fuera; el resto queda
    fuera = Image.eval(r, lambda v: 255 if v == 255 else 0)
    fuera = Image.composite(fuera, Image.new("L", foto.size, 0), Image.eval(g, lambda v: 255 if v == 0 else 0))
    fuera = Image.composite(fuera, Image.new("L", foto.size, 0), Image.eval(b, lambda v: 255 if v == 255 else 0))
    alfa = Image.eval(fuera, lambda v: 255 - v).filter(ImageFilter.GaussianBlur(0.7))   # borde suave
    res = foto.copy()
    res.putalpha(Image.eval(alfa, lambda v: v) if foto.getchannel("A").getextrema()[0] == 255 else
                 Image.composite(alfa, foto.getchannel("A"), foto.getchannel("A")))
    return res


class Telefono:
    """Una foto del N95 con sus teclas (un .svg de esta carpeta)."""

    def __init__(self, archivo, ancho, alto):
        self.svg = os.path.join(AQUI, archivo)
        self.ancho, self.alto = ancho, alto        # la pantalla, en pixeles de la interfaz
        self._foto = self._marco = None

    def cargar(self):
        """La foto (RGBA) y las formas de las teclas, en pixeles de la foto: {id: ('poli'|'elipse', puntos)}."""
        if self._foto:
            return self._foto, self._formas
        raiz = ET.parse(self.svg).getroot()
        foto, a_foto, formas = None, None, {}

        def recorrer(el, m):
            nonlocal foto, a_foto
            m = _mul(m, _transform(el.get("transform")))
            tag = el.tag.replace(SVG_NS, "")
            ident = el.get("id")
            g = lambda k, v=0.0: float(el.get(k, v))
            if tag == "image" and foto is None:
                datos = el.get(XLINK_HREF) or el.get("href") or ""
                b64 = re.sub(r"\s", "", datos.split("base64,", 1)[1])
                foto = Image.open(io.BytesIO(base64.b64decode(b64))).convert("RGBA")
                # del svg a los pixeles de la foto: deshacer la ubicacion de la imagen y escalar a su resolucion
                x, y, w, h = g("x"), g("y"), g("width"), g("height")
                ox, oy = _aplicar(m, x, y)
                fx, fy = _aplicar(m, x + w, y + h)
                kx, ky = foto.width / (fx - ox), foto.height / (fy - oy)
                a_foto = (kx, 0.0, 0.0, ky, -ox * kx, -oy * ky)
            elif tag in ("rect", "circle", "ellipse", "path") and ident:
                if tag == "rect":
                    x, y, w, h = g("x"), g("y"), g("width"), g("height")
                    pts = [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
                    tipo = "poli"
                elif tag in ("circle", "ellipse"):
                    rx = g("r") if tag == "circle" else g("rx")
                    ry = g("r") if tag == "circle" else g("ry")
                    pts = _elipse(g("cx"), g("cy"), rx, ry)
                    tipo = "elipse"
                else:
                    pts = _trazo(el.get("d", ""))
                    tipo = "poli"
                formas[ident] = (tipo, [_aplicar(m, px, py) for px, py in pts])
            for hijo in el:
                recorrer(hijo, m)

        recorrer(raiz, IDENTIDAD)
        if foto is None:
            raise SystemExit("%s: no tiene la foto del telefono" % self.svg)
        foto = _sin_fondo(foto)
        if "pantalla" not in formas:
            raise SystemExit("%s: falta la forma 'pantalla' (donde va la interfaz)" % self.svg)
        self._foto = foto
        self._formas = {k: (t, [_aplicar(a_foto, x, y) for x, y in p]) for k, (t, p) in formas.items()}
        return self._foto, self._formas

    def _caja(self, ident):
        pts = self.cargar()[1][ident][1]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        return min(xs), min(ys), max(xs), max(ys)

    def marco(self):
        """La foto escalada para que la pantalla mida ancho x alto * AMPLIAR, recortada al telefono. Devuelve
        (foto, escala, (dx, dy)) donde dx/dy es lo que se recorto (para llevar las formas a la foto final)."""
        if self._marco:
            return self._marco
        foto, _ = self.cargar()
        x0, _, x1, _ = self._caja("pantalla")
        esc = self.ancho * AMPLIAR / (x1 - x0)
        grande = foto.resize((round(foto.width * esc), round(foto.height * esc)), Image.LANCZOS)
        caja = grande.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
        self._marco = (grande.crop(caja), esc, (caja[0], caja[1]))
        return self._marco

    def teclas(self):
        return sorted(self.cargar()[1])

    def componer(self, ui, teclas=()):
        """La interfaz (ancho x alto) adentro del N95, con las teclas apretadas resaltadas. Devuelve RGBA."""
        _, formas = self.cargar()
        fondo, esc, (dx, dy) = self.marco()
        def llevar(px, py):
            return px * esc - dx, py * esc - dy
        im = fondo.copy()
        x0, y0, _, _ = self._caja("pantalla")
        x0, y0 = llevar(x0, y0)
        im.paste(ui.convert("RGB").resize((ui.width * AMPLIAR, ui.height * AMPLIAR), Image.NEAREST), (round(x0), round(y0)))
        if teclas:
            capa = Image.new("RGBA", im.size, (0, 0, 0, 0))
            d = ImageDraw.Draw(capa)
            for t in teclas:
                if t not in formas:
                    raise SystemExit("%s: no hay una tecla '%s' (hay: %s)" % (os.path.basename(self.svg), t, ", ".join(self.teclas())))
                tipo, geo = formas[t]
                pts = [llevar(px, py) for px, py in geo]
                if tipo == "elipse":
                    # los circulos del pad son chicos: el resaltado un poco mas grande que la marca
                    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
                    r = (max(xs) - min(xs)) / 2 * 1.9
                    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=VERDE + (165,), outline=(255, 255, 255, 235), width=4)
                else:
                    d.polygon(pts, fill=VERDE + (165,))
                    d.line(pts + [pts[0]], fill=(255, 255, 255, 235), width=4, joint="curve")
            im = Image.alpha_composite(im, capa)
        return im


HORIZONTAL = Telefono("n95.svg", 320, 240)
VERTICAL = Telefono("n95-vertical.svg", 240, 320)

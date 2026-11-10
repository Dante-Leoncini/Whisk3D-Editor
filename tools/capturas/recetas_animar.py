"""Las capturas del tutorial 9, Animar la puerta (docs/manual/<idioma>/tutoriales/animar-puerta.md).

Los pasos y los numeros son los del duenio (puerta-picaporte.w3d y puerta-picaporte-pasos.txt, 2026-11-09): el
picaporte baja del cuadro 1 al 9 (R Y -34), se queda abajo hasta el 26 y vuelve en el 33 (keyframes duplicados); la
puerta se abre recien en el 11, cuando el picaporte ya esta abajo, hasta el 26 (R Z -80); la escena va del 1 al 50. El
picaporte baja y vuelve con la Cuadratica (acelera y termina de golpe: TUK); la puerta, con la de siempre (acelera y
frena)."""
from capturas import receta, Guion, T, VISTA_3D, PorIdioma
import recetas_picaporte as rp
import recetas_pelota as rpel

TIMELINE = 5
TL_VISTA, TL_TIEMPO = 325, 330
TL_CUADRATICA = 404


def R(eje, numero):
    return ["tecla r", "tecla " + eje, "texto " + numero, "tecla Return"]


# las ETAPAS de la animacion, despues de la puerta con su picaporte (la ultima etapa del tutorial 8)
ETAPAS = [
    ("picaporte", ["selobj Manija", "ratonvista3d", "frame 1", "tecla i", "menuelegir Rotation Only", "frame 9"] +
                  R("y", "-34") + ["tecla i", "menuelegir Rotation Only"]),
    # (Shift+D saca la copia un cuadro a la derecha y el numero tipeado se suma: 9 + 1 + 16 = 26; 1 + 1 + 31 = 33)
    ("duplicados", ["tl selframe 9", "tl duplicar", "tl valor 16", "tl aceptar",
                    "tl selframe 1", "tl duplicar", "tl valor 31", "tl aceptar"]),
    ("puerta", ["selobj {P}", "ratonvista3d", "frame 11", "tecla i", "menuelegir Rotation Only", "frame 26"] +
               R("z", "-80") + ["tecla i", "menuelegir Rotation Only"]),
    ("fin", ["tl fin 50"]),
    ("limpias", ["selobj Manija", "tl borrarcurva rot x", "tl borrarcurva rot z",
                 "selobj {P}", "tl borrarcurva rot x", "tl borrarcurva rot y"]),
    ("curvas", ["selobj Manija", "keyinterp cuadratica 1 rot", "keyinterp cuadratica 9 rot",
                "keyinterp cuadratica 26 rot", "keyinterp cuadratica 33 rot"]),
]

VISTA_PICAPORTE = "vporbita -0.31 1.0 0.05 1.6 -12 -22"   # el picaporte de cerca
VISTA_PUERTA = "vporbita 0.2 1.0 0.2 3.6 -14 -28"             # la puerta entera


def guion(idioma, hasta=None, vista=VISTA_PUERTA, raton=True):
    """La puerta con su picaporte (tutorial 8) y la animacion HASTA esa etapa; la Linea de tiempo grande."""
    g = Guion(idioma, w=1440, h=840, raton=raton)
    g.linea("division 0.7")
    g.linea("divisionde 5 0.58")
    p = rp.PUERTA[idioma]
    for nombre, lineas in rp.ETAPAS:
        for l in lineas:
            g.linea(l.replace("{P}", p))
        if nombre == "emparentada":
            break
    if hasta:
        for nombre, lineas in ETAPAS:
            for l in lineas:
                g.linea(l.replace("{P}", p))
            if nombre == hasta:
                break
    g.linea(vista)
    g.linea("cursor3d 0 -30 0")
    g.linea("tl vista -1 51")
    return g


def elegir(g, p):
    """Un click en esa parte de la puerta (sin el gizmo encima: el click tiene que ELEGIR)."""
    g.linea("deselectall")
    g.foto(dur=400)
    rp.clic3d(g, p)


MANIJA = rp.W(-0.6, 0.19, 0)       # el brazo de la manija, del lado de adelante
PUERTA_P = rp.W(-0.75, 0.05, 0.85)  # arriba, lejos del picaporte

PLAY = PorIdioma({"es": "Espacio (Play)", "en": "Space (Play)"})


def reproducir(g, desde, hasta, dur=33, idioma="es"):
    for f in range(desde, hasta + 1):
        g.linea("frame %d" % f)
        g.foto(dur=dur, tecla=PLAY[idioma], puntero=False)


# ---- 1. El picaporte -------------------------------------------------------------------------------------------------
@receta("animar-picaporte-raton")
def picaporte(idioma):
    """Click en la manija; cuadro 1: I > Solo Rotacion; cuadro 9: R Y -34 (baja) e I > Solo Rotacion."""
    g = guion(idioma, vista=VISTA_PICAPORTE)
    g.linea("frame 1")
    elegir(g, MANIJA)
    g.foto(dur=600, tecla=PorIdioma({"es": "cuadro 1", "en": "frame 1"})[idioma])
    rpel.en_vista3d(g, 0.8, 0.75)
    rpel.insertar(g, "Rotation Only")
    rpel.tl_cuadro(g, 9)
    rpel.en_vista3d(g, 0.8, 0.75)
    rp.pasos(g, rp.teclas_numero(["r", "y"], "-34"), fin=500)
    rpel.insertar(g, "Rotation Only")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


# ---- 2. Duplicar keyframes -------------------------------------------------------------------------------------------
@receta("animar-duplicar-raton")
def duplicar(idioma):
    """El keyframe del 9 (abajo) duplicado al 26: el picaporte se queda abajo; el del 1 (arriba) al 33: vuelve. Play."""
    g = guion(idioma, "picaporte", vista=VISTA_PICAPORTE)
    g.foto(dur=700)
    rpel.tl_duplicar(g, 9, 26)
    rpel.tl_duplicar(g, 1, 33)
    reproducir(g, 1, 36, idioma=idioma)
    g.foto(dur=900, puntero=False)
    return g.cuadros(calidad=90)


# ---- 3. La puerta ----------------------------------------------------------------------------------------------------
@receta("animar-puerta-raton")
def puerta(idioma):
    """Click en la puerta; cuadro 11: I > Solo Rotacion (cerrada); cuadro 26: R Z -80 e I > Solo Rotacion. Play: el
    picaporte baja, la puerta se abre y el picaporte vuelve."""
    g = guion(idioma, "duplicados")
    g.linea("tl fin 50")
    elegir(g, PUERTA_P)
    rpel.tl_cuadro(g, 11)
    rpel.en_vista3d(g, 0.85, 0.7)
    rpel.insertar(g, "Rotation Only")
    rpel.tl_cuadro(g, 26)
    rpel.en_vista3d(g, 0.85, 0.7)
    rp.pasos(g, rp.teclas_numero(["r", "z"], "-80"), fin=500)
    rpel.insertar(g, "Rotation Only")
    g.linea("deselectall")
    reproducir(g, 1, 50, idioma=idioma)
    g.foto(dur=1200, puntero=False)
    return g.cuadros(calidad=88)


# ---- 4. El reloj: Fin y FPS ------------------------------------------------------------------------------------------
@receta("animar-reloj")
def reloj(idioma):
    """El reloj de la Linea de tiempo abierto (Cuadro, Inicio, Fin, FPS), con FPS marcado: una foto."""
    g = rp.guion(idioma, "emparentada", vista=rp.VISTA_PUERTA)
    i = g.info()
    x, y = i.boton_rol(5, 330)
    g.raton_mover(x, y)
    g.clic(x, y)
    i = g.info()
    fx, fy = i.tiempo(3)
    y0 = min(i.tiempo(0)[1], y) - 70                 # (el reloj se abre arriba o abajo del boton, segun entre)
    y1 = max(fy, y) + 45
    x0 = max(0, min(x, fx) - 200)
    g.raton = False                                  # (una foto: sin el mouse dibujado en la esquina)
    g.foto(dur=100, marcas=[("circulo", fx, fy, 30)], puntero=False)
    return g.cuadros(recorte=(x0, y0, 460, y1 - y0))


# ---- 5. Las curvas: la demo de las pelotas -----------------------------------------------------------------------------
# cada curva: (su nombre en el harness, el cartel). Al lado de cada pelota, el dibujo de SU curva
CURVAS = [
    ("lineal", PorIdioma({"es": "Lineal: robótico", "en": "Linear: robotic"})),
    ("inout", PorIdioma({"es": "Acelera y frena", "en": "Speeds up and slows down"})),
    ("in", PorIdioma({"es": "Acelera y frena de golpe", "en": "Speeds up, stops sharply"})),
    ("out", PorIdioma({"es": "Sale rápido y se va frenando", "en": "Starts fast, slows down"})),
    ("rebote", PorIdioma({"es": "Rebote", "en": "Bounce"})),
    ("elastica", PorIdioma({"es": "Elástica", "en": "Elastic"})),
]
CUADROS = 40
_PUNTOS = {}


def puntos_curva(idioma, tipo, n=48):
    """La forma de la curva, calculada por el MOTOR (curvapuntos = KfEasing): [(t, v)]."""
    if tipo not in _PUNTOS:
        g = Guion(idioma, w=400, h=300)
        for nombre, _ in CURVAS:
            g.linea("curvapuntos %s %d" % (nombre, n))
        salida = g._correr(g.lineas)
        actual, i = None, 0
        for linea in salida.splitlines():
            if "[curvapuntos]" not in linea:
                continue
            t, v = [float(x) for x in linea.split("]")[1].split()]
            if t == 0.0:
                actual = CURVAS[i][0]
                i += 1
                _PUNTOS[actual] = []
            _PUNTOS[actual].append((t, v))
    return _PUNTOS[tipo]


@receta("animar-curvas")
def curvas(idioma):
    """Seis pelotas de izquierda a derecha en el MISMO tiempo (del cuadro 1 al 40), cada una con su curva y, al lado,
    el dibujo de esa curva con un punto que avanza: lineal (robotica), acelera y frena, acelera y frena de golpe, sale
    rapido y se va frenando, rebote y elastica."""
    g = Guion(idioma, w=1300, h=820, raton=False)
    for n in ("Cube", "Camera", "Light"):
        g.linea("selobj " + T(idioma, n))
        g.linea("delete")
    g.linea("division 0.95")
    for i, (tipo, _) in enumerate(CURVAS):
        y = 7.0 - i * 1.4
        g.linea("cursor3d 0 %g 0" % y)
        g.linea("add sphere")
        g.linea("objname Bola%d" % (i + 1))
        g.linea("objscale 0.4 0.4 0.4")
        g.linea("keyobj 1 0")
        g.linea("keyobj %d 8" % CUADROS)
        g.linea("keyinterp " + tipo)
    g.linea("cursor3d 0 -30 0")
    g.linea("deselectall")
    g.linea("vista3d overlays 0")
    g.linea("vporbita 3.0 3.5 0 12 0 0")
    g.linea("n95 perspectiva")                    # (ortografica: las seis pelotas a la misma escala)
    g.linea("frame 1")
    i = g.info()
    v = i.vp(VISTA_3D)
    alto = (i.objetos["Bola2"]["y"] - i.objetos["Bola1"]["y"])          # lo que separa dos filas en pantalla
    graficos = []
    for k, (tipo, cartel) in enumerate(CURVAS):
        o = i.objetos["Bola%d" % (k + 1)]
        graficos.append((v["x"] + 16, o["y"] - alto // 2 + 6, 300, alto - 12, puntos_curva(idioma, tipo), cartel[idioma]))

    def marcas(t):
        return [("grafico", x, y, w, h, pts, t, tit) for (x, y, w, h, pts, tit) in graficos]

    g.foto(dur=1400, marcas=marcas(0.0))
    for f in range(2, CUADROS + 1):
        g.linea("frame %d" % f)
        g.foto(dur=50, marcas=marcas((f - 1) / float(CUADROS - 1)))
    g.foto(dur=1600, marcas=marcas(1.0))
    return g.cuadros(calidad=90, recorte=(v["x"], v["y"] + v["barra"], v["w"], v["h"] - v["barra"] - 40))


# ---- 6. La barra de herramientas de la Linea de tiempo -----------------------------------------------------------------
@receta("animar-barra-curvas")
def barra(idioma):
    """En el editor de curvas, con un keyframe elegido, el mouse pasa por cada boton de la barra de herramientas de la
    Linea de tiempo y se ve su nombre: las interpolaciones, el tipo de manija y el suavizado."""
    g = guion(idioma, "curvas")                    # (la manija ya en Cuadratica: asi sale tambien el Suavizado)
    g.linea("frame 1")
    g.linea("selobj Manija")
    g.linea("tl curvas")
    g.linea("tl encuadrar")
    g.linea("tl selframe 1")
    i = g.info()
    t = i.vp(TIMELINE)
    botones = [b for b in i.herramientas if b["kind"] == TIMELINE and b.get("nombre")]
    g.foto(dur=900)
    for b in botones:
        cx, cy = b["x"] + b["w"] // 2, b["y"] + b["h"] // 2
        g.raton_mover(cx, cy)
        nombre = T(idioma, b["nombre"])
        g.foto(dur=900, marcas=[("rect", b["x"] - 2, b["y"] - 2, b["w"] + 4, b["h"] + 4),
                                ("etiqueta", min(max(cx, t["x"] + 150), t["x"] + t["w"] - 150), b["y"] - 34, nombre)])
    return g.cuadros(calidad=90, recorte=(t["x"], t["y"], t["w"], t["h"]))


# ---- 7. La curva del picaporte: un click y la barra de herramientas --------------------------------------------------------
@receta("animar-picaporte-curva-raton")
def picaporte_curva(idioma):
    """La manija en el editor de curvas: las filas que no se mueven se borran con X; un click en el tramo que baja y
    Cuadratica en la barra de herramientas; un click en el tramo que vuelve y Cuadratica. Play: el picaporte acelera y
    termina de golpe (TUK), al bajar y al volver."""
    g = guion(idioma, "fin", vista=VISTA_PICAPORTE)
    g.linea("frame 1")
    elegir(g, MANIJA)
    x, y = g.info().boton_rol(TIMELINE, TL_VISTA)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=600)
    rp.menu_abierto(g, "Curves", despues=800)
    g.linea("tl encuadrar")
    g.foto(dur=900)
    for comp in (0, 1):                            # las rotaciones que no cambian: lineas planas
        x, y = g.info().tl_fila(1, comp)
        g.raton_mover(x, y)
        g.foto(dur=450)
        g.clic(x, y)
        g.foto(dur=500)
        rp.pasos(g, [("X", ["tecla x"])], fin=600)
    x, y = g.info().tl_fila(1, 2)                  # la fila que queda: un click y la Linea de tiempo encuadra su curva
    g.raton_mover(x, y)
    g.foto(dur=450)
    g.clic(x, y)
    g.foto(dur=1000)
    for a, b in ((1, 9), (26, 33)):                # el tramo que baja y el que vuelve
        i = g.info()
        xa, ya = i.tl_key_curva(1, 2, a)
        xb, yb = i.tl_key_curva(1, 2, b)
        cx, cy = (xa + xb) // 2, (ya + yb) // 2   # (la mitad del tramo: con la curva de siempre, en la mitad)
        g.raton_mover(cx, cy)
        g.foto(dur=500)
        g.clic(cx, cy)
        g.foto(dur=700)
        bx, by = g.info().herramienta_rol(TIMELINE, TL_CUADRATICA)
        g.raton_mover(bx, by)
        g.foto(dur=600, marcas=[("etiqueta", bx, by - 34, T(idioma, "Quadratic"))])
        g.clic(bx, by)
        g.foto(dur=1100)
    reproducir(g, 1, 36, idioma=idioma)
    g.foto(dur=900, puntero=False)
    return g.cuadros(calidad=88)

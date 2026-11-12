"""Recetas de las capturas (ver capturas.py). Una receta = una imagen de un tutorial o del manual.

Cada receta recibe el idioma de la interfaz ('es', 'en', 'pt') y devuelve los cuadros:
uno = imagen quieta, varios = animacion en bucle. Las posiciones NO se escriben a mano:
se piden con g.info() (el boton de la esquina de cada viewport, las filas del menu
abierto), asi la receta sigue andando aunque cambie el tamanio de la interfaz.
"""
import math

from capturas import (receta, Guion, PorIdioma, MENU_TIPO, VISTA_3D, OUTLINER, PROPIEDADES, EDITOR_UV, LINEA_DE_TIEMPO,
                      EXPANDIR, DIVIDIR_COLUMNAS, DIVIDIR_FILAS, MAXIMIZAR)

# lo que se escribe en el cartel de la tecla (el resto son flechas, que no se traducen)
SOFT_IZQ = PorIdioma({"es": "Soft izquierda", "en": "Left soft key", "pt": "Soft esquerda"})
TECLA_VERDE = PorIdioma({"es": "Tecla verde", "en": "Green key", "pt": "Tecla verde"})


def suave(t):
    """0..1 -> 0..1, arrancando y frenando despacio (para que el puntero no se mueva a saltos)."""
    return t * t * (3 - 2 * t)


def tres_viewports(g):
    """Dos columnas, la de la derecha en dos filas: la Vista 3D se expande y se queda con el lugar de
    la Linea de tiempo (Outliner arriba a la derecha, Propiedades abajo a la derecha)."""
    i = g.info()
    g.tocar(*i.esquina(VISTA_3D))
    i = g.info()
    g.tocar(*i.item(EXPANDIR))
    g.sin_puntero()


def accion_del_menu(g, kind, opcion, final_ms=1900):
    """La esquina del viewport -> el menu -> una opcion -> el resultado (4 cuadros)."""
    i = g.info()
    x, y = i.esquina(kind)
    g.dedo_flotando(x, y)
    g.foto(dur=700)
    g.tocar(x, y)
    g.foto(dur=550)
    i = g.info()
    ix, iy = i.item(opcion)
    g.dedo_flotando(ix, iy)
    g.foto(dur=800)
    g.tocar(ix, iy)
    g.sin_puntero()
    g.foto(dur=final_ms)


# ---- tutorial: los viewports ------------------------------------------------------
@receta("inicio")
def inicio(idioma):
    """Whisk3D recien abierto, sin tocar nada: lo que se ve la primera vez que inicia."""
    g = Guion(idioma)
    g.foto()
    return g.cuadros()


@receta("viewports-activo")
def activo(idioma):
    """El viewport activo: el puntero pasa por los cuatro viewports y el borde verde lo sigue (el foco es el
    viewport que esta debajo del mouse). Vuelve al punto de partida: el bucle no salta."""
    g = Guion(idioma)
    i = g.info()
    def punto(kind, fx, fy):
        v = i.vp(kind)
        alto = v["h"] - v["barra"]
        return round(v["x"] + v["w"] * fx), round(v["y"] + v["barra"] + alto * fy)
    paradas = [punto(VISTA_3D, 0.22, 0.72), punto(OUTLINER, 0.55, 0.62), punto(PROPIEDADES, 0.5, 0.55),
               punto(LINEA_DE_TIEMPO, 0.62, 0.8)]
    paradas.append(paradas[0])
    g.raton_mover(*paradas[0])
    g.foto(dur=1000)
    for (xa, ya), (xb, yb) in zip(paradas, paradas[1:]):
        n = 9
        for k in range(1, n + 1):
            t = suave(k / n)
            g.raton_mover(round(xa + (xb - xa) * t), round(ya + (yb - ya) * t))
            g.foto(dur=45)
        g.foto(dur=1000)
    return g.cuadros()


@receta("viewports-activo-nokia")
def activo_nokia(idioma):
    """En el N95: cada toque de la tecla verde pasa el foco (el borde verde) al viewport siguiente. El cambio
    pasa al SOLTAR la tecla, como en el telefono."""
    g = Guion(idioma, n95=True)
    for _ in range(2):                          # 3D -> Propiedades -> 3D: termina como empezo
        g.foto(dur=1100)
        g.foto(dur=320, teclas=("verde",), tecla=TECLA_VERDE[idioma])
        g.verde()
    return g.cuadros()


@receta("viewports-tamano-nokia")
def tamano_nokia(idioma):
    """En el N95: la tecla verde mantenida + las flechas mueven el borde del viewport activo (la Vista 3D):
    a la derecha se agranda, a la izquierda se achica, y vuelve al 55 % del principio."""
    g = Guion(idioma, n95=True)
    verde = TECLA_VERDE[idioma]
    g.foto(dur=900)
    g.foto(dur=300, teclas=("verde",), tecla=verde)
    for dx, veces in ((1, 5), (-1, 10), (1, 5)):    # 55 % -> 75 % -> 35 % -> 55 %
        flecha = "derecha" if dx > 0 else "izquierda"
        cartel = "%s + %s" % (verde, "→" if dx > 0 else "←")
        g.foto(dur=120, teclas=("verde", flecha), tecla=cartel)   # la flecha apretada, todavia sin moverse
        for _ in range(veces):
            g.verde_flecha(dx, 0, 4)            # 4 cuadros del telefono por foto: 0.04 por paso
            g.foto(dur=110, teclas=("verde", flecha), tecla=cartel)
        g.foto(dur=350, teclas=("verde",), tecla=verde)
    # cada cuadro redibuja toda la pantalla: con un poco menos de calidad pesa bastante menos
    return g.cuadros(calidad=70)


@receta("viewports-barras")
def barras(idioma):
    """Cada viewport con su barra de menus (verde) y el boton de su esquina (naranja)."""
    g = Guion(idioma)
    i = g.info()
    marcas = [("rect", v["x"] + 3, v["y"] + 2, v["w"] - 6, v["barra"]) for v in i.vps]
    for v in i.vps:
        x, y, w, h = v["esquina"]
        marcas.append(("circulo", x + w // 2, y + h // 2, 23))
    g.foto(marcas=marcas)
    return g.cuadros()


@receta("viewports-menu")
def menu(idioma):
    """Las opciones del viewport: el dedo va hasta el boton de la esquina de la Vista 3D, lo aprieta y se
    despliega el menu, que queda abierto un rato antes de volver a empezar."""
    g = Guion(idioma)
    i = g.info()
    x, y = i.esquina(VISTA_3D)
    x0, y0 = x + 230, y + 170                   # arranca sobre la Vista 3D
    g.dedo_flotando(x0, y0)
    g.foto(dur=700)
    n = 10
    for k in range(1, n + 1):                   # va hasta el boton (frenando al llegar)
        t = 1 - (1 - k / n) ** 2
        g.dedo_flotando(round(x0 + (x - x0) * t), round(y0 + (y - y0) * t))
        g.foto(dur=55)
    g.foto(dur=350)
    g.dedo_abajo(x, y)                          # lo aprieta...
    g.foto(dur=300)
    g.dedo_arriba()                             # ...y al soltar se despliega el menu
    g.foto(dur=500)
    g.sin_puntero()
    g.foto(dur=3200)                            # abierto un rato; despues el bucle arranca de nuevo
    return g.cuadros()


@receta("viewports-arrastrar")
def arrastrar(idioma):
    """El dedo agarra la esquina del viewport de abajo a la derecha y la lleva en circulos: los tres
    viewports cambian de tamanio. Bucle sin salto: el circulo termina donde empezo."""
    g = Guion(idioma)
    tres_viewports(g)
    i = g.info()
    x0, y0 = i.esquina(PROPIEDADES)
    radio, n = 100, 24                          # 24 cuadros de 100 ms: liviano y se sigue viendo fluido
    a0 = math.radians(45)                       # el arranque, abajo a la derecha del centro del circulo
    cx, cy = x0 - radio * math.cos(a0), y0 - radio * math.sin(a0)
    g.dedo_abajo(x0, y0)
    for vuelta in (0, 1):                       # la primera vuelta no se fotografia (pasa el umbral del gesto)
        for k in range(1, n + 1):
            a = a0 + 2 * math.pi * k / n
            g.dedo_mover(round(cx + radio * math.cos(a)), round(cy + radio * math.sin(a)))
            if vuelta == 1:
                g.foto(dur=100)
    g.dedo_arriba()
    # cambia TODA la pantalla en cada cuadro (la Vista 3D se redibuja): achicada y con perdida pesa ~370 KB
    return g.cuadros(ancho=720, calidad=62)


@receta("viewports-dividir-columnas")
def dividir_columnas(idioma):
    g = Guion(idioma)
    accion_del_menu(g, VISTA_3D, DIVIDIR_COLUMNAS)
    return g.cuadros()


@receta("viewports-dividir-filas")
def dividir_filas(idioma):
    g = Guion(idioma)
    accion_del_menu(g, VISTA_3D, DIVIDIR_FILAS)
    return g.cuadros()


@receta("viewports-expandir")
def expandir(idioma):
    """El Outliner se expande y se queda con el lugar de Propiedades."""
    g = Guion(idioma)
    accion_del_menu(g, OUTLINER, EXPANDIR)
    return g.cuadros()


@receta("viewports-maximizar")
def maximizar(idioma):
    """Maximizar la Vista 3D y despues Minimizar (la misma opcion) para volver."""
    g = Guion(idioma)
    accion_del_menu(g, VISTA_3D, MAXIMIZAR, final_ms=1300)
    accion_del_menu(g, VISTA_3D, MAXIMIZAR, final_ms=1300)
    return g.cuadros()


@receta("viewports-cambiar-tipo")
def cambiar_tipo(idioma):
    """La Vista 3D pasa a ser un Editor UV."""
    g = Guion(idioma)
    accion_del_menu(g, VISTA_3D, MENU_TIPO(EDITOR_UV))
    return g.cuadros()


@receta("viewports-barra-dedo")
def barra_dedo(idioma):
    """La barra del Outliner no entra entera: se arrastra con el dedo (o con el mouse apretado).
    Ventana mas angosta que las demas: la columna de la derecha queda chica y la barra se pasa bastante."""
    g = Guion(idioma, w=720)
    i = g.info()
    v = i.vp(OUTLINER)
    x0, y = v["x"] + v["w"] - 50, v["y"] + v["barra"] // 2
    g.dedo_abajo(x0, y)
    g.esperar(220)                              # (el tactil espera un instante antes de decidir el gesto)
    ida = [x0 - 9 * k for k in range(1, 25)]
    for vuelta in (0, 1):                       # la primera, sin fotos: pasa el umbral y deja el scroll en 0
        for x in ida + ida[-2::-1] + [x0]:
            g.dedo_mover(x, y)
            if vuelta == 1:
                g.foto(dur=55)
    g.dedo_arriba()
    return g.cuadros()


@receta("viewports-barra-rueda")
def barra_rueda(idioma):
    """La rueda del mouse sobre la barra del Outliner la desplaza (ventana angosta, como la del dedo)."""
    g = Guion(idioma, w=720)
    i = g.info()
    v = i.vp(OUTLINER)
    x, y = v["x"] + v["w"] // 2, v["y"] + v["barra"] // 2
    g.raton_mover(x, y)
    g.foto(dur=700)
    for pasos in [-1] * 6 + [1] * 6:
        g.rueda(pasos, x, y)
        g.foto(dur=280)
    return g.cuadros()


@receta("viewports-barra-nokia")
def barra_nokia(idioma):
    """En el N95: la soft izquierda entra a la barra y las flechas la recorren (la barra se desplaza sola).
    En la PC es la tecla M. Adentro del telefono, con la tecla que se aprieta resaltada."""
    g = Guion(idioma, n95=True)                 # el foco arranca en la Vista 3D
    # cada tecla: apretada (la pantalla todavia igual) -> la pantalla reacciona -> soltada. Asi en cada cuadro
    # cambia o la pantalla o las teclas, no las dos: la animacion pesa mucho menos
    def apretar(tecla, cual, cartel, dur):
        g.foto(dur=110, teclas=(cual,), tecla=cartel)
        g.tecla(tecla)
        g.foto(dur=250, teclas=(cual,), tecla=cartel)
        g.foto(dur=dur)
    g.foto(dur=900)
    apretar("m", "soft-izq", SOFT_IZQ[idioma], 750)
    for flecha, tecla, simbolo, veces in (("derecha", "Right", "→", 7), ("izquierda", "Left", "←", 3)):
        for _ in range(veces):
            apretar(tecla, flecha, simbolo, 330)
    g.tecla("Escape")
    return g.cuadros(calidad=75)


# ---- tutorial: navegar en la Vista 3D ---------------------------------------------------------------------
# Tres maneras de hacer lo mismo: con el mouse en la PC (se dibuja el mouse con el boton apretado), con los dedos
# en una pantalla tactil y con el teclado del N95 (parado, con el teclado numerico a la vista). Las de la PC y las
# tactiles muestran solo la Vista 3D.
FLECHA = {"derecha": "→", "izquierda": "←", "arriba": "↑", "abajo": "↓"}
MOVIDA = {"derecha": (1, 0), "izquierda": (-1, 0), "arriba": (0, -1), "abajo": (0, 1)}

# la escena con muchas cosas (vista local, vistas, perspectiva): (primitiva, nombre, posicion, tamanio)
NOMBRES = PorIdioma({"es": {"Caja": "Caja", "Esfera": "Esfera", "Cilindro": "Cilindro", "Cono": "Cono"},
           "en": {"Caja": "Box", "Esfera": "Sphere", "Cilindro": "Cylinder", "Cono": "Cone"},
           "pt": {"Caja": "Caixa", "Esfera": "Esfera", "Cilindro": "Cilindro", "Cono": "Cone"}})
LLENA = [("cube", "Caja", (3.5, -2.5), 0.7), ("uvsphere", "Esfera", (-3, -1.5), 0.9), ("cylinder", "Cilindro", (2.5, 2.5), 0.6),
         ("cone", "Cono", (0.2, -3.2), 0.8), ("cube", "Caja", (-2.5, 2.8), 0.5), ("uvsphere", "Esfera", (4.5, 0.8), 0.5),
         ("cone", "Cono", (-4.5, -3.5), 0.6), ("cylinder", "Cilindro", (-1, -5.5), 0.7), ("cube", "Caja", (1.8, -5.8), 0.6),
         ("uvsphere", "Esfera", (-5, 1), 0.6), ("cube", "Caja", (5.5, -4.5), 0.8)]


def escena_llena(g, idioma):
    """Once cosas alrededor del cubo, apoyadas en el piso. Devuelve el nombre del cono del medio (el que se elige)."""
    cuenta = {}
    elegido = None
    for prim, base, (x, z), e in LLENA:
        cuenta[base] = cuenta.get(base, 0) + 1
        nombre = "%s%d" % (NOMBRES[idioma][base], cuenta[base])
        g.linea("add %s" % prim)
        g.linea("objname %s" % nombre)
        g.linea("objpos %g %g %g" % (x, e, z))
        g.linea("objscale %g %g %g" % (e, e, e))
        if base == "Cono" and elegido is None:
            elegido = nombre
    return elegido


LEJOS_LLENA = "vporbita 0 0.5 -1.5 15"     # la escena llena entera a la vista


def vista_pc(g):
    """La Vista 3D de la PC: el recorte para mostrar solo ese viewport y el centro de su area de dibujo."""
    v = g.info().vp(VISTA_3D)
    alto = v["h"] - v["barra"]
    return (v["x"], v["y"], v["w"], v["h"]), (v["x"] + v["w"] // 2, v["y"] + v["barra"] + alto // 2)


def con_tecla(g, tecla, cartel, antes=1300, despues=1900, teclas=()):
    """La tecla: la vista quieta, el cartel (todavia sin cambio), la tecla, el resultado con el cartel y sin el."""
    g.foto(dur=antes)
    g.foto(dur=350, tecla=cartel, teclas=teclas)
    tecla()
    g.foto(dur=450, tecla=cartel, teclas=teclas)
    g.foto(dur=despues)


# -- orbitar --
@receta("navegacion-orbitar-raton")
def orbitar_raton(idioma):
    """El boton del medio (apretar la rueda) mantenido y arrastrar: la vista gira alrededor de la escena. El
    recorrido va y vuelve por el mismo camino (girar es distinto segun el orden: volviendo por donde se fue, la vista
    termina igual que empezo y el bucle no salta)."""
    g = Guion(idioma, raton=True)
    rec, (cx, cy) = vista_pc(g)
    x0, y0 = cx + 30, cy + 30
    g.raton_mover(x0, y0)
    g.foto(dur=700)
    g.raton_abajo(x0, y0, "medio")
    g.foto(dur=300)
    n = 30
    for k in range(1, n + 1):
        a = 2 * math.pi * k / n
        g.raton_mover(x0 + round(150 * math.sin(a)), y0 - round(45 * math.sin(a)))
        g.foto(dur=60)
    # el editor aplica el delta de cada movimiento en el siguiente: uno mas en el mismo lugar cierra el recorrido
    g.raton_mover(x0, y0)
    g.foto(dur=250)
    g.raton_arriba("medio")
    g.foto(dur=600)
    return g.cuadros(recorte=rec, calidad=68)


@receta("navegacion-orbitar-dedo")
def orbitar_dedo(idioma):
    """Un dedo arrastrado sobre la Vista 3D: orbita. El mismo ida y vuelta que con el mouse, pero desde el fondo:
    apoyado sobre el cubo, el dedo agarraria el gizmo y lo moveria."""
    g = Guion(idioma)
    rec, (cx, cy) = vista_pc(g)
    x0, y0 = cx - 150, cy + 110
    g.dedo_abajo(x0, y0)
    g.esperar(220)                              # (el tactil espera un instante antes de decidir el gesto)
    g.dedo_mover(x0 + 24, y0)                   # pasa el umbral del arrastre (sin foto) y vuelve
    g.dedo_mover(x0, y0)
    g.foto(dur=600)
    n = 30
    for k in range(1, n + 1):
        a = 2 * math.pi * k / n
        g.dedo_mover(x0 + round(150 * math.sin(a)), y0 - round(45 * math.sin(a)))
        g.foto(dur=60)
    # el editor aplica el delta de cada movimiento en el siguiente: uno mas en el mismo lugar cierra el recorrido
    g.dedo_mover(x0, y0)
    g.foto(dur=400)
    g.dedo_arriba()
    return g.cuadros(recorte=rec, calidad=68)


def flechas_n95(g, pasos, accion, modificador=None, cartel_mod=None, cuadros=3, dur=90):
    """N95: cada flecha mantenida un rato (la accion corre unos cuadros por foto), con la tecla resaltada."""
    for flecha, veces in pasos:
        dx, dy = MOVIDA[flecha]
        teclas = ((modificador,) if modificador else ()) + (flecha,)
        cartel = ("%s + %s" % (cartel_mod, FLECHA[flecha])) if cartel_mod else FLECHA[flecha]
        g.foto(dur=150, teclas=teclas, tecla=cartel)        # apretada, todavia sin moverse
        for _ in range(veces):
            if accion == "zoom":
                g.keypad("zoom", dy, cuadros)
            else:
                g.keypad(accion, dx, dy, cuadros)
            g.foto(dur=dur, teclas=teclas, tecla=cartel)
        if modificador:
            g.foto(dur=250, teclas=(modificador,), tecla=cartel_mod)
        g.foto(dur=450)


@receta("navegacion-orbitar-nokia")
def orbitar_nokia(idioma):
    """N95: las flechas mantenidas orbitan (derecha, izquierda, arriba, abajo: vuelve a donde empezo)."""
    g = Guion(idioma, n95="vertical")
    g.foto(dur=800)
    flechas_n95(g, (("derecha", 8), ("izquierda", 8), ("arriba", 4), ("abajo", 4)), "orbitar")
    return g.cuadros(calidad=66)


# -- desplazar --
@receta("navegacion-panear-raton")
def panear_raton(idioma):
    """Shift + el boton del medio, arrastrando: la vista se desplaza de costado (en circulo, vuelve al inicio)."""
    g = Guion(idioma, raton=True)
    rec, (cx, cy) = vista_pc(g)
    x0, y0 = cx + 60, cy + 30
    g.raton_mover(x0, y0)
    g.foto(dur=600)
    g.mod("Shift")
    g.foto(dur=350)
    g.raton_abajo(x0, y0, "medio")
    g.foto(dur=250)
    n, r = 26, 85
    for k in range(1, n + 1):
        a = 2 * math.pi * k / n
        g.raton_mover(x0 - r + round(r * math.cos(a)), y0 + round(r * math.sin(a)))
        g.foto(dur=60)
    # el editor aplica el delta de cada movimiento en el siguiente: uno mas en el mismo lugar cierra el recorrido
    g.raton_mover(x0, y0)
    g.foto(dur=250)
    g.raton_arriba("medio")
    g.mod()
    g.foto(dur=500)
    return g.cuadros(recorte=rec, calidad=68)


@receta("navegacion-panear-dedo")
def panear_dedo(idioma):
    """Dos dedos que se mueven juntos: la vista se desplaza (en circulo, vuelve al inicio)."""
    g = Guion(idioma)
    rec, (cx, cy) = vista_pc(g)
    sep = 70                                    # medio espacio entre los dos dedos
    g.dedo_abajo(cx - sep, cy, 1)
    g.dedo_abajo(cx + sep, cy, 2)
    g.foto(dur=600)
    n, r = 26, 80
    for k in range(1, n + 1):
        a = 2 * math.pi * k / n
        dx, dy = round(r * math.cos(a)) - r, round(r * math.sin(a))
        g.dedo_mover(cx - sep + dx, cy + dy, 1)
        g.dedo_mover(cx + sep + dx, cy + dy, 2)
        g.foto(dur=60)
    g.foto(dur=400)
    g.dedo_arriba(2)
    g.dedo_arriba(1)
    return g.cuadros(recorte=rec, calidad=68)


@receta("navegacion-panear-nokia")
def panear_nokia(idioma):
    """N95: la tecla * mantenida + las flechas desplazan la vista."""
    g = Guion(idioma, n95="vertical")
    g.foto(dur=800)
    flechas_n95(g, (("derecha", 6), ("izquierda", 6), ("arriba", 4), ("abajo", 4)), "panear",
                modificador="asterisco", cartel_mod="*")
    return g.cuadros(calidad=66)


# -- zoom --
@receta("navegacion-zoom-raton")
def zoom_raton(idioma):
    """La rueda del mouse: para arriba acerca, para abajo aleja (vuelve a donde empezo)."""
    g = Guion(idioma, raton=True)
    rec, (cx, cy) = vista_pc(g)
    g.raton_mover(cx + 30, cy + 20)
    g.foto(dur=700)
    for pasos in (1, -1):
        for _ in range(6):
            g.rueda(pasos, cx + 30, cy + 20)
            g.foto(dur=220)
        g.foto(dur=600)
    return g.cuadros(recorte=rec, calidad=68)


@receta("navegacion-zoom-dedo")
def zoom_dedo(idioma):
    """Pellizcar: separar los dos dedos acerca, juntarlos aleja."""
    g = Guion(idioma)
    rec, (cx, cy) = vista_pc(g)
    cerca, lejos, n = 50, 190, 12
    g.dedo_abajo(cx - cerca, cy + 20, 1)
    g.dedo_abajo(cx + cerca, cy + 20, 2)
    g.foto(dur=600)
    for ida in (True, False):
        for k in range(1, n + 1):
            t = suave(k / n)
            s = round(cerca + (lejos - cerca) * (t if ida else 1 - t))
            g.dedo_mover(cx - s, cy + 20, 1)
            g.dedo_mover(cx + s, cy + 20, 2)
            g.foto(dur=70)
        g.foto(dur=600)
    g.dedo_arriba(2)
    g.dedo_arriba(1)
    return g.cuadros(recorte=rec, calidad=68)


@receta("navegacion-zoom-nokia")
def zoom_nokia(idioma):
    """N95: el 0 mantenido + arriba acerca, + abajo aleja."""
    g = Guion(idioma, n95="vertical")
    g.foto(dur=800)
    flechas_n95(g, (("arriba", 8), ("abajo", 8)), "zoom", modificador="num0", cartel_mod="0", cuadros=2)
    return g.cuadros(calidad=66)


# -- encuadrar --
LEJOS = "vporbita 9 2 -7 42"       # la vista lejos y corrida: el cubo queda chiquito en una punta


@receta("navegacion-encuadrar")
def encuadrar(idioma):
    """Num . (el punto del teclado numerico): la vista se centra en lo seleccionado (el cubo) y lo encuadra."""
    g = Guion(idioma)
    rec, _ = vista_pc(g)
    g.linea(LEJOS)
    con_tecla(g, lambda: g.tecla("Keypad_."), "Num .")
    return g.cuadros(recorte=rec)


@receta("navegacion-encuadrar-nokia")
def encuadrar_nokia(idioma):
    """N95: el 0 solo (un toque, sin flechas) encuadra lo seleccionado."""
    g = Guion(idioma, n95="vertical")
    g.linea(LEJOS)
    con_tecla(g, lambda: g.keypad("encuadrar"), "0", teclas=("num0",))
    return g.cuadros(calidad=80)


@receta("navegacion-encuadrar-edicion")
def encuadrar_edicion(idioma):
    """En el Modo Edicion se encuadra lo seleccionado de la malla: unas caras de una esfera llenan la vista."""
    g = Guion(idioma)
    rec, _ = vista_pc(g)
    g.linea("delete")                           # (el cubo)
    g.linea("add uvsphere")
    g.linea("objscale 2 2 2")
    g.linea("mode edit")
    # un parche de seis caras que mira a la camara (cara = anillo * 16 + segmento)
    g.linea("caras " + " ".join(str(16 * anillo + seg) for anillo in (3, 4) for seg in (2, 3, 4)))
    con_tecla(g, lambda: g.tecla("Keypad_."), "Num .")
    return g.cuadros(recorte=rec)


# -- vista local --
@receta("navegacion-vista-local")
def vista_local(idioma):
    """/ : con muchas cosas en la escena, queda solo lo seleccionado (el cono del medio) y se encuadra. Otra vez
    / y vuelve todo, como estaba."""
    g = Guion(idioma)
    rec, _ = vista_pc(g)
    cono = escena_llena(g, idioma)
    g.linea("selobj %s" % cono)
    con_tecla(g, lambda: g.tecla("/"), "/", antes=1500, despues=1800)
    g.foto(dur=350, tecla="/")
    g.tecla("/")
    g.foto(dur=450, tecla="/")
    return g.cuadros(recorte=rec)


# -- las vistas --
VISTAS_PC = (("Keypad_1", None, "Num 1"), ("Keypad_3", None, "Num 3"), ("Keypad_7", None, "Num 7"),
             ("Keypad_1", "Left_Ctrl", "Ctrl + Num 1"), ("Keypad_3", "Left_Ctrl", "Ctrl + Num 3"),
             ("Keypad_7", "Left_Ctrl", "Ctrl + Num 7"))


@receta("navegacion-vistas")
def vistas(idioma):
    """El teclado numerico: frente (1), derecha (3), arriba (7) y, con Ctrl, las opuestas."""
    g = Guion(idioma)
    rec, _ = vista_pc(g)
    escena_llena(g, idioma)
    g.linea("deselectall")
    g.linea(LEJOS_LLENA)
    g.foto(dur=1200)
    for tecla, ctrl, cartel in VISTAS_PC:
        g.foto(dur=300, tecla=cartel)
        if ctrl:
            g.tecla(ctrl + " down")
        g.tecla(tecla)
        if ctrl:
            g.tecla(ctrl + " up")
        g.foto(dur=1100, tecla=cartel)
    return g.cuadros(recorte=rec)


@receta("navegacion-vistas-nokia")
def vistas_nokia(idioma):
    """N95: el # mantenido + una flecha: izquierda y derecha giran de a 90 grados, arriba va a la vista de arriba
    y abajo vuelve (y otra vez abajo, a la de abajo)."""
    g = Guion(idioma, n95="vertical")
    escena_llena(g, idioma)
    g.linea("deselectall")
    g.linea(LEJOS_LLENA)
    g.foto(dur=1000)
    for flecha in ("derecha", "derecha", "derecha", "derecha", "arriba", "abajo", "abajo", "arriba"):
        cartel = "# + %s" % FLECHA[flecha]
        g.foto(dur=250, teclas=("numeral", flecha), tecla=cartel)
        g.keypad("vista", *MOVIDA[flecha])
        g.foto(dur=900, teclas=("numeral",), tecla="#")
    g.foto(dur=600)
    return g.cuadros(calidad=66)


# -- perspectiva y ortografica --
PERSPECTIVA = "vporbita 0 0.8 -4.5 15 -14 30"     # un poco de arriba y de costado, mirando a lo largo de las filas


def escena_filas(g, idioma):
    """Dos filas de cajas iguales que se alejan (sin el cubo): en perspectiva las del fondo se ven chicas, en
    ortografica todas del mismo tamanio."""
    g.linea("delete")
    n = 0
    for x in (-2.2, 2.2):
        for z in (3, 0, -3, -6, -9, -12):
            n += 1
            g.linea("add cube")
            g.linea("objname %s%d" % (NOMBRES[idioma]["Caja"], n))
            g.linea("objpos %g 0.8 %g" % (x, z))
            g.linea("objscale 0.6 0.8 0.6")
    g.linea("deselectall")


@receta("navegacion-perspectiva")
def perspectiva(idioma):
    """Num 5: perspectiva <-> ortografica, sobre dos filas de cajas iguales (en ortografica lo lejano no se achica)."""
    g = Guion(idioma)
    rec, _ = vista_pc(g)
    escena_filas(g, idioma)
    g.linea(PERSPECTIVA)
    con_tecla(g, lambda: g.tecla("Keypad_5"), "Num 5", antes=1500, despues=1800)
    g.foto(dur=350, tecla="Num 5")
    g.tecla("Keypad_5")
    g.foto(dur=450, tecla="Num 5")
    return g.cuadros(recorte=rec)


@receta("navegacion-perspectiva-nokia")
def perspectiva_nokia(idioma):
    """N95: el # solo (un toque, sin flechas) cambia entre perspectiva y ortografica."""
    g = Guion(idioma, n95="vertical")
    escena_filas(g, idioma)
    g.linea(PERSPECTIVA)
    con_tecla(g, lambda: g.keypad("perspectiva"), "#", antes=1400, despues=1700, teclas=("numeral",))
    g.foto(dur=350, tecla="#", teclas=("numeral",))
    g.keypad("perspectiva")
    g.foto(dur=450, tecla="#", teclas=("numeral",))
    return g.cuadros(calidad=80)


# -- la camara activa --
@receta("navegacion-camara")
def camara(idioma):
    """Num 0: mirar por la camara activa (lo que va a salir en el juego y en los renders) y volver."""
    g = Guion(idioma)
    rec, _ = vista_pc(g)
    con_tecla(g, lambda: g.tecla("Keypad_0"), "Num 0", antes=1300, despues=2200)
    g.foto(dur=350, tecla="Num 0")
    g.tecla("Keypad_0")
    g.foto(dur=450, tecla="Num 0")
    return g.cuadros(recorte=rec)


@receta("navegacion-camara-nokia")
def camara_nokia(idioma):
    """N95: el 8 (fuera del Modo Edicion) mira por la camara activa; otra vez, vuelve."""
    g = Guion(idioma, n95="vertical")
    con_tecla(g, lambda: g.keypad("camara"), "8", antes=1200, despues=2200, teclas=("num8",))
    g.foto(dur=350, tecla="8", teclas=("num8",))
    g.keypad("camara")
    g.foto(dur=450, tecla="8", teclas=("num8",))
    return g.cuadros(calidad=80)


# -- bloquear la orbita: el plano de una casa, de arriba y en ortografica --
BLOQUEAR_ORBITA, CAMARA_A_LA_VISTA = 421, 423                   # sus opciones (ids de LayoutAccionView)
NOMBRES_PLANO = PorIdioma({"es": ("Pared", "Mueble"), "en": ("Wall", "Furniture"), "pt": ("Parede", "Movel")})
PAREDES = [(0, 3.5, 5.1, 0.1), (0, -3.5, 5.1, 0.1), (-5, 0, 0.1, 3.5), (5, 0, 0.1, 3.5),   # (x, z, medio ancho, medio largo)
           (1, -1.6, 0.1, 1.9), (-3.1, 0.8, 1.9, 0.1), (3.6, 0.8, 1.4, 0.1)]
MUEBLES = [("cube", -3.3, -2.0, (0.9, 0.25, 1.2)), ("cylinder", 3.0, -1.6, (0.7, 0.3, 0.7)),
           ("cube", -2.6, 2.4, (1.3, 0.3, 0.45)), ("cube", 3.6, 2.6, (0.5, 0.3, 0.5)), ("cylinder", -0.6, -2.6, (0.4, 0.3, 0.4))]


def escena_plano(g, idioma):
    """El plano de una casa (paredes finas y algunos muebles), sin el cubo, visto de arriba en ortografica."""
    pared, mueble = NOMBRES_PLANO[idioma]
    g.linea("delete")
    for k, (x, z, ax, az) in enumerate(PAREDES, 1):
        g.linea("add cube")
        g.linea("objname %s%d" % (pared, k))
        g.linea("objpos %g 0.4 %g" % (x, z))
        g.linea("objscale %g 0.4 %g" % (ax, az))
    for k, (prim, x, z, (sx, sy, sz)) in enumerate(MUEBLES, 1):
        g.linea("add %s" % prim)
        g.linea("objname %s%d" % (mueble, k))
        g.linea("objpos %g %g %g" % (x, sy, z))
        g.linea("objscale %g %g %g" % (sx, sy, sz))
    g.linea("deselectall")
    g.linea("vporbita 0 0 0 13")
    g.tecla("Keypad_7")                 # de arriba...
    g.tecla("Keypad_5")                 # ...y en ortografica


def ir_con_el_raton(g, x0, y0, x1, y1, pasos=8, dur=45):
    """El puntero va de un lugar a otro (frenando al llegar), con fotos."""
    for k in range(1, pasos + 1):
        t = suave(k / pasos)
        g.raton_mover(round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t))
        g.foto(dur=dur)


def circulo(n, r, x0, y0):
    """Los puntos de un circulo que arranca y termina en (x0, y0) (centro a la izquierda)."""
    return [(x0 - r + round(r * math.cos(2 * math.pi * k / n)), y0 + round(r * math.sin(2 * math.pi * k / n)))
            for k in range(1, n + 1)]


def maximizar_3d(g, raton=False):
    """La Vista 3D a toda la ventana (sin fotos): los menus de su barra entran enteros."""
    i = g.info()
    x, y = i.esquina(VISTA_3D)
    g.clic(x, y) if raton else g.tocar(x, y)
    i = g.info()
    x, y = i.item(MAXIMIZAR)
    g.clic(x, y) if raton else g.tocar(x, y)
    g.sin_puntero()


def al_item(g, item):
    """Donde esta una opcion del menu abierto. El puntero SALTA ahi: cualquier recorrido pasa por arriba de otra
    cosa que reacciona (Camaras o Punto de Vista abren su submenu; otro boton de la barra, su menu)."""
    return g.info().item(item)


@receta("navegacion-bloquear-orbita-raton")
def bloquear_orbita_raton(idioma):
    """Menu Vista -> Bloquear Orbita; despues el boton del medio ya no gira el plano: lo desplaza."""
    g = Guion(idioma, raton=True)
    escena_plano(g, idioma)
    maximizar_3d(g, raton=True)
    rec, (cx, cy) = vista_pc(g)
    vx, vy = g.info().boton_vista
    x0, y0 = cx + 120, cy + 80
    g.raton_mover(x0, y0)
    g.foto(dur=900)
    ir_con_el_raton(g, x0, y0, vx, vy)
    g.clic(vx, vy)
    g.foto(dur=600)
    ix, iy = al_item(g, BLOQUEAR_ORBITA)
    g.raton_mover(ix, iy)
    g.foto(dur=600)
    g.clic(ix, iy)
    g.foto(dur=700)
    ir_con_el_raton(g, ix, iy, cx, cy, pasos=6)
    g.raton_abajo(cx, cy, "medio")
    g.foto(dur=250)
    for x, y in circulo(26, 90, cx, cy):
        g.raton_mover(x, y)
        g.foto(dur=60)
    g.raton_mover(cx, cy)               # (cierra el recorrido: el delta va en el movimiento siguiente)
    g.raton_arriba("medio")
    g.foto(dur=900)
    return g.cuadros(recorte=rec, calidad=72)


@receta("navegacion-bloquear-orbita-dedo")
def bloquear_orbita_dedo(idioma):
    """Menu Vista -> Bloquear Orbita con el dedo; despues arrastrar desplaza el plano en vez de girarlo."""
    g = Guion(idioma)
    escena_plano(g, idioma)
    maximizar_3d(g)
    rec, (cx, cy) = vista_pc(g)
    vx, vy = g.info().boton_vista
    g.dedo_flotando(vx, vy)
    g.foto(dur=900)
    g.tocar(vx, vy)
    g.foto(dur=600)
    ix, iy = al_item(g, BLOQUEAR_ORBITA)
    g.dedo_flotando(ix, iy)
    g.foto(dur=600)
    g.tocar(ix, iy)
    g.sin_puntero()
    g.foto(dur=700)
    x0, y0 = cx + 60, cy + 60
    g.dedo_abajo(x0, y0)
    g.esperar(220)
    g.foto(dur=300)
    for x, y in circulo(26, 90, x0, y0):
        g.dedo_mover(x, y)
        g.foto(dur=60)
    g.dedo_mover(x0, y0)
    g.dedo_arriba()
    g.foto(dur=900)
    return g.cuadros(recorte=rec, calidad=72)


@receta("navegacion-bloquear-orbita-nokia")
def bloquear_orbita_nokia(idioma):
    """N95: el 9 bloquea la orbita; despues las flechas desplazan el plano en vez de girarlo."""
    g = Guion(idioma, n95="vertical")
    escena_plano(g, idioma)
    g.foto(dur=900)
    g.foto(dur=300, teclas=("num9",), tecla="9")
    g.keypad("bloquearorbita")
    g.foto(dur=900, teclas=("num9",), tecla="9")
    g.foto(dur=500)
    flechas_n95(g, (("derecha", 5), ("abajo", 4), ("izquierda", 5), ("arriba", 4)), "orbitar")
    return g.cuadros(calidad=70)


# -- la camara bloqueada a la vista: navegar mueve la camara activa, para acomodar la toma --
@receta("navegacion-camara-bloquear-raton")
def camara_bloquear_raton(idioma):
    """Num 0 (mirar por la camara) -> click en el candado -> orbitar y la rueda mueven la CAMARA: el cubo, que
    estaba cortado en una punta, queda encuadrado en el medio."""
    g = Guion(idioma, raton=True)
    rec, (cx, cy) = vista_pc(g)
    x0, y0 = cx + 120, cy + 70
    g.raton_mover(x0, y0)
    g.foto(dur=900)
    g.foto(dur=350, tecla="Num 0")
    g.tecla("Keypad_0")
    g.foto(dur=900, tecla="Num 0")
    lx, ly = g.info().centro_candado()
    ir_con_el_raton(g, x0, y0, lx, ly)
    g.foto(dur=300)
    g.clic(lx, ly)
    g.foto(dur=800)
    ir_con_el_raton(g, lx, ly, cx, cy, pasos=6)
    g.raton_abajo(cx, cy, "medio")
    g.foto(dur=200)
    for k in range(1, 11):              # la gira hasta que el cubo queda en el medio...
        g.raton_mover(cx - 7 * k, cy)
        g.foto(dur=60)
    g.raton_mover(cx - 70, cy)
    g.raton_arriba("medio")
    g.foto(dur=400)
    for _ in range(4):                  # ...y la aleja para que entre entero
        g.rueda(-1, cx - 70, cy)
        g.foto(dur=260)
    g.foto(dur=1900)
    return g.cuadros(recorte=rec, calidad=72)


@receta("navegacion-camara-bloquear-dedo")
def camara_bloquear_dedo(idioma):
    """Mirando por la camara: un toque en el candado, y arrastrar mueve la CAMARA hasta encuadrar el cubo."""
    g = Guion(idioma)
    rec, (cx, cy) = vista_pc(g)
    g.keypad("camara")                  # (arranca mirando por la camara)
    lx, ly = g.info().centro_candado()
    g.foto(dur=900)
    g.dedo_flotando(lx, ly)
    g.foto(dur=700)
    g.tocar(lx, ly)
    g.foto(dur=800)
    x0, y0 = cx + 60, cy + 30
    g.dedo_abajo(x0, y0)
    g.esperar(220)
    g.foto(dur=250)
    for k in range(1, 11):
        g.dedo_mover(x0 - 7 * k, y0)
        g.foto(dur=60)
    g.dedo_arriba()
    g.sin_puntero()
    g.foto(dur=1900)
    return g.cuadros(recorte=rec, calidad=72)


@receta("navegacion-camara-bloquear-nokia")
def camara_bloquear_nokia(idioma):
    """N95: el 8 mira por la camara; la soft izquierda y el menu Vista -> Mover la camara con la vista; las flechas
    y el 0 mueven la CAMARA hasta encuadrar el cubo."""
    g = Guion(idioma, n95="vertical")
    def apretar(accion, tecla, cual, cartel, dur):
        g.foto(dur=110, teclas=(cual,), tecla=cartel)
        accion() if accion else g.tecla(tecla)
        g.foto(dur=200, teclas=(cual,), tecla=cartel)
        g.foto(dur=dur)
    g.foto(dur=800)
    apretar(lambda: g.keypad("camara"), None, "num8", "8", 800)
    apretar(None, "m", "soft-izq", SOFT_IZQ[idioma], 400)
    for _ in range(5):                  # por la barra hasta el menu Vista
        apretar(None, "Right", "derecha", "→", 160)
    for _ in range(7):                  # bajando hasta "Mover la camara con la vista" (el menu abre sin fila marcada)
        apretar(None, "Down", "abajo", "↓", 160)
    g.foto(dur=500)
    apretar(None, "Return", "ok", "OK", 800)
    flechas_n95(g, (("derecha", 4),), "orbitar")
    flechas_n95(g, (("abajo", 4),), "zoom", modificador="num0", cartel_mod="0", cuadros=2)
    g.foto(dur=1500)
    return g.cuadros(calidad=64)


# el tutorial 3 (El Modo Objeto) tiene sus recetas aparte
import recetas_objeto  # noqa: E402,F401
import recetas_viewports  # noqa: E402,F401  (los articulos de Los viewports)
import recetas_picaporte  # noqa: E402,F401  (el tutorial 8: Modelar el picaporte)
import recetas_animar  # noqa: E402,F401  (el tutorial 9: Animar la puerta)
import recetas_pelota  # noqa: E402,F401  (el tutorial 10: Hacer picar una pelota)
import recetas_script  # noqa: E402,F401  (el tutorial 11: Tu primer script)

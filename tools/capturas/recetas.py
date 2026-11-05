"""Recetas de las capturas (ver capturas.py). Una receta = una imagen de un tutorial o del manual.

Cada receta recibe el idioma de la interfaz ('es', 'en', 'pt') y devuelve los cuadros:
uno = imagen quieta, varios = animacion en bucle. Las posiciones NO se escriben a mano:
se piden con g.info() (el boton de la esquina de cada viewport, las filas del menu
abierto), asi la receta sigue andando aunque cambie el tamanio de la interfaz.
"""
import math

from capturas import (receta, Guion, MENU_TIPO, VISTA_3D, OUTLINER, PROPIEDADES, EDITOR_UV, LINEA_DE_TIEMPO,
                      EXPANDIR, DIVIDIR_COLUMNAS, DIVIDIR_FILAS, MAXIMIZAR)

# lo que se escribe en el cartel de la tecla (el resto son flechas, que no se traducen)
SOFT_IZQ = {"es": "Soft izquierda", "en": "Left soft key", "pt": "Soft esquerda"}
TECLA_VERDE = {"es": "Tecla verde", "en": "Green key", "pt": "Tecla verde"}


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

"""Recetas de las capturas (ver capturas.py). Una receta = una imagen de un tutorial o del manual.

Cada receta recibe el idioma de la interfaz ('es', 'en', 'pt') y devuelve los cuadros:
uno = imagen quieta, varios = animacion en bucle. Las posiciones NO se escriben a mano:
se piden con g.info() (el boton de la esquina de cada viewport, las filas del menu
abierto), asi la receta sigue andando aunque cambie el tamanio de la interfaz.
"""
import math

from capturas import (receta, Guion, MENU_TIPO, VISTA_3D, OUTLINER, PROPIEDADES, EDITOR_UV,
                      EXPANDIR, DIVIDIR_COLUMNAS, DIVIDIR_FILAS, MAXIMIZAR)

# lo que se escribe en el cartel de la tecla (el resto son flechas, que no se traducen)
SOFT_IZQ = {"es": "Soft izquierda", "en": "Left soft key", "pt": "Soft esquerda"}


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
    """El menu del boton de la esquina, abierto."""
    g = Guion(idioma)
    i = g.info()
    g.tocar(*i.esquina(VISTA_3D))
    g.foto()
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
    En la PC es la tecla M. Escala 1, como en el telefono, y ampliada al doble para que se lea."""
    g = Guion(idioma, w=320, h=240, escala=1, ampliar=2)
    g.raton_mover(110, 110)                     # (el foco en la Vista 3D; el puntero no se dibuja)
    g.sin_puntero()
    g.foto(dur=900)
    g.tecla("m")
    g.foto(dur=1100, tecla=SOFT_IZQ[idioma])
    for _ in range(7):
        g.tecla("Right")
        g.foto(dur=650, tecla="→")
    for _ in range(3):
        g.tecla("Left")
        g.foto(dur=550, tecla="←")
    g.tecla("Escape")
    return g.cuadros()

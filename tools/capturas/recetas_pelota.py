"""Las capturas del tutorial 10, Hacer picar una pelota (docs/manual/<idioma>/tutoriales/pelota.md).

Los pasos y los numeros son los del duenio (pelota.w3d y pelota-pasos.txt, 2026-11-10): aplastada en el cuadro 2,
estirada en el 4, redonda otra vez en el 17; en el piso en el 3 y en el 29, arriba (Z 5) en el 16; de 1 a 28 para que
el 29 sea el 1 y el loop no repita un cuadro; la subida frena al llegar (cubica, Al llegar) y la caida acelera
(cubica, Al arrancar). Comprobado con 'keyver': el tutorial deja los MISMOS keyframes."""
from capturas import receta, Guion, T, VISTA_3D, OUTLINER, PorIdioma
from recetas_objeto import menu, tocar_en, tocar_menu, n95_comando, TB_EJE
import recetas_picaporte as rp

TIMELINE = 5
TL_TIEMPO, TL_VISTA = 330, 325                    # el reloj (Inicio / Fin / FPS) y el menu Vista de la Linea de tiempo
TL_CUBICA, TL_AL_ARRANCAR, TL_AL_LLEGAR = 405, 441, 442   # la barra de herramientas de la Linea de tiempo
BR_SUPERPOS = 7


def SHIFT(t):
    return ["tecla Left_Shift down", "tecla " + t, "tecla Left_Shift up"]


def N(t, numero):
    """Una tecla + un numero + Enter (S 0.75, G 5)."""
    return ["tecla " + c for c in t] + ["texto " + numero, "tecla Return"]


# las ETAPAS, en orden: cada una deja la escena como queda despues de esa parte del tutorial
ETAPAS = [
    ("esfera", ["selobj {Cube}", "delete", "cursor3d 0 1 0", "add sphere", "cursor3d 0 0 0"]),
    ("cursor", ["ratonvista3d", "tecla Tab", "elegirvertice abajo"] + SHIFT("s") +
               ["menuelegir Cursor to Selected", "tecla Tab"]),
    ("vacio", ["agregar vacio"]),
    ("emparentada", ["parent {Esfera} {Vacio}", "parent {Vacio} {Coleccion}", "selobj {Vacio}"]),
    ("aplastada", ["ratonvista3d", "frame 1", "tecla i", "menuelegir Scale Only", "frame 2"] + N("sz", "0.75") +
                  ["tecla s"] + SHIFT("z") + ["texto 1.6", "tecla Return", "tecla i", "menuelegir Scale Only"]),
    # (Shift+D saca la copia UN cuadro a la derecha; el numero tipeado se suma: 1 -> 1 + 1 + 2 = 4)
    ("redonda", ["tl selframe 1", "tl duplicar", "tl valor 2", "tl aceptar",
                 "tl selframe 1", "tl duplicar", "tl valor 15", "tl aceptar",
                 "tl selframe 1", "tl duplicar", "tl valor 27", "tl aceptar"]),
    ("estirada", ["frame 4", "ratonvista3d"] + N("sz", "1.1") + ["tecla s"] + SHIFT("z") +
                 ["texto 0.9", "tecla Return", "tecla i", "menuelegir Scale Only"]),
    ("salto", ["frame 3", "tecla i", "menuelegir Location Only", "frame 16"] + N("gz", "5") +
              ["tecla i", "menuelegir Location Only", "tl selframe 3", "tl duplicar", "tl valor 25", "tl aceptar"]),
    ("fin", ["tl fin 28"]),
    ("limpia", ["tl borrarcurva loc x", "tl borrarcurva loc y"]),
    ("curvas", ["keyinterp out 3 loc", "keyinterp in 16 loc"]),
]

VISTA_CERCA = "vporbita 0 1.0 0 6.5 -14 -30"      # la pelota de cerca
VISTA_SALTO = "vporbita 0 3.0 0 15 -8 -30"        # el salto entero (hasta Z 5 y un poco mas)


def nombres(idioma):
    return {"Cube": T(idioma, "Cube"), "Esfera": T(idioma, "UVSphere"), "Vacio": T(idioma, "Empty"),
            "Coleccion": T(idioma, "Collection")}


def armar(g, hasta):
    n = nombres(g.idioma)
    for nombre, lineas in ETAPAS:
        for l in lineas:
            g.linea(l.format(**n))
        if nombre == hasta:
            break


def guion(idioma, hasta, vista=VISTA_CERCA, raton=True, tl=(-1, 31)):
    """El editor grande: la Vista 3D arriba, la Linea de tiempo abajo (grande: es un tutorial de animacion) y al
    costado el Outliner y Propiedades; la escena armada HASTA esa etapa (incluida)."""
    g = Guion(idioma, w=1440, h=840, raton=raton)
    g.linea("division 0.7")
    g.linea("divisionde 5 0.58")
    armar(g, hasta)
    g.linea(vista)
    g.linea("cursor3d 0 -30 0")
    g.linea("tl vista %g %g" % tl)
    return g


# ---- la Linea de tiempo con el mouse ----------------------------------------------------------------------------------
def tl_cuadro(g, f, despues=700):
    """Un click en la barra de numeros de la Linea de tiempo, sobre el cuadro f: el cuadro actual pasa a ser ese."""
    i = g.info()
    x, y = i.tl_x(f), i.tlpanel["numeros"]
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=despues, tecla=PorIdioma({"es": "cuadro %d", "en": "frame %d"})[g.idioma] % f)


def tl_duplicar(g, desde, hasta, fila=0):
    """Click en el keyframe del cuadro 'desde' (en la fila del Resumen), Shift + D y la copia al cuadro 'hasta' (sale
    un cuadro a la derecha del mouse: se apunta al cuadro de antes), click."""
    i = g.info()
    x, y = i.tl_key(fila, desde)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=500)
    g.linea("tecla Left_Shift down"); g.linea("tecla d"); g.linea("tecla Left_Shift up")
    g.foto(dur=500, tecla="Shift + D")
    x2 = i.tl_x(hasta - 1)
    g.raton_mover(x + 1, y)                       # (el editor descarta el primer movimiento despues de Shift+D: se
    g.raton_mover(x, y)                           # pierden estos dos pixeles y no un pedazo del arrastre)
    pasos = 8
    for k in range(1, pasos + 1):
        g.raton_mover(int(x + (x2 - x) * k / pasos), y)
        g.foto(dur=45, tecla="Shift + D")
    g.raton_mover(x2 + 1, y)                      # (el ultimo paso se aplica con el evento que sigue)
    g.raton_mover(x2, y)
    g.foto(dur=350, tecla="Shift + D")
    g.clic(x2, y)
    g.foto(dur=700)


def en_vista3d(g, fx=0.72, fy=0.62):
    """El mouse sobre la Vista 3D, al costado de la pelota (las teclas van al viewport que tiene el mouse). Por la
    medida de la vista, no con rp.pantalla: esa usa el cursor 3D y lo deja lejos (el Vacio naceria ahi)."""
    v = g.info().vp(VISTA_3D)
    x, y = v["x"] + int(v["w"] * fx), v["y"] + int(v["h"] * fy)
    g.raton_mover(x, y)
    g.foto(dur=300)


def insertar(g, opcion_en):
    """I y la opcion del menu Insertar Keyframe (Solo Escala / Solo Localizacion)."""
    rp.pasos(g, [("I", ["tecla i"])], fin=300)
    rp.menu_abierto(g, opcion_en, despues=900)


SHIFT_Z = ("Shift + Z", SHIFT("z"))


def reproducir(g, vueltas=1, dur=33, desde=1, hasta=28, cartel=None):
    for _ in range(vueltas):
        for f in range(desde, hasta + 1):
            g.linea("frame %d" % f)
            g.foto(dur=dur, tecla=cartel, puntero=False)


# ---- 1. El Vacio en el piso ------------------------------------------------------------------------------------------
@receta("pelota-vacio-raton")
def vacio(idioma):
    """La esfera apoyada en el piso: Tab, Rayos X, el vertice de abajo, Shift+S > Cursor a lo seleccionado, Tab;
    Shift+A > Vacio (nace en el cursor); en el Outliner, la esfera y Ctrl + click en el Vacio; Ctrl+P > Objeto
    (Mantener Transform.)."""
    g = guion(idioma, "esfera")
    g.linea("selmode vertex")
    g.foto(dur=700)
    en_vista3d(g)
    rp.pasos(g, [("Tab", ["tecla Tab"])], fin=400)
    g.linea("select none")
    menu(g, BR_SUPERPOS, ["X-Ray"])
    g.tecla("Escape")
    g.foto(dur=700)
    rp.senalar3d(g, (0, 0, 0), "elegirvertice abajo", despues=900)
    rp.pasos(g, [("Shift + S", SHIFT("s"))], fin=300)
    rp.menu_abierto(g, "Cursor to Selected", despues=900)
    rp.pasos(g, [("Tab", ["tecla Tab"])], fin=400)
    menu(g, BR_SUPERPOS, ["X-Ray"])
    g.tecla("Escape")
    en_vista3d(g)
    rp.pasos(g, [("Shift + A", SHIFT("a"))], fin=300)
    rp.menu_abierto(g, "Empty", despues=1100)
    n = nombres(idioma)
    x, y = g.info().fila(n["Esfera"])
    g.raton_mover(x, y)
    g.foto(dur=500)
    g.clic(x, y)
    g.foto(dur=700)
    x, y = g.info().fila(n["Vacio"])
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.mod("Ctrl")
    g.clic(x, y)
    g.mod()
    g.foto(dur=900, tecla="Ctrl + click")
    en_vista3d(g)
    rp.pasos(g, [("Ctrl + P", ["tecla Left_Ctrl down", "tecla p", "tecla Left_Ctrl up"])], fin=300)
    rp.menu_abierto(g, "Object (Keep Transform)", despues=1800)
    return g.cuadros(calidad=90)


# ---- 2. Por que el Vacio -----------------------------------------------------------------------------------------------
@receta("pelota-por-que-raton")
def por_que(idioma):
    """S Z 0.5 a la esfera sola (se achica hacia su centro: queda flotando) y al Vacio (se aplasta contra el piso)."""
    g = guion(idioma, "emparentada")
    n = nombres(idioma)
    v = g.info().vp(VISTA_3D)
    arriba = v["y"] + v["barra"] + 30
    sola = PorIdioma({"es": "La esfera sola: se achica hacia su centro", "en": "The sphere alone: it shrinks toward its center"})
    con = PorIdioma({"es": "El Vacío: se aplasta contra el piso", "en": "The Empty: it squashes against the floor"})
    for nombre, cartel in ((n["Esfera"], sola), (n["Vacio"], con)):
        x, y = g.info().fila(nombre)
        g.raton_mover(x, y)
        g.foto(dur=400)
        g.clic(x, y)
        g.foto(dur=700)
        en_vista3d(g)
        marca = [("etiqueta", v["x"] + v["w"] // 2, arriba, cartel[idioma])]
        hechas = []
        for rot, lin in [("S", ["tecla s"]), ("Z", ["tecla z"]), ("0.5", ["texto 0.5"])]:
            hechas.append(rot)
            g.foto(dur=350, tecla=",  ".join(hechas))
            for l in lin:
                g.linea(l)
            g.foto(dur=600, tecla=",  ".join(hechas))
        g.foto(dur=2200, tecla=",  ".join(hechas), marcas=marca)
        g.tecla("Escape")
        g.foto(dur=800, tecla="Esc")
    return g.cuadros(calidad=90)


# ---- 3. Aplastar -------------------------------------------------------------------------------------------------------
@receta("pelota-aplastar-raton")
def aplastar(idioma):
    """Cuadro 1: I > Solo Escala; la flecha a la derecha (cuadro 2): S Z 0.75, S Shift+Z 1.6 e I > Solo Escala."""
    g = guion(idioma, "emparentada")
    g.linea("frame 1")
    en_vista3d(g)
    g.foto(dur=700, tecla=PorIdioma({"es": "cuadro 1", "en": "frame 1"})[idioma])
    insertar(g, "Scale Only")
    rp.pasos(g, [("→", ["tecla Right"])], fin=500)
    rp.pasos(g, rp.teclas_numero(["s", "z"], "0.75"), fin=500)
    rp.pasos(g, [("S", ["tecla s"]), SHIFT_Z, ("1.6", ["texto 1.6"]), ("Enter", ["tecla Return"])], fin=500)
    insertar(g, "Scale Only")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


# ---- 4. Redonda otra vez y estirada --------------------------------------------------------------------------------------
@receta("pelota-estirar-raton")
def estirar(idioma):
    """El keyframe del cuadro 1 (la pelota redonda) duplicado al 4, al 17 y al 29; en el 4: S Z 1.1, S Shift+Z 0.9 e
    I > Solo Escala."""
    g = guion(idioma, "aplastada")
    g.foto(dur=700)
    for f in (4, 17, 29):
        tl_duplicar(g, 1, f)
    tl_cuadro(g, 4)
    en_vista3d(g)
    rp.pasos(g, rp.teclas_numero(["s", "z"], "1.1"), fin=500)
    rp.pasos(g, [("S", ["tecla s"]), SHIFT_Z, ("0.9", ["texto 0.9"]), ("Enter", ["tecla Return"])], fin=500)
    insertar(g, "Scale Only")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


# ---- 5. El salto ---------------------------------------------------------------------------------------------------------
@receta("pelota-saltar-raton")
def saltar(idioma):
    """Cuadro 3: I > Solo Localizacion; cuadro 16: G Z 5 e I > Solo Localizacion; el keyframe del 3 duplicado al 29;
    Play: con la curva de siempre, la pelota frena antes de tocar el piso (flota)."""
    g = guion(idioma, "estirada", vista=VISTA_SALTO)
    g.foto(dur=600)
    tl_cuadro(g, 3)
    en_vista3d(g, 0.78, 0.7)
    insertar(g, "Location Only")
    tl_cuadro(g, 16)
    en_vista3d(g, 0.78, 0.7)
    rp.pasos(g, rp.teclas_numero(["g", "z"], "5"), fin=500)
    insertar(g, "Location Only")
    tl_duplicar(g, 3, 29)
    g.linea("tl fin 28")                          # (el Fin, en el video que sigue: aca, que el Play de la vuelta)
    reproducir(g, vueltas=2, cartel=PorIdioma({"es": "Espacio (Play)", "en": "Space (Play)"})[idioma])
    return g.cuadros(calidad=88)


# ---- 6. El Fin: el loop ---------------------------------------------------------------------------------------------------
@receta("pelota-fin-raton")
def fin(idioma):
    """El reloj de la Linea de tiempo: Fin 28 (el 29 es igual al 1: el loop no repite un cuadro)."""
    g = guion(idioma, "salto", vista=VISTA_SALTO)
    g.foto(dur=600)
    x, y = g.info().boton_rol(TIMELINE, TL_TIEMPO)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=800)
    x, y = g.info().tiempo(2)
    g.raton_mover(x, y)
    g.foto(dur=500)
    g.clic(x, y)
    g.foto(dur=500)
    g.linea("tecla Left_Ctrl down"); g.linea("tecla a"); g.linea("tecla Left_Ctrl up")
    rp.pasos(g, [("28", ["texto 28"]), ("Enter", ["tecla Return"])], fin=900)
    g.tecla("Escape")
    g.foto(dur=1500)
    return g.cuadros(calidad=90)


# ---- 7. Las curvas ---------------------------------------------------------------------------------------------------------
@receta("pelota-curvas-raton")
def curvas(idioma):
    """Las filas X e Y Localizacion (lineas planas): X las borra. Vista > Curvas: el keyframe del 3 (la subida) Cubica +
    Al llegar; el del 16 (la caida) Cubica + Al arrancar. Play."""
    g = guion(idioma, "fin", vista=VISTA_SALTO)
    # la Linea de tiempo mas ancha que en las otras capturas: con la barra de los suavizados a la vista (Cubica + los
    # cuatro de Al arrancar / Al llegar) no entra en 0.7 de 1440 y 'Al llegar' queda afuera de la pantalla
    g.linea("division 0.78")
    g.foto(dur=700)
    for comp in (0, 2):                            # X Localizacion; Y Localizacion (interno: Z)
        x, y = g.info().tl_fila(0, comp)
        g.raton_mover(x, y)
        g.foto(dur=450)
        g.clic(x, y)
        g.foto(dur=600)
        rp.pasos(g, [("X", ["tecla x"])], fin=600)
    x, y = g.info().boton_rol(TIMELINE, TL_VISTA)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=600)
    rp.menu_abierto(g, "Curves", despues=900)
    g.linea("tl vista -1 31 -0.6 5.6")
    g.foto(dur=1200)
    for f, ease in ((3, TL_AL_LLEGAR), (16, TL_AL_ARRANCAR)):
        x, y = g.info().tl_key_curva(0, 1, f)
        g.raton_mover(x, y)
        g.foto(dur=450)
        g.clic(x, y)
        g.foto(dur=600)
        for rol in (TL_CUBICA, ease):
            bx, by = g.info().herramienta_rol(TIMELINE, rol)
            g.raton_mover(bx, by)
            g.foto(dur=450)
            g.clic(bx, by)
            g.foto(dur=900)
    reproducir(g, vueltas=2, cartel=PorIdioma({"es": "Espacio (Play)", "en": "Space (Play)"})[idioma])
    return g.cuadros(calidad=88)


# ---- 8. Mirarla limpia -------------------------------------------------------------------------------------------------------
def piso(g, ancho, fondo):
    """Un piso (un cubo chato, con la cara de arriba en Z 0): sin Superposiciones no hay grilla y la pelota picaria en
    el aire."""
    g.linea("cursor3d 0 -0.05 0")
    g.linea("add cube")
    g.linea("objname Piso")
    g.linea("objscale %g 0.05 %g" % (ancho, fondo))
    g.linea("cursor3d 0 -30 0")


@receta("pelota-final")
def final(idioma):
    """La pelota terminada, sin Superposiciones (con un piso), tres vueltas (la portada del tutorial)."""
    g = guion(idioma, "curvas", vista=VISTA_SALTO)
    piso(g, 5, 5)
    g.linea("deselectall")
    g.linea("vista3d overlays 0")
    v = g.info().vp(VISTA_3D)
    reproducir(g, vueltas=3)
    return g.cuadros(calidad=88, recorte=(v["x"], v["y"] + v["barra"], v["w"], v["h"] - v["barra"]))


# ---- La demo: tres pelotas ----------------------------------------------------------------------------------------------------
DEMO = [
    (-5.0, "lineal", False, PorIdioma({"es": "Lineal", "en": "Linear"})),
    (0.0, "curvas", False, PorIdioma({"es": "Con curvas", "en": "With curves"})),
    (5.0, "curvas", True, PorIdioma({"es": "Curvas + estirar y aplastar", "en": "Curves + squash and stretch"})),
]


@receta("pelota-comparar")
def comparar(idioma):
    """Tres pelotas que pican en el MISMO tiempo: la lineal (robotica, rebota como un resorte), la de las curvas (la
    gravedad: frena arriba y llega rapido al piso) y la de las curvas con estirar y aplastar (la del tutorial)."""
    g = Guion(idioma, w=1300, h=640, raton=False)
    g.linea("division 0.95")
    n = nombres(idioma)
    for nm in ("Cube", "Camera", "Light"):
        g.linea("selobj " + T(idioma, nm))
        g.linea("delete")
    for k, (x, curva, estirar, _) in enumerate(DEMO):
        nombre = "Pelota%d" % (k + 1)
        g.linea("cursor3d %g 1 0" % x)
        g.linea("add sphere")
        g.linea("objname " + nombre)
        g.linea("cursor3d %g 0 0" % x)
        g.linea("origen cursor")
        g.linea("selobj " + nombre)
        for f, z in ((3, 0), (16, 5), (29, 0)):
            g.linea("keyponer %d loc %g 0 %g" % (f, x, z))
        if curva == "lineal":
            g.linea("keyinterp lineal 3 loc")
            g.linea("keyinterp lineal 16 loc")
        else:
            g.linea("keyinterp out 3 loc")
            g.linea("keyinterp in 16 loc")
        if estirar:
            for f, (sx, sz) in ((1, (1, 1)), (2, (1.6, 0.75)), (4, (0.9, 1.1)), (17, (1, 1)), (29, (1, 1))):
                g.linea("keyponer %d esc %g %g %g" % (f, sx, sx, sz))
    g.linea("tl fin 28")
    piso(g, 9, 2)
    g.linea("deselectall")
    g.linea("vista3d overlays 0")
    g.linea("vporbita 0 3.7 0 12 0 0")
    g.linea("n95 perspectiva")                    # (ortografica: las tres a la misma escala)
    g.linea("frame 1")
    i = g.info()
    v = i.vp(VISTA_3D)
    marcas = []
    for k, (x, _, _, cartel) in enumerate(DEMO):
        o = i.objetos["Pelota%d" % (k + 1)]
        marcas.append(("etiqueta", o["x"], v["y"] + v["barra"] + 34, cartel[idioma]))
    for _ in range(3):
        for f in range(1, 29):
            g.linea("frame %d" % f)
            g.foto(dur=33, marcas=marcas, puntero=False)
    return g.cuadros(calidad=88, recorte=(v["x"], v["y"] + v["barra"], v["w"], v["h"] - v["barra"] - 40))


# ======================================================================================================================
#  TACTIL y NOKIA N95: el keyframe con Animacion > Insertar Keyframe (el boton del rombo de la barra de menus), el
#  cuadro con un toque en los numeros de la Linea de tiempo; Escalar / Mover en la barra de herramientas con el teclado
#  de la barra de arriba. En el N95: 3 escalar, 1 mover, el eje, el lapiz + los numeros, OK
# ======================================================================================================================
BR_ANIMACION = 16


def guion_tactil(idioma, hasta, vista=VISTA_CERCA):
    g = guion(idioma, hasta, vista=vista, raton=False)
    o = g.info().vp(OUTLINER)
    g.tocar(o["x"] + o["w"] // 2, o["y"] + o["h"] - 12)   # (el primer toque pasa la interfaz a tactil)
    g.sin_puntero()
    return g


def cuadro_dedo(g, f):
    i = g.info()
    tocar_en(g, i.tl_x(f), i.tlpanel["numeros"], despues=700)


def keyframe_dedo(g, opcion_en):
    tocar_menu(g, BR_ANIMACION, ["Insert Keyframe", opcion_en])
    g.foto(dur=700)


@receta("pelota-aplastar-dedo")
def aplastar_dedo(idioma):
    """Tactil: en el cuadro 1, Animacion > Insertar Keyframe > Solo Escala; el cuadro 2 con un toque en los numeros;
    Escalar Z 0.75, X 1.6 e Y 1.6; otra vez Solo Escala."""
    g = guion_tactil(idioma, "emparentada")
    g.linea("frame 1")
    g.foto(dur=700)
    keyframe_dedo(g, "Scale Only")
    cuadro_dedo(g, 2)
    rp.numero_dedo(g, "Scale", "Z", "0.75")
    rp.numero_dedo(g, "Scale", "X", "1.6")
    rp.numero_dedo(g, "Scale", "Y", "1.6")
    keyframe_dedo(g, "Scale Only")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


@receta("pelota-saltar-dedo")
def saltar_dedo(idioma):
    """Tactil: en el cuadro 3, Solo Localizacion; en el 16, Mover Z 5 y Solo Localizacion."""
    g = guion_tactil(idioma, "estirada", vista=VISTA_SALTO)
    g.foto(dur=600)
    cuadro_dedo(g, 3)
    keyframe_dedo(g, "Location Only")
    cuadro_dedo(g, 16)
    rp.numero_dedo(g, "Move", "Z", "5")
    keyframe_dedo(g, "Location Only")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


def guion_n95(idioma, hasta, vista="vporbita 0 1.0 0 7.5 -14 -30"):
    g = Guion(idioma, n95="vertical")
    g.linea("vista3d gizmo 0")
    g.linea("vista3d barra 0")
    armar(g, hasta)
    g.linea(vista)
    g.linea("cursor3d 0 -30 0")
    return g


@receta("pelota-aplastar-nokia")
def aplastar_nokia(idioma):
    """N95 (el keyframe del cuadro 1 ya puesto y en el cuadro 2): 3, 3 (Z), lapiz + 0.75, OK; 3, 1 (X), 1.6; 3, 2 (Y),
    1.6."""
    g = guion_n95(idioma, "emparentada")
    g.linea("frame 1")
    g.linea("ratonvista3d")
    g.linea("tecla i")
    g.linea("menuelegir Scale Only")
    g.linea("frame 2")
    g.foto(dur=900)
    n95_comando(g, 3, "3", 2, "3", "0.75")
    n95_comando(g, 3, "3", 0, "1", "1.6")
    n95_comando(g, 3, "3", 1, "2", "1.6")
    return g.cuadros(calidad=88)


@receta("pelota-saltar-nokia")
def saltar_nokia(idioma):
    """N95 (en el cuadro 16): 1, 3 (Z), lapiz + 5, OK."""
    g = guion_n95(idioma, "emparentada", vista="vporbita 0 2.6 0 13 -10 -30")   # (el N95 no tiene Linea de tiempo)
    g.linea("frame 3")
    g.linea("ratonvista3d")
    g.linea("tecla i")
    g.linea("menuelegir Location Only")
    g.linea("frame 16")
    g.foto(dur=900)
    n95_comando(g, 1, "1", 2, "3", "5")
    return g.cuadros(calidad=88)


TL_KEY = 327


def key_dedo(g, opcion_en):
    """El menu del rombo de la Linea de tiempo (Key) y una opcion, con el dedo."""
    tocar_en(g, *g.info().boton_rol(TIMELINE, TL_KEY), despues=700)
    tocar_en(g, *g.info().item_texto(T(g.idioma, opcion_en), 0), antes=400, despues=800)


@receta("pelota-copiar-dedo")
def copiar_dedo(idioma):
    """Tactil: un toque en el rombo del cuadro 1, Key > Copiar; un toque en el 4 (los numeros), Key > Pegar; un toque
    en un lugar vacio de la Linea de tiempo la deja ahi."""
    g = guion_tactil(idioma, "aplastada")
    g.foto(dur=700)
    i = g.info()
    tocar_en(g, *i.tl_key(0, 1), despues=700)
    key_dedo(g, "Copy")
    cuadro_dedo(g, 4)
    key_dedo(g, "Paste")
    i = g.info()
    tocar_en(g, i.tl_x(12), i.tlpanel["y"] + 170, despues=1500)
    return g.cuadros(calidad=90)

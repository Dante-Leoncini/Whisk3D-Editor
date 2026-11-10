"""Recetas del tutorial 3: El Modo Objeto (docs/manual/<idioma>/tutoriales/modo-objeto.md).

Los ejemplos usan a Leon (un .glb con su esqueleto y sus animaciones) que va en
tools/capturas/modelos/leon/ y NO esta en git: es de Resident Evil 4 (de Capcom).
Sin esa carpeta, estas recetas avisan y no hacen nada.
"""
import math
import os

from capturas import receta, Guion, RAIZ, T, PorIdioma, VISTA_3D, OUTLINER, PROPIEDADES, MAXIMIZAR
from recetas import vista_pc, MOVIDA, FLECHA

LEON = os.path.join(RAIZ, "tools", "capturas", "modelos", "leon", "leon.glb")
RIG = "leon_rig"                                   # el esqueleto de Leon (el padre de su malla)
CAJA = PorIdioma({"es": "Caja", "en": "Box", "pt": "Caixa", "fr": "Boîte", "it": "Scatola", "de": "Kiste",
                  "pl": "Pudło", "tr": "Kutu", "ru": "Ящик"})

# roles de la barra de abajo de la Vista 3D (ViewPort3D.h, ToolbarRol3D) y de la de arriba (BarRol3D)
TB_ACEPTAR, TB_HIST = 100, 110                     # la tilde; el primer boton del historial (Mover, al empezar)
BR_ADD = 5                                         # el + (el menu Añadir)
APLICAR_ESCALA = 222                               # Objeto > Aplicar > Escalar (ViewPort3D.cpp, MenuApply)

# las manijas del gizmo (Gizmo.h, GizmoManija)
EJE_X, EJE_Y, EJE_Z, CENTRO, LIBRE = 0, 1, 2, 6, 8   # (en modo rotar no hay CENTRO: el interior es LIBRE)

VISTA_LEON = "vporbita 0 0.85 0 3.2 -12 28"         # Leon entero, un poco de costado


def hay_leon():
    if not os.path.isfile(LEON):
        raise SystemExit("capturas: falta %s (el modelo de Leon no esta en git: copiarlo de re4/re3_w3d/out/leon_full)" % LEON)


def escena_leon(g, vista=VISTA_LEON, caja=False):
    """Leon parado en el medio, sin el cubo, con su esqueleto elegido. La luz se corre al fondo a la derecha: su
    linea hasta el piso cruzaba a Leon. Los huesos no se dibujan (tapan al personaje). caja=True: el cubo queda,
    chico y al costado, con el nombre Caja (para emparentarlo)."""
    hay_leon()
    if caja:
        g.linea("objname " + CAJA[g.idioma])
        g.linea("objscale 0.18 0.18 0.18")
        g.linea("objpos 0.75 0.18 0")
    else:
        g.linea("delete")                          # (el cubo: arranca elegido)
    g.linea("deselectall")
    g.linea("selobj " + T(g.idioma, "Light"))
    g.linea("objpos 3 4 -3")
    g.linea("import " + LEON)
    g.linea("texturas")
    g.linea("vista3d huesos 0")
    g.linea("deselectall")
    g.linea("selobj " + RIG)
    g.linea(vista)


def tactil(g):
    """La interfaz pasa a modo tactil con el primer toque: la barra de abajo suma Shift y Ctrl (y se corre) y el
    gizmo se agranda. Un toque en el cielo vacio, ANTES de pedir posiciones; despues se vuelve a elegir a Leon."""
    v = g.info().vp(VISTA_3D)
    g.tocar(v["x"] + 40, v["y"] + v["barra"] + 40)
    g.sin_puntero()
    g.linea("deselectall")
    g.linea("selobj " + RIG)


def recorrido(a, b, pasos):
    """Los puntos de a hasta b, arrancando y frenando despacio."""
    out = []
    for k in range(1, pasos + 1):
        t = k / pasos
        t = t * t * (3 - 2 * t)
        out.append((round(a[0] + (b[0] - a[0]) * t), round(a[1] + (b[1] - a[1]) * t)))
    return out


def arco(c, p, angulo, pasos):
    """Los puntos de un arco alrededor de c, desde p, girando 'angulo' radianes."""
    r = math.hypot(p[0] - c[0], p[1] - c[1])
    a0 = math.atan2(p[1] - c[1], p[0] - c[0])
    out = []
    for k in range(1, pasos + 1):
        t = k / pasos
        t = t * t * (3 - 2 * t)
        a = a0 + angulo * t
        out.append((round(c[0] + r * math.cos(a)), round(c[1] + r * math.sin(a))))
    return out


def mas_alla(c, p, px):
    """El punto que sigue de c a p, 'px' pixeles mas alla de p."""
    dx, dy = p[0] - c[0], p[1] - c[1]
    n = math.hypot(dx, dy) or 1.0
    return round(p[0] + dx / n * px), round(p[1] + dy / n * px)


# ---- el gizmo: un click cambia de modo ------------------------------------------------------------------------
@receta("objeto-gizmo-modos-raton")
def gizmo_modos_raton(idioma):
    """Un click sobre el gizmo, sin arrastrar: Mover -> Rotar -> Escalar -> Mover."""
    g = Guion(idioma, raton=True)
    escena_leon(g)
    rec, _ = vista_pc(g)
    x, y = g.info().manija(CENTRO)
    g.raton_mover(x + 70, y - 60)
    g.foto(dur=900)
    g.raton_mover(x, y)
    g.foto(dur=700)
    for _ in range(3):
        g.raton_abajo(x, y)
        g.foto(dur=180)
        g.raton_arriba()
        g.foto(dur=1300)
    return g.cuadros(recorte=rec, calidad=74)


@receta("objeto-gizmo-modos-dedo")
def gizmo_modos_dedo(idioma):
    """Un toque sobre el gizmo: Mover -> Rotar -> Escalar -> Mover."""
    g = Guion(idioma)
    escena_leon(g)
    tactil(g)
    rec, _ = vista_pc(g)
    x, y = g.info().manija(CENTRO)
    g.dedo_flotando(x, y)
    g.foto(dur=900)
    for _ in range(3):
        g.dedo_abajo(x, y)
        g.foto(dur=180)
        g.dedo_arriba()
        g.foto(dur=200)
        g.dedos_quitar()
        g.dedo_flotando(x, y)
        g.foto(dur=1100)
    return g.cuadros(recorte=rec, calidad=74)


# ---- usar el gizmo ----------------------------------------------------------------------------------------------
def arrastre(g, puntos, dur=50, dedo=False, cerrar=True):
    """Arrastra por los puntos (el boton o el dedo ya apoyados) sacando una foto en cada uno. cerrar: el editor
    aplica el movimiento en el siguiente, asi que uno mas en el ultimo punto termina el recorrido (si vuelve al
    lugar de partida, el objeto queda exactamente como estaba y el bucle no salta)."""
    for x, y in puntos:
        if dedo:
            g.dedo_mover(x, y)
        else:
            g.raton_mover(x, y)
        g.foto(dur=dur)
    if cerrar and puntos:
        x, y = puntos[-1]
        if dedo:
            g.dedo_mover(x, y)
        else:
            g.raton_mover(x, y)


def agarrar(g, x, y, dedo=False):
    if dedo:
        g.acercar_dedo(x, y)                      # (el dedo llega flotando: se ve de donde viene)
        g.foto(dur=300)
        g.dedo_abajo(x, y)
        g.esperar(220)                            # (el tactil espera un instante antes de decidir el gesto)
    else:
        g.raton_mover(x, y)
        g.foto(dur=400)
        g.raton_abajo(x, y)
    g.foto(dur=250)


def soltar(g, dedo=False):
    if dedo:
        g.dedo_arriba()
        g.dedos_quitar()
    else:
        g.raton_arriba()
    g.foto(dur=500)


def cambiar_modo(g, dedo=False):
    """Un click (o un toque) en el centro del gizmo: pasa al modo siguiente."""
    i = g.info()
    x, y = i.manijas.get(CENTRO) or i.manija(LIBRE)
    if dedo:
        g.toque(x, y, antes=400, despues=150)
    else:
        g.raton_mover(x, y)
        g.foto(dur=400)
        g.raton_abajo(x, y)
        g.foto(dur=150)
        g.raton_arriba()
    g.foto(dur=700)


def usar_gizmo(g, dedo=False, escalar=True):
    # MOVER: la flecha roja, para afuera y de vuelta
    i = g.info()
    c, p = i.manija(CENTRO), i.manija(EJE_X)
    centro = c
    if dedo:                                      # (en tactil los cuadraditos son grandes: la flecha, mas cerca de la punta)
        p = mas_alla(c, p, 22)
    lejos = mas_alla(c, p, 150)
    agarrar(g, *p, dedo=dedo)
    arrastre(g, recorrido(p, lejos, 14) + recorrido(lejos, p, 14), dedo=dedo)
    soltar(g, dedo=dedo)
    # ROTAR: el semicirculo azul (Z, el de arriba), un tramo de arco y de vuelta
    cambiar_modo(g, dedo=dedo)
    i = g.info()
    c, p = centro, i.manija(EJE_Z)
    agarrar(g, *p, dedo=dedo)
    ida = arco(c, p, 1.3, 16)
    arrastre(g, ida + arco(c, ida[-1], -1.3, 16), dedo=dedo)
    soltar(g, dedo=dedo)
    if escalar:
        # ESCALAR: la punta azul (Z) para arriba: Leon se estira a lo alto. Se suelta arriba y Ctrl+Z lo deshace
        cambiar_modo(g, dedo=dedo)
        i = g.info()
        c, p = i.manija(CENTRO), i.manija(EJE_Z)
        arriba = mas_alla(c, p, 22)
        agarrar(g, *p, dedo=dedo)
        arrastre(g, recorrido(p, arriba, 12), dedo=dedo)
        soltar(g, dedo=dedo)
        g.foto(dur=700)
        g.foto(dur=450, tecla="Ctrl + Z")
        g.mod("Ctrl")
        g.tecla("z")
        g.mod()
        g.foto(dur=900, tecla="Ctrl + Z")
    # y otra vez a mover (como estaba al empezar: el bucle no salta). Sin escalar, se pasa por escalar de largo
    cambiar_modo(g, dedo=dedo)
    if not escalar:
        cambiar_modo(g, dedo=dedo)
    g.sin_puntero()
    g.foto(dur=700)


@receta("objeto-gizmo-raton")
def gizmo_raton(idioma):
    """El gizmo con el mouse: la flecha roja mueve en X, el semicirculo azul gira y el centro escala parejo."""
    g = Guion(idioma, raton=True)
    escena_leon(g)
    rec, _ = vista_pc(g)
    g.foto(dur=600)
    usar_gizmo(g)
    return g.cuadros(recorte=rec, calidad=66)


@receta("objeto-gizmo-dedo")
def gizmo_dedo(idioma):
    """El gizmo con el dedo: la flecha roja mueve en X y, en modo rotar, el semicirculo azul gira."""
    g = Guion(idioma)
    escena_leon(g)
    tactil(g)
    rec, _ = vista_pc(g)
    g.foto(dur=600)
    usar_gizmo(g, dedo=True, escalar=False)
    return g.cuadros(recorte=rec, calidad=66)


# ---- con el teclado ---------------------------------------------------------------------------------------------
def mover_raton(g, puntos, cartel, dur=60):
    for x, y in puntos:
        g.raton_mover(x, y)
        g.foto(dur=dur, tecla=cartel)


@receta("objeto-teclas-raton")
def teclas_raton(idioma):
    """G (Leon sigue al mouse), X (solo en X), click; R, click; S, click."""
    g = Guion(idioma, raton=True)
    escena_leon(g, vista="vporbita 0 0.9 0 4.6 -12 28")
    rec, (cx, cy) = vista_pc(g)
    p = (cx + 40, cy + 70)
    g.raton_mover(*p)
    g.foto(dur=900)
    # G: libre, unos pasos; despues X y el resto solo en X
    g.foto(dur=350, tecla="G")
    g.tecla("g")
    g.raton_mover(p[0] + 2, p[1])                 # (el primer movimiento de un transform no cuenta)
    q = (p[0] + 70, p[1] - 45)
    mover_raton(g, recorrido(p, q, 10), "G")
    g.foto(dur=350, tecla="X")
    g.tecla("x")
    r = (q[0] + 90, q[1] + 30)
    mover_raton(g, recorrido(q, r, 10), "X")
    g.raton_mover(*r)
    g.foto(dur=300)
    g.clic(*r)
    g.foto(dur=900)
    # R + Z: el mouse da media vuelta alrededor de Leon y Leon gira sobre su eje vertical (sin Z gira de frente a
    # la vista y se acuesta)
    centro = g.info().manija(CENTRO)              # (el pivote: los pies de Leon, donde quedo)
    g.foto(dur=350, tecla="R")
    g.tecla("r")
    g.raton_mover(r[0] + 2, r[1])
    g.foto(dur=300, tecla="Z")
    g.tecla("z")
    mover_raton(g, arco(centro, r, -1.6, 16), "Z")
    g.clic(*g.puntero_xy())
    g.foto(dur=900)
    # S: el mouse se aleja del pivote (Leon se agranda: la escala es la distancia al pivote)
    s0 = g.puntero_xy()
    d = math.hypot(s0[0] - centro[0], s0[1] - centro[1])
    g.foto(dur=350, tecla="S")
    g.tecla("s")
    g.raton_mover(s0[0] + 1, s0[1])
    mover_raton(g, recorrido(s0, mas_alla(centro, s0, d * 0.45), 10), "S")
    g.clic(*g.puntero_xy())
    g.sin_puntero()
    g.foto(dur=1600)
    return g.cuadros(recorte=rec, calidad=66)


@receta("objeto-teclas-nokia")
def teclas_nokia(idioma):
    """N95 (sin gizmo, como viene): 1 + flechas + OK (mover); 2, 3 (el eje Z) + flechas + OK (rotar sobre su eje
    vertical); 3 + flechas + OK (escalar)."""
    g = Guion(idioma, n95="vertical")
    escena_leon(g, vista="vporbita 0.4 0.9 0 3.6 -12 28")
    g.linea("vista3d gizmo 0")                     # (el N95 viene sin gizmo y sin la barra de abajo)
    g.linea("vista3d barra 0")
    g.foto(dur=900)
    pasos = (("1", "num1", 1, None, (("derecha", 3), ("arriba", 2))),
             ("2", "num2", 2, ("3", "num3", 2), (("derecha", 16),)),
             ("3", "num3", 3, None, (("derecha", 6),)))
    for cartel, tecla, modo, eje, flechas in pasos:
        g.foto(dur=400, tecla=cartel, teclas=(tecla,))
        g.keypad("transformar", modo)
        g.foto(dur=300)
        if eje:                                   # durante el transform, 1/2/3 eligen el eje X/Y/Z
            g.foto(dur=400, tecla=eje[0], teclas=(eje[1],))
            g.keypad("eje", eje[2])
            g.foto(dur=300)
        for flecha, veces in flechas:
            dx, dy = MOVIDA[flecha]
            g.foto(dur=150, tecla=FLECHA[flecha], teclas=(flecha,))
            for _ in range(veces):
                g.keypad("transflechas", dx, dy, 3)
                g.foto(dur=90, tecla=FLECHA[flecha], teclas=(flecha,))
        g.foto(dur=350, tecla="OK", teclas=("ok",))
        g.keypad("aceptar")
        g.foto(dur=900)
    return g.cuadros(calidad=66)


# ---- tactil: los comandos de la barra de abajo -----------------------------------------------------------------
@receta("objeto-comandos-dedo")
def comandos_dedo(idioma):
    """Tactil: Mover en la barra de abajo, el dedo arrastra en la vista, la tilde verde confirma."""
    g = Guion(idioma)
    escena_leon(g, vista="vporbita 0 0.9 0 4.6 -12 28")
    tactil(g)
    rec, (cx, cy) = vista_pc(g)
    i = g.info()
    bx, by = i.herramienta_rol(VISTA_3D, TB_HIST)  # Mover (el primero del historial)
    g.foto(dur=700)
    g.dedo_flotando(bx, by)
    g.foto(dur=700)
    g.tocar(bx, by)
    g.dedos_quitar()
    g.foto(dur=600)
    p = (cx - 160, cy + 60)
    g.dedo_abajo(*p)
    g.foto(dur=250)
    q = (p[0] + 150, p[1] - 30)
    arrastre(g, recorrido(p, q, 14), dur=60, dedo=True)
    g.dedo_arriba()
    g.dedos_quitar()
    g.foto(dur=500)
    tx, ty = g.info().herramienta_rol(VISTA_3D, TB_ACEPTAR)
    g.dedo_flotando(tx, ty)
    g.foto(dur=700)
    g.tocar(tx, ty)
    g.dedos_quitar()
    g.foto(dur=1500)
    return g.cuadros(recorte=rec, calidad=70)


TB_EJE_Z = 105                                    # el boton Z de la barra de herramientas durante un transform


def tocar_en(g, x, y, antes=600, despues=600):
    """Un toque que se puede seguir (ver Guion.toque): el dedo llega flotando y se ilumina al tocar."""
    g.toque(x, y, antes=antes, despues=despues)


@receta("objeto-numero-dedo")
def numero_dedo(idioma):
    """Tactil, con precision: Rotar en la barra de herramientas y Z; un toque en la barra de arriba (la que muestra
    el valor) abre el teclado numerico: 90 y OK. Leon queda girado exactamente 90 grados."""
    g = Guion(idioma)
    escena_leon(g, vista="vporbita 0 0.9 0 4.6 -12 28")
    tactil(g)
    rec, _ = vista_pc(g)
    g.foto(dur=900)
    tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_HIST + 1))   # Rotar (el segundo del historial)
    tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_EJE_Z))      # solo en Z, el eje vertical
    v = g.info().vp(VISTA_3D)
    tocar_en(g, v["x"] + v["w"] // 2, v["y"] + v["barra"] // 2, despues=900)   # la barra de arriba
    for t in ("9", "0", "OK"):
        tocar_en(g, *g.info().numpad[t], antes=350, despues=450)
    tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_ACEPTAR), despues=1800)   # la tilde verde lo deja hecho
    return g.cuadros(calidad=72)                  # (entera: el teclado numerico ocupa todo el ancho)


@receta("objeto-origen")
def origen_solo(idioma):
    """El origen de un objeto, de cerca (el cubo elegido, con el gizmo apagado para que se vea)."""
    g = Guion(idioma, ampliar=3)
    g.linea("vista3d gizmo 0")
    g.linea("vporbita 0 0 0 5 -30 25")
    o = g.info().objetos[T(idioma, "Cube")]
    g.foto()
    return g.cuadros(recorte=(o["x"] - 80, o["y"] - 55, 160, 110))


@receta("objeto-cursor3d")
def cursor3d_solo(idioma):
    """El cursor 3D, de cerca, en un lugar vacio del piso."""
    g = Guion(idioma, ampliar=3)
    g.linea("deselectall")
    g.linea("cursor3d 2.2 0 0.8")
    g.linea("vporbita 2.2 0 0.8 4 -30 25")         # (el cursor en el medio de la vista)
    x, y = g.info().cursor3d
    g.foto()
    return g.cuadros(recorte=(x - 80, y - 55, 160, 110))


# ---- la barra de abajo: los ultimos comandos ---------------------------------------------------------------------
def rect_historial(i):
    hs = [h for h in i.herramientas if h["kind"] == VISTA_3D and TB_HIST <= h["rol"] < TB_HIST + 8]
    x0 = min(h["x"] for h in hs) - 4
    y0 = min(h["y"] for h in hs) - 4
    x1 = max(h["x"] + h["w"] for h in hs) + 4
    y1 = max(h["y"] + h["h"] for h in hs) + 4
    return ("rect", x0, y0, x1 - x0, y1 - y0)


@receta("objeto-barra-inferior")
def barra_inferior(idioma):
    """La barra de abajo: Mover, Rotar, Escalar... Despues de escalar, Escalar pasa adelante."""
    g = Guion(idioma, raton=True)
    escena_leon(g)
    rec, (cx, cy) = vista_pc(g)
    g.foto(dur=1600, marcas=(rect_historial(g.info()),), puntero=False)
    p = (cx + 120, cy + 20)
    g.raton_mover(*p)
    g.foto(dur=300, tecla="S")
    g.tecla("s")
    g.raton_mover(p[0] + 2, p[1])
    mover_raton(g, recorrido(p, (p[0] + 50, p[1] - 15), 6), "S")
    g.clic(*g.puntero_xy())
    g.foto(dur=600)
    g.sin_puntero()
    g.foto(dur=2200, marcas=(rect_historial(g.info()),))
    return g.cuadros(recorte=rec, calidad=74)


# ---- las propiedades -------------------------------------------------------------------------------------------
@receta("objeto-propiedades")
def propiedades(idioma):
    """Propiedades con el esqueleto de Leon elegido: la tarjeta Transformar y la columna de rombos marcada."""
    g = Guion(idioma, w=960, h=720)
    escena_leon(g)
    g.linea("division 0.6")                       # (la columna de la derecha mas ancha: que no se corten las etiquetas)
    i = g.info()
    v = i.vp(PROPIEDADES)
    g.foto(marcas=(("rect", v["x"] + v["w"] - 72, v["y"] + 120, 40, v["h"] - 130),))
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], v["h"]))


# ---- aplicar la escala -----------------------------------------------------------------------------------------
@receta("objeto-escala-raton")
def escala_raton(idioma):
    """Una caja escalada (2 y 0.5): Ctrl+A -> Escalar y la escala vuelve a 1, sin que cambie lo que se ve."""
    g = Guion(idioma, w=960, h=860, raton=True)
    g.linea("division 0.62")                      # (Propiedades mas ancho y alto: que se vea la escala)
    g.linea("objname " + CAJA[idioma])
    g.linea("objscale 2 0.5 1")
    g.linea("vporbita 0 0 0 9 -20 30")
    rec, (cx, cy) = vista_pc(g)
    p = (cx + 150, cy + 40)
    g.raton_mover(*p)
    g.foto(dur=1800)
    g.foto(dur=500, tecla="Ctrl + A")
    g.mod("Ctrl")
    g.tecla("a")
    g.mod()
    g.foto(dur=700, tecla="Ctrl + A")
    ix, iy = g.info().item(APLICAR_ESCALA)
    for x, y in recorrido(p, (ix, iy), 6):
        g.raton_mover(x, y)
        g.foto(dur=60)
    g.foto(dur=600)
    g.clic(ix, iy)
    g.foto(dur=2200)
    return g.cuadros(calidad=74)


# ---- el Outliner -----------------------------------------------------------------------------------------------
@receta("objeto-outliner")
def outliner(idioma):
    """El Outliner con leon_rig desplegado: su malla adentro, corrida a la derecha."""
    g = Guion(idioma, w=960, h=720)
    escena_leon(g)
    g.linea("division 0.6")
    i = g.info()
    v = i.vp(OUTLINER)
    f0 = i.fila(RIG)
    f1 = max((f for f in i.filas if f["prof"] == 2), key=lambda f: f["y"])   # la ultima malla de Leon
    marca = ("rect", v["x"] + 4, f0[1] - 13, v["w"] - 8, f1["y"] - f0[1] + 26)
    g.foto(marcas=(marca,))
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], v["h"]))


@receta("objeto-emparentar-raton")
def emparentar_raton(idioma):
    """La caja se arrastra en el Outliner hasta leon_rig (queda adentro); con G se mueve a Leon y la caja va con el."""
    g = Guion(idioma, w=960, h=920, raton=True)
    escena_leon(g, caja=True, vista="vporbita 0.2 0.85 0 4.4 -12 28")
    g.linea("division 0.6")                       # (el Outliner mas ancho: que se lean los nombres)
    i = g.info()
    a, b = i.fila(CAJA[idioma]), i.fila(RIG)
    g.raton_mover(a[0] + 40, a[1] + 30)
    g.foto(dur=900)
    agarrar(g, *a)
    arrastre(g, recorrido(a, (b[0] + 20, b[1]), 12), dur=70)
    g.foto(dur=400)
    soltar(g)
    g.foto(dur=500)
    # la caja ya esta adentro de leon_rig: su fila, marcada
    i = g.info()
    v, f = i.vp(OUTLINER), i.fila(CAJA[idioma])
    g.foto(dur=1400, marcas=(("rect", v["x"] + 4, f[1] - 13, v["w"] - 8, 26),))
    # elegir a Leon (su fila) y moverlo con G en la Vista 3D
    b = g.info().fila(RIG)
    g.raton_mover(*b)
    g.foto(dur=400)
    g.clic(*b)
    g.foto(dur=700)
    _, (cx, cy) = vista_pc(g)
    p = (cx - 40, cy + 80)
    for x, y in recorrido(b, p, 8):
        g.raton_mover(x, y)
        g.foto(dur=50)
    g.foto(dur=300, tecla="G")
    g.tecla("g")
    g.raton_mover(p[0] + 2, p[1])
    mover_raton(g, recorrido(p, (p[0] - 170, p[1] + 10), 14), "G")
    g.clic(*g.puntero_xy())
    g.sin_puntero()
    g.foto(dur=1800)
    return g.cuadros(calidad=70)


# ---- los tipos de objetos y el importador --------------------------------------------------------------------------
@receta("objeto-tipos")
def tipos(idioma):
    """El menu Anadir abierto (el + de la barra de la Vista 3D): todos los tipos de objetos."""
    g = Guion(idioma, w=960, h=1040)               # (alto: que el menu entre entero, sin barra para bajar)
    x, y = g.info().boton_rol(VISTA_3D, BR_ADD)
    g.clic(x, y)
    g.sin_puntero()
    m = g.info().menus[0]
    if m["filas"] > len([it for it in g.info().items if it["nivel"] == 0]):
        raise SystemExit("capturas: el menu Anadir no entra entero en la ventana")
    g.foto()
    x0 = max(0, m["x"] - 360)
    return g.cuadros(recorte=(x0, 0, m["x"] + m["w"] + 8 - x0, m["y"] + m["h"] + 8))


@receta("objeto-importar")
def importar(idioma):
    """Anadir -> Importar -> GLB, y Leon aparece con su esqueleto y su malla en el Outliner."""
    hay_leon()
    g = Guion(idioma, w=960, h=720, raton=True)
    g.linea("delete")
    g.linea("vporbita 0 0.9 0 3.8 -12 28")
    g.foto(dur=900)
    x, y = g.info().boton_rol(VISTA_3D, BR_ADD)
    g.raton_mover(x + 60, y + 120)
    g.foto(dur=400)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=900)
    m = g.info().menus[0]
    mx, my = m["x"] + m["w"] // 2, m["y"] + m["h"] // 2
    for _ in range(12):                           # (el menu es largo: Importar esta abajo de todo; se baja sin fotos)
        g.rueda(-3, mx, my)
        i = g.info()
        filas = [it for it in i.items if it["nivel"] == 0]
        if filas and filas[-1]["i"] == m["filas"] - 1:
            break
    imp = filas[-2] if filas[-1]["i"] == m["filas"] - 1 else None   # "Importar": la anteultima (la ultima es Nucleo)
    if not imp:
        raise SystemExit("capturas: el menu Anadir no bajo hasta Importar (se ven: %s)" % [f.get("texto") for f in filas])
    g.raton_mover(imp["cx"], imp["cy"])
    g.foto(dur=1000)
    gx, gy = g.info().item_texto("GLB", 1)
    g.raton_mover(imp["cx"] + (gx - imp["cx"]) // 2, imp["cy"])
    g.raton_mover(gx, gy)
    g.foto(dur=1400)
    # (el explorador de archivos: se elige leon.glb; aca entra directo con el mismo importador)
    g.tecla("Escape")
    g.tecla("Escape")
    g.linea("import " + LEON)
    g.linea("texturas")
    g.linea("vista3d huesos 0")
    g.sin_puntero()
    g.foto(dur=2400)
    return g.cuadros(calidad=72)


# ---- los articulos: la PESTANIA COMPLETA de Propiedades (arriba de cada articulo, el texto abajo) ---------------------
# las pestanias (Properties.cpp, BarTabs): el numero es fijo; cada tipo de objeto muestra las suyas
TAB_OBJETO, TAB_TIPO, TAB_MALLA, TAB_MODIFICADORES, TAB_RESTRICCIONES = 1, 2, 3, 4, 7
# ModifierType (edit/Modifier.h) y W3dConstraintTipo (Core)
MOD_TORNILLO, MOD_ESPEJO, MOD_ARRAY, MOD_SUBDIVISION, MOD_BOOLEANO, MOD_ESQUELETO, MOD_CULLING = range(7)
COPIAR_POSICION, COPIAR_ROTACION, BILLBOARD, CHILD_OF = range(4)


def guion_panel(idioma):
    """Una ventana angosta y alta (el 'offscreen' no da mas de ~1080 de alto): Propiedades maximizado."""
    return Guion(idioma, w=440, h=1080)


def recortar_fondo(im, margen=12):
    """Saca de abajo lo que es solo fondo (la pestania termina antes que el panel). No mira los bordes: el de la
    izquierda (verde, el viewport activo) ni la franja de la derecha (la barra de desplazamiento)."""
    px = im.load()
    w, h = im.size
    fondo = px[w // 2, h - 3]
    def distinto(c):
        return sum(abs(a - b) for a, b in zip(c, fondo)) > 24
    y = h - 1
    while y > 0 and not any(distinto(px[x, y]) for x in range(6, w - 30, 2)):   # (sin el borde verde de la izquierda)
        y -= 1
    return im.crop((0, 0, w, min(h, y + margen)))


def coser(ims, arriba, abajo=6):
    """Una pestania larga en varios tramos (cada uno mas abajo con la rueda) -> una sola imagen. Las primeras 'arriba'
    filas son la barra fija del panel y las ultimas 'abajo', su borde: no se mueven con la rueda. De cada tramo nuevo
    se busca cuanto bajo (la primera fila desde la que el anterior y el nuevo coinciden enteros, sin la barra de
    desplazamiento) y se pega lo que agrega. Un tramo igual al anterior = se llego al final."""
    from PIL import Image
    w = ims[0].width
    def filas(im):
        c = im.crop((0, arriba, w - 30, im.height - abajo))
        b = c.tobytes()
        n = c.width * len(c.getbands())
        return [b[i * n:(i + 1) * n] for i in range(c.height)]
    tiras = [ims[0].crop((0, 0, w, ims[0].height - abajo))]
    prev = filas(ims[0])
    for im in ims[1:]:
        cur = filas(im)
        if cur == prev:
            break
        n = len(cur)
        d = next((d for d in range(1, n) if prev[d:] == cur[:n - d]), None)
        if d is None:
            raise SystemExit("capturas: no se pudo coser la pestania (un tramo no empalma con el anterior)")
        tiras.append(im.crop((0, arriba + n - d, w, im.height - abajo)))
        prev = cur
    out = Image.new("RGB", (w, sum(t.height for t in tiras)))
    y = 0
    for t in tiras:
        out.paste(t, (0, y))
        y += t.height
    return out


def panel_completo(g, pestania, tramos=6, paso=12):
    """La pestania 'pestania' de Propiedades con todas sus tarjetas, recortada a lo que ocupa (las largas, cosidas
    de varios tramos)."""
    from capturas import Cuadros
    g.linea("propstab %d" % pestania)
    x, y = g.info().esquina(PROPIEDADES)
    g.clic(x, y)
    g.clic(*g.info().item(MAXIMIZAR))
    g.sin_puntero()
    barra = g.info().vp(PROPIEDADES)["barra"]
    g.foto()
    for _ in range(tramos - 1):
        g.linea("propscroll %d" % paso)
        g.foto()
    c = g.cuadros()
    res = Cuadros()
    res.append((recortar_fondo(coser([im for im, _ in c], barra)), 100))
    return res


def panel_receta(nombre, pestania, preparar):
    """Registra la receta 'nombre': prepara la escena con preparar(g) y saca la pestania completa."""
    def fn(idioma):
        g = guion_panel(idioma)
        preparar(g)
        return panel_completo(g, pestania)
    fn.__doc__ = "La pestania %d completa de Propiedades (%s)." % (pestania, nombre)
    receta(nombre)(fn)


def elegir(nombre_en):
    """Elegir el objeto de la escena nueva que se llama asi (en ingles: la Luz, la Camara)."""
    def prep(g):
        g.linea("deselectall")
        g.linea("selobj " + T(g.idioma, nombre_en))
    return prep


def sonido(g):
    g.linea("agregar sonido")


def leon_malla(g):
    escena_leon(g)
    g.linea("deselectall")
    g.linea("selobj leon")


def con(que, tipo):
    """El cubo de la escena nueva con un modificador o una restriccion de ese tipo."""
    def prep(g):
        g.linea("agregar %s %d" % (que, tipo))
    return prep


panel_receta("objeto-luz-propiedades", TAB_TIPO, elegir("Light"))
panel_receta("objeto-camara-propiedades", TAB_TIPO, elegir("Camera"))
panel_receta("objeto-sonido-propiedades", TAB_TIPO, sonido)
panel_receta("objeto-esqueleto-propiedades", TAB_TIPO, escena_leon)
panel_receta("objeto-malla-objeto", TAB_OBJETO, leon_malla)
panel_receta("objeto-malla-material", TAB_TIPO, leon_malla)
panel_receta("objeto-malla-propiedades", TAB_MALLA, leon_malla)
panel_receta("objeto-malla-modificadores", TAB_MODIFICADORES, con("modificador", MOD_SUBDIVISION))
panel_receta("restricciones-copiar-posicion", TAB_RESTRICCIONES, con("restriccion", COPIAR_POSICION))
panel_receta("restricciones-copiar-rotacion", TAB_RESTRICCIONES, con("restriccion", COPIAR_ROTACION))
panel_receta("restricciones-billboard", TAB_RESTRICCIONES, con("restriccion", BILLBOARD))
panel_receta("restricciones-child-of", TAB_RESTRICCIONES, con("restriccion", CHILD_OF))
panel_receta("modificador-tornillo", TAB_MODIFICADORES, con("modificador", MOD_TORNILLO))
panel_receta("modificador-espejo", TAB_MODIFICADORES, con("modificador", MOD_ESPEJO))
panel_receta("modificador-array", TAB_MODIFICADORES, con("modificador", MOD_ARRAY))
panel_receta("modificador-subdivision", TAB_MODIFICADORES, con("modificador", MOD_SUBDIVISION))
panel_receta("modificador-booleano", TAB_MODIFICADORES, con("modificador", MOD_BOOLEANO))
panel_receta("modificador-culling", TAB_MODIFICADORES, con("modificador", MOD_CULLING))
panel_receta("modificador-esqueleto", TAB_MODIFICADORES, leon_malla)   # (la malla de Leon ya trae el suyo)


# ---- el origen, el cursor 3D, el pivote y Global / Local -----------------------------------------------------------
# los botones de la barra de la Vista 3D (BarRol3D) y las opciones de sus menus
BR_PIVOTE, BR_OBJETO, BR_ORIENTACION, BR_MALLA = 3, 6, 9, 15
OBJ_DEFINIR_ORIGEN, OBJ_IMAN = 4, 8                       # submenus del menu Objeto
IMAN_SELECCION_A_CURSOR, IMAN_CURSOR_A_SELECCION = 1, 4   # el menu Iman (LayoutSubmenuSnap)
ORIGEN_AL_CURSOR = 202                                    # Definir Origen > Origen al Cursor 3D
PIVOTE_CURSOR, PIVOTE_INDIVIDUAL, PIVOTE_MEDIO = 0, 1, 2  # TransformPivot
GLOBAL, LOCAL = 0, 1
PUERTA = PorIdioma({"es": "Puerta", "en": "Door", "pt": "Porta", "fr": "Porte", "it": "Porta", "de": "Tür",
                    "pl": "Drzwi", "tr": "Kapı", "ru": "Дверь"})


def menu(g, rol, ids, kind=VISTA_3D):
    """Abre el menu del boton de la barra con ese rol y recorre las opciones 'ids' (las de submenu, pasando por
    arriba: se abren solas), con un click en la ultima. Una opcion puede ser el texto en ingles (se busca traducido):
    para las que comparten id con otras."""
    x, y = g.info().boton_rol(kind, rol)
    g.raton_mover(x, y)
    g.foto(dur=350)
    g.clic(x, y)
    g.foto(dur=600)
    for nivel, ident in enumerate(ids):
        i = g.info()
        ix, iy = i.item_texto(T(g.idioma, ident), nivel) if isinstance(ident, str) else i.item(ident, nivel)
        x0, y0 = g.puntero_xy()
        if nivel:                                 # (a un submenu se entra de costado, sin pasar por las otras filas)
            g.raton_mover(ix, y0)
        g.raton_mover(ix, iy)
        g.foto(dur=650)
    g.clic(*g.puntero_xy())
    g.foto(dur=500)


def teclas(g, lista, cartel, dur=1600):
    """Varias teclas seguidas con un solo cartel. Un elemento '=90' se TIPEA (los numeros entran como texto)."""
    g.foto(dur=500, tecla=cartel)
    for t in lista:
        if t.startswith("="):
            g.linea("texto " + t[1:])
        else:
            g.tecla(t)
    g.foto(dur=dur, tecla=cartel)


def deshacer(g):
    g.foto(dur=400, tecla="Ctrl + Z")
    g.mod("Ctrl")
    g.tecla("z")
    g.mod()
    g.foto(dur=900, tecla="Ctrl + Z")


GIRAR_90 = (["r", "z", "=90", "Return"], "R  Z  90  Enter")


@receta("objeto-cursor")
def cursor(idioma):
    """Iman: Leon elegido -> Cursor a lo seleccionado (el cursor 3D va a sus pies); la caja -> Seleccion a Cursor."""
    g = Guion(idioma, w=1200, h=720, raton=True)     # (ancha: que la barra de la Vista 3D muestre el menu Objeto)
    escena_leon(g, caja=True, vista="vporbita 0 0.6 0 5 -25 28")
    g.linea("division 0.66")
    g.linea("deselectall")
    g.linea("selobj " + CAJA[idioma])
    g.linea("objpos 1.6 0.18 1.2")
    g.linea("deselectall")
    g.linea("selobj " + RIG)
    g.linea("objpos -1.2 0 -0.6")
    g.foto(dur=1200)
    menu(g, BR_OBJETO, (OBJ_IMAN, IMAN_CURSOR_A_SELECCION))
    g.foto(dur=1100)
    # la caja, desde su fila del Outliner
    x, y = g.info().fila(CAJA[idioma])
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=700)
    menu(g, BR_OBJETO, (OBJ_IMAN, IMAN_SELECCION_A_CURSOR))
    g.sin_puntero()
    g.foto(dur=1800)
    return g.cuadros(calidad=72)


@receta("objeto-pivote")
def pivote(idioma):
    """Tres barras en fila rotan 90 en Z: Punto medio (juntas), Origenes individuales (cada una en su lugar) y Cursor 3D
    (alrededor del cursor, en el centro del mundo, a un costado de la fila)."""
    g = Guion(idioma, raton=True)
    g.linea("delete")
    nombres = []
    for k, x in enumerate((1.4, 2.6, 3.8)):
        g.linea("add cube")
        n = "%s%d" % (CAJA[idioma], k + 1)
        g.linea("objname " + n)
        g.linea("objscale 0.5 0.15 0.15")         # (barras: un cubo girado 90 grados se ve igual)
        g.linea("objpos %g 0.15 0" % x)
        nombres.append(n)
    g.linea("deselectall")
    g.linea("selobj " + nombres[0])
    for n in nombres[1:]:
        g.linea("selname " + n)
    g.linea("vporbita 1.6 0 0 9 -60 0")
    rec, (cx, cy) = vista_pc(g)
    g.foto(dur=1000)
    for p in (PIVOTE_MEDIO, PIVOTE_INDIVIDUAL, PIVOTE_CURSOR):
        menu(g, BR_PIVOTE, (p,))
        g.raton_mover(cx + 200, cy + 120)
        g.foto(dur=300)
        teclas(g, *GIRAR_90)
        deshacer(g)
    menu(g, BR_PIVOTE, (PIVOTE_MEDIO,))          # (como viene: el bucle empieza igual)
    g.sin_puntero()
    g.foto(dur=600)
    return g.cuadros(recorte=rec, calidad=72)


@receta("objeto-local-global")
def local_global(idioma):
    """Leon girado 40: en Global la flecha verde lo mueve en la Y del mundo; en Local, derecho hacia donde mira."""
    g = Guion(idioma, raton=True)
    escena_leon(g, vista="vporbita 0 0.6 0 4.6 -35 28")
    g.linea("objrot 40")
    rec, _ = vista_pc(g)
    g.foto(dur=1000)
    for orient in (GLOBAL, LOCAL):
        if orient == LOCAL:
            menu(g, BR_ORIENTACION, (LOCAL,))
            g.foto(dur=700)
        i = g.info()
        c, p = i.manija(CENTRO), i.manija(EJE_Y)
        lejos = mas_alla(c, p, 120)
        agarrar(g, *p)
        arrastre(g, recorrido(p, lejos, 14))
        soltar(g)
        g.foto(dur=900)
        deshacer(g)
    menu(g, BR_ORIENTACION, (GLOBAL,))
    g.sin_puntero()
    g.foto(dur=600)
    return g.cuadros(recorte=rec, calidad=70)


# ---- tutorial: modelar una puerta (tutoriales/modelar-puerta.md) -----------------------------------------------
def modelar_guion(idioma, raton=True):
    """La Vista 3D con Propiedades al lado (que se vea la Escala del objeto) y el cubo de siempre, de 2 x 2 x 2 m."""
    g = Guion(idioma, w=1200, h=760, raton=raton)
    g.linea("division 0.66")
    g.linea("vporbita 0 0.8 0 9 -32 18")
    return g


def modelar_subido(g):
    g.linea("objpos 0 1 0")                       # (lo que hace el primer video: G Z 1)


def comando(g, pasos, mover=()):
    """Un comando del teclado PASO A PASO: cada tecla con su foto y el cartel que se va sumando ('G', 'G, Z',
    'G, Z, 1', 'G, Z, 1, Enter'), para que se vea que hace cada una. pasos: [(tecla, rotulo)]; '=1.5' se tipea.
    mover: puntos por donde pasa el mouse despues de la primera tecla (el objeto lo sigue: se ve que arranco)."""
    hechas = []
    for k, (t, rotulo) in enumerate(pasos):
        hechas.append(rotulo)
        cartel = ",  ".join(hechas)
        g.foto(dur=450, tecla=cartel)
        if t.startswith("="):
            g.linea("texto " + t[1:])
        else:
            g.tecla(t)
        g.foto(dur=900, tecla=cartel)
        if k == 0 and mover:
            for x, y in mover:
                g.raton_mover(x, y)
                g.foto(dur=60, tecla=cartel)
            g.raton_mover(*mover[-1])             # (el editor aplica cada movimiento en el siguiente)
            g.foto(dur=700, tecla=cartel)
    g.foto(dur=1400)


def vaiven(g, a, b, pasos=8):
    """Del punto a al b y de vuelta (el mouse 'prueba' el movimiento antes del numero)."""
    return recorrido(a, b, pasos) + recorrido(b, a, pasos)[1:]


@receta("modelar-subir-raton")
def modelar_subir(idioma):
    """El cubo, con la mitad bajo el piso: G (sigue al mouse), Z (queda en el eje vertical), 1 y Enter: sube 1 metro."""
    g = modelar_guion(idioma)
    rec, (cx, cy) = vista_pc(g)
    p = (cx + 200, cy + 140)
    g.raton_mover(*p)
    g.foto(dur=1000)
    g.linea("trpanel on")                         # (despues de Enter aparece abajo la ficha para corregir)
    comando(g, [("g", "G"), ("z", "Z"), ("=1", "1"), ("Return", "Enter")], mover=vaiven(g, p, (p[0] - 40, p[1] - 70)))
    g.foto(dur=1200)
    return g.cuadros(recorte=rec, calidad=90)


@receta("modelar-escalar-raton")
def modelar_escalar(idioma):
    """La que no se recomienda: en el Modo Objeto, S X 0.45 y S Y 0.05; la Escala queda distinta en cada eje.
    Ctrl+A > Escalar la aplica: la forma no cambia y la Escala vuelve a 1."""
    g = modelar_guion(idioma)
    modelar_subido(g)
    rec, (cx, cy) = vista_pc(g)
    p = (cx + 200, cy + 140)
    g.raton_mover(*p)
    g.foto(dur=900)
    comando(g, [("s", "S"), ("x", "X"), ("=0.45", "0.45"), ("Return", "Enter")], mover=vaiven(g, p, (p[0] - 60, p[1] - 30)))
    comando(g, [("s", "S"), ("y", "Y"), ("=0.05", "0.05"), ("Return", "Enter")])
    g.foto(dur=400, tecla="Ctrl + A")
    g.mod("Ctrl")
    g.tecla("a")
    g.mod()
    g.foto(dur=700, tecla="Ctrl + A")
    x, y = g.info().item_texto(T(idioma, "Scale"))
    for px, py in recorrido(g.puntero_xy(), (x, y), 8):
        g.raton_mover(px, py)
        g.foto(dur=50)
    g.foto(dur=500)
    g.clic(x, y)
    g.foto(dur=2200)
    return g.cuadros(calidad=90)                  # (entera: se ve la Escala en Propiedades)


@receta("modelar-edicion-raton")
def modelar_edicion(idioma):
    """La que se recomienda: Tab al Modo Edicion (todo elegido), S X 0.45 y S Y 0.05. La Escala del objeto sigue en 1."""
    g = modelar_guion(idioma)
    modelar_subido(g)
    rec, (cx, cy) = vista_pc(g)
    p = (cx + 200, cy + 140)
    g.raton_mover(*p)
    g.foto(dur=900)
    comando(g, [("Tab", "Tab")])
    comando(g, [("s", "S"), ("x", "X"), ("=0.45", "0.45"), ("Return", "Enter")], mover=vaiven(g, p, (p[0] - 60, p[1] - 30)))
    comando(g, [("s", "S"), ("y", "Y"), ("=0.05", "0.05"), ("Return", "Enter")])
    return g.cuadros(calidad=90)


@receta("modelar-exacto-raton")
def modelar_exacto(idioma):
    """Con medidas exactas: en el Modo Edicion, el boton de seleccion > Cara, un click en el costado +X del cubo y
    G, X, -1.1, Enter: el cubo de 2 m queda de 0,9 m de ancho."""
    g = modelar_guion(idioma)
    modelar_subido(g)
    g.linea("modo edicion")
    rec, (cx, cy) = vista_pc(g)
    g.linea("cursor3d 1 0.4 0")                   # (el costado +X, abajo: lejos de la flecha roja del gizmo)
    fx, fy = g.info().cursor3d
    g.linea("cursor3d 0 0 0")
    g.raton_mover(cx + 200, cy + 140)
    g.foto(dur=1000)
    menu(g, BR_SELMODO, ("Face",))
    for px, py in recorrido(g.puntero_xy(), (fx, fy), 10):
        g.raton_mover(px, py)
        g.foto(dur=50)
    g.foto(dur=500, marcas=(("circulo", fx, fy, 30),))
    g.clic(fx, fy)
    g.foto(dur=900)
    comando(g, [("g", "G"), ("x", "X"), ("=-1.1", "-1.1"), ("Return", "Enter")], mover=vaiven(g, (fx, fy), (fx - 50, fy - 20)))
    return g.cuadros(recorte=rec, calidad=90)


def gizmo_escalar(g, dedo=False):
    """Dos clicks (o toques) en el centro del gizmo: mover -> rotar -> escalar. Despues la punta roja (X) y la verde (Y)
    se arrastran hacia el centro, a ojo, hasta que tenga forma de puerta."""
    centro = g.info().manija(CENTRO)
    cambiar_modo(g, dedo=dedo)
    cambiar_modo(g, dedo=dedo)
    for eje, factor in ((EJE_X, 0.45), (EJE_Y, 0.14)):
        p = g.info().manija(eje)
        destino = (int(centro[0] + (p[0] - centro[0]) * factor), int(centro[1] + (p[1] - centro[1]) * factor))
        if dedo:                                  # (en tactil los cuadraditos son grandes: la punta, mas afuera)
            p = mas_alla(centro, p, 22)
        agarrar(g, *p, dedo=dedo)
        arrastre(g, recorrido(p, destino, 18), dur=50, dedo=dedo)
        soltar(g, dedo=dedo)
        g.foto(dur=700)


@receta("modelar-gizmo-raton")
def modelar_gizmo(idioma):
    """En el Modo Edicion, con todo elegido: el gizmo pasa a escalar y se achica en X y en Y arrastrando, a ojo."""
    g = modelar_guion(idioma)
    modelar_subido(g)
    g.linea("modo edicion")
    rec, (cx, cy) = vista_pc(g)
    g.raton_mover(cx + 200, cy + 140)
    g.foto(dur=1000)
    gizmo_escalar(g)
    g.sin_puntero()
    g.foto(dur=1500)
    return g.cuadros(recorte=rec, calidad=90)


@receta("modelar-gizmo-dedo")
def modelar_gizmo_dedo(idioma):
    """Tactil: lo mismo con el dedo, sobre el gizmo (mas grande en una pantalla tactil)."""
    g = modelar_tactil(idioma)
    modelar_subido(g)
    g.linea("modo edicion")
    rec, _ = vista_pc(g)
    g.foto(dur=1000)
    gizmo_escalar(g, dedo=True)
    g.foto(dur=1500)
    return g.cuadros(recorte=rec, calidad=90)


@receta("modelar-ficha")
def modelar_ficha(idioma):
    """La ficha 'Ajustar la ultima operacion', desplegada, despues de subir el cubo con G Z 1: Mover X, Y y Z (cada
    valor se puede corregir), la orientacion, la edicion proporcional, Cancelar y Aceptar."""
    g = modelar_guion(idioma, raton=False)
    g.linea("trpanel on")
    rec, (cx, cy) = vista_pc(g)
    g.raton_mover(cx + 200, cy + 140)
    for t in ("g", "z"):
        g.tecla(t)
    g.linea("texto 1")
    g.tecla("Return")
    v = g.info().vp(VISTA_3D)
    g.clic(v["x"] + 100, v["y"] + v["h"] - 40)    # la ficha plegada ("> Mover"): un click la despliega
    g.sin_puntero()
    g.foto()
    return g.cuadros(recorte=(v["x"], v["y"] + v["h"] - 320, 560, 320))


# -- la misma puerta en una pantalla tactil: los comandos de la barra de herramientas y el teclado numerico --
TB_EJE = {"X": 103, "Y": 104, "Z": 105}


def modelar_tactil(idioma):
    g = modelar_guion(idioma, raton=False)
    v = g.info().vp(VISTA_3D)
    g.tocar(v["x"] + 40, v["y"] + v["barra"] + 40)   # el primer toque pasa la interfaz a tactil (y la barra cambia)
    g.sin_puntero()
    g.linea("deselectall")
    g.linea("selobj " + T(idioma, "Cube"))
    return g


def tocar_menu(g, rol, ids):
    tocar_en(g, *g.info().boton_rol(VISTA_3D, rol), despues=700)
    for nivel, ident in enumerate(ids):
        i = g.info()
        x, y = i.item_texto(T(g.idioma, ident), nivel) if isinstance(ident, str) else i.item(ident, nivel)
        tocar_en(g, x, y, antes=400, despues=700)


def ficha_tactil(g, mirar=900):
    """La ficha 'Ajustar la ultima operacion' que aparece abajo a la izquierda despues de la tilde (en lugar de la barra
    de herramientas): un toque en su titulo la despliega (los valores se pueden corregir) y Aceptar la cierra."""
    v = g.info().vp(VISTA_3D)
    tocar_en(g, v["x"] + 100, v["y"] + v["h"] - 40, antes=700, despues=mirar)
    tocar_en(g, v["x"] + 347, v["y"] + v["h"] - 50, antes=500, despues=700)
    g.linea("trboton 1")                          # (por si el toque no la cerro)


def numero_tactil(g, comando_txt, eje, valor, ficha=True):
    """Mover / Escalar en la barra de herramientas, el eje, un toque en la barra de arriba, el numero y OK; la tilde."""
    tocar_en(g, *g.info().herramienta(VISTA_3D, T(g.idioma, comando_txt)))
    tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_EJE[eje]))
    v = g.info().vp(VISTA_3D)
    tocar_en(g, v["x"] + v["w"] // 2, v["y"] + v["barra"] // 2, despues=800)
    for c in valor + "!":
        teclas_np = g.info().numpad
        t = "OK" if c == "!" else c
        if t not in teclas_np and t == "-":
            t = next(k for k in teclas_np if k in ("-", "−", "+/-", "±"))
        tocar_en(g, *teclas_np[t], antes=300, despues=400)
    tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_ACEPTAR), despues=1200)
    if ficha:
        ficha_tactil(g)


@receta("modelar-subir-dedo")
def modelar_subir_dedo(idioma):
    """Tactil: Mover, Z, la barra de arriba, 1, OK y la tilde: el cubo sube 1 metro. Despues aparece abajo la ficha
    'Ajustar la ultima operacion' (Mover X Y Z), que permite corregirla; Aceptar la cierra."""
    g = modelar_tactil(idioma)
    g.linea("trpanel on")
    g.foto(dur=900)
    numero_tactil(g, "Move", "Z", "1", ficha=False)
    ficha_tactil(g, mirar=2200)
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


@receta("modelar-escalar-dedo")
def modelar_escalar_dedo(idioma):
    """Tactil, la que no se recomienda: Escalar X 0.45 y Escalar Y 0.05 en el Modo Objeto; Objeto > Aplicar > Escalar."""
    g = modelar_tactil(idioma)
    g.linea("trpanel on")
    modelar_subido(g)
    g.foto(dur=900)
    numero_tactil(g, "Scale", "X", "0.45")
    numero_tactil(g, "Scale", "Y", "0.05")
    tocar_menu(g, BR_OBJETO, ("Apply", "Scale"))
    g.foto(dur=1800)
    return g.cuadros(calidad=90)


@receta("modelar-edicion-dedo")
def modelar_edicion_dedo(idioma):
    """Tactil, la que se recomienda: el boton del modo > Modo Edicion; Escalar X 0.45 y Escalar Y 0.05."""
    g = modelar_tactil(idioma)
    g.linea("trpanel on")
    modelar_subido(g)
    g.foto(dur=900)
    tocar_menu(g, BR_MODO, ("Edit Mode",))
    numero_tactil(g, "Scale", "X", "0.45")
    numero_tactil(g, "Scale", "Y", "0.05")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


@receta("modelar-exacto-dedo")
def modelar_exacto_dedo(idioma):
    """Tactil, con medidas exactas: Cara, un toque en el costado, Mover X -1.1."""
    g = modelar_tactil(idioma)
    g.linea("trpanel on")
    modelar_subido(g)
    g.linea("modo edicion")
    g.linea("cursor3d 1 0.4 0")
    fx, fy = g.info().cursor3d
    g.linea("cursor3d 0 0 0")
    g.foto(dur=900)
    tocar_menu(g, BR_SELMODO, ("Face",))
    tocar_en(g, fx, fy, antes=600, despues=900)
    numero_tactil(g, "Move", "X", "-1.1")
    g.foto(dur=1200)
    return g.cuadros(calidad=90)


# -- y en el Nokia N95 parado: 1/2/3 empiezan y eligen el eje, el lapiz + los numeros escriben el valor --
TECLA_N95 = {".": "asterisco", "-": "numeral"}


def n95_comando(g, modo, tecla_modo, eje, tecla_eje, valor):
    """1/2/3 empieza, 1/2/3 elige el eje, el lapiz mantenido + los numeros escriben el valor (* = punto, # = signo) y
    OK confirma. El cartel se va sumando ('3, 1, lapiz + 0.45, OK'); el numero crece en una sola entrada."""
    hechas = []
    def paso(rotulo, teclas_ids, accion, junto=False):
        if junto and hechas:
            hechas[-1] = rotulo
        else:
            hechas.append(rotulo)
        cartel = ",  ".join(hechas)
        g.foto(dur=450, tecla=cartel, teclas=teclas_ids)
        accion()
        g.foto(dur=650, tecla=cartel)
    paso(tecla_modo, ("num" + tecla_modo,), lambda: g.keypad("transformar", modo))
    paso(tecla_eje, ("num" + tecla_eje,), lambda: g.keypad("eje", eje))
    escrito = ""
    for k, c in enumerate(valor):
        escrito += c
        tid = TECLA_N95.get(c, "num" + c)
        paso("lápiz + " + escrito, ("lapiz", tid), lambda c=c: g.linea("texto " + c), junto=(k > 0))
    paso("OK", ("ok",), lambda: g.keypad("aceptar"))
    g.foto(dur=1200)


def modelar_n95(idioma):
    g = Guion(idioma, n95="vertical")
    g.linea("vista3d gizmo 0")                     # (el N95 viene sin gizmo y sin la barra de abajo)
    g.linea("vista3d barra 0")
    g.linea("vporbita 0 0.8 0 9 -32 18")
    return g


@receta("modelar-subir-nokia")
def modelar_subir_nokia(idioma):
    """N95: 1 (mover), 3 (el eje Z), lapiz + 1, OK: el cubo sube 1 metro."""
    g = modelar_n95(idioma)
    g.foto(dur=900)
    n95_comando(g, 1, "1", 2, "3", "1")
    return g.cuadros(calidad=88)


@receta("modelar-edicion-nokia")
def modelar_edicion_nokia(idioma):
    """N95: OK al Modo Edicion; 3 (escalar), 1 (X), lapiz + 0.45, OK; 3, 2 (Y), lapiz + 0.05, OK."""
    g = modelar_n95(idioma)
    modelar_subido(g)
    g.foto(dur=900)
    g.foto(dur=450, tecla="OK", teclas=("ok",))
    g.linea("modo edicion")
    g.foto(dur=900, tecla="OK")
    n95_comando(g, 3, "3", 0, "1", "0.45")
    n95_comando(g, 3, "3", 1, "2", "0.05")
    return g.cuadros(calidad=88)


# ---- tutorial 4: cambiar el origen de una puerta (tutoriales/puerta.md) ----------------------------------------
BR_MODO, BR_SELMODO = 1, 2
BISAGRA = "0.5 1 0"           # el centro de la cara angosta de la +X (la derecha, la que se ve): donde va la bisagra
BISAGRA_CLICK = "0.5 0.45 0"   # donde se hace click para elegirla: mas abajo, lejos de la flecha roja del gizmo
PUERTA_GIRO = float(os.environ.get("W3D_PUERTA_GIRO", "-25"))              # el angulo de la camara alrededor de la puerta
PUERTA_VENTANA = dict(w=1800, h=640)


def recorte_pasos(g):
    """La Vista 3D hasta donde termina el menu abierto (o un ancho comodo si no hay menu)."""
    i = g.info()
    v = i.vp(VISTA_3D)
    der = max([m["x"] + m["w"] for m in i.menus] or [0]) + 30
    ancho = min(v["w"], max(der - v["x"], 1000))
    return (v["x"], v["y"], ancho, v["h"])


def puerta_escena(g):
    """La puerta: un cubo de 1 m de ancho, 2 de alto y 10 cm de grueso, con la escala aplicada. La Vista 3D ancha
    (que los menus de la barra se abran adentro de ella y la foto no los corte)."""
    g.linea("division 0.78")
    g.linea("objname " + PUERTA[g.idioma])
    g.linea("objscale 0.5 1 0.05")
    g.linea("objpos 0 1 0")
    g.linea("apply scale")
    g.linea("vporbita 0 1 0 5.5 %g 26" % PUERTA_GIRO)   # (de frente y un poco a la derecha: se ve el costado de la bisagra)


def abrir_menu(g, rol, ids, kind=VISTA_3D):
    """Como menu(), pero sin elegir la ultima opcion: el puntero queda encima de ella. Devuelve el rectangulo de esa
    fila (para marcarla en la foto)."""
    x, y = g.info().boton_rol(kind, rol)
    g.raton_mover(x, y)
    g.clic(x, y)
    for nivel, ident in enumerate(ids):
        i = g.info()
        ix, iy = i.item_texto(T(g.idioma, ident), nivel) if isinstance(ident, str) else i.item(ident, nivel)
        x0, y0 = g.puntero_xy()
        if nivel:                                 # (a un submenu se entra de costado)
            g.raton_mover(ix, y0)
        g.raton_mover(ix, iy)
    i = g.info()
    nivel = len(ids) - 1
    m = [mm for mm in i.menus if mm["nivel"] == nivel][0]
    filas = sorted(it["cy"] for it in i.items if it["nivel"] == nivel)
    alto = min(b - a for a, b in zip(filas, filas[1:])) if len(filas) > 1 else 34
    return (m["x"] + 6, iy - alto // 2, m["w"] - 12, alto)


def puerta_paso_menu(idioma, previo, rol, ids):
    g = Guion(idioma, **PUERTA_VENTANA)
    puerta_escena(g)
    for l in previo:
        if l == "bisagra":
            elegir_bisagra(g)
        else:
            g.linea(l)
    fila = abrir_menu(g, rol, ids)
    rec = recorte_pasos(g)
    g.foto(marcas=(("rect",) + tuple(fila),))
    return g.cuadros(recorte=rec)


@receta("puerta-paso-1")
def puerta_paso_1(idioma):
    """Paso 1: el boton del modo, abierto, con Modo Edicion marcado."""
    return puerta_paso_menu(idioma, [], BR_MODO, ("Edit Mode",))


@receta("puerta-paso-2")
def puerta_paso_2(idioma):
    """Paso 2: en el Modo Edicion, el boton Vertice / Arista / Cara, abierto, con Cara marcado."""
    return puerta_paso_menu(idioma, ["modo edicion"], BR_SELMODO, ("Face",))


def elegir_bisagra(g):
    """En el Modo Edicion: modo Cara y un click en el costado de la bisagra. Devuelve donde fue el click."""
    g.linea("caras 3")                            # (pasa a modo Cara; el click de abajo elige la de la bisagra)
    g.linea("cursor3d " + BISAGRA_CLICK)          # (donde queda ese costado en la pantalla)
    x, y = g.info().cursor3d
    g.linea("cursor3d 0 0 0")
    g.clic(x, y)
    return x, y


@receta("puerta-paso-3")
def puerta_paso_3(idioma):
    """Paso 3: la cara angosta del lado de la bisagra, elegida (en verde) y marcada, con el puntero encima."""
    g = Guion(idioma, **PUERTA_VENTANA)
    puerta_escena(g)
    g.linea("modo edicion")
    x, y = elegir_bisagra(g)
    rec = recorte_pasos(g)
    g.raton_mover(x, y)
    g.foto(marcas=(("circulo", x, y, 34),))
    return g.cuadros(recorte=rec)


@receta("puerta-paso-4")
def puerta_paso_4(idioma):
    """Paso 4: menu Malla > Iman, abierto, con Cursor a lo seleccionado marcado."""
    return puerta_paso_menu(idioma, ["modo edicion", "bisagra"], BR_MALLA, ("Snap", IMAN_CURSOR_A_SELECCION))


@receta("puerta-paso-5")
def puerta_paso_5(idioma):
    """Paso 5: con el cursor 3D en la bisagra, el boton del modo abierto, con Modo Objeto marcado."""
    return puerta_paso_menu(idioma, ["modo edicion", "bisagra", "cursor3d " + BISAGRA], BR_MODO, ("Object Mode",))


@receta("puerta-paso-6")
def puerta_paso_6(idioma):
    """Paso 6: menu Objeto > Definir Origen, abierto, con Origen al Cursor 3D marcado."""
    return puerta_paso_menu(idioma, ["cursor3d " + BISAGRA], BR_OBJETO, (OBJ_DEFINIR_ORIGEN, ORIGEN_AL_CURSOR))


def puerta_girar(g, angulo=1.57):
    """Con el gizmo: un click en su centro lo pasa a Rotar; el anillo azul se arrastra suave hasta 'angulo' (radianes,
    en la pantalla) y de vuelta. Se ve como se mueve el mouse y donde hace click."""
    rec, (cx, cy) = vista_pc(g)
    centro = g.info().manija(CENTRO)
    g.raton_mover(cx + 260, cy + 170)
    g.foto(dur=1000)
    for x, y in recorrido((cx + 260, cy + 170), centro, 10):
        g.raton_mover(x, y)
        g.foto(dur=40)
    cambiar_modo(g)                               # mover -> rotar
    p = g.info().manija(EJE_Z)
    for x, y in recorrido(centro, p, 8):
        g.raton_mover(x, y)
        g.foto(dur=40)
    agarrar(g, *p)
    ida = arco(centro, p, angulo, 36)
    arrastre(g, ida, dur=40)
    g.foto(dur=700)
    arrastre(g, arco(centro, ida[-1], -angulo, 36), dur=40)
    soltar(g)
    g.foto(dur=600)
    return rec


@receta("puerta-gira-mal-raton")
def puerta_gira_mal(idioma):
    """La puerta, con el origen en el centro: con el gizmo de rotar gira sobre si misma, como una puerta giratoria."""
    g = Guion(idioma, w=1400, h=640, raton=True)
    puerta_escena(g)
    rec = puerta_girar(g)
    g.sin_puntero()
    g.foto(dur=500)
    return g.cuadros(recorte=rec, ancho=960, calidad=92)


@receta("puerta-gira-bien-raton")
def puerta_gira_bien(idioma):
    """La puerta, con el origen en la bisagra (los pasos ya hechos): con el gizmo de rotar se abre y se cierra."""
    g = Guion(idioma, w=1400, h=640, raton=True)
    puerta_escena(g)
    g.linea("cursor3d " + BISAGRA)
    menu(g, BR_OBJETO, (OBJ_DEFINIR_ORIGEN, ORIGEN_AL_CURSOR))
    g.fotos = []                                  # (lo de arriba es la preparacion: no va en el video)
    rec = puerta_girar(g)
    g.sin_puntero()
    g.foto(dur=500)
    return g.cuadros(recorte=rec, ancho=960, calidad=92)


def puerta_tactil(idioma):
    """La puerta en una pantalla tactil: el primer toque pasa la interfaz a tactil (en el cielo vacio) y se vuelve a
    elegir la puerta."""
    g = Guion(idioma, **PUERTA_VENTANA)
    puerta_escena(g)
    v = g.info().vp(VISTA_3D)
    g.tocar(v["x"] + 40, v["y"] + v["barra"] + 40)
    g.sin_puntero()
    g.linea("deselectall")
    g.linea("selobj " + PUERTA[g.idioma])
    return g


def puerta_girar_dedo(g, angulo=1.57):
    """Con el dedo: un toque en el centro del gizmo lo pasa a Rotar; el anillo azul se arrastra hasta 'angulo' y de
    vuelta. El dedo llega flotando a cada lugar (se ve adonde va)."""
    rec, _ = vista_pc(g)
    centro = g.info().manija(CENTRO)
    cambiar_modo(g, dedo=True)                    # mover -> rotar
    p = g.info().manija(EJE_Z)
    agarrar(g, *p, dedo=True)
    ida = arco(centro, p, angulo, 36)
    arrastre(g, ida, dur=40, dedo=True)
    g.foto(dur=700)
    arrastre(g, arco(centro, ida[-1], -angulo, 36), dur=40, dedo=True)
    soltar(g, dedo=True)
    g.foto(dur=900)
    return rec


@receta("puerta-gira-mal-dedo")
def puerta_gira_mal_dedo(idioma):
    """Tactil: con el origen en el centro, la puerta gira sobre si misma."""
    g = puerta_tactil(idioma)
    rec = puerta_girar_dedo(g)
    return g.cuadros(recorte=rec, ancho=960, calidad=90)


@receta("puerta-gira-bien-dedo")
def puerta_gira_bien_dedo(idioma):
    """Tactil: con el origen en la bisagra, la puerta se abre y se cierra desde ahi."""
    g = puerta_tactil(idioma)
    g.linea("cursor3d " + BISAGRA)
    g.linea("origen cursor")
    rec = puerta_girar_dedo(g)
    return g.cuadros(recorte=rec, ancho=960, calidad=90)


def puerta_n95(idioma):
    """La puerta en el Nokia N95 parado (sin gizmo ni barra de herramientas, como viene)."""
    g = Guion(idioma, n95="vertical")
    g.linea("objname " + PUERTA[idioma])
    g.linea("objscale 0.5 1 0.05")
    g.linea("objpos 0 1 0")
    g.linea("apply scale")
    g.linea("vista3d gizmo 0")
    g.linea("vista3d barra 0")
    g.linea("vporbita 0 1 0 6 -25 26")
    return g


def puerta_girar_n95(g, cuadros=32):
    """2 (rotar), 3 (el eje Z), la flecha derecha mantenida la abre, la izquierda la cierra y OK confirma."""
    g.foto(dur=900)
    hechas = []
    def cartel(rotulo):
        hechas.append(rotulo)
        return ",  ".join(hechas)
    c = cartel("2")
    g.foto(dur=450, tecla=c, teclas=("num2",)); g.keypad("transformar", 2); g.foto(dur=500, tecla=c)
    c = cartel("3")
    g.foto(dur=450, tecla=c, teclas=("num3",)); g.keypad("eje", 2); g.foto(dur=500, tecla=c)
    for flecha in ("derecha", "izquierda"):
        c = cartel(FLECHA[flecha])
        dx, dy = MOVIDA[flecha]
        for _ in range(cuadros):
            g.keypad("transflechas", dx, dy, 3)   # (3 cuadros de la flecha mantenida por foto: unos 80 grados)
            g.foto(dur=60, tecla=c, teclas=(flecha,))
        g.foto(dur=600, tecla=c)
    c = cartel("OK")
    g.foto(dur=450, tecla=c, teclas=("ok",)); g.keypad("aceptar"); g.foto(dur=1200)


@receta("puerta-gira-mal-nokia")
def puerta_gira_mal_nokia(idioma):
    """N95: con el origen en el centro, 2, 3 y las flechas la giran sobre si misma."""
    g = puerta_n95(idioma)
    puerta_girar_n95(g)
    return g.cuadros(calidad=88)


@receta("puerta-gira-bien-nokia")
def puerta_gira_bien_nokia(idioma):
    """N95: con el origen en la bisagra, 2, 3 y las flechas la abren y la cierran desde ahi."""
    g = puerta_n95(idioma)
    g.linea("cursor3d " + BISAGRA)
    g.linea("origen cursor")
    puerta_girar_n95(g)
    return g.cuadros(calidad=88)


# ---- elegir con una caja y el menu contextual --------------------------------------------------------------------
@receta("objeto-caja-raton")
def caja_raton(idioma):
    """Cinco cajas en fila. De izquierda a derecha (verde): entran las dos enteras adentro. De derecha a izquierda
    (azul), la misma caja: entra tambien la que apenas toca."""
    g = Guion(idioma, raton=True)
    g.linea("delete")
    nombres = []
    for k in range(5):
        g.linea("add cube")
        n = "%s%d" % (CAJA[idioma], k + 1)
        g.linea("objname " + n)
        g.linea("objscale 0.4 0.4 0.4")
        g.linea("objpos %g 0.4 0" % (-3.2 + 1.6 * k))
        nombres.append(n)
    g.linea("deselectall")
    g.linea("selobj " + T(idioma, "Light"))       # (la luz, al fondo: caia en la fila)
    g.linea("objpos 3 4 -6")
    g.linea("deselectall")
    g.linea("vporbita 0 0.4 0 7.5 -35 0")
    rec, (cx, cy) = vista_pc(g)
    obj = g.info().objetos
    p = [(obj[n]["x"], obj[n]["y"]) for n in nombres]
    paso = p[1][0] - p[0][0]
    # la caja: de la mitad entre la 1 y la 2, hasta el centro de la 4 (la 4 queda partida al medio)
    izq, der = p[0][0] + paso // 2, p[3][0]
    arriba, abajo = p[0][1] - int(paso * 0.55), p[0][1] + int(paso * 0.55)
    g.raton_mover(izq - 40, abajo + 60)
    g.foto(dur=900)
    for a, b in (((izq, arriba), (der, abajo)), ((der, arriba), (izq, abajo))):
        g.raton_mover(*a)
        g.foto(dur=500)
        g.raton_abajo(*a)
        g.foto(dur=200)
        for x, y in recorrido(a, b, 12):
            g.raton_mover(x, y)
            g.foto(dur=60)
        g.foto(dur=600)
        g.raton_arriba()
        g.foto(dur=1500)
        g.linea("deselectall")
        g.foto(dur=500)
    return g.cuadros(recorte=rec, calidad=72)


@receta("objeto-menu-contextual")
def menu_contextual(idioma):
    """El menu contextual (click derecho) abierto sobre Leon."""
    g = Guion(idioma, w=960, h=760, raton=True)
    escena_leon(g)
    _, (cx, cy) = vista_pc(g)
    x, y = cx - 150, cy - 120                    # (a la izquierda: que el menu entre en la Vista 3D)
    g.raton_mover(x, y)
    g.raton_abajo(x, y, "der")
    g.raton_arriba("der")
    g.foto()
    return g.cuadros()


@receta("objeto-menu-contextual-dedo")
def menu_contextual_dedo(idioma):
    """Tactil: un dedo quieto sobre Leon, medio segundo: se abre el menu contextual."""
    g = Guion(idioma, w=960, h=760)
    escena_leon(g)
    tactil(g)
    _, (cx, cy) = vista_pc(g)
    x, y = cx - 150, cy - 120
    g.dedo_flotando(x, y)
    g.foto(dur=800)
    g.dedo_abajo(x, y)
    g.foto(dur=900)                              # (el dedo quieto: el menu todavia no salio)
    g.esperar(550)                               # (el reloj de los controles cuenta el medio segundo)
    g.linea("tick")
    g.foto(dur=1100)
    g.dedo_arriba()
    g.dedos_quitar()
    g.foto(dur=2000)
    return g.cuadros(calidad=74)


# ---- el VACIO: como se dibuja (la forma, el tamanio y el giro del dibujo; solo la vista) ------------------------------
VACIO_FORMAS = [("Cross", 0, (0, 0, 0)), ("Square", 1, (0, 0, 0)), ("Cube", 4, (0, 0, 0)), ("Circle", 2, (0, 0, 0)),
                ("Sphere", 3, (0, 0, 0)), ("Circle", 2, (90, 0, 0))]


@receta("objeto-vacio-formas")
def vacio_formas(idioma):
    """Cinco Vacios en fila: la cruz, el cuadrado, el circulo (el de siempre, acostado en el piso), la esfera y un
    circulo girado 90 grados en X (parado). Debajo de cada uno, su forma."""
    g = Guion(idioma, w=1700, h=520)
    for n in ("Cube", "Camera", "Light"):         # (solo los Vacios en la foto)
        g.linea("selobj " + T(idioma, n))
        g.linea("delete")
    g.linea("division 0.95")
    for i, (_, forma, rot) in enumerate(VACIO_FORMAS):
        g.linea("cursor3d %g 0 0" % (-6.5 + i * 2.6))
        g.linea("agregar vacio")
        g.linea("vacioponer %d 1.6 %g %g %g" % (forma, rot[0], rot[1], rot[2]))
    g.linea("cursor3d 0 -30 0")                  # (el cursor 3D, fuera de cuadro)
    g.linea("deselectall")
    g.linea("vista3d gizmo 0")
    g.linea("vporbita 0 0.3 0 10 -30 0")          # (inclinacion, giro: desde arriba, de frente)
    g.linea("n95 perspectiva")                    # ORTOGRAFICA: sin la deformacion de la lente a los costados
    i = g.info()
    v = i.vp(VISTA_3D)
    marcas = []
    vacios = sorted([o for n, o in i.objetos.items() if n.startswith(T(idioma, "Empty"))], key=lambda o: o["x"])
    for (nombre, _, rot), o in zip(VACIO_FORMAS, vacios):
        texto = T(idioma, nombre) + (" (X 90°)" if rot[0] else "")
        marcas.append(("etiqueta", o["x"], o["y"] + 105, texto))
    g.foto(marcas=marcas)
    return g.cuadros(recorte=(v["x"], v["y"] + v["barra"], v["w"], v["h"] - v["barra"]), calidad=90)

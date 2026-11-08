"""Recetas de los articulos de tutorial 1 (Los viewports): La Vista 3D, El Outliner y Propiedades
(docs/manual/<idioma>/tutoriales/vista-3d.md, outliner.md, propiedades.md)."""
from capturas import receta, Guion, T, VISTA_3D, OUTLINER, PROPIEDADES
from recetas_objeto import recorrido, teclas, deshacer, mover_raton


def guion_props(idioma, w=960, h=720, raton=True, **kw):
    """La columna de la derecha ancha (Propiedades y Outliner legibles)."""
    g = Guion(idioma, w=w, h=h, raton=raton, **kw)
    g.linea("division 0.6")
    return g


def recorte_props(g):
    v = g.info().vp(PROPIEDADES)
    return (v["x"], v["y"], v["w"], v["h"])


def elegir(g, nombre_en):
    g.linea("deselectall")
    g.linea("selobj " + T(g.idioma, nombre_en))


def clic_en(g, x, y, antes=450, despues=700):
    """El mouse va hasta (x, y) y hace click (con una foto antes y otra despues)."""
    if g.puntero:
        for px, py in recorrido(g.puntero_xy(), (x, y), 6):
            g.raton_mover(px, py)
    else:
        g.raton_mover(x, y)
    g.foto(dur=antes)
    g.raton_abajo(x, y)
    g.foto(dur=150)
    g.raton_arriba()
    g.foto(dur=despues)


# ---- Propiedades ---------------------------------------------------------------------------------------------------
@receta("propiedades-pestanas")
def pestanas(idioma):
    """Las pestanias cambian con lo elegido: el cubo, la luz, la camara (la barra de pestanias marcada)."""
    g = guion_props(idioma, raton=False)
    g.linea("propstab 2")                         # (el cubo, en la pestania del material)
    rec = recorte_props(g)
    for nombre in ("Cube", "Light", "Camera"):
        elegir(g, nombre)
        i = g.info()
        ps = i.pestanias
        x0, y0 = min(p["x"] for p in ps) - 3, min(p["y"] for p in ps) - 3
        x1, y1 = max(p["x"] + p["w"] for p in ps) + 3, max(p["y"] + p["h"] for p in ps) + 3
        g.foto(dur=1700, marcas=(("rect", x0, y0, x1 - x0, y1 - y0),))
    return g.cuadros(recorte=rec, calidad=80)


@receta("propiedades-tarjetas")
def tarjetas(idioma):
    """Un click en el titulo de la tarjeta Transformar la pliega; otro la despliega."""
    g = guion_props(idioma)
    rec = recorte_props(g)
    t = g.info().tarjeta(T(idioma, "Transform"))
    x, y = t["x"] + t["w"] // 3, t["y"] + t["h"] // 2
    g.raton_mover(x + 60, y + 200)
    g.foto(dur=800)
    clic_en(g, x, y, despues=1300)
    clic_en(g, x, y, despues=1300)
    return g.cuadros(recorte=rec, calidad=80)


@receta("propiedades-numero-raton")
def numero_raton(idioma):
    """PC: un click en la Posicion X, se escribe 2 y Enter: el cubo se corre 2 metros."""
    g = guion_props(idioma)
    p = g.info().prop(T(idioma, "Location") + " X")
    g.raton_mover(p["valor"] - 40, p["y"] + 120)
    g.foto(dur=800)
    clic_en(g, p["valor"], p["y"], despues=700)
    teclas(g, ["=2"], "2", dur=600)
    teclas(g, ["Return"], "Enter", dur=1600)
    return g.cuadros(calidad=78)


@receta("propiedades-arrastrar-raton")
def arrastrar_raton(idioma):
    """PC: arrastrar la Posicion X a la derecha y a la izquierda: el valor cambia y el cubo lo sigue."""
    g = guion_props(idioma)
    p = g.info().prop(T(idioma, "Location") + " X")
    x, y = p["valor"], p["y"]
    g.raton_mover(x - 30, y + 100)
    g.foto(dur=700)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.raton_abajo(x, y)
    g.foto(dur=250)
    for px, py in recorrido((x, y), (x + 90, y), 12) + recorrido((x + 90, y), (x - 60, y), 16) + recorrido((x - 60, y), (x, y), 10):
        g.raton_mover(px, py)
        g.foto(dur=60)
    g.raton_mover(x, y)
    g.raton_arriba()
    g.foto(dur=1000)
    deshacer(g)
    return g.cuadros(calidad=74)


@receta("propiedades-desplegable")
def desplegable(idioma):
    """La luz: el desplegable Tipo se abre y se elige Sol."""
    g = guion_props(idioma)
    elegir(g, "Light")
    g.linea("propstab 2")
    rec = recorte_props(g)
    p = g.info().prop(T(idioma, "Type"))
    g.raton_mover(p["valor"] - 30, p["y"] + 120)
    g.foto(dur=700)
    clic_en(g, p["valor"], p["y"], despues=900)
    ix, iy = g.info().item_texto(T(idioma, "Sun"))
    clic_en(g, ix, iy, antes=800, despues=1600)
    return g.cuadros(recorte=rec, calidad=78)


@receta("propiedades-color")
def color(idioma):
    """La luz: un click en el color Difuso abre el selector de color."""
    g = guion_props(idioma)
    elegir(g, "Light")
    g.linea("propstab 2")
    p = g.info().prop(T(idioma, "Diffuse"))
    g.raton_mover(p["valor"] - 30, p["y"] + 120)
    g.foto(dur=700)
    clic_en(g, p["valor"], p["y"], despues=2200)
    return g.cuadros(calidad=78)


@receta("propiedades-texto")
def texto(idioma):
    """El nombre del objeto: un click en el campo Nombre, se escribe otro y Enter (el Outliner lo muestra)."""
    g = guion_props(idioma)
    p = g.info().prop(T(idioma, "Name"))
    g.raton_mover(p["valor"] - 30, p["y"] - 100)
    g.foto(dur=700)
    clic_en(g, p["valor"], p["y"], despues=600)
    nuevo = {"es": "Caja", "en": "Box", "pt": "Caixa"}.get(idioma, T(idioma, "Cube") + "2")
    g.mod("Ctrl")
    g.tecla("a")                                  # (todo el texto elegido: lo que se escribe lo reemplaza)
    g.mod()
    teclas(g, ["=" + nuevo], nuevo, dur=600)
    teclas(g, ["Return"], "Enter", dur=1800)
    return g.cuadros(calidad=78)


@receta("propiedades-casilla")
def casilla(idioma):
    """Un click en la casilla Visible: el cubo desaparece de la vista; otro click, vuelve."""
    g = guion_props(idioma)
    p = g.info().prop(T(idioma, "Visible"))
    x = p["valor"]
    g.raton_mover(x - 30, p["y"] - 100)
    g.foto(dur=700)
    clic_en(g, x, p["y"], despues=1400)
    clic_en(g, x, p["y"], despues=1400)
    return g.cuadros(calidad=78)


@receta("propiedades-lista")
def lista(idioma):
    """Una lista con sus botones: el cubo con dos modificadores; el de abajo sube con Subir."""
    g = guion_props(idioma)
    g.linea("agregar modificador 1")              # Espejo
    g.linea("agregar modificador 3")              # Superficie de subdivision (queda elegido)
    g.linea("propstab 4")
    rec = recorte_props(g)
    i = g.info()
    t = i.tarjeta(T(idioma, "Modifiers"))
    filas = [p for p in i.props if p["g"] == t["i"] and not p.get("nombre")]   # [Anadir | Quitar], [Subir | Bajar]
    sub = filas[1]
    x = t["x"] + t["w"] // 4                      # (la mitad izquierda de la fila: Subir)
    g.raton_mover(x, sub["y"] + 120)
    g.foto(dur=1000)
    clic_en(g, x, sub["y"], despues=1600)
    return g.cuadros(recorte=rec, calidad=78)


@receta("propiedades-rombo")
def rombo(idioma):
    """El rombo de la Posicion X: gris (sin animar); un click guarda un keyframe (verde); en otro frame queda tenue;
    cambiar el valor lo marca (cambio sin guardar)."""
    g = guion_props(idioma)
    rec = recorte_props(g)
    p = g.info().prop(T(idioma, "Location") + " X")
    marca = ("circulo", p["rombo"], p["y"], 16)
    g.raton_mover(p["rombo"] - 60, p["y"] + 90)
    g.foto(dur=1300, marcas=(marca,))
    clic_en(g, p["rombo"], p["y"], despues=1400)
    g.linea("frame 20")
    g.foto(dur=1500, marcas=(marca,))
    g.linea("objpos 1 0 0")                       # (cambiar el valor no cambia el color: sigue tenue)
    g.foto(dur=1500, marcas=(marca,))
    clic_en(g, p["rombo"], p["y"], despues=1600)  # un click: el keyframe nuevo, verde otra vez
    return g.cuadros(recorte=rec, calidad=80)


# ---- Propiedades con el dedo y en el Nokia ------------------------------------------------------------------------
def tactil_props(g):
    """El primer toque pasa la interfaz a tactil (un toque en el vacio del Outliner: no cambia nada)."""
    v = g.info().vp(OUTLINER)
    g.tocar(v["x"] + v["w"] // 2, v["y"] + v["h"] - 12)
    g.sin_puntero()


def tocar_en(g, x, y, antes=500, despues=600):
    g.dedo_flotando(x, y)
    g.foto(dur=antes)
    g.dedo_abajo(x, y)
    g.foto(dur=150)
    g.dedo_arriba()
    g.dedos_quitar()
    g.foto(dur=despues)


@receta("propiedades-numero-dedo")
def numero_dedo(idioma):
    """Tactil: un toque en la Posicion X abre el teclado numerico; 2*3 y OK: queda en 6."""
    g = guion_props(idioma, raton=False)
    tactil_props(g)
    p = g.info().prop(T(idioma, "Location") + " X")
    g.foto(dur=700)
    tocar_en(g, p["valor"], p["y"], despues=900)
    for t in ("2", "*", "3", "OK"):
        x, y = g.info().numpad[t]
        tocar_en(g, x, y, antes=350, despues=450)
    g.foto(dur=1600)
    return g.cuadros(calidad=78)


@receta("propiedades-arrastrar-dedo")
def arrastrar_dedo(idioma):
    """Tactil: el dedo sobre el valor de la Posicion X, arrastrado a la derecha y a la izquierda."""
    g = guion_props(idioma, raton=False)
    tactil_props(g)
    p = g.info().prop(T(idioma, "Location") + " X")
    x, y = p["valor"], p["y"]
    g.dedo_flotando(x, y)
    g.foto(dur=700)
    g.dedo_abajo(x, y)
    g.esperar(220)
    g.foto(dur=250)
    for px, py in recorrido((x, y), (x + 90, y), 12) + recorrido((x + 90, y), (x - 60, y), 16) + recorrido((x - 60, y), (x, y), 10):
        g.dedo_mover(px, py)
        g.foto(dur=60)
    g.dedo_mover(x, y)
    g.dedo_arriba()
    g.dedos_quitar()
    g.foto(dur=1200)
    return g.cuadros(calidad=74)


@receta("propiedades-nokia")
def nokia(idioma):
    """N95: la tecla verde pasa a Propiedades; abajo hasta la Posicion X; OK; 2; OK."""
    from recetas import TECLA_VERDE
    g = Guion(idioma, n95="vertical")
    g.linea("vista3d gizmo 0")                     # (el N95 viene sin gizmo y sin la barra de abajo)
    g.linea("vista3d barra 0")
    g.foto(dur=900)
    g.foto(dur=350, teclas=("verde",), tecla=TECLA_VERDE[idioma])
    g.verde()
    g.foto(dur=700)
    g.foto(dur=250, teclas=("abajo",), tecla="↓")  # (de la barra de pestanias a la primera fila: la Posicion X)
    g.tecla("Down")
    g.foto(dur=500)
    g.foto(dur=300, teclas=("ok",), tecla="OK")
    g.tecla("Return")
    g.foto(dur=600)
    g.foto(dur=300, teclas=("num2",), tecla="2")
    g.linea("texto 2")
    g.foto(dur=600)
    g.foto(dur=300, teclas=("ok",), tecla="OK")
    g.tecla("Return")
    g.foto(dur=1800)
    return g.cuadros(calidad=70)


# ---- El Outliner ---------------------------------------------------------------------------------------------------
def guion_outliner(idioma, h=720):
    g = Guion(idioma, w=960, h=h, raton=True)
    g.linea("division 0.6")                       # (el Outliner ancho: que se lean los nombres)
    return g


def con_tecla_mod(g, mod, x, y, despues=900):
    """Un click en (x, y) con una tecla mantenida (Shift, Ctrl), con su cartel."""
    g.mod(mod)
    g.foto(dur=300, tecla=mod)
    clic_en(g, x, y, antes=350, despues=despues)
    g.mod()


@receta("outliner-seleccionar-raton")
def out_seleccionar(idioma):
    """Click en la Camara; Shift + click en el Cubo (el rango); click en la Luz (sola); Ctrl + click en el Cubo (se
    suma) y otra vez (se saca)."""
    g = guion_outliner(idioma)
    cam, luz, cubo = T(idioma, "Camera"), T(idioma, "Light"), T(idioma, "Cube")
    i = g.info()
    g.raton_mover(i.fila(cam)[0] + 40, i.fila(cam)[1] + 60)
    g.foto(dur=700)
    clic_en(g, *i.fila(cam), despues=900)
    con_tecla_mod(g, "Shift", *i.fila(cubo))
    clic_en(g, *i.fila(luz), despues=900)
    con_tecla_mod(g, "Ctrl", *i.fila(cubo))
    con_tecla_mod(g, "Ctrl", *i.fila(cubo), despues=1300)
    return g.cuadros(calidad=78)


@receta("outliner-ocultar-raton")
def out_ocultar(idioma):
    """El ojo del Cubo: se oculta en la Vista 3D y vuelve. La camara del Cubo: sale del render (sigue a la vista)."""
    g = guion_outliner(idioma)
    f = g.info().fila_info(T(idioma, "Cube"))
    g.raton_mover(f["ojo"] - 60, f["y"] + 60)
    g.foto(dur=700)
    clic_en(g, f["ojo"], f["y"], despues=1400)
    clic_en(g, f["ojo"], f["y"], despues=1000)
    clic_en(g, f["camara"], f["y"], despues=1600)
    return g.cuadros(calidad=78)


@receta("outliner-mover-raton")
def out_mover(idioma):
    """La Luz se arrastra al borde de arriba de la Camara (queda primera); la Camara, al medio de la fila del Cubo (queda
    adentro)."""
    from recetas_objeto import agarrar, arrastre, soltar
    g = guion_outliner(idioma)
    cam, luz, cubo = T(idioma, "Camera"), T(idioma, "Light"), T(idioma, "Cube")
    i = g.info()
    alto = abs(i.fila(luz)[1] - i.fila(cam)[1])   # (el alto de una fila)
    a, b = i.fila(luz), i.fila(cam)
    g.raton_mover(a[0] + 50, a[1] + 60)
    g.foto(dur=700)
    agarrar(g, *a)
    arrastre(g, recorrido(a, (b[0] + 10, b[1] - alto // 2 + 3), 10), dur=70)
    g.foto(dur=500)
    soltar(g)
    g.foto(dur=900)
    i = g.info()
    a, b = i.fila(cam), i.fila(cubo)
    agarrar(g, *a)
    arrastre(g, recorrido(a, (b[0] + 10, b[1]), 10), dur=70)
    g.foto(dur=500)
    soltar(g)
    g.foto(dur=1800)
    return g.cuadros(calidad=76)


@receta("outliner-plegar-raton")
def out_plegar(idioma):
    """La flecha de la Coleccion: se pliega (no se ven sus hijos) y se despliega."""
    g = guion_outliner(idioma)
    f = g.info().fila_info(T(idioma, "Collection"))
    g.raton_mover(f["flecha"] + 60, f["y"] + 80)
    g.foto(dur=700)
    clic_en(g, f["flecha"], f["y"], despues=1400)
    clic_en(g, f["flecha"], f["y"], despues=1400)
    return g.cuadros(calidad=78)


@receta("outliner-renombrar-raton")
def out_renombrar(idioma):
    """Doble click en el nombre del Cubo, se escribe Caja y Enter. Despues: la Luz con F2, Lampara y Enter."""
    from recetas_objeto import CAJA
    g = guion_outliner(idioma)
    lampara = {"es": "Lámpara", "en": "Lamp", "pt": "Lâmpada", "fr": "Lampe", "it": "Lampada", "de": "Lampe",
               "pl": "Lampa", "tr": "Lamba", "ru": "Лампа"}.get(idioma, "Lamp")
    i = g.info()
    x, y = i.fila(T(idioma, "Cube"))
    g.raton_mover(x + 40, y + 60)
    g.foto(dur=700)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.lineas.append("raton doble %d %d izq" % (x, y))
    g.foto(dur=700, tecla="2 clicks")
    g.mod("Ctrl"); g.tecla("a"); g.mod()
    teclas(g, ["=" + CAJA[idioma]], CAJA[idioma], dur=500)
    teclas(g, ["Return"], "Enter", dur=1000)
    x, y = g.info().fila(T(idioma, "Light"))
    clic_en(g, x, y, despues=500)
    teclas(g, ["F2"], "F2", dur=600)
    g.mod("Ctrl"); g.tecla("a"); g.mod()
    teclas(g, ["=" + lampara], lampara, dur=500)
    teclas(g, ["Return"], "Enter", dur=1600)
    return g.cuadros(calidad=78)


@receta("outliner-activo-raton")
def out_activo(idioma):
    """Muchos objetos: se elige uno en la Vista 3D cuya fila no se ve; con el mouse sobre el Outliner, Num . lleva la
    lista hasta el (la fila queda en el medio)."""
    g = guion_outliner(idioma)
    g.linea("delete")
    nombres = []
    for k in range(36):
        g.linea("add cube")
        n = "%s%02d" % (T(idioma, "Cube"), k + 1)
        g.linea("objname " + n)
        g.linea("objscale 0.3 0.3 0.3")
        g.linea("objpos %g 0.3 %g" % ((k % 6) * 1.2 - 3, (k // 6) * 1.2 - 3))
        nombres.append(n)
    g.linea("deselectall")
    g.linea("vporbita 0 0 0 11 -50 20")
    i = g.info()
    obj = i.objetos[nombres[-1]]
    v = i.vp(OUTLINER)
    g.raton_mover(obj["x"] - 40, obj["y"] + 40)
    g.foto(dur=800)
    clic_en(g, obj["x"], obj["y"], despues=900)
    mx, my = v["x"] + v["w"] // 2, v["y"] + v["h"] // 2
    for px, py in recorrido(g.puntero_xy(), (mx, my), 8):
        g.raton_mover(px, py)
        g.foto(dur=50)
    g.foto(dur=500)
    teclas(g, ["Keypad_."], "Num .", dur=2000)
    return g.cuadros(calidad=76)


@receta("outliner-nokia")
def out_nokia(idioma):
    """N95: Propiedades pasa a Outliner y se activa con la tecla verde. Abajo hasta la Luz, 1 (mover), arriba (sube),
    1 (confirma); abajo hasta el Cubo, 2 (se oculta), 2 (vuelve), 3 (sale del render)."""
    from recetas import TECLA_VERDE
    from capturas import MENU_TIPO
    g = Guion(idioma, n95="vertical")
    g.linea("vista3d gizmo 0")
    g.linea("vista3d barra 0")
    i = g.info()
    x, y = i.esquina(PROPIEDADES)                  # (el viewport de abajo pasa a ser un Outliner, como en el tutorial 1)
    g.clic(x, y)
    g.clic(*g.info().item(MENU_TIPO(OUTLINER)))
    g.sin_puntero()
    g.foto(dur=900)
    def tecla(cartel, ident, lineas, dur=600):
        g.foto(dur=300, teclas=(ident,), tecla=cartel)
        for l in lineas:
            g.linea(l)
        g.foto(dur=dur)
    tecla(TECLA_VERDE[idioma], "verde", ["verde"])
    # hasta la Luz: la 1ra flecha entra en Constructor; despues Limpiar pantalla, Coleccion, Camara, Luz
    for _ in range(5):
        tecla("↓", "abajo", ["outliner tecla Down"], dur=350)
    tecla("1", "num1", ["outliner n95 mover"], dur=800)
    tecla("↑", "arriba", ["outliner tecla Up"], dur=800)
    tecla("1", "num1", ["outliner n95 mover"], dur=900)
    for _ in range(2):
        tecla("↓", "abajo", ["outliner tecla Down"], dur=350)
    tecla("2", "num2", ["outliner n95 visible"], dur=1100)
    tecla("2", "num2", ["outliner n95 visible"], dur=900)
    tecla("3", "num3", ["outliner n95 render"], dur=1600)
    return g.cuadros(calidad=70)


# ---- La Vista 3D ---------------------------------------------------------------------------------------------------
@receta("vista-3d-barra")
def v3d_barra(idioma):
    """La Vista 3D ancha, con el cubo elegido: la barra de arriba entera."""
    g = Guion(idioma, w=1600, h=600)
    g.linea("division 0.78")
    v = g.info().vp(VISTA_3D)
    g.foto()
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], v["h"]))


@receta("vista-3d-sombreado")
def v3d_sombreado(idioma):
    """Leon: Z (o el 6 en el N95) pasa por los modos de vista: Vista previa de material, Solido, Normales, Alambrico,
    ZBuffer, Alfa, Renderizado y vuelve."""
    from recetas_objeto import escena_leon
    from recetas import vista_pc
    g = Guion(idioma, raton=True)
    escena_leon(g)
    g.linea("deselectall")
    rec, (cx, cy) = vista_pc(g)
    g.raton_mover(cx + 200, cy + 120)
    g.foto(dur=1300)
    for _ in range(7):
        g.foto(dur=300, tecla="Z")
        g.tecla("z")
        g.foto(dur=1300)
    return g.cuadros(recorte=rec, calidad=72)


@receta("vista-3d-superposiciones")
def v3d_superposiciones(idioma):
    """El menu Superposiciones abierto (el ojo de la barra de la Vista 3D)."""
    g = Guion(idioma, w=960, h=760)
    x, y = g.info().boton_rol(VISTA_3D, 7)        # BR_Overlays
    g.clic(x, y)
    g.sin_puntero()
    v = g.info().vp(VISTA_3D)
    g.foto()
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], v["h"]))

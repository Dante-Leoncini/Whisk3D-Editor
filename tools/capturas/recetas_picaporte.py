"""Las capturas del tutorial 8, Modelar el picaporte (docs/manual/<idioma>/tutoriales/modelar-picaporte.md).

Cada receta arma la escena hasta su seccion con los MISMOS pasos del texto (sin filmar: 'preparar') y filma solo su
parte. Los numeros son los del texto, comprobados con el harness: la placa queda de 13,5 cm y la manija con 12
vertices, 20 aristas y 10 caras, igual que la del duenio (puerta-picaporte-pasos.txt, 2026-11-09)."""
from capturas import receta, Guion, T, VISTA_3D, OUTLINER
from recetas_objeto import menu, PUERTA

BR_SELMODO, BR_OBJETO, BR_SUPERPOS, BR_MALLA = 2, 6, 7, 15


def W(ux, uy, uz):
    """Un punto de la puerta (en local, como Propiedades: Z arriba) en el mundo interno (Y arriba). El origen de la
    puerta esta en la bisagra: (0.45, 1, 0)."""
    return (0.45 + ux, 1.0 + uz, uy)


def k(*teclas):
    """Teclas y numeros como lineas del harness ('=0.15' se tipea)."""
    return [("texto " + t[1:]) if t.startswith("=") else ("tecla " + t) for t in teclas]


SHIFT_D = ["tecla Left_Shift down", "tecla d", "tecla Left_Shift up", "tecla Escape"]
CTRL = lambda t: ["tecla Left_Ctrl down", "tecla " + t, "tecla Left_Ctrl up"]

# LAS ETAPAS, en orden: cada una deja la escena como queda despues de esa parte del tutorial ({P} = la puerta)
ETAPAS = [
    ("puerta", ["objname {P}", "objscale 0.45 1 0.05", "objpos 0 1 0", "apply scale", "cursor3d 0.45 1 0",
                "origen cursor", "cursor3d 0 -30 0", "selobj {P}"]),
    ("edicion", ["ratonvista3d", "tecla Tab", "selmode face", "select none"]),
    ("cara", ["elegircerca -0.45 0.05 0"]),
    ("placa", SHIFT_D + k("s", "=0.15", "Return", "s", "z", "=0.45", "Return", "g", "x", "=-0.31", "Return",
                          "e", "=0.03", "Return")),
    ("separada", ["vinculado", "ratonvista3d", "tecla p", "tecla Tab"]),
    ("materiales", ["selobj {P}", "matnombre Madera", "matcolor 0.55 0.33 0.17", "selobj {P}.001", "matnew",
                    "matnombre Metal", "matcolor 0.75 0.75 0.78"]),
    ("espejo", ["selobj {P}.001", "agregar modificador 1", "propstab 4", "cuadro", "propclic Mirror X",
                "propclic Mirror Y"]),
    ("manija_edicion", ["selobj {P}.001", "ratonvista3d", "tecla Keypad_/", "tecla Tab", "selmode face",
                        "elegircerca -0.76 0.08 0"]),
    ("manija_eje", SHIFT_D + k("s", "=0.6", "Return", "e", "=0.08", "Return")),
    ("manija_brazo", ["selmode edge", "elegircerca -0.8005 0.16 0"] + k("g", "y", "=0.05", "Return") +
                     ["selmode face", "elegircerca -0.76 0.185 0"] + k("e", "x", "=0.34", "Return", "s", "x", "=0", "Return")),
    ("manija", ["vinculado", "tecla p", "tecla Tab", "tecla Keypad_/", "renombrarfila {P}.002 Manija"]),
    ("unida", ["selobj {P}.001", "propstab 4", "cuadro", "propclic Apply Modifier", "selobj {P}.001", "selname {P}",
               "ratonvista3d"] + CTRL("j")),
    ("sincaras", ["selobj {P}", "ratonvista3d", "tecla Tab", "selmode face", "elegircerca -0.76 0.05 0",
                  "elegircerca -0.76 -0.05 0 +", "tecla x", "menuelegir Faces", "tecla Tab"]),
    ("origen", ["selobj Manija", "propstab 4", "cuadro", "propclic Apply Modifier", "ratonvista3d", "tecla Tab",
                "selmode face", "elegircerca -0.76 0.08 0", "elegircerca -0.76 -0.08 0 +", "tecla Left_Shift down",
                "tecla s", "tecla Left_Shift up", "menuelegir Cursor to Selected", "tecla x", "menuelegir Faces",
                "tecla Tab", "origen cursor"]),
    ("emparentada", ["selobj Manija", "selname {P}", "ratonvista3d"] + CTRL("p") + ["menuelegir Object (Keep Transform)"]),
]

# la camara: de frente y un poco de costado, a la altura del picaporte
VISTA_PLACA = "vporbita -0.31 1.0 0.05 1.5 -12 -22"
VISTA_LADO = "vporbita -0.31 1.0 0.0 1.3 -12 -90"      # de canto, desde el borde: una placa de cada lado
VISTA_PUERTA = "vporbita 0 1.0 0 5.2 -20 -25"          # la puerta entera
VISTA_45 = "vporbita -0.31 1.0 0.0 1.4 -20 -45"       # a 45 grados: las caras de atras se ven como CARAS (de canto eran lineas)


def guion(idioma, hasta, vista=VISTA_PLACA):
    """El editor con la Vista 3D y, al costado, el Outliner y Propiedades; la escena armada HASTA esa etapa
    (incluida)."""
    g = Guion(idioma, w=1200, h=760, raton=True)
    g.linea("division 0.66")
    p = PUERTA[idioma]
    for nombre, lineas in ETAPAS:
        for l in lineas:
            g.linea(l.replace("{P}", p))
        if nombre == hasta:
            break
    g.linea(vista)
    g.linea("cursor3d 0 -30 0")
    return g


def pantalla(g, p):
    """Donde cae en la pantalla un punto del mundo (interno)."""
    g.linea("cursor3d %g %g %g" % p)
    xy = g.info().cursor3d
    g.linea("cursor3d 0 -30 0")
    return xy


def pasos(g, lista, fin=1400):
    """PASO A PASO, con el cartel que se va sumando (como comando() de recetas_objeto): [(rotulo, lineas)]."""
    hechas = []
    for rotulo, lineas in lista:
        hechas.append(rotulo)
        cartel = ",  ".join(hechas)
        g.foto(dur=450, tecla=cartel)
        for l in lineas:
            g.linea(l)
        g.foto(dur=900, tecla=cartel)
    g.foto(dur=fin)


def teclas_numero(teclas_, numero):
    """[('S', ...), ('0.15', ...), ('Enter', ...)] a partir de 'S' 'Z' y el numero."""
    out = [(t.upper(), ["tecla " + t]) for t in teclas_]
    out.append((numero, ["texto " + numero]))
    out.append(("Enter", ["tecla Return"]))
    return out


def clic3d(g, p, antes=500, despues=800):
    """El mouse va hasta ese punto de la escena y hace click (el pick de verdad)."""
    x, y = pantalla(g, p)
    g.raton_mover(x, y)
    g.foto(dur=antes)
    g.clic(x, y)
    g.foto(dur=despues)


def senalar3d(g, p, linea, antes=500, despues=800):
    """El mouse va hasta ese punto y se elige con el harness (las caras de atras, con Rayos X: el pick las confunde
    con la cara de adelante de la puerta, que esta en el mismo plano)."""
    x, y = pantalla(g, p)
    g.raton_mover(x, y)
    g.foto(dur=antes)
    g.linea(linea)
    g.foto(dur=despues)


def clic_prop(g, nombre, despues=800):
    """Un click en el valor de una fila de Propiedades (por su nombre en ingles). Si la fila quedo abajo, primero se
    desplaza Propiedades con la rueda hasta que se vea."""
    for _ in range(12):
        i = g.info()
        f = i.prop(T(g.idioma, nombre))
        v = i.vp(3)
        if v["y"] + v["barra"] < f["y"] < v["y"] + v["h"] - 10:
            break
        g.linea("propscroll %d" % (3 if f["y"] >= v["y"] + v["h"] - 10 else -3))
    g.raton_mover(f["valor"], f["y"])
    g.foto(dur=450)
    g.clic(f["valor"], f["y"])
    g.foto(dur=despues)


def menu_ruta(g, ruta, despues=900):
    """Recorre el menu que YA esta abierto (el de la W, el de click derecho): cada opcion por su texto en ingles; las
    de submenu se abren pasando por arriba y la ultima se elige con un click."""
    for nivel, texto in enumerate(ruta):
        x, y = g.info().item_texto(T(g.idioma, texto), nivel)
        if nivel:
            g.raton_mover(x, g.puntero_xy()[1])   # (a un submenu se entra de costado)
        g.raton_mover(x, y)
        g.foto(dur=600)
    g.clic(*g.puntero_xy())
    g.foto(dur=despues)


def pestania(g, n):
    x, y = g.info().pestania(n)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.clic(x, y)
    g.foto(dur=600)


def menu_abierto(g, texto_en, despues=800):
    """Elige una opcion del menu que ya esta abierto (por su texto en ingles), con el mouse."""
    x, y = g.info().item_texto(T(g.idioma, texto_en), 0)
    g.raton_mover(x, y)
    g.foto(dur=500)
    g.clic(x, y)
    g.foto(dur=despues)


# ---- 1. La placa ---------------------------------------------------------------------------------------------------
@receta("picaporte-placa-raton")
def placa(idioma):
    """Click en la cara de adelante; Shift+D y Esc (la copia queda encima); S 0.15 y S Z 0.45 (un cuadrado de 13,5 cm);
    G X -0.31 (al lado contrario de la bisagra); E 0.03 (3 cm de grosor)."""
    g = guion(idioma, "edicion")
    g.foto(dur=700)
    clic3d(g, W(-0.45, 0.05, 0))
    pasos(g, [("Shift + D", ["tecla Left_Shift down", "tecla d", "tecla Left_Shift up"]), ("Esc", ["tecla Escape"])], fin=700)
    pasos(g, teclas_numero(["s"], "0.15"), fin=600)
    pasos(g, teclas_numero(["s", "z"], "0.45"), fin=600)
    g.linea(VISTA_PLACA.replace("-0.31", "-0.15").replace("1.5", "2.0"))
    pasos(g, teclas_numero(["g", "x"], "-0.31"), fin=600)
    g.linea(VISTA_PLACA)
    pasos(g, teclas_numero(["e"], "0.03"))
    return g.cuadros(calidad=90)


# ---- 2. Separar la placa ---------------------------------------------------------------------------------------------
@receta("picaporte-separar-raton")
def separar(idioma):
    """Con el mouse sobre la placa: L (Seleccionar Vinculado: la placa entera) y P (Separar). En el Outliner aparece
    Puerta.001."""
    g = guion(idioma, "placa")
    x, y = pantalla(g, W(-0.76, 0.08, 0))
    g.raton_mover(x, y)
    g.foto(dur=800)
    pasos(g, [("L", ["tecla l"]), ("P", ["tecla p"])], fin=900)
    pasos(g, [("Tab", ["tecla Tab"])], fin=1800)
    return g.cuadros(calidad=90)


# ---- 3. Los materiales ----------------------------------------------------------------------------------------------
@receta("picaporte-materiales")
def materiales(idioma):
    """La puerta de madera y la placa de metal, con la tarjeta Material de la placa (Metal) en Propiedades."""
    g = guion(idioma, "materiales")
    g.linea("selobj " + PUERTA[idioma] + ".001")
    g.linea("propstab 2")
    g.linea("vista3d sombreado 1")
    g.linea("propscroll 4")                       # (que se vea la tarjeta Material entera)
    g.foto(dur=100, puntero=False)
    return g.cuadros(calidad=92)


# ---- 4. El Espejo ---------------------------------------------------------------------------------------------------
@receta("picaporte-espejo-raton")
def espejo(idioma):
    """De costado: Modificadores, Aniadir Espejo, Espejo X apagado y Espejo Y prendido: aparece la placa de atras."""
    g = guion(idioma, "materiales", vista=VISTA_LADO)
    g.linea("selobj " + PUERTA[idioma] + ".001")
    g.foto(dur=700)
    pestania(g, 4)
    clic_prop(g, "")                              # (el boton de agregar un modificador)
    menu_abierto(g, "Mirror", despues=1000)
    clic_prop(g, "Mirror X")
    clic_prop(g, "Mirror Y", despues=2000)
    return g.cuadros(calidad=90)


# ---- 5. La manija ---------------------------------------------------------------------------------------------------
@receta("picaporte-manija-raton")
def manija(idioma):
    """En la vista local de la placa: click en su cara de adelante; Shift+D y Esc; S 0.6; E 0.08 (el eje)."""
    g = guion(idioma, "espejo")
    g.linea("selobj " + PUERTA[idioma] + ".001")
    x, y = pantalla(g, W(-0.76, 0.08, 0))
    g.raton_mover(x, y)
    g.foto(dur=600)
    pasos(g, [("/", ["tecla Keypad_/"]), ("Tab", ["tecla Tab"])], fin=500)
    g.linea("selmode face")
    g.linea("select none")
    clic3d(g, W(-0.76, 0.08, 0))
    pasos(g, [("Shift + D", ["tecla Left_Shift down", "tecla d", "tecla Left_Shift up"]), ("Esc", ["tecla Escape"])], fin=600)
    pasos(g, teclas_numero(["s"], "0.6"), fin=600)
    pasos(g, teclas_numero(["e"], "0.08"))
    return g.cuadros(calidad=90)


@receta("picaporte-brazo-raton")
def brazo(idioma):
    """Arista: la de afuera del lado del borde, G Y 0.05 (la cara de adelante queda inclinada). Cara: esa cara, E X 0.34
    (el brazo, en X y no en la normal) y S X 0 (la punta queda derecha)."""
    g = guion(idioma, "manija_eje", vista="vporbita -0.16 1.0 0.1 1.2 -25 -20")
    g.foto(dur=600)
    menu(g, BR_SELMODO, ["Edge"])
    clic3d(g, W(-0.8005, 0.16, 0))
    pasos(g, teclas_numero(["g", "y"], "0.05"), fin=600)
    menu(g, BR_SELMODO, ["Face"])
    senalar3d(g, W(-0.76, 0.185, 0), "elegircerca -0.76 0.185 0")
    pasos(g, teclas_numero(["e", "x"], "0.34"), fin=600)
    pasos(g, teclas_numero(["s", "x"], "0"))
    return g.cuadros(calidad=90)


@receta("picaporte-manija-separar-raton")
def manija_separar(idioma):
    """L y P separan la manija (Puerta.002); doble click en el Outliner y se llama Manija."""
    g = guion(idioma, "manija_brazo", vista="vporbita -0.16 1.0 0.1 1.2 -25 -20")
    x, y = pantalla(g, W(-0.55, 0.19, 0))
    g.raton_mover(x, y)
    g.foto(dur=600)
    pasos(g, [("L", ["tecla l"]), ("P", ["tecla p"]), ("Tab", ["tecla Tab"]), ("/", ["tecla Keypad_/"])], fin=900)
    f = g.info().fila_info(PUERTA[idioma] + ".002")
    g.raton_mover(f["x"], f["y"])
    g.foto(dur=600)
    g.linea("renombrarfila %s.002 Manija" % PUERTA[idioma])
    g.foto(dur=1800)
    return g.cuadros(calidad=90)


# ---- 6. Unir la placa a la puerta ------------------------------------------------------------------------------------
@receta("picaporte-unir-raton")
def unir(idioma):
    """Aplicar Modificador (el espejo de la placa); click en la placa, Shift + click en la puerta; Ctrl + J."""
    g = guion(idioma, "manija", vista=VISTA_LADO)
    g.linea("selobj " + PUERTA[idioma] + ".001")
    g.linea("propstab 4")
    g.foto(dur=700)
    clic_prop(g, "Apply Modifier", despues=1000)
    clic3d(g, W(-0.76, 0.08, 0.05))
    x, y = pantalla(g, W(-0.2, 0.05, 0.4))
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.linea("mod shift")
    g.clic(x, y)
    g.linea("mod")
    g.foto(dur=800, tecla="Shift + click")
    pasos(g, [("Ctrl + J", ["tecla Left_Ctrl down", "tecla j", "tecla Left_Ctrl up"])], fin=2000)
    return g.cuadros(calidad=90)


# ---- 7. Las caras que nunca se ven -------------------------------------------------------------------------------------
@receta("picaporte-rayosx-raton")
def rayosx(idioma):
    """La puerta en Edicion; Superposiciones > Rayos X; las dos caras de atras de las placas; X > Caras."""
    g = guion(idioma, "unida", vista=VISTA_45)
    g.linea("selobj " + PUERTA[idioma])
    g.foto(dur=600)
    pasos(g, [("Tab", ["tecla Tab"])], fin=400)
    g.linea("selmode face")
    g.linea("select none")
    menu(g, BR_SUPERPOS, ["X-Ray"])
    g.tecla("Escape")
    g.foto(dur=900)
    senalar3d(g, W(-0.76, 0.05, 0), "elegircerca -0.76 0.05 0")
    senalar3d(g, W(-0.76, -0.05, 0), "elegircerca -0.76 -0.05 0 +", despues=1000)
    pasos(g, [("X", ["tecla x"])], fin=500)
    menu_abierto(g, "Faces", despues=1800)
    return g.cuadros(calidad=90)


# ---- 8. El origen de la manija ----------------------------------------------------------------------------------------
@receta("picaporte-origen-raton")
def origen(idioma):
    """La manija: Aplicar su Espejo; en Edicion, las dos caras de atras; Shift+S > Cursor a lo seleccionado; X > Caras;
    Tab; W > Definir Origen > Origen al Cursor 3D."""
    g = guion(idioma, "sincaras", vista=VISTA_45)
    g.linea("selobj Manija")
    g.linea("propstab 4")
    g.foto(dur=600)
    clic_prop(g, "Apply Modifier", despues=900)
    pasos(g, [("Tab", ["ratonvista3d", "tecla Tab"])], fin=400)
    g.linea("selmode face")
    g.linea("select none")
    senalar3d(g, W(-0.76, 0.08, 0), "elegircerca -0.76 0.08 0")
    senalar3d(g, W(-0.76, -0.08, 0), "elegircerca -0.76 -0.08 0 +")
    pasos(g, [("Shift + S", ["tecla Left_Shift down", "tecla s", "tecla Left_Shift up"])], fin=400)
    menu_abierto(g, "Cursor to Selected", despues=1200)
    pasos(g, [("X", ["tecla x"])], fin=400)
    menu_abierto(g, "Faces", despues=900)
    x, y = pantalla(g, W(-0.6, 0.19, 0.12))
    g.raton_mover(x, y)
    pasos(g, [("Tab", ["tecla Tab"]), ("W", ["tecla w"])], fin=400)
    menu_ruta(g, ["Set Origin", "Origin to 3D Cursor"], despues=2000)
    return g.cuadros(calidad=90)


# ---- 9. Emparentar ------------------------------------------------------------------------------------------------------
@receta("picaporte-flota-raton")
def flota(idioma):
    """Sin emparentar: R Z -70 gira la puerta y la manija se queda flotando. Ctrl+Z."""
    g = guion(idioma, "origen", vista=VISTA_PUERTA)
    g.linea("selobj " + PUERTA[idioma])
    g.foto(dur=900)
    pasos(g, teclas_numero(["r", "z"], "-70"), fin=1800)
    pasos(g, [("Ctrl + Z", ["tecla Left_Ctrl down", "tecla z", "tecla Left_Ctrl up"])], fin=1200)
    return g.cuadros(calidad=90)


@receta("picaporte-emparentar-raton")
def emparentar(idioma):
    """Click en la manija, Shift + click en la puerta, Ctrl+P > Objeto (Mantener Transform.); R Z -70: ahora la manija
    acompania a la puerta."""
    g = guion(idioma, "origen", vista=VISTA_PUERTA)
    g.linea("deselectall")                        # (sin el gizmo encima de la manija: el click la tiene que ELEGIR)
    g.foto(dur=600)
    clic3d(g, W(-0.6, 0.19, 0))
    x, y = pantalla(g, W(-0.75, 0.05, 0.85))     # (arriba, lejos del gizmo: un click en el gizmo cambia su modo)
    g.raton_mover(x, y)
    g.foto(dur=400)
    g.linea("mod shift")
    g.clic(x, y)
    g.linea("mod")
    g.foto(dur=800, tecla="Shift + click")
    pasos(g, [("Ctrl + P", ["tecla Left_Ctrl down", "tecla p", "tecla Left_Ctrl up"])], fin=400)
    menu_abierto(g, "Object (Keep Transform)", despues=1200)
    clic3d(g, W(-0.75, 0.05, 0.85))               # solo la PUERTA: la manija la acompania por ser su hija
    pasos(g, teclas_numero(["r", "z"], "-70"), fin=2200)
    return g.cuadros(calidad=90)


# ======================================================================================================================
#  TACTIL y NOKIA N95: los mismos pasos, con el dedo (la barra de herramientas, los menus y el teclado de la barra de
#  arriba) y con el teclado del telefono (1 mover, 3 escalar, 7 extruir, el lapiz + los numeros, OK)
# ======================================================================================================================
from recetas_objeto import tocar_en, tocar_menu, n95_comando, TB_EJE, TB_ACEPTAR, TECLA_N95

TB_CANCELAR = 101
BR_SELECCIONAR = 4


def guion_tactil(idioma, hasta, vista=VISTA_PLACA):
    """Como guion(), con el dedo: el primer toque pasa la interfaz a tactil (la barra de herramientas cambia)."""
    g = Guion(idioma, w=1200, h=760, raton=False)
    g.linea("division 0.66")
    p = PUERTA[idioma]
    for nombre, lineas in ETAPAS:
        for l in lineas:
            g.linea(l.replace("{P}", p))
        if nombre == hasta:
            break
    g.linea(vista)
    g.linea("cursor3d 0 -30 0")
    # DESPUES de la preparacion (que usa mouse y teclas, y deja la interfaz en modo mouse): un toque en el fondo vacio
    # del Outliner pasa la interfaz a tactil sin elegir nada. Si no, el primer toque del video se gastaba en eso
    o = g.info().vp(OUTLINER)
    g.tocar(o["x"] + o["w"] // 2, o["y"] + o["h"] - 12)
    g.sin_puntero()
    return g


def numero_dedo(g, comando_en, eje, valor):
    """Escalar / Mover / Extruir en la barra de herramientas, el eje (si hay), un toque en la barra de arriba, el
    numero en el teclado y OK; la tilde lo deja."""
    tocar_en(g, *g.info().herramienta(VISTA_3D, T(g.idioma, comando_en)))
    if eje:
        tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_EJE[eje]))
    v = g.info().vp(VISTA_3D)
    tocar_en(g, v["x"] + v["w"] // 2, v["y"] + v["barra"] // 2, despues=700)
    for c in valor + "!":
        np_ = g.info().numpad
        t = "OK" if c == "!" else c
        if t not in np_ and t == "-":
            t = next(k for k in np_ if k in ("-", "−", "+/-", "±"))
        tocar_en(g, *np_[t], antes=250, despues=350)
    tocar_en(g, *g.info().herramienta_rol(VISTA_3D, TB_ACEPTAR), despues=1000)


def toque3d(g, p, despues=800):
    tocar_en(g, *pantalla(g, p), despues=despues)


def duplicar_dedo(g):
    """Malla > Duplicar: la copia sale moviendose; la cruz la deja donde estaba (encima de la original)."""
    tocar_menu(g, BR_MALLA, ["Duplicate"])
    i = g.info()
    hay_cruz = any(h.get("rol") == TB_CANCELAR for h in i.herramientas)
    if hay_cruz:                                  # (si la copia quedo moviendose, la cruz la deja en su lugar)
        tocar_en(g, *i.herramienta_rol(VISTA_3D, TB_CANCELAR), despues=900)
    else:
        g.foto(dur=900)


@receta("picaporte-placa-dedo")
def placa_dedo(idioma):
    """Tactil: un toque en la cara de adelante; Malla > Duplicar y la cruz; Escalar 0.15; Escalar Z 0.45; Mover X
    -0.31; Extruir 0.03."""
    g = guion_tactil(idioma, "edicion")
    g.foto(dur=700)
    toque3d(g, W(-0.45, 0.05, 0))
    duplicar_dedo(g)
    numero_dedo(g, "Scale", None, "0.15")
    numero_dedo(g, "Scale", "Z", "0.45")
    g.linea(VISTA_PLACA.replace("-0.31", "-0.15").replace("1.5", "2.0"))
    numero_dedo(g, "Move", "X", "-0.31")
    g.linea(VISTA_PLACA)
    numero_dedo(g, "Extrude", None, "0.03")
    g.foto(dur=1500)
    return g.cuadros(calidad=90)


@receta("picaporte-manija-dedo")
def manija_dedo(idioma):
    """Tactil, en la vista local de la placa: la cara de adelante, Duplicar, Escalar 0.6, Extruir 0.08; Arista, la de
    afuera, Mover Y 0.05; Cara, la inclinada, Extruir X 0.34 y Escalar X 0."""
    g = guion_tactil(idioma, "manija_edicion", vista="vporbita -0.16 1.0 0.1 1.2 -25 -20")
    g.linea("select none")
    g.foto(dur=600)
    toque3d(g, W(-0.76, 0.08, 0))
    duplicar_dedo(g)
    numero_dedo(g, "Scale", None, "0.6")
    numero_dedo(g, "Extrude", None, "0.08")
    tocar_menu(g, BR_SELMODO, ["Edge"])
    toque3d(g, W(-0.8005, 0.16, 0))
    numero_dedo(g, "Move", "Y", "0.05")
    tocar_menu(g, BR_SELMODO, ["Face"])
    x, y = pantalla(g, W(-0.76, 0.185, 0))
    tocar_en(g, x, y)
    g.linea("elegircerca -0.76 0.185 0")          # (la inclinada, sin confundirla con la de al lado)
    numero_dedo(g, "Extrude", "X", "0.34")
    numero_dedo(g, "Scale", "X", "0")
    g.foto(dur=1500)
    return g.cuadros(calidad=90)


@receta("picaporte-separar-dedo")
def separar_dedo(idioma):
    """Tactil: Seleccionar > Seleccionar Vinculado y un toque en la placa; Malla > Separar."""
    g = guion_tactil(idioma, "placa")
    g.foto(dur=700)
    tocar_menu(g, BR_SELECCIONAR, ["Select Linked"])
    toque3d(g, W(-0.76, 0.08, 0))
    tocar_menu(g, BR_MALLA, ["Separate"])
    g.foto(dur=1800)
    return g.cuadros(calidad=90)


@receta("picaporte-unir-dedo")
def unir_dedo(idioma):
    """Tactil: con la placa y la puerta elegidas (la puerta activa), Objeto > Unir."""
    g = guion_tactil(idioma, "manija", vista=VISTA_LADO)
    g.linea("selobj " + PUERTA[idioma] + ".001")
    g.linea("propstab 4")
    g.linea("propclic Apply Modifier")
    g.linea("selname " + PUERTA[idioma])
    g.foto(dur=900)
    tocar_menu(g, BR_OBJETO, ["Join"])
    g.foto(dur=1800)
    return g.cuadros(calidad=90)


@receta("picaporte-emparentar-dedo")
def emparentar_dedo(idioma):
    """Tactil: con la manija y la puerta elegidas (la puerta activa), Objeto > Emparentar a > Objeto (Mantener
    Transform.)."""
    g = guion_tactil(idioma, "origen", vista=VISTA_PUERTA)
    g.linea("selobj Manija")
    g.linea("selname " + PUERTA[idioma])
    g.foto(dur=900)
    tocar_menu(g, BR_OBJETO, ["Set Parent", "Object (Keep Transform)"])
    g.foto(dur=1800)
    return g.cuadros(calidad=90)


# ---- Nokia N95 ------------------------------------------------------------------------------------------------------
def guion_n95(idioma, hasta, vista="vporbita -0.31 1.0 0.05 1.8 -12 -22"):
    g = Guion(idioma, n95="vertical")
    g.linea("vista3d gizmo 0")
    g.linea("vista3d barra 0")
    p = PUERTA[idioma]
    for nombre, lineas in ETAPAS:
        for l in lineas:
            g.linea(l.replace("{P}", p))
        if nombre == hasta:
            break
    g.linea(vista)
    g.linea("cursor3d 0 -30 0")
    return g


def n95_sin_eje(g, modo, tecla_modo, valor, extruir=False):
    """Como n95_comando, sin elegir eje (escalar parejo / extruir por la normal): la tecla, el lapiz + el numero, OK."""
    hechas = []
    def paso(rotulo, ids, accion, junto=False):
        if junto and hechas:
            hechas[-1] = rotulo
        else:
            hechas.append(rotulo)
        cartel = ",  ".join(hechas)
        g.foto(dur=450, tecla=cartel, teclas=ids)
        accion()
        g.foto(dur=650, tecla=cartel)
    if extruir:
        paso("7", ("num7",), lambda: g.keypad("extruir"))
    else:
        paso(tecla_modo, ("num" + tecla_modo,), lambda: g.keypad("transformar", modo))
    escrito = ""
    for k, c in enumerate(valor):
        escrito += c
        paso("lápiz + " + escrito, ("lapiz", TECLA_N95.get(c, "num" + c)), lambda c=c: g.linea("texto " + c), junto=(k > 0))
    paso("OK", ("ok",), lambda: g.keypad("aceptar"))
    g.foto(dur=1100)


@receta("picaporte-placa-nokia")
def placa_nokia(idioma):
    """N95 (la copia de la cara ya hecha con Malla > Duplicar): 3 escala, lapiz + 0.15, OK; 3, 3 (Z), 0.45; 1, 1 (X),
    -0.31; 7 (extruir), 0.03."""
    g = guion_n95(idioma, "cara")
    g.linea("duplicate")
    g.foto(dur=900)
    n95_sin_eje(g, 3, "3", "0.15")
    n95_comando(g, 3, "3", 2, "3", "0.45")
    n95_comando(g, 1, "1", 0, "1", "-0.31")
    n95_sin_eje(g, 1, "7", "0.03", extruir=True)
    return g.cuadros(calidad=88)


@receta("picaporte-manija-nokia")
def manija_nokia(idioma):
    """N95 (la cara de la placa ya duplicada): 3, 0.6; 7, 0.08; la arista: 1, 2 (Y), 0.05; la cara inclinada: 7, 1
    (X), 0.34; 3, 1 (X), 0."""
    g = guion_n95(idioma, "manija_edicion", vista="vporbita -0.16 1.0 0.1 1.3 -25 -20")
    g.linea("duplicate")
    g.foto(dur=900)
    n95_sin_eje(g, 3, "3", "0.6")
    n95_sin_eje(g, 1, "7", "0.08", extruir=True)
    g.linea("selmode edge")
    g.linea("elegircerca -0.8005 0.16 0")
    g.foto(dur=700)
    n95_comando(g, 1, "1", 1, "2", "0.05")
    g.linea("selmode face")
    g.linea("elegircerca -0.76 0.185 0")
    g.foto(dur=700)
    hechas_extruir_x(g)
    n95_comando(g, 3, "3", 0, "1", "0")
    return g.cuadros(calidad=88)


def hechas_extruir_x(g):
    """7 (extruir), 1 (el eje X: en X, no en la normal), el lapiz + 0.34 y OK."""
    hechas = []
    for rotulo, ids, accion in [("7", ("num7",), lambda: g.keypad("extruir")),
                                ("1", ("num1",), lambda: g.keypad("eje", 0))] + \
                               [("lápiz + " + "0.34"[:k + 1], ("lapiz", TECLA_N95.get(c, "num" + c)),
                                 (lambda c=c: g.linea("texto " + c))) for k, c in enumerate("0.34")] + \
                               [("OK", ("ok",), lambda: g.keypad("aceptar"))]:
        if rotulo.startswith("lápiz") and hechas and hechas[-1].startswith("lápiz"):
            hechas[-1] = rotulo
        else:
            hechas.append(rotulo)
        cartel = ",  ".join(hechas)
        g.foto(dur=450, tecla=cartel, teclas=ids)
        accion()
        g.foto(dur=650, tecla=cartel)
    g.foto(dur=1100)

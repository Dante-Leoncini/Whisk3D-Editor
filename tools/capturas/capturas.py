#!/usr/bin/env python3
"""
Capturas de los tutoriales y del manual de Whisk3D.

Cada RECETA (recetas.py) maneja el editor como una persona: toca, arrastra, gira la
rueda y aprieta teclas con eventos reales (los comandos raton/dedo/rueda/tecla del
harness, main/test/W3dCapturas.cpp) y saca fotos con 'uishot'. Todo corre con
'whisk3d --script', sin abrir ninguna ventana. Despues se le dibuja encima el dedo,
el puntero o la tecla apretada, y se arma la imagen final: un .webp quieto o animado
en bucle.

El editor cambia seguido: estas capturas NO se retocan a mano, se REGENERAN.
Despues de tocar la interfaz, correr esto y commitear las imagenes nuevas.

Uso (desde cualquier carpeta):
    python3 tools/capturas/capturas.py                  todas, en los 9 idiomas del editor
    python3 tools/capturas/capturas.py viewports        las que empiezan con "viewports"
    python3 tools/capturas/capturas.py --idiomas es     solo en castellano
    python3 tools/capturas/capturas.py --lista          las recetas que hay

Salida: docs/manual/<idioma>/tutoriales/img/<nombre>.webp
El editor tiene la interfaz en 9 idiomas (IDIOMAS): las paginas en los demas (hindi,
japones, chino) muestran las capturas en ingles (la web las busca ahi).
Binario: W3D_BIN, o el build de linux del repo (platform/linux/build/whisk3d).
"""
import math, os, re, shutil, subprocess, sys, tempfile

from PIL import Image, ImageDraw, ImageFont

import n95 as n95_mod

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.abspath(os.path.join(AQUI, "..", ".."))
BIN = os.environ.get("W3D_BIN", os.path.join(RAIZ, "platform", "linux", "build", "whisk3d"))
SALIDA = os.path.join(RAIZ, "docs", "manual", "{idioma}", "tutoriales", "img")

# el idioma de la interfaz sale del sistema (W3dIdiomaDetectar): se le pasa por LANG. Son los 9 del editor
# (res/idiomas/); las paginas en otros idiomas (hindi, japones, chino) muestran las capturas en ingles.
IDIOMAS = {"es": "es_AR.UTF-8", "en": "en_US.UTF-8", "pt": "pt_BR.UTF-8", "fr": "fr_FR.UTF-8", "it": "it_IT.UTF-8",
           "tr": "tr_TR.UTF-8", "de": "de_DE.UTF-8", "pl": "pl_PL.UTF-8", "ru": "ru_RU.UTF-8"}


class PorIdioma(dict):
    """Un texto de las recetas por idioma ({'es': ..., 'en': ...}); el idioma que falta usa el ingles."""
    def __missing__(self, idioma):
        return self["en"]


_TRADUCCIONES = {}


def T(idioma, en):
    """Lo que muestra el editor en ese idioma para el texto en ingles 'en' (res/idiomas/<idioma>.txt, la misma tabla
    que usa T() en el editor). Sirve para los nombres que pone el editor, como la "Luz" o la "Camara" de la escena."""
    if idioma == "en":
        return en
    if idioma not in _TRADUCCIONES:
        tabla = {}
        ruta = os.path.join(RAIZ, "res", "idiomas", idioma + ".txt")
        if os.path.isfile(ruta):
            for linea in open(ruta, encoding="utf-8").read().split("\n"):
                if linea.count("\t") == 1:
                    a, b = linea.split("\t")
                    if b:
                        tabla[a] = b
        _TRADUCCIONES[idioma] = tabla
    return _TRADUCCIONES[idioma].get(en, en)

# tipos de viewport (ViewportKind) y opciones del menu de la esquina (ids de LayoutAccionTipo)
VISTA_3D, OUTLINER, PROPIEDADES, EDITOR_UV, LINEA_DE_TIEMPO, EDITOR_2D, CONSOLA, IDE, BIBLIOTECA = range(1, 10)
def MENU_TIPO(kind):
    """La opcion del menu de la esquina que convierte el viewport en ese tipo ("Vista 3D" = 0...)."""
    return kind - 1


EXPANDIR, DIVIDIR_COLUMNAS, DIVIDIR_FILAS, MAXIMIZAR = 20, 21, 22, 23   # Maximizar y Minimizar son el 23

# colores de las marcas (los de la interfaz: verde = activo, naranja = lo que hay que tocar)
VERDE = (69, 208, 106)
NARANJA = (255, 152, 41)


def fuente(tam):
    for f in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
              "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
              "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"):
        if os.path.isfile(f):
            return ImageFont.truetype(f, tam)
    return ImageFont.load_default()


class Info:
    """Lo que imprime 'capinfo': viewports, sus botones y el menu abierto."""

    def __init__(self, salida):
        self.pantalla = (0, 0)
        self.vps, self.botones, self.menus, self.items = [], [], [], []
        self.herramientas = []     # la barra de abajo de cada viewport (Mover, Rotar, Escalar..., la tilde y la cruz)
        self.manijas = {}          # las manijas del gizmo de la Vista 3D: numero -> (x, y)
        self.modo_gizmo = None     # 0 mover, 1 rotar, 2 escalar
        self.filas = []            # las filas del Outliner: {'nombre', 'x', 'y', 'prof'}
        self.pestanias = []        # Propiedades: las pestanias visibles {'i', 'x', 'y', 'w', 'h', 'activa'}
        self.tarjetas = []         # Propiedades: el titulo de cada tarjeta {'i', 'x', 'y', 'w', 'h', 'abierta', 'titulo'}
        self.props = []            # Propiedades: cada fila {'g', 'j', 'y', 'h', 'tipo', 'valor', 'rombo', 'nombre'}
        self.objetos = {}          # la Vista 3D: nombre -> {'x', 'y', 'elegido'} (el origen en pantalla)
        self.cursor3d = None       # la Vista 3D: (x, y) del cursor 3D en pantalla
        self.numpad = {}           # el teclado numerico tactil abierto: texto de la tecla -> (x, y)
        self.candado = None        # el candado de la vista de camara: (x, y, lado), si se ve
        self.boton_vista = None    # el centro del boton del menu View de la Vista 3D
        self.tlpanel = None        # la Linea de tiempo: {'x', 'y', 'w' (el panel de filas), 'numeros' (su Y), 'f1', 'f11'}
        self.tlfilas = []          # sus filas: {'r', 'y', 'tipo', 'prop', 'comp', 'nombre'}
        self.tlkeys = []           # sus keyframes: {'r', 'f', 'x', 'y'} (en el editor de curvas, sobre la curva)
        self.archivos = []         # el explorador de archivos abierto: {'i', 'x', 'y', 'w', 'h', 'sel', 'nombre'}
        self.popup = None          # la ventana emergente abierta: {'x', 'y', 'w', 'h', 'nombre'}
        self.botones_explorador = []   # sus botones de abajo (la accion y Cancelar): {'x', 'y', 'w', 'h', 'texto'}
        self.tiempos = {}          # el reloj abierto: campo j -> (x, y) (0 cuadro actual, 1 Inicio, 2 Fin, 3 FPS)
        for linea in salida.splitlines():
            if not linea.startswith("@cap "):
                continue
            partes = linea[5:].split(" ", 1)
            tipo, resto = partes[0], partes[1] if len(partes) > 1 else ""
            d = {}
            # texto=... / titulo=... / nombre=... van siempre ultimos y pueden tener espacios
            for campo in ("texto", "titulo", "nombre"):
                if (" %s=" % campo) in " " + resto:
                    resto, valor = (" " + resto).split(" %s=" % campo, 1)
                    d[campo] = valor
            for k, v in re.findall(r"(\w+)=(\S+)", resto):
                try:
                    d[k] = [int(n) for n in v.split(",")] if "," in v else int(v)
                except ValueError:
                    d[k] = v
            if tipo == "pantalla":
                self.pantalla = (d["w"], d["h"])
            elif tipo == "vp":
                self.vps.append(d)
            elif tipo == "boton":
                self.botones.append(d)
            elif tipo == "menu":
                self.menus.append(d)
            elif tipo == "item":
                self.items.append(d)
            elif tipo == "botonvista":
                self.boton_vista = (d["x"] + d["w"] // 2, d["y"] + d["h"] // 2)
            elif tipo == "candado":
                self.candado = (d["x"], d["y"], d["lado"])
            elif tipo == "herramienta":
                self.herramientas.append(d)
            elif tipo == "gizmo":
                self.manijas[d["manija"]] = (d["x"], d["y"])
                self.modo_gizmo = d["modo"]
            elif tipo == "fila":
                self.filas.append(d)
            elif tipo == "pestania":
                self.pestanias.append(d)
            elif tipo == "tarjeta":
                self.tarjetas.append(d)
            elif tipo == "prop":
                self.props.append(d)
            elif tipo == "objeto":
                self.objetos[d.get("nombre")] = d
            elif tipo == "cursor3d":
                self.cursor3d = (d["x"], d["y"])
            elif tipo == "numpad":
                self.numpad[d.get("texto")] = (d["x"], d["y"])
            elif tipo == "tlpanel":
                self.tlpanel = d
            elif tipo == "tlfila":
                self.tlfilas.append(d)
            elif tipo == "popup":
                self.popup = d
            elif tipo == "archivo":
                self.archivos.append(d)
            elif tipo == "exploradorboton":
                self.botones_explorador.append(d)
            elif tipo == "tlkey":
                self.tlkeys.append(d)
            elif tipo == "tiempo":
                self.tiempos[d["j"]] = (d["x"], d["y"])

    # ---- la Linea de tiempo ----
    def tl_x(self, f):
        """La X (pantalla) del cuadro f."""
        p = self.tlpanel
        if not p:
            raise SystemExit("capturas: no hay una Linea de tiempo")
        return int(round(p["f1"] + (f - 1) * (p["f11"] - p["f1"]) / 10.0))

    def tl_key(self, r, f):
        """El keyframe del cuadro f en la fila r (0 = el Resumen)."""
        for k in self.tlkeys:
            if k["r"] == r and k["f"] == f:
                return k["x"], k["y"]
        raise SystemExit("capturas: la fila %d de la Linea de tiempo no tiene un keyframe en %d (tiene: %s)"
                         % (r, f, [k["f"] for k in self.tlkeys if k["r"] == r]))

    def tl_fila_r(self, prop, comp):
        for d in self.tlfilas:
            if d["prop"] == prop and d["comp"] == comp:
                return d
        raise SystemExit("capturas: la Linea de tiempo no tiene la fila %d/%d (tiene: %s)"
                         % (prop, comp, [(d["prop"], d["comp"], d.get("nombre")) for d in self.tlfilas]))

    def tl_fila(self, prop, comp):
        """Donde hacer click para elegir la fila de un canal (prop: 0 Localizacion, 1 Rotacion, 2 Escala; comp interno)."""
        d = self.tl_fila_r(prop, comp)
        p = self.tlpanel
        return p["x"] + min(p["w"] // 2, p["ojo"] - p["x"] - 30), d["y"]

    def tl_key_curva(self, prop, comp, f):
        """El keyframe del cuadro f de esa curva (en el editor de curvas: sobre la curva)."""
        return self.tl_key(self.tl_fila_r(prop, comp)["r"], f)

    def tiempo(self, j):
        """El centro de un campo del reloj abierto (0 cuadro actual, 1 Inicio, 2 Fin, 3 FPS)."""
        if j not in self.tiempos:
            raise SystemExit("capturas: el reloj no esta abierto (o no tiene el campo %d)" % j)
        return self.tiempos[j]

    def vp(self, kind, n=0):
        hay = [v for v in self.vps if v["kind"] == kind]
        if len(hay) <= n:
            raise SystemExit("capturas: no hay un viewport de tipo %d (hay: %s)" % (kind, [v["kind"] for v in self.vps]))
        return hay[n]

    def esquina(self, kind, n=0):
        """Centro del boton de la esquina (arriba a la izquierda) del viewport."""
        x, y, w, h = self.vp(kind, n)["esquina"]
        return x + w // 2, y + h // 2

    def boton(self, kind, texto):
        """Centro del boton de la barra de un viewport, por su texto (el del idioma de la interfaz)."""
        for b in self.botones:
            if b["kind"] == kind and b.get("texto") == texto:
                return b["x"] + b["w"] // 2, b["y"] + b["h"] // 2
        raise SystemExit("capturas: el viewport %d no tiene un boton '%s' (tiene: %s)"
                         % (kind, texto, [b.get("texto") for b in self.botones if b["kind"] == kind]))

    def boton_rol(self, kind, rol):
        """Centro de un boton de la barra de ARRIBA de un viewport por su ROL (los que son solo un icono)."""
        for b in self.botones:
            if b["kind"] == kind and b.get("rol") == rol and b.get("visible", 1):
                return b["x"] + b["w"] // 2, b["y"] + b["h"] // 2
        raise SystemExit("capturas: la barra del viewport %d no tiene un boton con el rol %d" % (kind, rol))

    def item_texto(self, texto, nivel=0):
        """Centro de una fila del menu abierto por su texto (para los nombres que no se traducen, como GLB)."""
        for it in self.items:
            if it["nivel"] == nivel and it.get("texto") == texto:
                return it["cx"], it["cy"]
        raise SystemExit("capturas: el menu abierto no tiene '%s' (tiene: %s)" % (texto, [i.get("texto") for i in self.items]))

    def _herramienta_visible(self, kind, b, nombre):
        """El centro de un boton de la barra de abajo, si cae ADENTRO de su viewport. Una barra que no entra en el ancho
        corta los ultimos botones: el harness igual informa su lugar, y un click ahi cae en el viewport de al lado sin
        hacer nada (asi el tutorial de la pelota salio con la subida mal: 'Al llegar' quedaba afuera y el keyframe se
        quedaba con el suavizado automatico). Mejor cortar aca que sacar una captura que muestra otra cosa."""
        cx, cy = b["x"] + b["w"] // 2, b["y"] + b["h"] // 2
        if not any(v["x"] <= cx < v["x"] + v["w"] for v in self.vps if v["kind"] == kind):
            raise SystemExit("capturas: el boton %s de la barra de abajo del viewport %d queda afuera (x=%d): la barra no "
                             "entra en el ancho; ensanchar el viewport o correr la barra con la rueda" % (nombre, kind, cx))
        return cx, cy

    def herramienta(self, kind, texto):
        """Centro de un boton de la barra de abajo de un viewport, por su texto (el del idioma de la interfaz)."""
        for b in self.herramientas:
            if b["kind"] == kind and b.get("texto") == texto:
                return self._herramienta_visible(kind, b, "'%s'" % texto)
        raise SystemExit("capturas: la barra de abajo del viewport %d no tiene '%s' (tiene: %s)"
                         % (kind, texto, [b.get("texto") for b in self.herramientas if b["kind"] == kind]))

    def herramienta_rol(self, kind, rol):
        """Centro de un boton de la barra de abajo por su ROL (la tilde y la cruz no tienen texto)."""
        for b in self.herramientas:
            if b["kind"] == kind and b["rol"] == rol:
                return self._herramienta_visible(kind, b, "de rol %d" % rol)
        raise SystemExit("capturas: la barra de abajo del viewport %d no tiene el rol %d (tiene: %s)"
                         % (kind, rol, [(b["rol"], b.get("texto")) for b in self.herramientas if b["kind"] == kind]))

    def manija(self, n):
        """Centro de una manija del gizmo (GizmoManija: 0..2 ejes X/Y/Z, 3..5 planos, 6 centro, 7 anillo, 8 libre)."""
        if n not in self.manijas:
            raise SystemExit("capturas: no se ve la manija %d del gizmo (se ven: %s)" % (n, sorted(self.manijas)))
        return self.manijas[n]

    def archivo(self, nombre):
        """Centro de un archivo o carpeta del explorador abierto, por su nombre."""
        for a in self.archivos:
            if a.get("nombre") == nombre:
                return a["x"] + a["w"] // 2, a["y"] + a["h"] // 2
        raise SystemExit("capturas: el explorador no muestra '%s' (muestra: %s)" % (nombre, [a.get("nombre") for a in self.archivos]))

    def pestania(self, n):
        """Centro de la pestania n de Propiedades (el numero fijo: 1 Objeto, 2 la del tipo...)."""
        for t in self.pestanias:
            if t["i"] == n:
                return t["x"] + t["w"] // 2, t["y"] + t["h"] // 2
        raise SystemExit("capturas: Propiedades no muestra la pestania %d (muestra: %s)" % (n, [t["i"] for t in self.pestanias]))

    def tarjeta(self, titulo):
        """El titulo de una tarjeta de Propiedades (texto en el idioma de la interfaz): su dict."""
        for t in self.tarjetas:
            if t.get("titulo") == titulo:
                return t
        raise SystemExit("capturas: no hay una tarjeta '%s' (hay: %s)" % (titulo, [t.get("titulo") for t in self.tarjetas]))

    def prop(self, nombre, n=0):
        """Una fila de Propiedades por su nombre (el n-esimo, si se repite): su dict (y, valor, rombo...)."""
        hay = [p for p in self.props if p.get("nombre") == nombre]
        if len(hay) <= n:
            raise SystemExit("capturas: no hay una fila '%s' (hay: %s)" % (nombre, [p.get("nombre") for p in self.props]))
        return hay[n]

    def fila_info(self, nombre):
        """La fila de un objeto en el Outliner: {'x', 'y' (el nombre), 'ojo', 'camara', 'flecha' (sus X), 'elegido'}."""
        for f in self.filas:
            if f.get("nombre") == nombre:
                return f
        raise SystemExit("capturas: el Outliner no muestra '%s' (muestra: %s)" % (nombre, [f.get("nombre") for f in self.filas]))

    def fila(self, nombre):
        """Centro del nombre de un objeto en el Outliner."""
        for f in self.filas:
            if f.get("nombre") == nombre:
                return f["x"], f["y"]
        raise SystemExit("capturas: el Outliner no muestra '%s' (muestra: %s)" % (nombre, [f.get("nombre") for f in self.filas]))

    def centro_candado(self):
        if not self.candado:
            raise SystemExit("capturas: no se ve el candado de la vista de camara")
        x, y, lado = self.candado
        return x + lado // 2, y + lado // 2

    def item(self, ident, nivel=0):
        """Centro de una fila del menu abierto, por su id (no por el texto: cambia con el idioma)."""
        for it in self.items:
            if it["nivel"] == nivel and it["id"] == ident:
                return it["cx"], it["cy"]
        raise SystemExit("capturas: el menu abierto no tiene la opcion %d (tiene: %s)"
                         % (ident, [(i["id"], i.get("texto")) for i in self.items]))


class Guion:
    """Un guion del harness que se va armando: entrada real + fotos.

    w, h     tamanio del layout (la 'ventana' del editor)
    escala   escala de la interfaz (1 = como en el N95)
    ampliar  agrandar cada foto N veces sin suavizar (para la escala 1, que es chica)
    n95      la demo va adentro de la foto del Nokia N95 (n95.py) y se resaltan las teclas que se aprietan:
             True = apaisado (320x240, la Vista 3D al 55 % y Propiedades al 45 %);
             "vertical" = parado (240x320, con el teclado numerico: la Vista 3D arriba y Propiedades abajo)
    raton    dibujar un mouse abajo a la derecha con los botones que se aprietan y la rueda (las demos de PC)
    """

    def __init__(self, idioma, w=960, h=540, escala=2, ampliar=1, n95=False, raton=False):
        self.telefono = None
        if n95 == "vertical":
            self.telefono = n95_mod.VERTICAL
        elif n95:
            self.telefono = n95_mod.HORIZONTAL
        if self.telefono:   # la pantalla del telefono, en escala 1, adentro de su foto (n95.py)
            w, h, escala, ampliar = self.telefono.ancho, self.telefono.alto, 1, 1
        self.idioma, self.w, self.h, self.ampliar, self.n95 = idioma, w, h, ampliar, bool(self.telefono)
        self.lineas = ["ventana %d %d" % (w, h), "escala %d" % escala]
        if self.telefono:
            self.lineas += ["layoutpordefecto"]
            if self.telefono is n95_mod.HORIZONTAL:
                self.lineas += ["division 0.55"]
        self.fotos = []
        self.punteros = {}         # id -> {'tipo': 'dedo'|'raton', 'x', 'y', 'abajo'} (id 1 = el dedo o el mouse)
        self.raton = raton
        self.botones = set()       # los botones del mouse apretados ('izq', 'medio', 'der')
        self.giro = 0              # la rueda en la proxima foto: 1 = arriba, -1 = abajo
        self.mods = []             # las teclas mantenidas mientras se usa el mouse ('Shift', 'Alt')
        self.tmp = tempfile.mkdtemp(prefix="w3d_capturas_")

    @property
    def puntero(self):
        return self.punteros.get(1)

    @puntero.setter
    def puntero(self, p):
        if p is None:
            self.punteros.pop(1, None)
        else:
            self.punteros[1] = p

    # ---- correr ---------------------------------------------------------------
    def _correr(self, lineas):
        guion = os.path.join(self.tmp, "guion.w3s")
        with open(guion, "w", encoding="utf-8") as f:
            f.write("\n".join(lineas) + "\n")
        env = dict(os.environ, LANG=IDIOMAS[self.idioma], LC_ALL=IDIOMAS[self.idioma], LANGUAGE="")
        env.update(getattr(self, "entorno", {}))   # (una receta puede cambiar el HOME: el explorador arranca ahi)
        r = subprocess.run([BIN, "--script", guion], cwd=RAIZ, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        salida = r.stdout.decode("utf-8", "replace")
        if r.returncode != 0:
            raise SystemExit("capturas: el harness fallo\n" + salida[-3000:])
        return salida

    def info(self):
        """Corre lo que hay hasta aca (sin sacar fotos) y devuelve lo que se ve."""
        return Info(self._correr([l for l in self.lineas if not l.startswith("uishot ")] + ["capinfo"]))

    # ---- entrada ---------------------------------------------------------------
    def dedo_abajo(self, x, y, id=1):
        self.lineas.append("dedo down %d %d %d" % (x, y, id))
        self.punteros[id] = {"tipo": "dedo", "x": x, "y": y, "abajo": True}

    def dedo_mover(self, x, y, id=1):
        self.lineas.append("dedo move %d %d %d" % (x, y, id))
        self.punteros[id] = dict(self.punteros.get(id) or {"tipo": "dedo", "abajo": True}, x=x, y=y)

    def dedo_arriba(self, id=1):
        p = self.punteros[id]
        self.lineas.append("dedo up %d %d %d" % (p["x"], p["y"], id))
        self.punteros[id] = dict(p, abajo=False)

    def dedos_quitar(self):
        """Solo el dibujo: saca los dedos que ya se levantaron."""
        self.punteros = {k: p for k, p in self.punteros.items() if p.get("abajo")}

    def tocar(self, x, y):
        self.dedo_abajo(x, y)
        self.dedo_arriba()

    def dedo_flotando(self, x, y):
        """Solo el dibujo: el dedo arriba de (x, y), sin tocar (para mostrar adonde va)."""
        self.puntero = {"tipo": "dedo", "x": x, "y": y, "abajo": False}

    def acercar_dedo(self, x, y, dur=40):
        """El dedo va FLOTANDO desde donde estaba hasta (x, y), por la pantalla y sin apuro (de medio segundo a un
        segundo, segun la distancia), para GUIAR LA MIRADA de quien mira el tutorial. Si no estaba en ningun lado,
        entra desde abajo a la derecha, como una mano que llega a la pantalla."""
        desde = self.puntero
        if desde and desde.get("tipo") == "dedo":
            x0, y0 = desde["x"], desde["y"]
        else:
            x0, y0 = x + 160, y + 200
        dist = math.hypot(x - x0, y - y0)
        if dist < 4:
            self.dedo_flotando(x, y)
            return
        pasos = max(12, min(26, int(dist / 18)))
        for k in range(1, pasos + 1):
            t = k / float(pasos)
            t = t * t * (3 - 2 * t)                  # arranca y frena despacio
            self.dedo_flotando(int(x0 + (x - x0) * t), int(y0 + (y - y0) * t))
            self.foto(dur=dur)

    def toque(self, x, y, antes=500, despues=600):
        """Un toque que se puede SEGUIR: el dedo llega flotando, se ilumina al apoyarse y queda arriba del lugar.
        (El toque de verdad es corto: mantenido, la Vista 3D abriria el menu de la pulsacion larga. El iluminado es
        solo el dibujo de esa foto.)"""
        self.acercar_dedo(x, y)
        self.foto(dur=antes)
        self.puntero = {"tipo": "dedo", "x": x, "y": y, "abajo": True}
        self.foto(dur=300)                           # el dedo apoyado (iluminado) sobre lo que toca, ANTES del cambio
        self.tocar(x, y)
        self.dedo_flotando(x, y)
        self.foto(dur=despues)                       # y lo que pasa despues del toque

    def raton_mover(self, x, y):
        """Mueve el mouse (si hay botones apretados, es un arrastre)."""
        self.lineas.append("raton move %d %d" % (x, y))
        self.puntero = {"tipo": "raton", "x": x, "y": y, "abajo": bool(self.botones)}

    def raton_abajo(self, x, y, boton="izq"):
        self.lineas.append("raton down %d %d %s" % (x, y, boton))
        self.botones.add(boton)
        self.puntero = {"tipo": "raton", "x": x, "y": y, "abajo": True}

    def raton_arriba(self, boton="izq"):
        p = self.puntero
        self.lineas.append("raton up %d %d %s" % (p["x"], p["y"], boton))
        self.botones.discard(boton)
        self.puntero = dict(p, abajo=bool(self.botones))

    def mod(self, *teclas):
        """Mantener teclas modificadoras ('Shift', 'Alt', 'Ctrl') mientras se usa el mouse; mod() las suelta."""
        self.lineas.append(("mod " + " ".join(t.lower() for t in teclas)).strip())
        self.mods = list(teclas)

    def clic(self, x, y, boton="izq"):
        self.lineas += ["raton down %d %d %s" % (x, y, boton), "raton up %d %d %s" % (x, y, boton)]
        self.puntero = {"tipo": "raton", "x": x, "y": y, "abajo": False}

    def rueda(self, pasos, x, y):
        self.lineas.append("rueda %d %d %d" % (pasos, x, y))
        self.giro = 1 if pasos > 0 else -1          # (se dibuja en la proxima foto)

    def tecla(self, nombre):
        self.lineas.append("tecla %s" % nombre)

    def esperar(self, ms):
        self.lineas.append("esperar %d" % ms)

    def sin_puntero(self):
        self.punteros = {}

    def puntero_xy(self):
        """Donde esta el mouse (o el dedo) ahora."""
        p = self.puntero
        return p["x"], p["y"]

    # ---- fotos ----------------------------------------------------------------
    def verde(self):
        """N95: un toque de la tecla verde de llamada (pasa al viewport siguiente)."""
        self.lineas.append("verde")

    def verde_flecha(self, dx, dy, cuadros=1):
        """N95: la tecla verde mantenida + una flecha durante esos cuadros (cambia el tamanio del viewport activo)."""
        self.lineas.append("verde %d %d %d" % (dx, dy, cuadros))

    def keypad(self, accion, *args):
        """N95: el keypad sobre la Vista 3D (ver el comando 'n95' de W3dCapturas.cpp): orbitar dx dy cuadros,
        zoom dy cuadros, panear dx dy cuadros, vista dx dy, perspectiva, encuadrar, camara."""
        self.lineas.append(" ".join(["n95", accion] + [str(a) for a in args]))

    def linea(self, texto):
        """Un comando del harness tal cual (preparar la escena: add, objpos, selobj, vporbita...)."""
        self.lineas.append(texto)

    def foto(self, dur=100, marcas=(), tecla=None, puntero=True, teclas=()):
        """Saca una foto de la interfaz. dur = cuanto dura ese cuadro en la animacion (ms).
        marcas: ('rect', x, y, w, h) | ('circulo', cx, cy, r) | ('etiqueta', cx, cy, texto)  resaltados encima.
        tecla:  texto de la tecla apretada (se dibuja abajo a la derecha).
        teclas: con n95=True, las teclas del telefono que se ven apretadas (ids de n95.svg: 'verde', 'derecha'...)."""
        png = os.path.join(self.tmp, "f%04d.png" % len(self.fotos))
        self.lineas.append("uishot " + png)
        raton = None
        if self.raton:
            raton = {"botones": set(self.botones), "giro": self.giro, "mods": list(self.mods)}
            self.giro = 0
        self.fotos.append({"png": png, "dur": dur, "marcas": list(marcas), "tecla": tecla, "teclas": tuple(teclas),
                           "punteros": [dict(p) for _, p in sorted(self.punteros.items())] if puntero else [],
                           "raton": raton})

    def cuadros(self, ancho=None, calidad=82, recorte=None):
        """Corre el guion entero y devuelve las fotos ya con el dedo / puntero / tecla dibujados.
        ancho: achicar el resultado a ese ancho (las animaciones donde cambia toda la pantalla pesan mucho).
        calidad: la del .webp animado (0-100; las quietas van sin perdida).
        recorte: (x, y, w, h) de la interfaz: mostrar solo eso (la Vista 3D, por ejemplo)."""
        self._correr(self.lineas)
        res = Cuadros()
        res.calidad = calidad
        for f in self.fotos:
            im = Image.open(f["png"]).convert("RGB")
            origen = (0, 0)
            if recorte:
                x, y, w, h = recorte
                im = im.crop((x, y, x + w, y + h))
                origen = (x, y)
            if self.ampliar > 1:
                im = im.resize((im.width * self.ampliar, im.height * self.ampliar), Image.NEAREST)
            if self.telefono:
                # el cartel de la tecla va debajo del telefono, no sobre la pantalla
                im = self.telefono.componer(dibujar(im, dict(f, tecla=None), 1, origen), f["teclas"])
                im = con_cartel(im, f["tecla"])
            else:
                im = dibujar(im, f, self.ampliar, origen)
            if ancho and im.width > ancho:
                im = im.resize((ancho, round(im.height * ancho / im.width)), Image.LANCZOS)
            res.append((im, f["dur"]))
        shutil.rmtree(self.tmp, ignore_errors=True)
        return res


class Cuadros(list):
    """[(imagen, ms), ...] + la calidad con que se guarda la animacion."""
    calidad = 82


# ---- lo que se dibuja encima ----------------------------------------------------
def dibujar(im, f, k, origen=(0, 0)):
    capa = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(capa)
    ox, oy = origen
    for m in f["marcas"]:
        if m[0] == "rect":
            x, y, w, h = [v * k for v in (m[1] - ox, m[2] - oy) + tuple(m[3:5])]
            d.rounded_rectangle([x, y, x + w - 1, y + h - 1], radius=6, outline=VERDE + (255,), width=3, fill=VERDE + (38,))
        elif m[0] == "circulo":
            cx, cy, r = [v * k for v in (m[1] - ox, m[2] - oy, m[3])]
            d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=NARANJA + (255,), width=4)
        elif m[0] == "grafico":    # la FORMA de una curva: ("grafico", x, y, w, h, [(t, v)...], t_actual, titulo)
            gx, gy, gw, gh, puntos, t, titulo = m[1:8]
            x0, y0, w, h = (gx - ox) * k, (gy - oy) * k, gw * k, gh * k
            d.rounded_rectangle([x0, y0, x0 + w, y0 + h], radius=10, fill=(18, 18, 18, 235), outline=(95, 95, 95, 255), width=2)
            fnt = fuente(17 if k == 1 else 21)
            d.text((x0 + 10, y0 + 6), titulo, font=fnt, fill=NARANJA + (255,))
            alto_t = (24 if k == 1 else 28)
            px0, py0, px1, py1 = x0 + 14, y0 + alto_t + 10, x0 + w - 14, y0 + h - 10
            vmin = min(0.0, min(v for _, v in puntos))
            vmax = max(1.0, max(v for _, v in puntos))
            def P(tt, vv):
                return (px0 + (px1 - px0) * tt, py1 - (py1 - py0) * (vv - vmin) / (vmax - vmin))
            for vv in (0.0, 1.0):                  # de donde sale y adonde llega
                d.line([P(0, vv), P(1, vv)], fill=(70, 70, 70, 255), width=1)
            d.line([P(tt, vv) for tt, vv in puntos], fill=VERDE + (255,), width=3, joint="curve")
            if t is not None:                      # por donde va ahora
                v = puntos[-1][1]
                for (ta, va), (tb, vb) in zip(puntos, puntos[1:]):
                    if ta <= t <= tb:
                        v = va + (vb - va) * ((t - ta) / (tb - ta) if tb > ta else 0)
                        break
                cx, cy = P(t, v)
                d.line([(cx, py0), (cx, py1)], fill=(120, 120, 120, 200), width=1)
                r = 6 if k == 1 else 7
                d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255, 255), outline=(25, 25, 25, 255), width=2)
        elif m[0] == "etiqueta":   # un nombre, centrado en (x, y): para decir que es lo marcado
            cx, cy = (m[1] - ox) * k, (m[2] - oy) * k
            fnt = fuente(20 if k == 1 else 24)
            caja = d.textbbox((0, 0), m[3], font=fnt)
            tw, th = caja[2] - caja[0], caja[3] - caja[1]
            x0, y0 = cx - tw / 2 - 12, cy - th / 2 - 8
            d.rounded_rectangle([x0, y0, x0 + tw + 24, y0 + th + 16], radius=9, fill=NARANJA + (240,))
            d.text((x0 + 12 - caja[0], y0 + 8 - caja[1]), m[3], font=fnt, fill=(25, 25, 25, 255))
    for p in f["punteros"]:
        dibujar_puntero(d, dict(p, x=p["x"] - ox, y=p["y"] - oy), k)
    x1 = im.width - 16
    if f.get("raton"):
        x1 = dibujar_raton(capa, f["raton"], x1, im.height - 16)
        for t in reversed(f["raton"]["mods"]):
            x1 = cartel(d, t, x1, im.height - 16, 22 if k == 1 else 26) - 12
    if f["tecla"]:
        cartel(d, f["tecla"], x1, im.height - 16, 22 if k == 1 else 26)
    return Image.alpha_composite(im.convert("RGBA"), capa).convert("RGB")


def dibujar_puntero(d, p, k):
    if p["tipo"] == "dedo":
        x, y = p["x"] * k, p["y"] * k
        r = 24
        if p["abajo"]:   # apoyado: se ILUMINA (verde, con un halo); arriba (flotando): solo el borde
            d.ellipse([x - r - 9, y - r - 9, x + r + 9, y + r + 9], outline=VERDE + (110,), width=6)
            d.ellipse([x - r - 3, y - r - 3, x + r + 3, y + r + 3], outline=(0, 0, 0, 110), width=3)
            d.ellipse([x - r, y - r, x + r, y + r], fill=VERDE + (170,), outline=(255, 255, 255, 240), width=3)
            d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(255, 255, 255, 235))
        else:            # (bien visible tambien flotando: es lo que guia la mirada)
            d.ellipse([x - r - 4, y - r - 4, x + r + 4, y + r + 4], outline=(0, 0, 0, 140), width=4)
            d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, 95), outline=(255, 255, 255, 245), width=4)
            d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=(255, 255, 255, 200))
    elif p["tipo"] == "raton":
        x, y = p["x"] * k, p["y"] * k
        s = 1.4
        flecha = [(0, 0), (0, 18), (4.5, 14), (7.5, 21), (10.5, 19.8), (7.5, 13), (13, 13)]
        pts = [(x + a * s, y + b * s) for a, b in flecha]
        d.polygon(pts, fill=(255, 255, 255, 255), outline=(0, 0, 0, 255))
        d.line(pts + [pts[0]], fill=(0, 0, 0, 255), width=2)


def dibujar_raton(capa, r, x1, y1, ancho=58, alto=90):
    """Un mouse con la esquina de abajo a la derecha en (x1, y1): los botones apretados en verde, la rueda en verde
    si se aprieta (boton del medio) o si gira (con una flecha al costado). Devuelve donde termina a la izquierda."""
    s = 4                                           # se dibuja grande y se achica: bordes suaves
    W, H = ancho * s, alto * s
    corte = int(H * 0.42)                           # donde terminan los botones
    cuerpo = Image.new("L", (W, H), 0)
    ImageDraw.Draw(cuerpo).rounded_rectangle([0, 0, W - 1, H - 1], radius=int(W * 0.48), fill=255)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    im.paste((246, 246, 246, 255), (0, 0), cuerpo)
    for boton, caja in (("izq", (0, 0, W // 2, corte)), ("der", (W // 2, 0, W, corte))):
        if boton in r["botones"]:
            m = Image.new("L", (W, H), 0)
            ImageDraw.Draw(m).rectangle(caja, fill=255)
            im.paste(VERDE + (255,), (0, 0), _y(cuerpo, m))
    d = ImageDraw.Draw(im)
    oscuro = (40, 40, 40, 255)
    d.line([(W // 2, 0), (W // 2, corte)], fill=oscuro, width=2 * s)
    d.line([(0, corte), (W, corte)], fill=oscuro, width=2 * s)
    rw, rh = int(W * 0.2), int(corte * 0.55)
    rueda = [W // 2 - rw // 2, int(corte * 0.2), W // 2 + rw // 2, int(corte * 0.2) + rh]
    activa = "medio" in r["botones"] or r["giro"]
    d.rounded_rectangle(rueda, radius=rw // 2, fill=VERDE + (255,) if activa else (120, 120, 120, 255), outline=oscuro, width=2 * s)
    borde = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(borde).rounded_rectangle([0, 0, W - 1, H - 1], radius=int(W * 0.48), outline=oscuro, width=3 * s)
    im = Image.alpha_composite(im, borde).resize((ancho, alto), Image.LANCZOS)
    x0, y0 = x1 - ancho, y1 - alto
    sombra = Image.new("RGBA", capa.size, (0, 0, 0, 0))
    ImageDraw.Draw(sombra).rounded_rectangle([x0 + 3, y0 + 4, x1 + 3, y1 + 4], radius=int(ancho * 0.48), fill=(0, 0, 0, 110))
    capa.alpha_composite(sombra)
    capa.alpha_composite(im, (x0, y0))
    if r["giro"]:                                   # la flecha del giro, a la izquierda de la rueda
        d = ImageDraw.Draw(capa)
        cx, cy, t = x0 - 16, y0 + int(alto * 0.42 * 0.48), 11
        tri = [(cx - t, cy + t * 0.6), (cx + t, cy + t * 0.6), (cx, cy - t * 0.9)] if r["giro"] > 0 else \
              [(cx - t, cy - t * 0.6), (cx + t, cy - t * 0.6), (cx, cy + t * 0.9)]
        d.polygon(tri, fill=VERDE + (255,), outline=(255, 255, 255, 255))
        d.line(tri + [tri[0]], fill=(255, 255, 255, 255), width=2)
        return x0 - 34
    return x0 - 14


def _y(a, b):
    """La interseccion de dos mascaras (L)."""
    from PIL import ImageChops
    return ImageChops.multiply(a, b)


def cartel(d, texto, x1, y1, tam=26):
    """El cartel de una tecla (blanco, con sombra), con la esquina de abajo a la derecha en (x1, y1). Devuelve
    donde empieza a la izquierda."""
    fnt = fuente(tam)
    caja = d.textbbox((0, 0), texto, font=fnt)
    tw, th = caja[2] - caja[0], caja[3] - caja[1]
    x0, y0 = x1 - tw - 28, y1 - th - 22
    d.rounded_rectangle([x0 + 3, y0 + 4, x1 + 3, y1 + 4], radius=10, fill=(0, 0, 0, 120))
    d.rounded_rectangle([x0, y0, x1, y1], radius=10, fill=(245, 245, 245, 245), outline=(40, 40, 40, 255), width=2)
    d.text((x0 + 14 - caja[0], y0 + 11 - caja[1]), texto, font=fnt, fill=(25, 25, 25, 255))
    return x0


def con_cartel(im, texto, alto=64):
    """El telefono (RGBA, fondo transparente) con una franja abajo para el cartel de la tecla, que va debajo de
    las teclas. La franja esta siempre (con o sin cartel) para que todos los cuadros midan lo mismo."""
    res = Image.new("RGBA", (im.width, im.height + alto), (0, 0, 0, 0))
    res.paste(im, (0, 0))
    if texto:
        d = ImageDraw.Draw(res)
        cartel(d, texto, im.width - 40, im.height + alto - 10)
    return res


def guardar(cuadros, ruta):
    """Un cuadro = imagen quieta (sin perdida). Varios = animacion en bucle."""
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    if len(cuadros) == 1:
        cuadros[0][0].save(ruta, "WEBP", lossless=True, method=6)
    else:
        ims = [c[0] for c in cuadros]
        ims[0].save(ruta, "WEBP", save_all=True, append_images=ims[1:], duration=[c[1] for c in cuadros],
                    loop=0, lossless=False, quality=getattr(cuadros, "calidad", 82), method=6,
                    minimize_size=True, allow_mixed=True)
    return os.path.getsize(ruta)


# ---- recetas ----------------------------------------------------------------------
RECETAS = {}


def receta(nombre):
    """@receta("viewports-menu") def f(idioma) -> [ (imagen, ms), ... ]"""
    def reg(fn):
        RECETAS[nombre] = fn
        return fn
    return reg


def main(argv):
    # las recetas se registran en ESTE modulo importado como 'capturas' (corriendo como script, este
    # archivo es __main__: si registraran ahi, serian dos copias distintas de RECETAS)
    import capturas
    import recetas  # noqa: F401
    RECETAS, guardar = capturas.RECETAS, capturas.guardar
    idiomas = list(IDIOMAS)
    filtros = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--lista":
            for n in sorted(RECETAS):
                print(n)
            return 0
        if a == "--idiomas":
            idiomas = argv[i + 1].split(","); i += 2; continue
        filtros.append(a); i += 1
    for idi in idiomas:
        if idi not in IDIOMAS:
            raise SystemExit("capturas: el editor no tiene la interfaz en '%s' (hay: %s)" % (idi, ", ".join(IDIOMAS)))
    if not os.path.isfile(BIN):
        raise SystemExit("capturas: no encuentro el editor en %s (compilarlo o pasar W3D_BIN)" % BIN)
    elegidas = [n for n in sorted(RECETAS) if not filtros or any(n.startswith(f) for f in filtros)]
    if not elegidas:
        raise SystemExit("capturas: ninguna receta empieza con %s (ver --lista)" % filtros)
    total = 0
    for n in elegidas:
        for idi in idiomas:
            ruta = os.path.join(SALIDA.format(idioma=idi), n + ".webp")
            tam = guardar(RECETAS[n](idi), ruta)
            total += tam
            print("%-34s %s  %6.1f KB" % (n, idi, tam / 1024.0))
    print("listo: %d recetas x %d idiomas, %.1f MB" % (len(elegidas), len(idiomas), total / 1048576.0))
    return 0


if __name__ == "__main__":
    sys.path.insert(0, AQUI)
    sys.exit(main(sys.argv[1:]))

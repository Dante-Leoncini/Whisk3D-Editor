"""Las capturas del tutorial 11, Tu primer script: un cubo que gira (docs/manual/<idioma>/tutoriales/primer-script.md).

El tutorial se hace igual en la PC, en el celular y en el Nokia, asi que las capturas son pocas y quietas; la unica
animada es la del final: el juego andando mientras se cambian las propiedades del script en Propiedades.

La escena es la de fabrica (camara, luz y cubo) con el script del tutorial ya agregado al cubo: lo que queda despues
de los pasos 1 a 4. Se arma en una carpeta temporal (el .w3d en texto y el girar.lua al lado), con los nombres en el
idioma de la interfaz. SCRIPT es el mismo texto que el tutorial: si se cambia uno, cambiar el otro."""
import os
import tempfile

from capturas import receta, Guion, T, PorIdioma, VISTA_3D, IDE, PROPIEDADES, LINEA_DE_TIEMPO, MENU_TIPO
from recetas_viewports import clic_en
from recetas_objeto import teclas, recorrido

PESTANIA_SCRIPTS = 8          # la del joystick en Propiedades

SCRIPT = PorIdioma({
    "es": """propiedades = {
  girar     = true,   -- si el cubo gira o no
  velocidad = 90,     -- cuantos grados gira por segundo
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
    # (los mismos comentarios que el tutorial en cada idioma; hi, ja y zh usan el ingles: el IDE no muestra esas letras)
    "en": """propiedades = {
  girar     = true,   -- whether the cube spins or not
  velocidad = 90,     -- how many degrees it spins per second
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
    "pt": """propiedades = {
  girar     = true,   -- se o cubo gira ou não
  velocidad = 90,     -- quantos graus gira por segundo
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
    "fr": """propiedades = {
  girar     = true,   -- si le cube tourne ou non
  velocidad = 90,     -- combien de degrés il tourne par seconde
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
    "it": """propiedades = {
  girar     = true,   -- se il cubo gira o no
  velocidad = 90,     -- quanti gradi gira al secondo
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
    "tr": """propiedades = {
  girar     = true,   -- küp dönsün mü
  velocidad = 90,     -- saniyede kaç derece döner
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
    "ru": """propiedades = {
  girar     = true,   -- вращается куб или нет
  velocidad = 90,     -- на сколько градусов поворачивается в секунду
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
""",
})

ESCENA = """Whisk3D {
    version: 20251127
    Escena {
        name: "%(escena)s"
        Camera {
            name: "%(camara)s"
            x: -3.0
            y: 2.5
            z: 1.8
        }
        Light {
            name: "%(luz)s"
            x: 1.0
            y: 2.25
            z: 2.25
            r: 1.0
            g: 1.0
            b: 1.0
        }
        Mesh {
            name: "%(cubo)s"
%(script)s        }
    }
}
"""


def proyecto(idioma, con_script=True, codigo=None, archivo="girar.lua"):
    """La carpeta con el .w3d y el girar.lua (queda en /tmp). con_script=False: el cubo todavia sin el script (antes
    de Aniadir). codigo: otro script en vez del del tutorial (las capturas del manual de Lua)."""
    d = tempfile.mkdtemp(prefix="w3d_script_")
    with open(os.path.join(d, archivo), "w", encoding="utf-8") as f:
        f.write(codigo if codigo is not None else SCRIPT[idioma])
    with open(os.path.join(d, "girar.w3d"), "w", encoding="utf-8") as f:
        f.write(ESCENA % {"escena": T(idioma, "Scene"), "camara": T(idioma, "Camera"), "luz": T(idioma, "Light"),
                          "cubo": T(idioma, "Cube"),
                          "script": '            script: "%s"\n' % archivo if con_script else ""})
    return d


def guion(idioma, raton=False, con_script=True, codigo=None, archivo="girar.lua"):
    """La ventana ancha (la barra de la Vista 3D entra entera, con el boton Reproducir) y la Linea de tiempo grande,
    ya convertida en el Editor de scripts con girar.lua abierto. La escena ya es un juego."""
    d = proyecto(idioma, con_script, codigo, archivo)
    g = Guion(idioma, w=1600, h=900, raton=raton)
    g.linea("abrirw3d " + os.path.join(d, "girar.w3d"))
    g.linea("timeline pause")                      # (un .w3d de texto con scripts se abre ya jugando: como el tutorial no)
    g.linea("layoutpordefecto")
    g.linea("division 0.69")
    g.linea("divisionde %d 0.5" % LINEA_DE_TIEMPO)
    g.linea("vpkind %d %d" % (LINEA_DE_TIEMPO, MENU_TIPO(IDE)))
    g.linea("idecargar " + os.path.join(d, archivo))
    g.linea("raiztipo juego")
    g.linea("selobj " + T(idioma, "Cube"))
    g.linea("tick 2")                              # (Propiedades arma las pestanias del cubo en el cuadro siguiente)
    g.clic(*g.info().pestania(PESTANIA_SCRIPTS))   # (propstab no la encuentra: se elige con un click, como a mano)
    g.linea("tick 2")
    g.sin_puntero()
    return g


@receta("script-cambiar-ide")
def cambiar_ide(idioma):
    """El paso 1, con el mouse: el editor como arranca; click en el boton de la esquina de la Linea de tiempo, IDE en
    el menu (la Linea de tiempo pasa a ser el IDE) y despues el mismo boton de la esquina arrastrado para arriba: el IDE
    se agranda y hay mas lugar para escribir."""
    g = Guion(idioma, w=1600, h=900, raton=True)
    g.linea("division 0.69")
    i = g.info()
    esq = i.esquina(LINEA_DE_TIEMPO)
    g.raton_mover(esq[0] + 300, esq[1] - 260)
    g.foto(dur=900)
    clic_en(g, *esq, despues=700)                  # se abre el menu
    x, y = g.info().item(MENU_TIPO(IDE))
    clic_en(g, x, y, antes=800, despues=1000)      # IDE: la Linea de tiempo pasa a ser el IDE
    # agrandarlo: el boton de la esquina, arrastrado para arriba, sube el borde de arriba del IDE
    ex, ey = g.info().esquina(IDE)
    for px, py in recorrido(g.puntero_xy(), (ex, ey), 6):
        g.raton_mover(px, py)
        g.foto(dur=60)
    g.foto(dur=400)
    g.raton_abajo(ex, ey)
    g.foto(dur=300)
    arriba = round(g.h * 0.45)
    for px, py in recorrido((ex, ey), (ex, arriba), 14):
        g.raton_mover(px, py)
        g.foto(dur=60)
    g.raton_arriba()
    g.foto(dur=2200)
    return g.cuadros(ancho=1200, calidad=88)


def guion_proyecto(idioma, raton=False, script_adentro=True):
    """Lo de los pasos 3 y 4: el proyecto ya GUARDADO (un .w3d v4: "Dentro del .w3d" lo necesita) en la carpeta de
    ejemplo (/tmp/Whisk3D, el HOME del editor: ahi arranca el explorador), con el IDE abajo y girar.lua abierto. Con
    script_adentro, el script ya esta guardado ADENTRO del .w3d (Archivo > Guardar en el w3d del IDE), pero todavia no
    esta en el cubo. El cubo elegido y Propiedades en la pestania del joystick."""
    casa = carpeta_ejemplo(idioma)
    d = proyecto(idioma, con_script=False)
    w3d = os.path.join(casa, "girar-cubo.w3d")
    g = Guion(idioma, w=1600, h=900, raton=raton)
    g.entorno = {"HOME": casa}
    g.linea("abrirw3d " + os.path.join(d, "girar.w3d"))
    g.linea("timeline pause")
    g.linea("guardarw3d " + w3d)
    g.linea("abrirw3d " + w3d)
    g.linea("timeline pause")
    g.linea("layoutpordefecto")
    g.linea("division 0.69")
    g.linea("divisionde %d 0.5" % LINEA_DE_TIEMPO)
    g.linea("vpkind %d %d" % (LINEA_DE_TIEMPO, MENU_TIPO(IDE)))
    g.linea("idecargar " + os.path.join(casa, "girar.lua"))
    if script_adentro:
        g.linea("idearchivo 3")                   # Archivo > Guardar en el w3d (el IDE lo pasa adentro del proyecto)
    g.linea("selobj " + T(idioma, "Cube"))
    g.linea("tick 2")
    g.clic(*g.info().pestania(PESTANIA_SCRIPTS))
    g.linea("tick 2")
    g.sin_puntero()
    return g


def foto_formulario(g, recorte_margen=40):
    """La foto del formulario abierto (el "Guardar como..."), recortada a su alrededor."""
    i = g.info()
    p = i.popup
    if not p:
        raise SystemExit("capturas: no hay una ventana emergente abierta")
    g.foto(puntero=False)
    m = recorte_margen
    x0, y0 = max(0, p["x"] - m), max(0, p["y"] - m)
    return g.cuadros(recorte=(x0, y0, min(g.w - x0, p["w"] + 2 * m), min(g.h - y0, p["h"] + 2 * m)))


@receta("script-guardar")
def guardar(idioma):
    """Paso 3: el "Guardar como..." del IDE, con el nombre girar.lua y "Dentro del .w3d" TILDADO (el proyecto ya esta
    guardado: sin eso no se puede)."""
    g = guion_proyecto(idioma, script_adentro=False)
    g.linea("idearchivo 2")                       # Archivo > Guardar como...
    g.linea("guardarcomofijar girar.lua 1")
    return foto_formulario(g)


@receta("script-guardar-afuera")
def guardar_afuera(idioma):
    """Paso 3: el mismo formulario SIN tildar "Dentro del .w3d": aparece la Carpeta (afuera del proyecto)."""
    g = guion_proyecto(idioma, script_adentro=False)
    g.linea("idearchivo 2")
    g.linea("guardarcomofijar girar.lua 0")
    return foto_formulario(g)


@receta("script-ide")
def ide(idioma):
    """El Editor de scripts abajo, donde estaba la Linea de tiempo, con el script escrito."""
    g = guion(idioma)
    g.foto()
    return g.cuadros()


def boton_aniadir(i, idioma):
    """El rectangulo del boton Aniadir de la tarjeta Control (x, y, w, h). Los botones Aniadir y Quitar son una fila
    sin nombre de la tarjeta; Aniadir es la mitad izquierda. capinfo da el centro de la FILA, que tiene aire abajo: el
    boton arranca 2 px arriba de la fila y mide 10 px menos (medido en las capturas, escala 2)."""
    c = i.tarjeta(T(idioma, "Control"))   # (en portugues es "Controle")
    filas = [p for p in i.props if p.get("g") == c["i"]]
    fila = [p for p in filas if not p.get("nombre")][0]
    arriba = fila["y"] - fila["h"] // 2 - 2
    # sin scripts la tarjeta no tiene la lista ni Quitar: Aniadir va de borde a borde
    ancho = (c["w"] // 2 + 5) if any(p.get("nombre") for p in filas) else c["w"] - 8
    return c["x"], arriba, ancho, fila["h"] - 10


def carpeta_ejemplo(idioma):
    """La carpeta donde la persona guardo girar.lua: el HOME del editor en las capturas del explorador (arranca ahi).
    Una carpeta FALSA, con nombres comunes, para que la captura no muestre nada de la maquina donde se genera."""
    import shutil
    d = os.path.join(tempfile.gettempdir(), "Whisk3D")
    shutil.rmtree(d, ignore_errors=True)
    for sub in PorIdioma({"es": ["Documentos", "Descargas", "Imágenes", "Música"],
                          "en": ["Documents", "Downloads", "Pictures", "Music"],
                          "pt": ["Documentos", "Downloads", "Imagens", "Músicas"],
                          "fr": ["Documents", "Téléchargements", "Images", "Musique"],
                          "it": ["Documenti", "Scaricati", "Immagini", "Musica"],
                          "tr": ["Belgeler", "İndirilenler", "Resimler", "Müzik"],
                          "ru": ["Документы", "Загрузки", "Изображения", "Музыка"]})[idioma]:
        os.makedirs(os.path.join(d, sub))
    with open(os.path.join(d, "girar.lua"), "w", encoding="utf-8") as f:
        f.write(SCRIPT[idioma])
    return d


@receta("script-anadir")
def anadir(idioma):
    """Paso 4: el cubo todavia sin script; click en Aniadir (la tarjeta Control): se despliega el menu con los scripts
    del proyecto (girar.lua, el que se guardo adentro del .w3d) y Cargar .lua...; el puntero sobre girar.lua."""
    g = guion_proyecto(idioma)
    i = g.info()
    v = i.vp(PROPIEDADES)
    bx, by, bw, bh = boton_aniadir(i, idioma)
    g.clic(bx + bw // 2, by + bh // 2)
    g.linea("tick 1")
    i2 = g.info()
    item = [it for it in i2.items if it.get("texto") == "girar.lua"][0]
    g.raton_mover(item["cx"], item["cy"])
    g.linea("tick 1")
    g.foto()
    m = i2.menus[0]
    alto = max(by + bh + 24, m["y"] + m["h"] + 16) - v["y"]
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], min(alto, g.h - v["y"])))


@receta("script-cargar-lua")
def cargar_lua(idioma):
    """Paso 4, un script de AFUERA: Aniadir > Cargar .lua... abre el explorador (Cargar script) en la carpeta donde
    esta el .lua; girar.lua elegido y el puntero encima (el boton verde de abajo, Abrir, lo carga)."""
    g = guion_proyecto(idioma, script_adentro=False)
    bx, by, bw, bh = boton_aniadir(g.info(), idioma)
    g.clic(bx + bw // 2, by + bh // 2)
    g.linea("tick 1")
    x, y = g.info().item(100000)                  # Cargar .lua... (kScriptCargarLua en Properties.cpp)
    g.clic(x, y)
    g.linea("tick 2")
    x, y = g.info().archivo("girar.lua")
    g.raton_mover(x, y)
    g.clic(x, y)                                  # un click lo elige (dos, lo abren)
    g.linea("tick 1")
    g.foto()
    return g.cuadros()


@receta("script-propiedades")
def propiedades(idioma):
    """Paso 4, despues de Aniadir: girar.lua en la lista de la tarjeta Control y la tarjeta del script, con sus
    propiedades: girar (una casilla) y velocidad (un numero). Sin marcas."""
    g = guion(idioma)
    i = g.info()
    v = i.vp(PROPIEDADES)
    ultima = i.prop("velocidad")                    # (la ultima fila de la tarjeta del script)
    alto = ultima["y"] + ultima["h"] // 2 + 16 - v["y"]
    g.foto(puntero=False)
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], alto))


@receta("script-convertir-juego")
def convertir_juego(idioma):
    """Paso 5: el menu del boton de la escena (en la barra de la Vista 3D) abierto, con el puntero sobre Convertir en
    juego. La escena de fabrica con el IDE abajo (como queda despues del paso 3)."""
    d = proyecto(idioma, con_script=False)
    g = Guion(idioma, w=1600, h=900)
    g.linea("division 0.69")
    g.linea("divisionde %d 0.5" % LINEA_DE_TIEMPO)
    g.linea("vpkind %d %d" % (LINEA_DE_TIEMPO, MENU_TIPO(IDE)))
    g.linea("idecargar " + os.path.join(d, "girar.lua"))
    g.linea("tick 2")
    i = g.info()
    escena = [b for b in i.botones if b["kind"] == VISTA_3D and b.get("rol") == 18][0]   # el boton de la escena
    g.clic(escena["x"] + escena["w"] // 2, escena["y"] + escena["h"] // 2)
    g.linea("tick 1")
    x, y = g.info().item(3004)                      # Convertir en juego (el id de la opcion, no el texto)
    g.raton_mover(x, y)
    g.linea("tick 1")
    g.foto()
    v = i.vp(VISTA_3D)
    return g.cuadros(recorte=(v["x"], v["y"], v["w"], v["h"]))


@receta("script-jugar")
def jugar(idioma):
    """Reproducir: el cubo gira a 90. velocidad 360: gira mas rapido. girar destildado: se queda quieto. Parar: TODO
    vuelve a como estaba antes del Play (el cubo a su lugar, la velocidad a 90 y girar tildada): lo que pasa jugando se
    pierde. Por eso el bucle empalma solo (arranca y termina igual)."""
    g = guion(idioma, raton=True)
    i = g.info()
    play = i.boton(VISTA_3D, T(idioma, "Play"))
    vel = i.prop("velocidad")
    gir = i.prop("girar")
    g.fotos = []
    inicio = (play[0] + 60, play[1] + 160)
    g.raton_mover(*inicio)
    g.foto(dur=800)
    clic_en(g, *play, despues=100)

    def andar(n, dur=90):
        for _ in range(n):                         # (tick dibuja pero no avanza el juego: simplay si, 3 cuadros = 0,1 s)
            g.linea("simplay 3")
            g.foto(dur=dur)

    andar(12)
    # la velocidad: click en el campo, 360 y Enter
    clic_en(g, vel["valor"] + 40, vel["y"], antes=90, despues=90)
    teclas(g, ["=360"], "360", dur=300)
    teclas(g, ["Return"], "Enter", dur=90)
    andar(14)
    # girar: destildarlo (se queda quieto)
    clic_en(g, gir["valor"], gir["y"], antes=90, despues=90)
    andar(8)
    # Parar (con el juego andando, la barra de la Vista 3D es otra: se busca ahi)
    parar = g.info().boton(VISTA_3D, T(idioma, "Stop"))
    clic_en(g, *parar, despues=100)
    g.linea("tick 1")
    g.foto(dur=1400)
    for px, py in recorrido(g.puntero_xy(), inicio, 6):   # el puntero vuelve adonde arranco: el bucle no salta
        g.raton_mover(px, py)
        g.foto(dur=60)
    return g.cuadros(ancho=1200, calidad=88)


# ---- el manual de Lua: "Un script es un componente de un objeto" (docs/manual/<idioma>/lua/scripts-y-objetos.md) ----
LUA_PROPIEDADES = PorIdioma({
    "es": """propiedades = {
  pelota     = "objeto",                          -- un campo para elegir un objeto de la escena
  dificultad = { "facil", "normal", "dificil" },  -- una lista desplegable (la primera es la de fabrica)
  velocidad  = 200,                               -- un numero propio de cada objeto que use el script
  activo     = true,                              -- un si/no
  etiqueta   = "caja",                            -- un texto
}

function inicio()
  local p   = objeto("pelota")
  local dif = opcion("dificultad")
  local v   = propiedad("velocidad")
end
""",
})
LUA_PROPIEDADES["en"] = LUA_PROPIEDADES["es"]   # (la captura es la tarjeta, no el codigo: sirve igual en todos)
LUA_BOTON = PorIdioma({
    "es": """propiedades = {
  velocidad = 90,
  cero = { "Volver a cero", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
    # (lo que dice el boton, como en el manual de cada idioma: lua/scripts-y-objetos.md)
    "en": """propiedades = {
  velocidad = 90,
  cero = { "Back to zero", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
    "pt": """propiedades = {
  velocidad = 90,
  cero = { "Voltar a zero", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
    "fr": """propiedades = {
  velocidad = 90,
  cero = { "Revenir à zéro", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
    "it": """propiedades = {
  velocidad = 90,
  cero = { "Torna a zero", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
    "tr": """propiedades = {
  velocidad = 90,
  cero = { "Sıfıra dön", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
    "ru": """propiedades = {
  velocidad = 90,
  cero = { "Вернуть в ноль", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
""",
})


def tarjeta_del_script(idioma, codigo):
    """Propiedades del cubo con ese script: solo la tarjeta del script (de su titulo a su ultima fila)."""
    g = guion(idioma, codigo=codigo, archivo="ejemplo.lua")
    i = g.info()
    v = i.vp(PROPIEDADES)
    t = i.tarjeta("ejemplo.lua")
    filas = [p for p in i.props if p.get("g") == t["i"]]
    abajo = max(p["y"] + p["h"] // 2 for p in filas) + 12
    g.foto(puntero=False)
    return g.cuadros(recorte=(v["x"], t["y"] - 6, v["w"], abajo - (t["y"] - 6)))


@receta("lua-propiedades")
def lua_propiedades(idioma):
    """El script de ejemplo de la pagina (pelota, dificultad, velocidad, activo, etiqueta) en la tarjeta del script."""
    return tarjeta_del_script(idioma, LUA_PROPIEDADES[idioma])


@receta("lua-boton")
def lua_boton(idioma):
    """Un BOTON en las propiedades: velocidad y el boton "Volver a cero" (una funcion en `propiedades`)."""
    return tarjeta_del_script(idioma, LUA_BOTON[idioma])


# ---- el proyecto del ejemplo, para descargar ------------------------------------------------------------------------
def proyecto_ejemplo(idioma, salida):
    """Arma girar-cubo.w3d: lo que queda al terminar el tutorial, hecho con el editor por la MISMA puerta que la persona.
    La escena de fabrica, guardada (Dentro del .w3d lo necesita); Convertir en juego (una COPIA: la escena queda y el
    juego "Scene.001" pasa a ser el activo); girar.lua guardado ADENTRO del .w3d desde el IDE (Archivo > Guardar en el
    w3d); al cubo del juego con Aniadir > girar.lua; y el juego como escena de arranque (al abrir el ejemplo se ve el
    juego, con Reproducir). Se guarda con el IDE en lugar de la Linea de tiempo."""
    from capturas import IDIOMAS, BIN, RAIZ
    import subprocess
    d = tempfile.mkdtemp(prefix="w3d_proyecto_")
    lua = os.path.join(d, "girar.lua")
    with open(lua, "w", encoding="utf-8") as f:
        f.write(SCRIPT[idioma])
    w3d = os.path.join(d, "girar-cubo.w3d")

    def correr(lineas):
        ruta = os.path.join(d, "guion.w3s")
        with open(ruta, "w", encoding="utf-8") as f:
            f.write("\n".join(lineas) + "\n")
        env = dict(os.environ, LANG=IDIOMAS[idioma], LC_ALL=IDIOMAS[idioma], LANGUAGE="")
        r = subprocess.run([BIN, "--script", ruta], cwd=RAIZ, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=600)
        if r.returncode != 0:
            raise SystemExit("proyecto: el harness fallo\n" + r.stdout.decode("utf-8", "replace")[-3000:])

    cubo = T(idioma, "Cube")
    os.makedirs(os.path.dirname(os.path.abspath(salida)), exist_ok=True)
    correr(["ventana 1600 900", "escala 2", "tick 2",
            "guardarw3d " + w3d, "abrirw3d " + w3d, "tick 2",                        # el proyecto, guardado
            "raizmenu 3d Convert to Game", "tick 2",                                  # el juego (una copia)
            "layoutpordefecto", "divisionde %d 0.5" % LINEA_DE_TIEMPO, "vpkind %d %d" % (LINEA_DE_TIEMPO, MENU_TIPO(IDE)),
            "idecargar " + lua, "idearchivo 3",                                       # girar.lua, adentro del .w3d
            "selobj " + cubo, "tick 2",
            "scriptanadir", "menuelegir girar.lua",                                   # Aniadir > girar.lua
            "raizmenu 3d Set as Start Scene", "tick 1",                               # el juego, de arranque
            "guardarw3d " + os.path.abspath(salida)])
    # control: abierto de cero es un juego y el cubo gira 90 grados en 1 segundo (30 cuadros)
    correr(["abrirw3d " + os.path.abspath(salida), "raizmodo kind 2", "raizmodo juego 1", "selobj " + cubo,
            "simplay 30", "objroteuler 0 90 0 0.5", "simstop"])
    return salida

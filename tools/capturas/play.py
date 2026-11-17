"""
Las imagenes de la ficha de Whisk3D en GOOGLE PLAY, generadas con el editor de verdad (como las de los tutoriales):

    icono-512.png                el icono (512x512, sRGB, el engranaje de 384 en el cuadro de las keylines, fondo
                                 negro, sin transparencia ni esquinas redondas: Play las pone)
    destacado-1024x500.png       el "elemento grafico destacado" (1024x500)
    captura-01..08.png           las capturas de pantalla (1920x1080, 16:9)

Uso:
    python3 tools/capturas/play.py [carpeta]          (por defecto ~/Escritorio/google-play): todo, en los 9 idiomas
    python3 tools/capturas/play.py [carpeta] render   solo las que tienen "render" en el nombre
    python3 tools/capturas/play.py [carpeta] --idiomas=en,pt   solo esos idiomas
Cada idioma va en su carpeta ("en-US English"...: el codigo de la ficha de Play) con las capturas y el destacado.

Necesita el build de Linux (o W3D_BIN), Pillow y Google Chrome (el icono sale del .svg con Chrome sin ventana).
Las escenas se arman con los comandos del harness (main/test/W3dCapturas.cpp): 'vista3d trazado' (el trazado de
rayos en la Vista 3D) y 'curvamod' (los modificadores de curva) existen para estas capturas.
"""
import math, os, subprocess, sys, tempfile

from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
from capturas import Guion, T, MENU_TIPO, IDE, LINEA_DE_TIEMPO, RAIZ
import recetas                     # (las recetas se importan entre si: primero el modulo que las junta)
import recetas_script as rs

EJEMPLOS = os.path.join(RAIZ, "examples", "assets")
W, H, ESCALA = 1920, 1080, 3       # 16:9; escala 3 = como se ve en un telefono (se lee en las miniaturas de Play)


# los IDIOMAS del editor (capturas.IDIOMAS) con su codigo de Google Play y el nombre de su carpeta
IDIOMAS_PLAY = [("es", "es-419", "Español"), ("en", "en-US", "English"), ("pt", "pt-BR", "Português"),
                ("fr", "fr-FR", "Français"), ("it", "it-IT", "Italiano"), ("tr", "tr-TR", "Türkçe"),
                ("de", "de-DE", "Deutsch"), ("pl", "pl-PL", "Polski"), ("ru", "ru-RU", "Русский")]

# los nombres de los objetos de las escenas, en cada idioma (los de la escena de fabrica los traduce el editor)
NOMBRES = {
    "es": dict(pelota="Pelota", piso="Piso", esfera="Esfera", oro="Oro", dron="Dron"),
    "en": dict(pelota="Ball", piso="Floor", esfera="Sphere", oro="Gold", dron="Drone"),
    "pt": dict(pelota="Bola", piso="Chão", esfera="Esfera", oro="Ouro", dron="Drone"),
    "fr": dict(pelota="Balle", piso="Sol", esfera="Sphère", oro="Or", dron="Drone"),
    "it": dict(pelota="Palla", piso="Pavimento", esfera="Sfera", oro="Oro", dron="Drone"),
    "tr": dict(pelota="Top", piso="Zemin", esfera="Küre", oro="Altın", dron="Dron"),
    "de": dict(pelota="Ball", piso="Boden", esfera="Kugel", oro="Gold", dron="Drohne"),
    "pl": dict(pelota="Piłka", piso="Podłoga", esfera="Kula", oro="Złoto", dron="Dron"),
    "ru": dict(pelota="Мяч", piso="Пол", esfera="Сфера", oro="Золото", dron="Дрон"),
}


def guion(idioma, w, h, esc):
    g = Guion(idioma, w=w, h=h, escala=esc)
    g.nombres = dict(NOMBRES[idioma], cubo=T(idioma, "Cube"), luz=T(idioma, "Light"))
    return g


def guardar(g, ruta):
    g.sin_puntero()
    g.foto(puntero=False)
    g.cuadros()[-1][0].convert("RGB").save(ruta, optimize=True)
    print("  ", os.path.basename(ruta))


def lineas(g, texto):
    nombres = getattr(g, "nombres", None)   # ({cubo}, {pelota}...: los nombres en el idioma del guion)
    if nombres:
        texto = texto.format(**nombres)
    for l in texto.strip().split("\n"):
        l = l.strip()
        if l and not l.startswith("#"):
            g.linea(l)


# ---- el icono -----------------------------------------------------------------------------------------------------------
def png_srgb(ruta):
    """El espacio de color que pide Play, DICHO en el archivo: despues del IHDR, los chunks sRGB (intencion perceptual) y
    gAMA (1/2.2, el que el estandar PNG pide junto al sRGB). Pillow no los escribe; se insertan a mano."""
    import struct, zlib
    d = open(ruta, "rb").read()
    def chunk(tipo, datos):
        return struct.pack(">I", len(datos)) + tipo + datos + struct.pack(">I", zlib.crc32(tipo + datos) & 0xffffffff)
    p, partes = 8, []
    while p < len(d):
        n = struct.unpack(">I", d[p:p + 4])[0]
        tipo = d[p + 4:p + 8]
        if tipo not in (b"sRGB", b"gAMA", b"cHRM", b"iCCP"):   # (los que contradirian al sRGB)
            partes.append(d[p:p + 12 + n])
            if tipo == b"IHDR":
                partes.append(chunk(b"sRGB", b"\x00") + chunk(b"gAMA", struct.pack(">I", 45455)))
        p += 12 + n
    open(ruta, "wb").write(d[:8] + b"".join(partes))


def icono(carpeta):
    svg = os.path.join(RAIZ, "res", "Whisk3D.svg")
    tmp = tempfile.mkdtemp(prefix="w3d_play_")
    def render(lado, salida):
        html = os.path.join(tmp, "icono.html")
        with open(html, "w") as f:
            f.write('<!doctype html><html><head><style>html,body{margin:0;background:transparent}'
                    'img{display:block;width:%dpx;height:%dpx}</style></head><body><img src="file://%s"></body></html>'
                    % (lado, lado, svg))
        subprocess.run(["google-chrome", "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--default-background-color=00000000", "--window-size=%d,%d" % (lado, lado),
                        "--allow-file-access-from-files", "--screenshot=" + salida, "file://" + html],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    # LA ESPECIFICACION DE PLAY (developer.android.com/distribute/google-play/resources/icon-design-specifications):
    # 512x512, PNG de 32 bits, sRGB, hasta 1024 KB; un CUADRADO LLENO (Play redondea las esquinas, radio = 30 % del lado,
    # y le pone la sombra: el archivo no lleva ni esquinas redondas ni sombra) y SIN TRANSPARENCIA (un fondo de color).
    # Un logo con forma propia, como el engranaje, no se estira a todo el cuadro: va sobre la grilla de keylines, que es
    # el cuadro de 384 (512 x 0,75) del medio: el engranaje mide 384 (lo pidio asi el usuario). El fondo, negro.
    chico = os.path.join(tmp, "chico.png")
    render(384, chico)
    fondo = Image.new("RGBA", (512, 512), (0, 0, 0, 255))
    eng = Image.open(chico).convert("RGBA")
    fondo.alpha_composite(eng, ((512 - eng.width) // 2, (512 - eng.height) // 2))
    ruta = os.path.join(carpeta, "icono-512.png")
    fondo.save(ruta, optimize=True)
    png_srgb(ruta)
    print("   icono-512.png")
    viejo = os.path.join(carpeta, "icono-512-transparente.png")   # (lo generaba una version anterior: Play no lo acepta)
    if os.path.isfile(viejo):
        os.remove(viejo)


# ---- las escenas -----------------------------------------------------------------------------------------------------------
def escena_animacion(idioma="es", w=W, h=H, esc=ESCALA):
    """Una pelota que pica (con estirar y aplastar) y la Linea de tiempo grande en el editor de CURVAS."""
    g = guion(idioma, w, h, esc)
    lineas(g, """
        selobj {cubo}
        delete
        cursor3d 0 1 0
        add sphere
        objname {pelota}
        cursor3d 0 0 0
        origen cursor
        selobj {pelota}
        matnew
        matcolor 1 0.45 0.1
        keyponer 3 loc 0 0 0
        keyponer 16 loc 0 0 5
        keyponer 29 loc 0 0 0
        keyinterp out 3 loc
        keyinterp in 16 loc
        keyponer 1 esc 1 1 1
        keyponer 2 esc 1.6 1.6 0.75
        keyponer 4 esc 0.9 0.9 1.1
        keyponer 17 esc 1 1 1
        keyponer 29 esc 1 1 1
        tl fin 28
        cursor3d 0 -0.05 0
        add cube
        objname {piso}
        objscale 5 0.05 5
        matnew
        matcolor 0.2 0.55 0.9
        cursor3d 0 -30 0
        selobj {pelota}
        vpvista material
        vista3d gizmo 0
        division 0.7
        divisionde 5 0.5
        tl curvas
        frame 9
        tick 3""")
    x, y = g.info().tl_fila(0, 1)          # la fila "Z Posicion": elige su curva (las tarjetas Curva / Keyframes)
    g.clic(x, y)
    lineas(g, """
        tick 2
        tl vista -1 31 -0.6 5.6
        vporbita 0 2.8 0 13 -8 -30
        propstab 9
        tick 2""")
    return g


COMENTARIOS = {
    "es": ("Personaje: camina con las flechas, salta con Espacio", "y junta las monedas que toca su hitbox",
           "velocidad vertical (el salto)", "salto: sube, la gravedad lo baja y no pasa del piso"),
    "en": ("Character: walks with the arrows, jumps with Space", "and collects the coins its hitbox touches",
           "vertical speed (the jump)", "jump: goes up, gravity brings it down, never below the floor"),
    "pt": ("Personagem: anda com as setas, pula com Espaço", "e junta as moedas que o seu hitbox toca",
           "velocidade vertical (o pulo)", "pulo: sobe, a gravidade o traz de volta e não passa do chão"),
    "fr": ("Personnage : marche avec les flèches, saute avec Espace", "et ramasse les pièces que touche sa hitbox",
           "vitesse verticale (le saut)", "saut : il monte, la gravité le fait redescendre, jamais sous le sol"),
    "it": ("Personaggio: cammina con le frecce, salta con Spazio", "e raccoglie le monete che tocca la sua hitbox",
           "velocità verticale (il salto)", "salto: sale, la gravità lo riporta giù e non passa il pavimento"),
    "tr": ("Karakter: oklarla yürür, Boşluk ile zıplar", "ve hitbox'ının dokunduğu paraları toplar",
           "dikey hız (zıplama)", "zıplama: yükselir, yerçekimi indirir, zeminin altına geçmez"),
    "de": ("Figur: läuft mit den Pfeiltasten, springt mit Leertaste", "und sammelt die Münzen, die ihre Hitbox berührt",
           "vertikale Geschwindigkeit (der Sprung)", "Sprung: steigt, die Schwerkraft zieht sie runter, nie unter den Boden"),
    "pl": ("Postać: chodzi strzałkami, skacze Spacją", "i zbiera monety, których dotknie jej hitbox",
           "prędkość pionowa (skok)", "skok: wznosi się, grawitacja ściąga w dół, nie schodzi pod podłogę"),
    "ru": ("Персонаж: ходит стрелками, прыгает пробелом", "и собирает монеты, которых касается его хитбокс",
           "вертикальная скорость (прыжок)", "прыжок: вверх, гравитация тянет вниз, не ниже пола"),
}


def escena_codigo(idioma="es", w=W, h=H, esc=ESCALA):
    """El Editor de scripts con el script de un personaje (del manual de Lua) y sus propiedades en el panel."""
    c = COMENTARIOS[idioma]
    codigo = """-- %s
-- %s
propiedades = { velocidad = 4, salto = 6 }

local vz = 0        -- %s
local monedas = 0

function actualizar(dt)
  local yo_ = yo()
  local v = propiedad("velocidad")
  local dx, dy = 0, 0
  if tecla("izquierda") then dx = dx - 1 end
  if tecla("derecha")   then dx = dx + 1 end
  if tecla("arriba")    then dy = dy - 1 end
  if tecla("abajo")     then dy = dy + 1 end
  mover(yo_, dx * v * dt, dy * v * dt, 0)

  -- %s
  local x, y, z = posicion(yo_)
  if teclaApretada("espacio") and z <= 0 then
    vz = propiedad("salto")
  end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosicion(yo_, x, y, z)
end

function alEntrar(otro, miHitbox, suHitbox)
  if string.sub(nombre(otro), 1, 6) == "Moneda" then
    destruir(otro)
    monedas = monedas + 1
    setCompartido("monedas", monedas)
    sonido("sonidos/moneda.wav")
  end
end
""" % c
    d = rs.proyecto(idioma, codigo=codigo, archivo="personaje.lua")
    g = guion(idioma, w, h, esc)
    g.linea("abrirw3d " + os.path.join(d, "girar.w3d"))
    lineas(g, """
        timeline pause
        layoutpordefecto
        division 0.7
        divisionde %d 0.36
        vpkind %d %d""" % (LINEA_DE_TIEMPO, LINEA_DE_TIEMPO, MENU_TIPO(IDE)))
    g.linea("idecargar " + os.path.join(d, "personaje.lua"))
    lineas(g, """
        raiztipo juego
        selobj %s
        matnew
        matcolor 0.2 0.8 0.35
        vpvista material
        vista3d gizmo 0
        tick 2""" % T(idioma, "Cube"))
    g.clic(*g.info().pestania(rs.PESTANIA_SCRIPTS))
    g.linea("tick 2")
    return g


def escena_render(idioma="es", w=W, h=H, esc=ESCALA, oro=True):
    """El TRAZADO DE RAYOS adentro de la Vista 3D: sombras suaves y una esfera de oro que refleja."""
    g = guion(idioma, w, h, esc)
    lineas(g, """
        selobj {cubo}
        objpos 0.3 1 -2
        objrot 0 28 0
        matnew
        matcolor 0.12 0.35 0.95
        matrt 0.35 0
        cursor3d 0 0 0
        add plane
        objname {piso}
        objscale 9 1 9
        matnew
        matcolor 0.8 0.8 0.78
        matrt 0.5 0
        cursor3d -2.2 1 0.8
        add sphere
        objname {esfera}
        matnew
        matcolor 0.9 0.12 0.08
        matrt 0.2 0
        modadd subsurf
        modsub 2 0
        cursor3d 2.1 1 0.6
        add sphere
        objname {oro}
        matnew
        matcolor 0.95 0.72 0.28
        matnombre {oro}
        matrt 0.08 1
        modadd subsurf
        modsub 2 0
        cursor3d 0 -30 0
        selobj {luz}
        objpos 3 6 3
        rtlight 0.8 4 0
        selobj {oro}
        tick 2""")
    if oro:
        lineas(g, """
            propstab 2
            tick 2
            propscroll 10""")
    lineas(g, """
        vporbita 0 0.9 0 11 -18 -28
        vista3d gizmo 0
        vista3d overlays 0
        vista3d trazado 1 48""")
    for _ in range(80):                    # el trazado avanza con el TIEMPO (hilos): esperar y dibujar
        g.linea("esperar 150")
        g.linea("tick 1")
    return g


def escena_modelado(idioma="es", w=W, h=H, esc=ESCALA, pestania=4):
    """Un jarron hecho con extruir y escalar sobre un cubo con Subdivision Surface, en el Modo Edicion."""
    g = guion(idioma, w, h, esc)
    lineas(g, """
        selobj {cubo}
        matnew
        matcolor 0.9 0.55 0.2
        modadd subsurf
        modsub 2 0
        mode edit
        selmode face
        caras 4""")
    # extruir / escalar con el teclado, como una persona (el mouse se mueve entre una y otra)
    for i, (tecla, valor) in enumerate((("e", "0.8"), ("s", "1.7"), ("e", "0.6"), ("s", "0.35"), ("e", "1.4"))):
        x, y = 400 + i * 10, 300 + i * 10
        lineas(g, """
            raton move %d %d
            tecla %s
            raton move %d %d
            texto %s
            tecla Return
            tick 2""" % (x, y, tecla, x + 2, y, valor))
    # sombreado suave: la operacion no tiene tecla de fabrica; se le asigna una (Ajustes > Atajos de teclado)
    lineas(g, """
        atajo 3d.suave F9
        raton move 450 350
        tecla a
        tecla F9
        tecla a
        caras 4
        modsub 3 0
        vpvista material
        vista3d gizmo 0
        vporbita 0 1.6 0 10 -20 -35
        propstab %d
        tick 3""" % pestania)
    return g


def escena_uv(idioma="es", w=W, h=H, esc=ESCALA):
    """Una cabeza de mono con la textura de prueba y sus islas UV (Smart UV Project) en el Editor UV."""
    g = guion(idioma, w, h, esc)
    g.linea("selobj " + T(idioma, "Cube"))
    g.linea("delete")
    g.linea("import " + os.path.join(EJEMPLOS, "monkey.obj"))
    g.linea("texturas")
    g.linea("matnew")
    g.linea("matmapa base textura " + os.path.join(EJEMPLOS, "UV_Texture.png"))
    lineas(g, """
        texturas
        modo edicion
        raton move 500 300
        tecla a
        smartuv 66 0.02
        modo objeto
        raton move 500 300
        tecla r
        tecla x
        texto 90
        tecla Return
        vpvista material
        vista3d gizmo 0
        modo edicion
        raton move 500 300
        tecla a
        vista3d overlays 0
        vpkind 5 3
        tick 2
        divisionde 4 0.45
        vporbita 0 0 0 3.6 -6 195
        tick 3""")
    return g


def escena_pintura(idioma="es", w=W, h=H, esc=ESCALA):
    """Pintura de vertices: una pelota con trazos de colores (arrastrando el pincel, como una persona)."""
    g = guion(idioma, w, h, esc)
    lineas(g, """
        selobj {cubo}
        delete
        cursor3d 0 1.3 0
        add sphere
        objname {pelota}
        modadd subsurf
        modsub 2 0
        modapply
        cursor3d 0 -30 0
        vporbita 0 1.3 0 5.5 -10 -25
        vista3d gizmo 0
        vpvista material
        modo vertices
        brushradius 70
        tick 2""")
    v = g.info().vp(1)
    cx, cy = v["x"] + v["w"] // 2, v["y"] + v["barra"] + (v["h"] - v["barra"]) // 2
    r = int(min(v["w"], v["h"]) * 0.30)
    def trazo(puntos, pasos=24):
        g.linea("raton move %d %d" % puntos[0])
        g.linea("raton down %d %d izq" % puntos[0])
        for a, b in zip(puntos, puntos[1:]):
            for k in range(1, pasos + 1):
                t = k / pasos
                g.linea("raton move %d %d" % (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        g.linea("raton up %d %d izq" % puntos[-1])
    for col, dy in (("0.95 0.25 0.2", -0.55), ("1 0.8 0.15", -0.2), ("0.25 0.8 0.35", 0.15), ("0.2 0.5 0.95", 0.5)):
        g.linea("brushcolor %s 1 1" % col)
        y = cy + int(dy * r)
        pts = [(cx - r - 40 + i * (2 * r + 80) // 6, y + int(math.sin(i * 1.3) * 20)) for i in range(7)]
        trazo(pts)
        trazo([(px, py + int(0.12 * r)) for (px, py) in pts])
    g.linea("raton move %d %d" % (cx + r + 120, cy - r))
    g.linea("tick 2")
    return g


def escena_modificadores(idioma="es", w=W, h=H, esc=ESCALA):
    """Los MODIFICADORES DE CURVA: un dron que flota, con Ruido en su altura (la curva con su ruido, en el timeline)."""
    g = guion(idioma, w, h, esc)
    lineas(g, """
        selobj {cubo}
        objname {dron}
        objscale 1.2 0.3 1.2
        matnew
        matcolor 0.95 0.3 0.55
        keyponer 1 loc 0 0 1
        keyponer 30 loc 0 0 2.6
        keyponer 60 loc 0 0 1
        keyinterp inout 1 loc
        keyinterp inout 30 loc
        curvamod loc z ruido fuerza 0.6 escala 3 profundidad 2
        cursor3d 0 -0.05 0
        add cube
        objname {piso}
        objscale 4 0.05 4
        matnew
        matcolor 0.25 0.3 0.35
        cursor3d 0 -30 0
        selobj {dron}
        vpvista material
        vista3d gizmo 0
        division 0.7
        divisionde 5 0.52
        tl curvas
        tl fin 60
        frame 40
        tick 3""")
    x, y = g.info().tl_fila(0, 1)          # la fila "Z Posicion": la curva ACTIVA (la de los modificadores de curva)
    g.clic(x, y)
    lineas(g, """
        tick 2
        tl vista -2 64 0.2 3.6
        propstab 4
        tick 2
        vporbita 0 1.4 0 9 -15 -30""")
    return g


def escena_atajos(idioma="es", w=W, h=H, esc=ESCALA):
    """La ventana de los atajos de teclado (Vista / Contexto) arriba del jarron."""
    g = escena_modelado(idioma, w, h, esc, pestania=0)
    # (la ventana sale en el medio: el jarron se corre a la izquierda para que se vea al lado)
    lineas(g, """
        modo objeto
        objpos -3.4 0 -5.5
        vporbita 0 1.6 0 13 -20 -35
        raton move 300 300
        tick 2
        atajosventana 9 3
        tick 2""")
    return g


CAPTURAS = [
    ("captura-01-animacion.png", escena_animacion),
    ("captura-02-codigo.png", escena_codigo),
    ("captura-03-render.png", escena_render),
    ("captura-04-modelado.png", escena_modelado),
    ("captura-05-uv.png", escena_uv),
    ("captura-06-pintura.png", escena_pintura),
    ("captura-07-modificadores-de-curva.png", escena_modificadores),
    ("captura-08-atajos.png", escena_atajos),
]


def main(argv):
    """play.py [carpeta] [filtro] [--idiomas es,en,...]: una carpeta por idioma ("en-US English"...) con el destacado y
    las 8 capturas; el icono (el mismo para todos) va afuera."""
    args = [a for a in argv[1:] if not a.startswith("--")]
    idiomas = [i for i, _, _ in IDIOMAS_PLAY]
    for a in argv[1:]:
        if a.startswith("--idiomas="):
            idiomas = a.split("=", 1)[1].split(",")
    carpeta = os.path.expanduser(args[0]) if args else os.path.expanduser("~/Escritorio/google-play")
    filtro = args[1] if len(args) > 1 else ""
    os.makedirs(carpeta, exist_ok=True)
    if not filtro or "icono" in filtro:
        icono(carpeta)
        if filtro == "icono":
            return
    for idioma, codigo, nombre in IDIOMAS_PLAY:
        if idioma not in idiomas:
            continue
        sub = os.path.join(carpeta, "%s %s" % (codigo, nombre))
        os.makedirs(sub, exist_ok=True)
        print(sub)
        if not filtro or "destacado" in filtro:
            # el elemento grafico destacado: el trazado de rayos, a 1024x500 (escala 2)
            guardar(escena_render(idioma, 1024, 500, 2, oro=False), os.path.join(sub, "destacado-1024x500.png"))
        for archivo, fn in CAPTURAS:
            if filtro and filtro not in archivo:
                continue
            guardar(fn(idioma), os.path.join(sub, archivo))


if __name__ == "__main__":
    main(sys.argv)

# Capturas de los tutoriales y del manual

Las imágenes y animaciones de `docs/manual/<idioma>/tutoriales/img/` **no se hacen a mano**: las genera
`capturas.py` manejando el editor de verdad. Como Whisk3D cambia seguido, cuando se toca la interfaz se
vuelven a generar y listo, sin rehacer nada a mano.

La única excepción son los íconos de teclas (`img/tecla-*.svg`, por ejemplo la tecla verde de llamada
del Nokia): son dibujos fijos, hechos a mano, que van dentro del texto.

```bash
python3 tools/capturas/capturas.py                  # todas, en es/en/pt (~2 minutos)
python3 tools/capturas/capturas.py viewports        # solo las que empiezan con "viewports"
python3 tools/capturas/capturas.py --idiomas es     # solo en castellano
python3 tools/capturas/capturas.py --lista          # las recetas que hay
```

Necesita el build de Linux (`platform/linux/build/whisk3d`, o la ruta en `W3D_BIN`) y Pillow con WebP.
No abre ninguna ventana: usa `whisk3d --script` con el driver `offscreen` de SDL.

## Cómo funciona

1. Una **receta** (`recetas.py`) arma un guion del harness: `ventana`, `escala` y la entrada de una persona
   con los comandos de `main/test/W3dCapturas.cpp`:
   - `raton down|move|up x y [izq|medio|der]`, `dedo down|move|up x y [id]` (id 2: un segundo dedo),
     `rueda pasos x y`, `mod shift|alt|ctrl` (teclas mantenidas con el mouse; `mod` solo las suelta) y
     `tecla nombre` (nombre de SDL: `m`, `Right`, `Return`, `Escape`...; `_` = espacio: `Keypad_7`, `Left_Ctrl`).
   - Para preparar la escena sirven los comandos de siempre del harness (`add`, `objname`, `objpos`,
     `objscale`, `selobj`, `delete`, `mode edit`, `vporbita`...) con `g.linea("...")`, y `caras n n ...`
     elige varias caras en el Modo Edición.
   - Entran por `InputUsuarioSDL3`, la misma puerta que los eventos reales, y cada uno dibuja un cuadro
     después, como el loop del editor.
2. Las posiciones **no se escriben a mano**: `g.info()` corre el guion hasta ahí y devuelve lo que imprime
   `capinfo` (cada viewport, el botón de su esquina, las filas del menú abierto y cuál está marcada, el
   botón del menú View de la Vista 3D y el candado de la vista de cámara: `i.boton_vista`,
   `i.centro_candado()`). Las opciones del menú se
   buscan por **id**, no por texto, así la misma receta sirve en los tres idiomas.
3. `g.foto()` agrega un `uishot`. Al final se dibuja encima el dedo, el puntero, la tecla apretada o los
   resaltados, y se guarda un `.webp`: quieto (sin pérdida) o animado en bucle.

El idioma de la interfaz sale de `LANG`. El editor está en castellano, inglés y portugués; las páginas en
otros idiomas usan las capturas en inglés.

## Agregar una captura

```python
@receta("viewports-menu")
def menu(idioma):
    g = Guion(idioma)                    # 960x540, escala 2 (escala=1, ampliar=2: como en el N95)
    i = g.info()
    g.tocar(*i.esquina(VISTA_3D))        # toca el botón de la esquina de la Vista 3D
    g.foto()                             # quieta; con foto(dur=ms) varias veces, animada
    return g.cuadros()
```

Y en el `.md`: `![Texto que la describe](img/viewports-menu.webp)`.

- Una animación tiene que **cerrar el bucle**: el último cuadro tiene que empalmar con el primero, sin
  salto. En un gesto, hacer una vuelta sin fotos antes de la que se fotografía, porque el primer
  movimiento solo pasa el umbral.
- Si cambia toda la pantalla en cada cuadro, achicarla: `g.cuadros(ancho=720, calidad=62)`.
- Para mostrar solo un viewport: `g.cuadros(recorte=(x, y, w, h))` (las demos de navegación muestran solo
  la Vista 3D).
- El editor aplica el desplazamiento de cada movimiento del mouse en el **siguiente**: al terminar un
  arrastre, un movimiento más en el mismo lugar cierra el recorrido (si no, el bucle salta). Y orbitar
  depende del orden: para que la vista vuelva igual, volver por el mismo camino.
- Con el dedo, empezar el arrastre sobre el fondo: apoyado sobre el gizmo, lo agarra y mueve el objeto.

### El mouse dibujado

`Guion(idioma, raton=True)` dibuja un mouse abajo a la derecha con lo que se aprieta: el botón (en verde),
la rueda (verde si se aprieta o gira, con una flecha) y las teclas mantenidas (`g.mod("Shift")`):

```python
g = Guion(idioma, raton=True)
g.raton_abajo(x, y, "medio")    # el boton del medio, apretado
g.raton_mover(x + 40, y)        # arrastre
g.raton_arriba("medio")
g.rueda(1, x, y)                # la rueda para adelante
```
- Las que necesitan un comando nuevo del harness: agregarlo en `main/test/W3dCapturas.cpp`, no en
  `W3dScript.cpp`, que tarda minutos en compilar.

## Demos del Nokia N95

Hay dos fotos del N95, cada una un `.svg` de Inkscape con las teclas marcadas (`n95.py` las lee):

- `Guion(idioma, n95=True)`: **apaisado** (`n95.svg`), pantalla de 320x240, escala 1, con 2 viewports: la
  Vista 3D al 55 % y Propiedades al 45 %.
- `Guion(idioma, n95="vertical")`: **parado** (`n95-vertical.svg`), pantalla de 240x320 con la Vista 3D
  arriba y Propiedades abajo, y el **teclado numérico** a la vista (para el 0, el `*`, el `#`, el 8...).

```python
g = Guion(idioma, n95=True)
g.foto(dur=300, teclas=("verde",), tecla="Tecla verde")   # la tecla verde, resaltada en el telefono
g.verde()                                                 # un toque: pasa al viewport siguiente
g.verde_flecha(1, 0, 4)                                   # verde mantenida + flecha derecha, 4 cuadros
g.tecla("m")                                              # la soft izquierda (en la PC es la M)
```

Sobre la Vista 3D, el keypad del teléfono va con `g.keypad(...)`, que llama a las mismas funciones que el N95:

```python
g = Guion(idioma, n95="vertical")
g.keypad("orbitar", 1, 0, 3)       # flecha derecha mantenida, 3 cuadros
g.keypad("zoom", -1, 2)            # 0 + arriba (acercar)
g.keypad("panear", 0, 1, 3)        # * + abajo
g.keypad("vista", 1, 0)            # # + derecha (una vista por pulsacion)
g.keypad("perspectiva")            # # solo
g.keypad("encuadrar")              # 0 solo
g.keypad("camara")                 # 8
g.foto(teclas=("num0", "arriba"), tecla="0 + ↑")
```

- Las teclas se nombran con el id de su forma en el `.svg`: `pantalla`, `verde`, `soft-izq`, `soft-der`,
  `arriba`, `abajo`, `izquierda`, `derecha`, `ok`, `c`, `lapiz` y, en el parado, `num0`...`num9`,
  `asterisco`, `numeral`. Para sumar una, dibujarla en Inkscape (rectángulo, círculo o trazo) y ponerle un
  id; se respetan las transformaciones de los grupos. Si la foto trae fondo blanco, `n95.py` lo vuelve
  transparente.
- En cada cuadro conviene que cambie la pantalla **o** las teclas, no las dos: primero un cuadro corto con
  la tecla apretada y la pantalla igual, después la pantalla que reacciona. Pesa mucho menos.

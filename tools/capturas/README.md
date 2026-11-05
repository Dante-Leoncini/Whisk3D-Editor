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
   - `raton down|move|up x y [izq|medio|der]`, `dedo down|move|up x y`, `rueda pasos x y`, `tecla nombre`
     (nombre de SDL: `m`, `Right`, `Return`, `Escape`...).
   - Entran por `InputUsuarioSDL3`, la misma puerta que los eventos reales, y cada uno dibuja un cuadro
     después, como el loop del editor.
2. Las posiciones **no se escriben a mano**: `g.info()` corre el guion hasta ahí y devuelve lo que imprime
   `capinfo` (cada viewport, el botón de su esquina, las filas del menú abierto). Las opciones del menú se
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
- Las que necesitan un comando nuevo del harness: agregarlo en `main/test/W3dCapturas.cpp`, no en
  `W3dScript.cpp`, que tarda minutos en compilar.

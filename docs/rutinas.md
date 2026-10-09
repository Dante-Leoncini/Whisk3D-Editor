# Rutinas: el modo avanzado del render

Los objetos clásicos (malla, luz, niebla, culling...) siguen igual: son la forma fácil. Una **Rutina** hace lo
mismo paso a paso, como las llamadas de OpenGL, para el que quiere controlar cada llamada. El objetivo es gastar la
menor CPU y la menor cantidad de llamadas posible: nada se hace si no está en la lista.

- **Add > Core (advanced)** y el **Añadir** del panel: los pasos agrupados en submenús con su ícono (Transform,
  Draw, Texture, Fog, Lights, Alpha test, Color, Depth & clear, Blending, Control) y Face culling suelto.
- **No tiene posición, rotación ni escala**: no hay pestaña Objeto ni Constraints, ni gizmo; G/R/S no la mueven y
  el .w3d no guarda `pos/rot/escala`.
- **Scripts y animaciones**: un script de la rutina cambia sus pasos por índice (`setPaso`, ver "Desde Lua") y los
  pasos se animan con las animaciones de escena (I sobre un paso). Cada paso tiene una **identidad** (`"id"` en el
  .w3d) que no cambia al moverlo: sus curvas lo siguen; borrar el paso borra sus curvas.
- **Padres e hijos**: con el paso **Draw children** los hijos se dibujan ahí, con el estado que la rutina dejó
  puesto; sin ese paso, después de la lista.
- **El orden importa**: se ejecuta en su lugar del árbol, y sus pasos en el orden de la lista.

## Constructor y rutina

El outliner tiene dos partes separadas por una línea y un texto:

- **Arriba, el CONSTRUCTOR** (la rutina con la llave): prepara el estado. En el editor corre en cada cuadro (la
  interfaz de Whisk3D pisa el estado). En el **modo solo-juego** (VERDE+0 en el editor, o el juego compilado) corre
  **una vez**; después el motor saca una FOTO del estado cacheado y en cada cuadro solo repone lo que algo cambió
  (el HUD 2D, por ejemplo): solo llama al driver por esas diferencias. Medido en una prueba: 29 → 23 llamadas por
  cuadro, la imagen idéntica y `glaudit` sin desincronizaciones. (En GLES2/web no hay foto: corre en cada cuadro.)
  Lo que el cache no lleva (parámetros de niebla, de las luces, el clear color) queda puesto si nadie lo toca; las
  luces 1..7 se prenden en la rutina de cada cuadro.
- **Abajo, la RUTINA** de cada cuadro. Todo proyecto nuevo trae primero **Limpiar pantalla** ("Clear" de color y
  profundidad); el color de limpieza ("Clear color", el fondo del tema) lo pone el constructor una vez.

### Limpiar la pantalla es del proyecto

Como en GL, son dos pasos: **Clear color** (glClearColor, con el selector de color) y **Clear** (glClear) con una
casilla por buffer: color, profundidad, estencil (la máscara de glClear: limpiar color y profundidad juntos es UNA
llamada). Un proyecto con constructor limpia **solo** con sus pasos, también en el editor: sin un "Clear" no se
limpia nada (queda la estela del cuadro anterior); con "Clear" solo de profundidad, el color no se toca. Un "Clear"
en el medio de la lista borra lo dibujado antes.

- El editor no limpia el viewport: deja el **scissor** en su rectángulo mientras se dibuja la escena (un "Clear" del
  proyecto no borra el resto de la ventana) y dibuja la **grilla después** de la escena (contra su z). Si en un
  cuadro nadie limpió el color, limpia solo las franjas de sus barras (son translúcidas: si no, se encimaban).
- Los modos de análisis (**Z-Buffer, Alfa, Normales**) limpian con su fondo fijo: ahí "Clear color" no cambia nada y
  "Clear" no limpia el color (la profundidad sí).
- El "Clear" de profundidad limpia aunque la escritura de z esté apagada (glClear la respetaría): la deja prendida.
- El juego compilado y el modo juego puro (también en el N95) no limpian por su cuenta: un clear de más es un
  cuadro entero de relleno. Un proyecto viejo (sin constructor) lo sigue limpiando el viewport, y si lua apaga el
  pase 3D (`dibujar3D(false)`) también.

## La lista

Cada fila es un paso con su **ícono** (el de su grupo; "Call routine" a una rutina que solo pone estado lleva el de
material) y, a la derecha, lo que hace de un vistazo: una **casilla** en los on/off (clic: prende / apaga) o un
**cuadro con el color** en los de color fijo.

- **Arrastrar** con el mouse reordena (como el outliner, con la línea de destino).
- **G** (en el N95, **1**) agarra el paso elegido: arriba / abajo lo mueve, Enter confirma, Esc cancela.
- **X** o **Supr** lo borra; **I** le inserta un keyframe a sus números (o a su on/off) en el cuadro actual.

## Los pasos

Cada paso fija **un** estado, como OpenGL.

| grupo | pasos |
|---|---|
| Transform | Push / Pop matrix, Translate, Rotate, Scale, Object matrix (la matriz de mundo de un objeto: **Multiply**, o **Load** = cámara x objeto en UNA llamada, sin apilar) |
| Draw | **Array on/off**, **Vertex / Normal / UV / Color pointer**, **Draw elements**, Point size, Line width, Draw children, Call routine |
| Texture | Texture on/off, Bind texture (de una lista por carpetas), Texture mode (modular, reemplazar, **normal map DOT3**, **solo alfa**), Texture filter, Texture wrap, **UV offset** (la matriz de textura), **Matcap** |
| Fog | on/off, mode, start, end, density, color |
| Lights | Lighting on/off, Light on/off (GL_LIGHT0..7), Light color (ambiente/difuso/especular), Light position (puntual/direccional), Ambient light |
| Alpha test | on/off, reference |
| Color | Color, Vertex color (COLOR_MATERIAL), Material color (ambiente/difuso/especular/emisión), Shininess, **Smooth shading** |
| Depth & clear | Depth test, Depth write, Depth function, Depth bias (polygon offset), Depth shift (m), **Depth range**, **Clear color**, **Clear** (color / profundidad / estencil) |
| Blending | on/off, Blend mode |
| Control | Skip if zero, **Skip if hidden** (el objeto o un padre ocultos), **Visibility test**, **Call Lua** |

Los colores se eligen con el **selector de color** de Whisk3D (o, con "Números / memorias", con los 4 números). Cualquier
número puede ser fijo o `@nombre[i]`: una memoria de 256 floats que escribe lua (`setMemoria`, `setMemorias`, `memoria`).

### Un material, como pasos

Todo lo que hace un material se hace con pasos (y solo se paga lo que cambia):

- **Brillo**: Material color especular + Shininess. **Transparencia**: Blending on + Blend mode, o Alpha test.
- **Normales**: Array on/off de normales + su puntero; **Smooth shading** para el look suave o facetado.
- **Varias texturas**: una pasada por textura sobre la misma superficie (Bind texture, Blend mode, Depth function
  "Equal", Depth write off, Draw elements otra vez).
- **Normal map**: Texture mode "Normal map (DOT3)" con la textura de normales y la dirección de la luz horneada en
  los colores de vértice (sin CPU por cuadro), multiplicado sobre la base.
- **Reflejo**: **Matcap** (las normales de una malla como UV con la matriz del matcap: anda en el N95; va después de
  la matriz del objeto, y "off" la saca).
- **UV que se deslizan** (un reflejo, una cinta): **UV offset** con números en una memoria: Lua escribe 2 números en
  vez de reescribir y volver a subir las UV.
- **De fondo**: Depth range (1, 1) con Depth function "Always" y Depth write: queda detrás de todo y repone la
  profundidad (el cielo de Daytona, sin limpiar la pantalla).
- **Color de vértice**: va ANTES de los colores del material: prendido, GL copia el color en el ambiente y el difuso.

### Dibujar: arrays + punteros + elementos

Como en GL, son tres cosas aparte: **Array on/off** prende o apaga un array (glEnableClientState: vértices,
normales, UV o colores; se prende UNA vez y queda, no hay que tocarlo en cada dibujo), los **punteros** apuntan a
los arrays de una **malla 3D de la biblioteca** (el recurso, no un objeto) y **Draw elements** manda los índices de
**una parte** de una malla con la primitiva elegida.

- Los punteros pueden ser de mallas distintas: UV, colores o normales **compartidos** (menos memoria). Un puntero
  solo apunta (no prende nada); a una malla sin ese array no cambia nada. Apuntar otra vez al mismo array no llama
  al driver (cache de punteros): 63 instancias de una pieza ponen sus punteros una vez.
- El motor no toca los arrays: una rutina los encuentra como los dejó lo que se dibujó antes (el constructor, otra
  malla, los hijos).
- La malla y la parte se eligen de un desplegable por **carpetas → mallas → partes** (una malla de una sola parte se
  elige directo). La info va en filas: Triángulos, Vértices, Índices (con la primitiva elegida y como lista).
- **Primitivas**: puntos (los vértices de la parte), líneas (sus aristas), tira de líneas, lazo de líneas, triángulos,
  **tira de triángulos** (las tiras de la parte cosidas con degenerados en una sola llamada: 2 triángulos = 4 índices),
  abanico. Quads, tira de quads y polígono **no existen en GL ES 1.1**: se simulan convirtiéndolos a triángulos (el
  panel lo avisa: más índices, más lento). Los flujos derivados se arman una vez y quedan en un IBO
  (`libs/Whisk3DCore/objects/MallaFlujos.cpp`).
- La tira solo conviene si la malla **comparte vértices**: un quad suelto cuesta 4 + 2 degenerados = 6 índices, lo
  mismo que la lista. En Daytona (casi todo quads sueltos, cada uno con su UV y su color) la tira gana en 5 de 1965
  partes: el conversor elige la tira solo donde manda menos índices.
- Con la primitiva Triángulos: **rango manual** (primer / último triángulo de la parte, `max` = el último) o un
  **array** en una memoria (`cantidad, primero, último, ...`) que escribe quien decide qué se ve. Con **Banderas**
  (otra memoria, opcional) cada rango lleva la suya (`cantidad, primero, último, bandera, ...`) y se dibujan los de
  bandera prendida; los seguidos (uno empieza donde terminó el anterior) van en UNA llamada. Es lo que usa un mundo
  en una sola malla: un VBO para todo, una parte por material con los triángulos celda por celda, y las banderas las
  escriben los tests de visibilidad de las celdas.
- "Dibujar elementos" solo elige la **parte** (un desplegable: carpetas → mallas → partes) y la primitiva: con qué
  arrays se dibuja es estado de los pasos de puntero (dibujar 40 veces el mismo auto = los punteros una vez).

**Test de visibilidad**: la caja de una parte (movida por la matriz de un objeto, opcional) o, sin malla, 6 números
de una memoria (mínimo y máximo: una celda o una pieza de un mundo unido) contra el frustum de la cámara (los planos se
arman una vez por rutina; no lee matrices de GL) → 1/0 en una memoria, para "Skip if zero" o las banderas de un array.
Con **Distancia máxima** (fija o de una memoria; 0 = sin límite) también da 0 si la caja está más lejos que eso de la
cámara (al punto más cercano de la caja): lo chico y lejano no se manda. En un teléfono cada triángulo cuesta (el N95
transforma ~270 mil por segundo): probar la caja de cada pieza de un mundo, con una distancia para el nivel y otra más
corta para la decoración, manda menos de la mitad de triángulos que probar celdas grandes.

## Desde Lua

**Cambiar los pasos** desde el juego (el índice: 1 = el primero de la lista que usa la rutina):

```lua
setPaso(rutina, 4, "on", false)            -- prende / apaga
setPaso(rutina, 4, "n", 1, 0, 0, 1)        -- sus números ("color" es lo mismo; nil = ese queda)
setPaso(rutina, 4, "ref", "Cubo2")         -- otra malla / textura / objeto / rutina (error si no existe)
setPaso(rutina, 4, "modo", 2)              -- la opción ("entero": la parte, la luz...)
local r, g, b, a = leerPaso(rutina, 4, "n")
```

El paso **Call Lua** llama, mientras se dibuja, a una función del script de la rutina o de **otro objeto** (el que se
elige en el paso, "Script de"). Ahí adentro `paso(nombre, ...)` ejecuta cualquier paso. Los argumentos se reparten
por tipo:

```lua
function dibujar()
  paso("array", "normales", false)          -- (o "Normals"; vértices, uv, colores)
  paso("colorLimpieza", 0, 0, 0, 1)
  paso("limpiar", "color", "profundidad")   -- (o la máscara: 1 color, 2 profundidad, 4 estencil)
  paso("punteroVertices", "Cubo")
  paso("color", 0.2, 0.6, 1, 1)
  paso("apilar")
  paso("trasladar", 0, 2.2, 0)
  paso("dibujar", "Cubo", -1, "tira")       -- malla, parte, primitiva
  paso("desapilar")
end
```

Texto = la referencia y después la opción (`"tira"`, `"Diffuse"`...); `"@nombre[i]"` = una memoria; número = la parte
/ luz / cuántos saltear y después los valores; booleano = on/off. Fuera del dibujo `paso()` da error.

## Una lista por modo de render

La lista **Todo** (todos los modos) y, opcional, una por modo (Solid, Material, Rendered, Wireframe, Z-Buffer). En el
panel: **Modo de render** elige cuál se edita; **Usa** dice qué usa ese modo.

## Validación (solo el editor)

El núcleo no chequea nada. El **editor** revisa antes de dibujar (`main/edit/RutinaEditor.cpp`), también las
subrutinas: referencias vivas, partes y rangos, el array de la memoria, luces, opciones, memorias, Push/Pop
equilibrados y sin ciclos. Si falla, no se ejecuta y el outliner la pinta en rojo con el motivo.

**Los arrays contra el dibujo.** La selección de la malla (los punteros) va por un lado, prender los arrays por otro
y el dibujo (los índices de una parte) por otro. El editor sigue cada array a lo largo de la lista (a qué malla
apunta y si está prendido) y en cada "Dibujar elementos" marca error si: el array de vértices está apagado o no hay
puntero de vértices; los índices de la parte se pasan de los vértices del puntero de vértices; un array **prendido**
(normales, UV, color) no lo apuntó esta rutina, apunta a una malla sin ese array o a una con **menos vértices** que
la del puntero de vértices. Una rutina del árbol arranca con los arrays **como estaban** cuando empezó a dibujarse
en el último cuadro (lo que sabía el cache: los prendió el constructor, los dejó otra malla); la validación del
panel usa ese mismo estado. Después de "Dibujar hijos" no se sabe: hay que volver a apuntar y fijar con "Array
on/off" los que puedan estar prendidos. Una subrutina hereda y devuelve el estado. Una rutina que no se dibuja sola
(oculta: una librería, como las de material) se valida sin suponer nada y se vuelve a validar en el contexto de
quien la llama.

**Archivos viejos.** El .w3d guarda `"version": 2` en cada rutina. Una sin versión (la 1: los punteros prendían su
array y el "Clear" llevaba el color) se convierte al abrir: cada puntero prendido pasa a "Array on" + puntero (apagado
si la malla no tiene ese array), uno apagado a "Array off", y el "Clear" viejo a "Clear color" + "Clear"; los "Saltar
si" se recuentan.

## Perfil

`rutinaperfil on` / `ver` (harness): por tipo de paso, cuántas veces corrió y cuántas llamadas GL hizo.

## Medido: Daytona USA en modo avanzado

`make_daytona_w3d.py` arma con `m2avanzado.py` el mundo estático, el cielo, los rivales y el auto como rutinas, **sin
materiales**: cada parte lleva sus estados por clave; lo que todas las partes de una pasada ponen igual va una vez y
cada bloque pone solo lo que varía (arranca sin saber lo demás: los de antes se pudieron saltear). **Dos pasadas**
(todo lo opaco y después las calcomanías y lo translúcido). Lo que Lua prende y apaga pieza por pieza (el auto) queda
como vacíos con su nombre y la rutina los dibuja con "Skip if hidden" y la matriz cargada.

Carrera, modo juego puro, cuadro estable (simular + dibujar; `statsvivo`: TODAS las llamadas al driver de la escena,
cada `gl*` = 1, ver "El contador de llamadas" en `libs/Whisk3DCore/gfx/w3dGraphics.h`):

| | antes | ahora |
|---|---|---|
| llamadas GL de la escena | 2060 | 1463 |
| el cuadro entero (escena + HUD) | 2300 | 1711 |
| pasos de rutina ejecutados (mundo, cielo, rivales) | 6016 | 2582 |

`gltraza on` / `gltraza ver` (harness) cuenta cada llamada por su lugar en el código: lo que sigue pesando son los
binds de textura y de VBO y las mallas clásicas que quedan (las ruedas y los vidrios de los rivales cercanos, las
celdas translúcidas).

Después de la escena el motor repone el estado de material que la interfaz no pone (`gfx::EstadoBase`: alpha test,
sesgo, matriz y modo de textura, sombreado, luz): un alpha test que quedaba prendido borraba las filas tenues del
outliner, la barra del viewport y la interfaz de la cámara.

## Archivos

- `main/objects/Rutina.h/.cpp`: el objeto, los pasos, las memorias, el ejecutor (cache de punteros y de IBO), el
  constructor, `paso()` de lua y su .w3d.
- `libs/Whisk3DCore/objects/MallaFlujos.h/.cpp`: los flujos por parte (tiras, aristas, vértices, simuladas) y la caja.
- `main/edit/RutinaEditor.cpp`: la validación, los íconos y el menú agrupado (solo el editor).
- `main/ui/ViewPorts/PropsRutina.cpp`: la tarjeta del panel.
- `libs/Whisk3DCore/gfx/w3dGraphics.h`: `DrawElements` (primitivas), `LightN` / `LightNfv` / `LightModelAmbient`,
  `EstadoFotoTomar` / `EstadoFotoReponer`.

# Atajos de teclado

Todas las funciones de las tablas de abajo se pueden reasignar, y cada una puede tener **una o varias** combinaciones (o ninguna). Funciona en PC, Android (con teclado físico) y Web. En el N95 la ventana no puede capturar teclas: ahí el archivo `atajos.ini` se edita a mano.

## Cómo cambiar un atajo

1. Abrir **Propiedades → pestaña Archivo/Render → tarjeta Ajustes → Atajos de teclado…**
2. Elegir el **Contexto** (General, Vista 3D, Durante una transformación, Editor UV, Línea de tiempo, Outliner, Editor 2D, Propiedades, Editor de scripts). Las flechas izquierda/derecha también lo cambian.
3. Elegir la función en la lista y tocar **Agregar atajo** (o tocar otra vez la fila elegida). Aparece "Apretá una tecla…".
4. Apretar la combinación nueva (por ejemplo <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> cancela sin cambiar nada.

| Botón | Qué hace |
|---|---|
| Agregar atajo | Suma una combinación a la función. Si otra función del mismo contexto la tenía, se la saca y avisa. |
| Limpiar | Deja la función sin atajo. |
| Restablecer | Vuelve la función a su atajo de fábrica. |
| Restablecer todo | Todas las funciones vuelven a fábrica. |

Los menús muestran al lado de cada opción el atajo **configurado**: si reasignás una función, el menú muestra la combinación nueva. En el N95 los menús muestran la tecla del teléfono (por ejemplo <kbd>1</kbd> para Mover o <kbd>C</kbd> para Borrar); si hay un teclado Bluetooth conectado, muestran la tecla de ese teclado cuando la entiende.

Las combinaciones cambiadas se ven en verde. Cuando un atajo de fábrica se le saca a su función, esa tecla queda libre: ya no hace nada en ese contexto.

Los atajos de cada contexto valen cuando el puntero está sobre ese panel; los de **General** valen en todos. Los de **Durante una transformación** valen mientras se mueve, rota o escala algo, y tienen prioridad. En el editor de scripts, las letras siempre escriben: ahí solo se pueden reasignar combinaciones con <kbd>Ctrl</kbd>, <kbd>Alt</kbd> o teclas F.

## El archivo atajos.ini

Los cambios se guardan solos en `res/atajos.ini`, al lado de `config.ini`. Solo figuran las funciones cambiadas, y se puede editar a mano:

```ini
# funcion = combinacion, otra combinacion   (vacio = sin atajo)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Nombres de teclas: letras y números tal cual, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Modificadores: `Ctrl+`, `Shift+`, `Alt+`.

## Atajos de fábrica

<!-- Las tablas de abajo, hasta ATAJOS-FIN, las genera tools/genmanual_atajos.py desde el código: no editarlas a mano. -->

<!--ATAJOS-INICIO-->

### General (cualquier panel)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Guardar proyecto | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Deshacer | <kbd>Ctrl+Z</kbd> | <kbd>C+Izq</kbd> | `global.deshacer` |
| Rehacer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Der</kbd> | `global.rehacer` |
| Modo Objeto / Edición | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Activar/desactivar imán | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menú de imán (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Reproducir / Pausa | <kbd>Space</kbd> | <kbd>Verde+Soft izq</kbd> | `global.play` |
| Barra de menú | <kbd>M</kbd> | <kbd>Soft izq</kbd> | `global.barraMenu` |
| Padre | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Desemparentar | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Pantalla completa | <kbd>F11</kbd> | — | `global.pantalla` |
| Renderizar Imagen | <kbd>F12</kbd> | — | `global.render` |
| Renderizar Animación | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Vista 3D

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | <kbd>Lápiz+Arriba</kbd> | `3d.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | <kbd>Lápiz+Arriba</kbd> | `3d.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Selección por caja | <kbd>B</kbd> | — | `3d.caja` |
| Seleccionar Vinculado | <kbd>L</kbd> | — | `3d.vinculado` |
| Añadir | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Aplicar | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Rotar | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Limpiar posición (Pose) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Limpiar rotación (Pose) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Engordar / Limpiar escala | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Duplicar | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Ocultar / Mostrar | <kbd>H</kbd> | — | `3d.ocultar` |
| Unir | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Insertar Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insertar keyframe (todo) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Edición Proporcional | <kbd>O</kbd> | — | `3d.proporcional` |
| Superposiciones | <kbd>K</kbd> | — | `3d.overlays` |
| Vista local | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Encuadrar Selección | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Vista frontal | <kbd>Num1</kbd> | <kbd>#+Izq/Der</kbd> | `3d.vistaFrente` |
| Vista trasera | <kbd>Ctrl+Num1</kbd> | <kbd>#+Izq/Der</kbd> | `3d.vistaAtras` |
| Vista derecha | <kbd>Num3</kbd> | <kbd>#+Izq/Der</kbd> | `3d.vistaDerecha` |
| Vista izquierda | <kbd>Ctrl+Num3</kbd> | <kbd>#+Izq/Der</kbd> | `3d.vistaIzquierda` |
| Vista superior | <kbd>Num7</kbd> | <kbd>#+Arriba</kbd> | `3d.vistaArriba` |
| Vista inferior | <kbd>Ctrl+Num7</kbd> | <kbd>#+Abajo</kbd> | `3d.vistaAbajo` |
| Perspectiva / Ortográfica | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Girar vista a la izquierda | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Girar vista a la derecha | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Vista de cámara | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Hacer cámara activa | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Extruir | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Nueva cara desde vértices | <kbd>F</kbd> | — | `3d.cara` |
| Recalcular Normales | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Loop Cut y Deslizar | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Conectar camino de vértices | <kbd>J</kbd> | — | `3d.conectar` |
| Rasgar | <kbd>V</kbd> | — | `3d.rip` |
| Deslizar vértices | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separar | <kbd>P</kbd> | — | `3d.separar` |
| Fusionar | <kbd>M</kbd> | — | `3d.merge` |
| Menú contextual | <kbd>W</kbd> | — | `3d.menuContexto` |
| Triangular | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| Menú UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Durante una transformación (mover / rotar / escalar)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Confirmar | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Cancelar | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| Eje X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Eje Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Eje Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Excluir X (plano) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Excluir Y (plano) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Excluir Z (plano) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Radio proporcional + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Radio proporcional - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### Editor UV

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Seleccionar Vinculado | <kbd>L</kbd> | — | `uv.vinculado` |
| Selección por caja | <kbd>B</kbd> | — | `uv.caja` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Rotar | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Insertar Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insertar keyframe (todo) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Extruir | <kbd>E</kbd> | — | `uv.extruir` |
| Duplicar | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Encuadrar Selección | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Línea de tiempo

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Rotar manijas | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplicar | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Borrar | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolación | <kbd>T</kbd> | — | `tl.interp` |
| Tipo de manija | <kbd>V</kbd> | — | `tl.handle` |
| Encuadrar Selección | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Fotograma anterior | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Fotograma siguiente | <kbd>Right</kbd> | — | `tl.frameSig` |
| Keyframe anterior | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Keyframe siguiente | <kbd>Up</kbd> | — | `tl.keySig` |
| Ir al inicio | <kbd>Home</kbd> | — | `tl.inicio` |
| Ir al final | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Mover (reordenar) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Renombrar | <kbd>F2</kbd> | — | `out.renombrar` |
| Duplicar | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Ocultar / Mostrar | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Mostrar el activo | <kbd>Num.</kbd> | — | `out.encuadrar` |

### Editor 2D

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Rotar | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Duplicar | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Encuadrar Selección | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Vista de juego | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Propiedades (pasos de una rutina)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Agarrar paso | <kbd>G</kbd> | — | `props.agarrar` |
| Borrar paso | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insertar Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Editor de scripts

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Guardar script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Deshacer | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Pegar | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Seleccionar todo | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Rehacer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Buscar | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Reemplazar | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Teclas fijas (no reasignables)

Algunas teclas no están en la lista porque dependen de lo que se está haciendo: las flechas al navegar menús y paneles, <kbd>Enter</kbd> y <kbd>Esc</kbd> en menús y ventanas, las flechas del outliner mientras se mueve una fila, el teclado del IDE al escribir, y el teclado del teléfono en el N95 (ver [Navegación](navegacion.md)).

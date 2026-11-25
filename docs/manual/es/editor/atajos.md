# Atajos de teclado

Todas las funciones de las tablas de abajo se pueden reasignar, y cada una puede tener **una o varias** combinaciones (o ninguna). Funciona en PC, Android (con teclado físico) y Web. En el N95 la ventana no puede capturar teclas: ahí el archivo `atajos.ini` se edita a mano.

## Cómo cambiar un atajo

1. Abrir **Propiedades → pestaña Archivo/Render → tarjeta Ajustes → Atajos de teclado…**
2. Elegir la **Vista** (Todas las vistas, Vista 3D, Editor UV, Línea de tiempo, Outliner, Editor 2D, Propiedades, Editor de scripts) y, abajo, el **Contexto** dentro de esa vista: en la Vista 3D, cualquier modo, Modo Objeto, Modo Edición o durante una transformación; en el Editor UV, la línea de tiempo y el Editor 2D, general o durante una transformación. Los dos son desplegables. Las flechas izquierda/derecha cambian la vista.
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

La lista también tiene las **operaciones sin atajo de fábrica** (las de los menús: Origen a la geometría, Sombreado suave, Marcar costura, Mostrar manijas, Interpolación: lineal…): se les puede poner una combinación y hacen lo mismo que la opción del menú.

Los atajos de cada contexto valen cuando el puntero está sobre ese panel; los de **Todas las vistas** valen en todos. En la Vista 3D, los de Modo Objeto o Modo Edición valen solo en ese modo y le ganan a los de cualquier modo. Los de **Durante una transformación** valen mientras se mueve, rota o escala algo, y tienen prioridad. En el editor de scripts, las letras siempre escriben: ahí solo se pueden reasignar combinaciones con <kbd>Ctrl</kbd>, <kbd>Alt</kbd> o teclas F.

## El archivo shortcuts.ini

Los cambios se guardan solos en `res/shortcuts.ini`, al lado de `config.ini`. Solo figuran las funciones cambiadas, y se puede editar a mano:

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

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Guardar proyecto | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Deshacer | <kbd>Ctrl+Z</kbd> | <kbd>C+Izq</kbd> | `global.undo` |
| Rehacer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Der</kbd> | `global.redo` |
| Modo Objeto / Edición | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Activar/desactivar imán | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menú de imán (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Reproducir / Pausa | <kbd>Space</kbd> | <kbd>Verde+Soft izq</kbd> | `global.play` |
| Barra de menú | <kbd>M</kbd> | <kbd>Soft izq</kbd> | `global.menuBar` |
| Padre | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Desemparentar | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Pantalla completa | <kbd>F11</kbd> | — | `global.screen` |
| Renderizar Imagen | <kbd>F12</kbd> | — | `global.render` |
| Renderizar Animación | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Vista 3D (cualquier modo)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | <kbd>Lápiz+Arriba</kbd> | `3d.selectAll` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | <kbd>Lápiz+Arriba</kbd> | `3d.deselect` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Selección por caja | <kbd>B</kbd> | — | `3d.box` |
| Seleccionar Vinculado | <kbd>L</kbd> | — | `3d.linked` |
| Añadir | <kbd>Shift+A</kbd> | — | `3d.add` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Rotar | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Limpiar posición (Pose) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Limpiar rotación (Pose) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Engordar / Limpiar escala | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Duplicar | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Ocultar / Mostrar | <kbd>H</kbd> | — | `3d.hide` |
| Edición Proporcional | <kbd>O</kbd> | — | `3d.proportional` |
| Superposiciones | <kbd>K</kbd> | — | `3d.overlays` |
| Vista local | <kbd>/</kbd> | — | `3d.viewLocal` |
| Encuadrar Selección | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Vista frontal | <kbd>Num1</kbd> | <kbd>#+Izq/Der</kbd> | `3d.viewFront` |
| Vista trasera | <kbd>Ctrl+Num1</kbd> | <kbd>#+Izq/Der</kbd> | `3d.viewBack` |
| Vista derecha | <kbd>Num3</kbd> | <kbd>#+Izq/Der</kbd> | `3d.viewRight` |
| Vista izquierda | <kbd>Ctrl+Num3</kbd> | <kbd>#+Izq/Der</kbd> | `3d.viewLeft` |
| Vista superior | <kbd>Num7</kbd> | <kbd>#+Arriba</kbd> | `3d.viewTop` |
| Vista inferior | <kbd>Ctrl+Num7</kbd> | <kbd>#+Abajo</kbd> | `3d.viewBottom` |
| Perspectiva / Ortográfica | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Siguiente modo de vista | <kbd>Z</kbd> | <kbd>6</kbd> | `3d.viewMode` |
| Girar vista a la izquierda | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Girar vista a la derecha | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Vista de cámara | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Hacer cámara activa | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Menú contextual | <kbd>W</kbd> | — | `3d.contextMenu` |
| Bloquear Órbita | — | — | `3d.lockOrbit` |
| Mover la cámara con la vista | — | — | `3d.lockCamera` |
| Vista Render | — | — | `3d.showRender` |
| Vista Material | — | — | `3d.showMaterial` |
| Vista Sólido | — | — | `3d.showSolid` |
| Vista Alámbrica | — | — | `3d.showWire` |

### Vista 3D: Modo Objeto

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Aplicar | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Unir | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Insertar Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insertar keyframe (todo) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `3d.cut` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `3d.copy` |
| Pegar | <kbd>Ctrl+V</kbd> | — | `3d.paste` |
| Geometría al Origen | — | — | `3d.geoToOrigin` |
| Origen a Geometría | — | — | `3d.originToGeo` |
| Origen al Cursor 3D | — | — | `3d.originToCursor` |
| Aplicar posición | — | — | `3d.applyPos` |
| Aplicar rotación | — | — | `3d.applyRot` |
| Aplicar escala | — | — | `3d.applyScale` |
| Aplicar todas las transformaciones | — | — | `3d.applyAll` |
| Insertar keyframe de posición | — | — | `3d.keyPos` |
| Insertar keyframe de rotación | — | — | `3d.keyRot` |
| Insertar keyframe de escala | — | — | `3d.keyEsc` |
| Crear prefab | — | — | `3d.createPrefab` |
| Desempaquetar prefab | — | — | `3d.unpackPrefab` |

### Vista 3D: Modo Edición

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Extruir | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| Nueva cara desde vértices | <kbd>F</kbd> | — | `3d.face` |
| Recalcular Normales | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Loop Cut y Deslizar | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Conectar camino de vértices | <kbd>J</kbd> | — | `3d.connect` |
| Rasgar | <kbd>V</kbd> | — | `3d.rip` |
| Deslizar vértices | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separar | <kbd>P</kbd> | — | `3d.separate` |
| Fusionar | <kbd>M</kbd> | — | `3d.merge` |
| Triangular | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| Menú UV | <kbd>U</kbd> | — | `3d.menuUV` |
| Insertar caras | <kbd>I</kbd> | — | `3d.inset` |
| Biselar aristas | <kbd>Ctrl+B</kbd> | — | `3d.bevel` |
| Biselar vértices | <kbd>Ctrl+Shift+B</kbd> | — | `3d.bevelVertices` |
| Sombrear Suave | — | — | `3d.smooth` |
| Sombrear Plano | — | — | `3d.plane` |
| Invertir Normales | — | — | `3d.flipNormals` |
| Marcar Afilado | — | — | `3d.markSharp` |
| Limpiar Afilado | — | — | `3d.clearSharp` |
| Marcar Costura | — | — | `3d.markSeam` |
| Limpiar Costura | — | — | `3d.clearSeam` |
| Fusionar en el centro | — | — | `3d.joinCenter` |
| Fusionar en el cursor | — | — | `3d.joinCursor` |
| Colapsar | — | — | `3d.collapse` |
| Fusionar por distancia | — | — | `3d.joinDistance` |
| Borrar vértices | — | — | `3d.deleteVerts` |
| Borrar aristas | — | — | `3d.deleteEdges` |
| Borrar caras | — | — | `3d.deleteFaces` |
| Borrar loops de aristas | — | — | `3d.deleteLoops` |
| Desplegar Basado en Ángulos | — | — | `3d.unwrap` |
| Proyección UV Inteligente | — | — | `3d.smartUV` |
| Proyectar desde Vista | — | — | `3d.projectFromView` |

### Durante una transformación (mover / rotar / escalar)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Confirmar | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Cancelar | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| Eje X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Eje Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Eje Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Excluir X (plano) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Excluir Y (plano) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Excluir Z (plano) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Radio proporcional + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Radio proporcional - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### Editor UV

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `uv.selectAll` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Seleccionar Vinculado | <kbd>L</kbd> | — | `uv.linked` |
| Selección por caja | <kbd>B</kbd> | — | `uv.box` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Rotar | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Insertar Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insertar keyframe (todo) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Extruir | <kbd>E</kbd> | — | `uv.extrude` |
| Duplicar | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Encuadrar Selección | <kbd>Num.</kbd> | — | `uv.focus` |
| Encuadrar Todo | — | — | `uv.focusAll` |
| Cursor a Selección | — | — | `uv.cursorASel` |
| Selección a Cursor | — | — | `uv.selACursor` |
| Cursor a Centro | — | — | `uv.cursorCenter` |

### Línea de tiempo

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `tl.selectAll` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Rotar manijas | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Duplicar | <kbd>Ctrl+D</kbd>, <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `tl.cut` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `tl.copy` |
| Pegar | <kbd>Ctrl+V</kbd> | — | `tl.paste` |
| Borrar | <kbd>X</kbd> | — | `tl.delete` |
| Interpolación | <kbd>E</kbd>, <kbd>T</kbd> | — | `tl.interp` |
| Suavizado | <kbd>Ctrl+E</kbd> | — | `tl.easing` |
| Tipo de manija | <kbd>V</kbd> | — | `tl.handle` |
| Encuadrar Selección | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Fotograma anterior | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Fotograma siguiente | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Keyframe anterior | <kbd>Down</kbd> | — | `tl.prevKey` |
| Keyframe siguiente | <kbd>Up</kbd> | — | `tl.nextKey` |
| Ir al inicio | <kbd>Home</kbd> | — | `tl.start` |
| Ir al final | <kbd>End</kbd> | — | `tl.end` |
| Hoja de claves | — | — | `tl.showDope` |
| Curvas | — | — | `tl.showCurves` |
| Mostrar panel | — | — | `tl.panel` |
| Mostrar manijas | — | — | `tl.showHandles` |
| Euler Inteligente | — | — | `tl.smartEuler` |
| Interpolación: constante | — | — | `tl.constantInterp` |
| Interpolación: lineal | — | — | `tl.linearInterp` |
| Interpolación: Bezier | — | — | `tl.interpBezier` |
| Manijas: libres | — | — | `tl.freeHandle` |
| Manijas: alineadas | — | — | `tl.alignedHandle` |
| Manijas: vector | — | — | `tl.handleVector` |
| Manijas: automáticas | — | — | `tl.handleAuto` |
| Manijas: auto limitadas | — | — | `tl.autoClampedHandle` |
| Manijas: planas | — | — | `tl.flatHandle` |

### Outliner

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `out.selectAll` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Mover (reordenar) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Renombrar | <kbd>F2</kbd> | — | `out.rename` |
| Duplicar | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Duplicar Vinculado | <kbd>Alt+D</kbd> | — | `out.instance` |
| Ocultar / Mostrar | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Mostrar el activo | <kbd>Num.</kbd> | — | `out.focus` |

### Editor 2D

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `2d.selectAll` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Rotar | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Duplicar | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Borrar | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Encuadrar Selección | <kbd>Num.</kbd> | — | `2d.focus` |
| Vista de juego | <kbd>Num0</kbd> | — | `2d.gameView` |
| Emparentar | — | — | `2d.parent` |
| Desemparentar | — | — | `2d.clearParent` |
| Zoom 1:1 | — | — | `2d.zoom1` |
| Zoom 2:1 | — | — | `2d.zoom2` |

### Propiedades (pasos de una rutina)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Agarrar paso | <kbd>G</kbd> | — | `props.grab` |
| Borrar paso | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Insertar Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Editor de scripts

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en shortcuts.ini |
|---|---|---|---|
| Guardar script | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Deshacer | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Pegar | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Seleccionar todo | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Rehacer | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Buscar | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Reemplazar | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## Teclas fijas (no reasignables)

Algunas teclas no están en la lista porque dependen de lo que se está haciendo: las flechas al navegar menús y paneles, <kbd>Enter</kbd> y <kbd>Esc</kbd> en menús y ventanas, las flechas del outliner mientras se mueve una fila, el teclado del IDE al escribir, y el teclado del teléfono en el N95 (ver [Navegación](navegacion.md)).

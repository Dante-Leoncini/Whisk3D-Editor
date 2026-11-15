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

### Vista 3D (cualquier modo)

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | <kbd>Lápiz+Arriba</kbd> | `3d.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | <kbd>Lápiz+Arriba</kbd> | `3d.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Selección por caja | <kbd>B</kbd> | — | `3d.caja` |
| Seleccionar Vinculado | <kbd>L</kbd> | — | `3d.vinculado` |
| Añadir | <kbd>Shift+A</kbd> | — | `3d.agregar` |
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
| Siguiente modo de vista | <kbd>Z</kbd> | <kbd>6</kbd> | `3d.modoVista` |
| Girar vista a la izquierda | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Girar vista a la derecha | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Vista de cámara | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Hacer cámara activa | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Menú contextual | <kbd>W</kbd> | — | `3d.menuContexto` |
| Bloquear Órbita | — | — | `3d.bloquearOrbita` |
| Mover la cámara con la vista | — | — | `3d.bloquearCamara` |
| Vista Render | — | — | `3d.verRender` |
| Vista Material | — | — | `3d.verMaterial` |
| Vista Sólido | — | — | `3d.verSolido` |
| Vista Alámbrica | — | — | `3d.verAlambre` |

### Vista 3D: Modo Objeto

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Aplicar | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Unir | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Insertar Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insertar keyframe (todo) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `3d.cortar` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `3d.copiar` |
| Pegar | <kbd>Ctrl+V</kbd> | — | `3d.pegar` |
| Geometría al Origen | — | — | `3d.geoAOrigen` |
| Origen a Geometría | — | — | `3d.origenAGeo` |
| Origen al Cursor 3D | — | — | `3d.origenACursor` |
| Aplicar posición | — | — | `3d.aplicarPos` |
| Aplicar rotación | — | — | `3d.aplicarRot` |
| Aplicar escala | — | — | `3d.aplicarEsc` |
| Aplicar todas las transformaciones | — | — | `3d.aplicarTodo` |
| Insertar keyframe de posición | — | — | `3d.keyPos` |
| Insertar keyframe de rotación | — | — | `3d.keyRot` |
| Insertar keyframe de escala | — | — | `3d.keyEsc` |
| Crear prefab | — | — | `3d.crearPrefab` |
| Desempaquetar prefab | — | — | `3d.desarmarPrefab` |

### Vista 3D: Modo Edición

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Extruir | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Nueva cara desde vértices | <kbd>F</kbd> | — | `3d.cara` |
| Recalcular Normales | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Loop Cut y Deslizar | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Conectar camino de vértices | <kbd>J</kbd> | — | `3d.conectar` |
| Rasgar | <kbd>V</kbd> | — | `3d.rip` |
| Deslizar vértices | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separar | <kbd>P</kbd> | — | `3d.separar` |
| Fusionar | <kbd>M</kbd> | — | `3d.merge` |
| Triangular | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| Menú UV | <kbd>U</kbd> | — | `3d.menuUV` |
| Insertar caras | <kbd>I</kbd> | — | `3d.inset` |
| Biselar aristas | <kbd>Ctrl+B</kbd> | — | `3d.bisel` |
| Biselar vértices | <kbd>Ctrl+Shift+B</kbd> | — | `3d.biselVert` |
| Sombrear Suave | — | — | `3d.suave` |
| Sombrear Plano | — | — | `3d.plano` |
| Invertir Normales | — | — | `3d.voltearNormales` |
| Marcar Afilado | — | — | `3d.marcarAfilado` |
| Limpiar Afilado | — | — | `3d.limpiarAfilado` |
| Marcar Costura | — | — | `3d.marcarCostura` |
| Limpiar Costura | — | — | `3d.limpiarCostura` |
| Fusionar en el centro | — | — | `3d.unirCentro` |
| Fusionar en el cursor | — | — | `3d.unirCursor` |
| Colapsar | — | — | `3d.colapsar` |
| Fusionar por distancia | — | — | `3d.unirDistancia` |
| Borrar vértices | — | — | `3d.borrarVerts` |
| Borrar aristas | — | — | `3d.borrarAristas` |
| Borrar caras | — | — | `3d.borrarCaras` |
| Borrar loops de aristas | — | — | `3d.borrarLoops` |
| Desplegar Basado en Ángulos | — | — | `3d.unwrap` |
| Proyección UV Inteligente | — | — | `3d.smartUV` |
| Proyectar desde Vista | — | — | `3d.proyVista` |

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
| Encuadrar Todo | — | — | `uv.encuadrarTodo` |
| Cursor a Selección | — | — | `uv.cursorASel` |
| Selección a Cursor | — | — | `uv.selACursor` |
| Cursor a Centro | — | — | `uv.cursorCentro` |

### Línea de tiempo

| Función | Atajo de fábrica | Nokia N95 (teclado del teléfono) | Nombre en atajos.ini |
|---|---|---|---|
| Seleccionar todo | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Deseleccionar todo | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Invertir selección | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Mover | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Escalar | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Rotar manijas | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplicar | <kbd>Ctrl+D</kbd>, <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Cortar | <kbd>Ctrl+X</kbd> | — | `tl.cortar` |
| Copiar | <kbd>Ctrl+C</kbd> | — | `tl.copiar` |
| Pegar | <kbd>Ctrl+V</kbd> | — | `tl.pegar` |
| Borrar | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolación | <kbd>E</kbd>, <kbd>T</kbd> | — | `tl.interp` |
| Suavizado | <kbd>Ctrl+E</kbd> | — | `tl.easing` |
| Tipo de manija | <kbd>V</kbd> | — | `tl.handle` |
| Encuadrar Selección | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Fotograma anterior | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Fotograma siguiente | <kbd>Right</kbd> | — | `tl.frameSig` |
| Keyframe anterior | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Keyframe siguiente | <kbd>Up</kbd> | — | `tl.keySig` |
| Ir al inicio | <kbd>Home</kbd> | — | `tl.inicio` |
| Ir al final | <kbd>End</kbd> | — | `tl.fin` |
| Hoja de claves | — | — | `tl.verDope` |
| Curvas | — | — | `tl.verCurvas` |
| Mostrar panel | — | — | `tl.panel` |
| Mostrar manijas | — | — | `tl.verHandles` |
| Euler Inteligente | — | — | `tl.smartEuler` |
| Interpolación: constante | — | — | `tl.interpConstante` |
| Interpolación: lineal | — | — | `tl.interpLineal` |
| Interpolación: Bezier | — | — | `tl.interpBezier` |
| Manijas: libres | — | — | `tl.handleLibre` |
| Manijas: alineadas | — | — | `tl.handleAlineado` |
| Manijas: vector | — | — | `tl.handleVector` |
| Manijas: automáticas | — | — | `tl.handleAuto` |
| Manijas: auto limitadas | — | — | `tl.handleAutoLimite` |
| Manijas: planas | — | — | `tl.handlePlano` |

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
| Emparentar | — | — | `2d.emparentar` |
| Desemparentar | — | — | `2d.desemparentar` |
| Zoom 1:1 | — | — | `2d.zoom1` |
| Zoom 2:1 | — | — | `2d.zoom2` |

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

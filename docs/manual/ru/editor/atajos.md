# Горячие клавиши

Все функции из таблиц ниже можно переназначить, и у каждой может быть **одно или несколько** сочетаний (или ни одного). Это работает на ПК, Android (с физической клавиатурой) и Web. На N95 окно не может перехватывать клавиши: там файл `atajos.ini` редактируется вручную.

## Как изменить сочетание клавиш

1. Открыть **Properties → вкладка File/Render → карточка Settings → Keyboard Shortcuts…**
2. Выбрать **Context** (General, 3D Viewport, During a transform, UV Editor, Timeline, Outliner, 2D Editor, Properties, Script Editor). Стрелки влево/вправо тоже его переключают.
3. Выбрать функцию в списке и нажать **Add shortcut** (или ещё раз коснуться выбранной строки). Появится "Press a key…".
4. Нажать новое сочетание (например, <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> отменяет, ничего не меняя.

| Кнопка | Что делает |
|---|---|
| Add shortcut | Добавляет сочетание к функции. Если оно было у другой функции того же контекста, оно у неё снимается, и выводится предупреждение. |
| Clear | Оставляет функцию без сочетания. |
| Reset | Возвращает функции её сочетание по умолчанию. |
| Reset all | Все функции возвращаются к сочетаниям по умолчанию. |

Меню показывают рядом с каждым пунктом **настроенное** сочетание: если ты переназначишь функцию, меню покажет новое сочетание. На N95 меню показывают клавишу телефона (например, <kbd>1</kbd> для Move или <kbd>C</kbd> для Delete); если подключена Bluetooth-клавиатура, они показывают клавишу этой клавиатуры, когда могут её распознать.

Изменённые сочетания выделяются зелёным. Если у функции убрать сочетание по умолчанию, эта клавиша освобождается: в этом контексте она больше ничего не делает.

Сочетания каждого контекста действуют, когда указатель находится над этой панелью; сочетания **General** действуют везде. Сочетания **During a transform** действуют, пока что-то перемещается, вращается или масштабируется, и имеют приоритет. В Script Editor буквы всегда вводят текст: там можно переназначать только сочетания с <kbd>Ctrl</kbd>, <kbd>Alt</kbd> или F-клавишами.

## Файл atajos.ini

Изменения сохраняются автоматически в `res/atajos.ini`, рядом с `config.ini`. В нём есть только изменённые функции, и его можно редактировать вручную:

```ini
# функция = сочетание, другое сочетание   (пусто = без сочетания)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Имена клавиш: буквы и цифры как есть, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Модификаторы: `Ctrl+`, `Shift+`, `Alt+`.

## Сочетания по умолчанию

<!-- Таблицы ниже, до ATAJOS-FIN, генерирует tools/genmanual_atajos.py из кода: не редактируй их вручную. -->

<!--ATAJOS-INICIO-->

### General (любая панель)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Save project | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Undo | <kbd>Ctrl+Z</kbd> | <kbd>C+Left</kbd> | `global.deshacer` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Right</kbd> | `global.rehacer` |
| Object / Edit Mode | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Toggle Snap | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Snap menu | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Play / Pause | <kbd>Space</kbd> | <kbd>Green+Left soft</kbd> | `global.play` |
| Menu bar | <kbd>M</kbd> | <kbd>Left soft</kbd> | `global.barraMenu` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Clear Parent | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Fullscreen | <kbd>F11</kbd> | — | `global.pantalla` |
| Render Image | <kbd>F12</kbd> | — | `global.render` |
| Render Animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Viewport

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | <kbd>Pencil+Up</kbd> | `3d.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | <kbd>Pencil+Up</kbd> | `3d.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Box Select | <kbd>B</kbd> | — | `3d.caja` |
| Select Linked | <kbd>L</kbd> | — | `3d.vinculado` |
| Add | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Apply | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Clear Location (Pose) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Clear Rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Shrink/Fatten / Clear Scale | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Duplicate | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Hide / Show | <kbd>H</kbd> | — | `3d.ocultar` |
| Join | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Insert Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Proportional Editing | <kbd>O</kbd> | — | `3d.proporcional` |
| Overlays | <kbd>K</kbd> | — | `3d.overlays` |
| Local View | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Front View | <kbd>Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaFrente` |
| Back View | <kbd>Ctrl+Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaAtras` |
| Right View | <kbd>Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaDerecha` |
| Left View | <kbd>Ctrl+Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaIzquierda` |
| Top View | <kbd>Num7</kbd> | <kbd>#+Up</kbd> | `3d.vistaArriba` |
| Bottom View | <kbd>Ctrl+Num7</kbd> | <kbd>#+Down</kbd> | `3d.vistaAbajo` |
| Perspective / Orthographic | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Roll Left | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Roll Right | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Camera View | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Set Active Camera | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Extrude | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| New Face from Vertices | <kbd>F</kbd> | — | `3d.cara` |
| Recalculate Normals | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Loop Cut and Slide | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connect Vertex Path | <kbd>J</kbd> | — | `3d.conectar` |
| Rip | <kbd>V</kbd> | — | `3d.rip` |
| Vertex Slide | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separate | <kbd>P</kbd> | — | `3d.separar` |
| Merge | <kbd>M</kbd> | — | `3d.merge` |
| Context menu | <kbd>W</kbd> | — | `3d.menuContexto` |
| Triangulate | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| UV menu | <kbd>U</kbd> | — | `3d.menuUV` |

### During a transform (перемещение / вращение / масштабирование)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Confirm | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Cancel | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| X axis | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Y axis | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Z axis | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Exclude X (plane) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Exclude Y (plane) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Exclude Z (plane) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Proportional radius + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Proportional radius - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### UV Editor

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Select Linked | <kbd>L</kbd> | — | `uv.vinculado` |
| Box Select | <kbd>B</kbd> | — | `uv.caja` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Insert Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Extrude | <kbd>E</kbd> | — | `uv.extruir` |
| Duplicate | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Frame Selected | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Timeline

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Rotate handles | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Delete | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Handle type | <kbd>V</kbd> | — | `tl.handle` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Previous frame | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Next frame | <kbd>Right</kbd> | — | `tl.frameSig` |
| Previous keyframe | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Next keyframe | <kbd>Up</kbd> | — | `tl.keySig` |
| Go to start | <kbd>Home</kbd> | — | `tl.inicio` |
| Go to end | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Move (reorder) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Rename | <kbd>F2</kbd> | — | `out.renombrar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Hide / Show | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Show active | <kbd>Num.</kbd> | — | `out.encuadrar` |

### 2D Editor

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Frame Selected | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Game View | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Properties (шаги процедуры)

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.agarrar` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| Функция | Сочетание по умолчанию | Nokia N95 (клавиатура телефона) | Имя в atajos.ini |
|---|---|---|---|
| Save script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Undo | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copy | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Cut | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Paste | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Select All | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Find | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Replace | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Фиксированные клавиши (не переназначаются)

Некоторых клавиш нет в списке, потому что их действие зависит от того, что ты делаешь: стрелки при навигации по меню и панелям, <kbd>Enter</kbd> и <kbd>Esc</kbd> в меню и окнах, стрелки в Outliner во время перемещения строки, клавиатура в IDE при наборе текста и клавиатура телефона на N95 (см. [Навигация](navegacion.md)).

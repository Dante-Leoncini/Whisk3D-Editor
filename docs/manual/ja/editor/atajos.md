# キーボードショートカット

下の表にあるすべての機能は割り当てを変更でき、それぞれに**1つまたは複数**の組み合わせを設定できます（何も設定しないこともできます）。PC、Android（物理キーボード使用時）、Webで使えます。N95ではウィンドウがキー入力を取得できないため、`atajos.ini`ファイルを手で編集します。

## ショートカットの変更方法

1. **Properties → File/Renderタブ → Settingsカード → Keyboard Shortcuts…**を開きます。
2. **Context**（General、3D Viewport、During a transform、UV Editor、Timeline、Outliner、2D Editor、Properties、Script Editor）を選びます。左右の方向キーでも切り替えられます。
3. リストで機能を選び、**Add shortcut**をタップします（または、選択中の行をもう一度タップします）。「Press a key…」と表示されます。
4. 新しい組み合わせを押します（たとえば<kbd>Ctrl+M</kbd>）。<kbd>Esc</kbd>を押すと、何も変更せずにキャンセルします。

| ボタン | 動作 |
|---|---|
| Add shortcut | 機能に組み合わせを追加します。同じコンテキストの別の機能がその組み合わせを使っていた場合は、そちらから外して通知します。 |
| Clear | 機能のショートカットをなくします。 |
| Reset | 機能を初期設定のショートカットに戻します。 |
| Reset all | すべての機能を初期設定に戻します。 |

メニューでは、各項目の横に**設定済みの**ショートカットが表示されます。機能の割り当てを変更すると、メニューには新しい組み合わせが表示されます。N95では、メニューに電話のキーが表示されます（たとえば、Moveなら<kbd>1</kbd>、Deleteなら<kbd>C</kbd>）。Bluetoothキーボードが接続されている場合は、認識できるキーであれば、そのキーボードのキーが表示されます。

変更した組み合わせは緑で表示されます。初期設定のショートカットを機能から外すと、そのキーは空きになり、そのコンテキストでは何もしなくなります。

各コンテキストのショートカットは、ポインターがそのパネルの上にあるときに有効です。**General**のショートカットはどこでも有効です。**During a transform**のショートカットは、何かを移動、回転、拡大縮小している間に有効で、ほかより優先されます。Script Editorでは文字キーは常に入力に使われるため、割り当てを変更できるのは<kbd>Ctrl</kbd>、<kbd>Alt</kbd>、またはFキーを使った組み合わせだけです。

## atajos.iniファイル

変更内容は、`config.ini`と同じ場所にある`res/atajos.ini`に自動で保存されます。変更した機能だけが記録され、手で編集することもできます：

```ini
# 機能 = 組み合わせ, 別の組み合わせ   (空 = ショートカットなし)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

キーの名前：文字と数字はそのまま、`Enter`、`Esc`、`Tab`、`Space`、`Backspace`、`Delete`、`Home`、`End`、`PageUp`、`PageDown`、`Left`、`Right`、`Up`、`Down`、`F1`…`F12`、`Num0`…`Num9`、`Num.`、`NumEnter`、`Comma`、`Plus`。修飾キー：`Ctrl+`、`Shift+`、`Alt+`。

## 初期設定のショートカット

<!-- 下の表（ATAJOS-FINまで）は、tools/genmanual_atajos.pyがコードから生成しています。手で編集しないでください。 -->

<!--ATAJOS-INICIO-->

### General（すべてのパネル）

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

### During a transform（移動 / 回転 / 拡大縮小）

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

### Properties（ルーチンのステップ）

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.agarrar` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
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

## 固定のキー（割り当て変更不可）

いくつかのキーは、そのときの操作によって働きが変わるため、リストに含まれていません：メニューやパネルを移動するときの方向キー、メニューやウィンドウでの<kbd>Enter</kbd>と<kbd>Esc</kbd>、行を移動している間のOutlinerの方向キー、入力中のIDEのキーボード、そしてN95の電話のキーパッド（[ナビゲーション](navegacion.md)を参照）。

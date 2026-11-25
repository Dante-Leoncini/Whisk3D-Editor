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

変更内容は、`config.ini`と同じ場所にある`res/shortcuts.ini`に自動で保存されます。変更した機能だけが記録され、手で編集することもできます：

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
| Save project | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Undo | <kbd>Ctrl+Z</kbd> | <kbd>C+Left</kbd> | `global.undo` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Right</kbd> | `global.redo` |
| Object / Edit Mode | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Toggle Snap | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Snap menu | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Play / Pause | <kbd>Space</kbd> | <kbd>Green+Left soft</kbd> | `global.play` |
| Menu bar | <kbd>M</kbd> | <kbd>Left soft</kbd> | `global.menuBar` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Clear Parent | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Fullscreen | <kbd>F11</kbd> | — | `global.screen` |
| Render Image | <kbd>F12</kbd> | — | `global.render` |
| Render Animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Viewport

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Select All | <kbd>A</kbd> | <kbd>Pencil+Up</kbd> | `3d.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | <kbd>Pencil+Up</kbd> | `3d.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Box Select | <kbd>B</kbd> | — | `3d.box` |
| Select Linked | <kbd>L</kbd> | — | `3d.linked` |
| Add | <kbd>Shift+A</kbd> | — | `3d.add` |
| Apply | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Clear Location (Pose) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Clear Rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Shrink/Fatten / Clear Scale | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Duplicate | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Hide / Show | <kbd>H</kbd> | — | `3d.hide` |
| Join | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Insert Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Proportional Editing | <kbd>O</kbd> | — | `3d.proportional` |
| Overlays | <kbd>K</kbd> | — | `3d.overlays` |
| Local View | <kbd>/</kbd> | — | `3d.viewLocal` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Front View | <kbd>Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewFront` |
| Back View | <kbd>Ctrl+Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewBack` |
| Right View | <kbd>Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewRight` |
| Left View | <kbd>Ctrl+Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.viewLeft` |
| Top View | <kbd>Num7</kbd> | <kbd>#+Up</kbd> | `3d.viewTop` |
| Bottom View | <kbd>Ctrl+Num7</kbd> | <kbd>#+Down</kbd> | `3d.viewBottom` |
| Perspective / Orthographic | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Roll Left | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Roll Right | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Camera View | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Set Active Camera | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Extrude | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| New Face from Vertices | <kbd>F</kbd> | — | `3d.face` |
| Recalculate Normals | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Loop Cut and Slide | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connect Vertex Path | <kbd>J</kbd> | — | `3d.connect` |
| Rip | <kbd>V</kbd> | — | `3d.rip` |
| Vertex Slide | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separate | <kbd>P</kbd> | — | `3d.separate` |
| Merge | <kbd>M</kbd> | — | `3d.merge` |
| Context menu | <kbd>W</kbd> | — | `3d.contextMenu` |
| Triangulate | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| UV menu | <kbd>U</kbd> | — | `3d.menuUV` |

### During a transform（移動 / 回転 / 拡大縮小）

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Confirm | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Cancel | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| X axis | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Y axis | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Z axis | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Exclude X (plane) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Exclude Y (plane) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Exclude Z (plane) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Proportional radius + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Proportional radius - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### UV Editor

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `uv.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Select Linked | <kbd>L</kbd> | — | `uv.linked` |
| Box Select | <kbd>B</kbd> | — | `uv.box` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Insert Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Extrude | <kbd>E</kbd> | — | `uv.extrude` |
| Duplicate | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Frame Selected | <kbd>Num.</kbd> | — | `uv.focus` |

### Timeline

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `tl.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Rotate handles | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Duplicate | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Delete | <kbd>X</kbd> | — | `tl.delete` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Handle type | <kbd>V</kbd> | — | `tl.handle` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Previous frame | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Next frame | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Previous keyframe | <kbd>Down</kbd> | — | `tl.prevKey` |
| Next keyframe | <kbd>Up</kbd> | — | `tl.nextKey` |
| Go to start | <kbd>Home</kbd> | — | `tl.start` |
| Go to end | <kbd>End</kbd> | — | `tl.end` |

### Outliner

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `out.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Move (reorder) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Rename | <kbd>F2</kbd> | — | `out.rename` |
| Duplicate | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `out.instance` |
| Hide / Show | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Show active | <kbd>Num.</kbd> | — | `out.focus` |

### 2D Editor

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `2d.selectAll` |
| Deselect All | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Duplicate | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Frame Selected | <kbd>Num.</kbd> | — | `2d.focus` |
| Game View | <kbd>Num0</kbd> | — | `2d.gameView` |

### Properties（ルーチンのステップ）

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.grab` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| 機能 | 初期設定のショートカット | Nokia N95（電話のキーパッド） | atajos.iniでの名前 |
|---|---|---|---|
| Save script | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Undo | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Copy | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Cut | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Paste | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Select All | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Find | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Replace | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## 固定のキー（割り当て変更不可）

いくつかのキーは、そのときの操作によって働きが変わるため、リストに含まれていません：メニューやパネルを移動するときの方向キー、メニューやウィンドウでの<kbd>Enter</kbd>と<kbd>Esc</kbd>、行を移動している間のOutlinerの方向キー、入力中のIDEのキーボード、そしてN95の電話のキーパッド（[ナビゲーション](navegacion.md)を参照）。

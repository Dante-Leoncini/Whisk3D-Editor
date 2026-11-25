# 键盘快捷键

下面表格中的所有功能都可以重新分配快捷键，每个功能可以有**一个或多个**组合键（也可以一个都没有）。适用于 PC、Android（使用实体键盘）和 Web。在 N95 上，窗口无法捕获按键：需要手动编辑 `atajos.ini` 文件。

## 如何修改快捷键

1. 打开 **Properties → File/Render 选项卡 → Settings 卡片 → Keyboard Shortcuts…**
2. 选择 **Context**（General、3D Viewport、During a transform、UV Editor、Timeline、Outliner、2D Editor、Properties、Script Editor）。用左/右方向键也可以切换。
3. 在列表中选择功能，然后点击 **Add shortcut**（或再点一次已选中的那一行）。这时会出现“Press a key...”。
4. 按下新的组合键（例如 <kbd>Ctrl+M</kbd>）。按 <kbd>Esc</kbd> 取消，不做任何更改。

| 按钮 | 作用 |
|---|---|
| Add shortcut | 为该功能添加一个组合键。如果同一上下文中的另一个功能已经在用它，会把它从那个功能上移除，并提示你。 |
| Clear | 让该功能不再有快捷键。 |
| Reset | 把该功能恢复为默认快捷键。 |
| Reset all | 所有功能都恢复为默认设置。 |

菜单会在每个选项旁边显示**当前设置**的快捷键：如果你重新分配了某个功能，菜单就会显示新的组合键。在 N95 上，菜单显示的是手机按键（例如 Move 是 <kbd>1</kbd>，Delete 是 <kbd>C</kbd>）；如果连接了蓝牙键盘，并且能识别该键盘上的按键，菜单就会显示该键盘的按键。

修改过的组合键显示为绿色。当某个默认快捷键从它的功能上移除后，这个按键就空出来了：在该上下文中不再有任何作用。

每个上下文的快捷键在指针位于对应面板上时生效；**General** 的快捷键在所有面板中都生效。**During a transform** 的快捷键在移动、旋转或缩放某个元素时生效，并且优先。在 Script Editor 中，字母键总是用来输入：那里只能重新分配带 <kbd>Ctrl</kbd>、<kbd>Alt</kbd> 或 F 键的组合。

## shortcuts.ini 文件

修改会自动保存到 `res/shortcuts.ini` 中，它和 `config.ini` 放在一起。文件中只列出修改过的功能，你也可以手动编辑它：

```ini
# 功能 = 组合键, 另一个组合键   (留空 = 没有快捷键)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

按键名称：字母和数字照原样写，`Enter`、`Esc`、`Tab`、`Space`、`Backspace`、`Delete`、`Home`、`End`、`PageUp`、`PageDown`、`Left`、`Right`、`Up`、`Down`、`F1`…`F12`、`Num0`…`Num9`、`Num.`、`NumEnter`、`Comma`、`Plus`。修饰键：`Ctrl+`、`Shift+`、`Alt+`。

## 默认快捷键

<!-- 下面直到 ATAJOS-FIN 的表格由 tools/genmanual_atajos.py 根据代码生成：请勿手动编辑。 -->

<!--ATAJOS-INICIO-->

### General（任意面板）

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

### During a transform（移动 / 旋转 / 缩放）

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

### Properties（例程的步骤）

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.grab` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| 功能 | 默认快捷键 | Nokia N95（手机键盘） | shortcuts.ini 中的名称 |
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

## 固定按键（不可重新分配）

有些按键不在列表中，因为它们取决于你正在做的事情：浏览菜单和面板时的方向键，菜单和窗口中的 <kbd>Enter</kbd> 和 <kbd>Esc</kbd>，在 Outliner 中移动某一行时的方向键，在 IDE 中输入时的键盘，以及 N95 上的手机键盘（参见[导航](navegacion.md)）。

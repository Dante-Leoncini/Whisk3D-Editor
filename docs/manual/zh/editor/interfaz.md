# 界面

> [!NOTE]
> ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 支持鼠标和键盘（通过蓝牙）。本页中关于 Nokia 的部分，讲的是不用鼠标和键盘、只用手机按键时怎么操作。

Whisk3D 的窗口被划分为多个**视口**：每个视口是一个面板，里面有自己的编辑器（3D Viewport、Outliner、Properties……）。你可以拆分视口、调整大小，并选择每个视口显示什么。用鼠标、在触屏上用手指，或者用 Nokia 的键盘，操作方式都一样。

这是第一次启动程序时看到的界面：

![刚打开的 Whisk3D：带有立方体、相机和灯光的 3D Viewport，右侧是 Outliner 和 Properties，底部是 Timeline](../tutoriales/img/inicio.webp)

## 活动视口

鼠标下方的视口就是**活动视口**，它会用绿色边框标出。键盘命令、手柄以及其他任何外设的输入，都由它接收。把鼠标从一个视口移到另一个视口时，绿色边框会跟着移动：

![鼠标指针依次经过 3D Viewport、Outliner、Properties 和 Timeline，绿色边框每次都标出指针下方的视口](../tutoriales/img/viewports-activo.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上：**每按一下绿色的**通话键** ![](../tutoriales/img/tecla-verde.svg)，就会切换到下一个视口。N95 的屏幕上放得下两个视口：左边是 3D Viewport，右边是 Properties。

![一部 Nokia N95，左边是 3D Viewport，右边是 Properties：每按一次绿色键，绿色边框就移到另一个视口](../tutoriales/img/viewports-activo-nokia.webp)

## 菜单栏和角落按钮

每个视口顶部都有一个**菜单栏**（图中绿色部分），放着该编辑器的菜单和按钮。左上角始终是**角落按钮**（橙色部分）：它用来切换或修改视口。

![角落按钮的放大图：3D Viewport 菜单栏左上角的向下小箭头](../tutoriales/img/viewports-esquina.webp)

所有视口中的角落按钮都一样，而且它是所有视口唯一共有的按钮。

![Whisk3D 的四个视口，每个视口的菜单栏用绿色标出，角落按钮用橙色标出](../tutoriales/img/viewports-barras.webp)

## 调整大小

![PC](../tutoriales/img/icono-pc.svg) ![Android](../tutoriales/img/icono-android.svg) 用手指或鼠标**按住**角落按钮并拖动，就能调整大小：这会移动该视口的左边框和上边框，旁边的视口会随之调整。不拖动、**轻点一下**，则会打开包含视口选项的下拉菜单。下面的例子中，右下角视口的按钮被绕圈拖动，三个视口的大小都随之变化：

![一根手指绕圈拖动右下角视口的角落按钮，三个视口的大小随之变化](../tutoriales/img/viewports-arrastrar.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia 上：**按住绿色的**通话键** ![](../tutoriales/img/tecla-verde.svg)，再按方向键：活动视口（带绿色边框的那个）的边框会朝那个方向移动，视口随之变大或变小。只按绿色键、不按方向键，则会切换到下一个视口。

![按住绿色键的 Nokia N95：按右方向键，3D Viewport 变大；按左方向键，它变小，然后恢复原样](../tutoriales/img/viewports-tamano-nokia.webp)

## 视口选项

点击角落按钮会打开这个菜单：

![一根手指移到 3D Viewport 的角落按钮上点一下，菜单随之打开：上面是视口类型，分隔线下面是 Expand、Split in Columns、Split in Rows 和 Maximize](../tutoriales/img/viewports-menu.webp)

上面是**视口类型**：选择其中一个，视口就会变成那个编辑器。分隔线下面是可以对视口执行的操作：

| 选项 | 作用 |
|---|---|
| Expand | 视口占用旁边视口的位置，旁边的视口会关闭（如果旁边的视口已经拆分，整组都会关闭）。 |
| Split in Columns | 把它分成左右并排的两个。 |
| Split in Rows | 把它分成上下排列的两个。 |
| Maximize | 把它放大到整个窗口。最大化期间，菜单里只有 **Minimize**，用它让视口回到原位。 |

### 更改类型

把 3D Viewport 改成 UV Editor：

![点击 3D Viewport 的角落按钮，选择 UV Editor，视口就变成了 UV Editor](../tutoriales/img/viewports-cambiar-tipo.webp)

### Split in Columns / Rows

![Split in Columns：3D Viewport 被分成左右并排的两个视图](../tutoriales/img/viewports-dividir-columnas.webp)

![Split in Rows：3D Viewport 被分成上下排列的两个视图](../tutoriales/img/viewports-dividir-filas.webp)

### Expand

Outliner 占用了 Properties 的位置：

![在 Outliner 中选择 Expand，它会连 Properties 的位置一起占用](../tutoriales/img/viewports-expandir.webp)

### Maximize / Minimize

![3D Viewport 最大化到整个窗口，然后用 Minimize 回到原位](../tutoriales/img/viewports-maximizar.webp)

## 菜单栏的选项显示不全时

如果视口比较窄，它的菜单栏就放不下。3D Viewport 底部的工具栏也是一样。要查看其余部分：

- ![Android](../tutoriales/img/icono-android.svg) **用手指**：横向拖动栏。
- ![PC](../tutoriales/img/icono-pc.svg) **用鼠标**：在栏上按住鼠标键并拖动，或者在栏上滚动**滚轮**。
- ![PC](../tutoriales/img/icono-pc.svg) ![Nokia](../tutoriales/img/icono-nokia.svg) **用键盘**：在 PC 上按 <kbd>M</kbd>，在 Nokia 上按**左软键**，即可进入活动视口的栏。用左右方向键在栏上移动，到达某个菜单时它会自动打开。用上下方向键在菜单中移动，按 OK（PC 上是 <kbd>Enter</kbd>）选择。栏会自动滚动，显示你当前的位置。

![一根手指横向拖动 Outliner 的栏，原来放不下的按钮显示出来](../tutoriales/img/viewports-barra-dedo.webp)

![鼠标指针位于 Outliner 的栏上：滚动滚轮，栏就会滚动](../tutoriales/img/viewports-barra-rueda.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) 在 Nokia N95 上：左软键进入栏，方向键在栏上移动。

![Nokia N95 的界面：左软键打开模式菜单，右方向键依次经过栏上的各个菜单，栏会自动滚动](../tutoriales/img/viewports-barra-nokia.webp)

## 视口类型

| 视口 | 用途 |
|---|---|
| [3D Viewport](vista-3d.md) | 查看和编辑场景的地方：建模、移动对象、绘制、摆姿势和制作动画。 |
| [Outliner](outliner.md) | 场景中所有内容的列表。 |
| [Library](outliner.md#library-视图) | 项目的资源：网格、材质、纹理、动画、声音、脚本等。 |
| [Properties](propiedades.md) | 活动对象的数据，分成多个选项卡。 |
| [UV Editor](editor-uv.md) | 纹理如何贴在网格上。 |
| [Timeline](linea-de-tiempo.md) | 动画：关键帧和曲线。 |
| [2D Editor](editor-2d.md) | 游戏界面、HUD 和 2D 游戏。 |
| [Console](consola.md) | 引擎日志：错误、警告和信息。 |
| [IDE](editor-de-scripts.md) | Lua 脚本和着色器的编辑器。 |


Whisk3D 还在不断发展：以后会加入新的视口，现有的视口也可能会变化。它们都会以同样的方式工作：顶部有菜单栏，角落有按钮。

## 界面的其他功能

| 功能 | 说明 | 使用方法 |
|---|---|---|
| 用键盘打开菜单栏 | 打开活动视口的菜单栏，用方向键浏览。 | <kbd>M</kbd>（在 Edit Mode 中，<kbd>M</kbd> 是 Merge）。 |
| Undo / Redo | 撤销或重做上一个操作。 | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> 或 <kbd>Ctrl+Shift+Z</kbd>；也可用工具栏的箭头。 |
| Save project | 保存当前打开的 .w3d（如果是新项目，会要求你输入名称）。 | <kbd>Ctrl+S</kbd>，或 Properties → File/Render 选项卡 → File 卡片（参见[项目](proyecto.md)）。 |
| Fullscreen | 进入或退出全屏。 | <kbd>F11</kbd>。在 Android 上，如果手机没有刘海，会自动全屏。 |
| Play / Pause | 在任意面板中播放动画（或游戏）。 | <kbd>Space</kbd>，或 Timeline 上的 Play 按钮。 |

![Android](../tutoriales/img/icono-android.svg) **在 Android 上**：竖屏时，布局是上方 3D Viewport、下方 Properties；横屏时还会加上 Timeline。打开屏幕键盘时（例如在 Script Editor 中），活动视口会在可见区域内最大化，关闭键盘后再回到原位。

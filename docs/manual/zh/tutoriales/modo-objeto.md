# Object Mode

在 **Object Mode** 中，你操作的是整个对象：选择、移动、旋转、缩放、添加、删除，以及把一个对象放进另一个对象里。Whisk3D 启动时就处于这个模式，3D Viewport 左上角的按钮也会显示它。要改变网格的形状（它的顶点、边和面），则要用 Edit Mode，它有自己的教程。

## 什么是对象

场景中的一切都是**对象**：新项目开始时的立方体、相机、灯光、一个角色。每个对象都有：

- 一个**名称**，显示在 Outliner 中；
- 它的**变换**：在哪里（位置）、朝哪个方向转（旋转）以及有多大（缩放）；
- **属性**，取决于它的类型：灯光有颜色和强度，相机有镜头，网格有材质。

对象的类型有很多，但它们的选择、移动、旋转和缩放方式都一样。

## 选择对象

| | 如何选择 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 在对象上单击。<kbd>Shift</kbd> + 单击可加选或减选另一个对象。在空白处单击则什么都不选。 |
| ![Android](img/icono-android.svg) 触屏 | 在对象上点一下。打开 3D Viewport 工具栏上的 **Shift** 后，每点一下就加选或减选另一个对象。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | **铅笔**键从一个对象切换到下一个。用铅笔键加方向键，可以加选、减选、全选或全不选。 |

选中的对象会带有轮廓线，最后选中的那个就是**活动对象**：Properties 显示的就是它。

### 用框选择多个对象

![PC](img/icono-pc.svg) 从背景上开始，在 3D Viewport 中按住主键拖动鼠标：会画出一个**框**，松开后，框住的对象就被选中。也可以用 <kbd>B</kbd> 来画框。

![Android](img/icono-android.svg) 在触屏上，拖动一根手指会环绕视图：要用框选，先在 **Select → Box Select** 菜单中开启，然后再拖动手指。

拖动的方向决定选中什么，框的颜色会提示你：

| 拖动方向 | 框的颜色 | 选中什么 |
|---|---|---|
| 从左往右 | 绿色 | 只选中**完全位于框内**的对象。 |
| 从右往左 | 蓝色 | 框**碰到**的所有对象，哪怕只碰到一点。 |

![一排五个盒子：从左往右拖动时，绿色的框只选中完全在框内的两个；从右往左拖动时，蓝色的框连只碰到一点的那个也选中了](img/objeto-caja-raton.webp)

## 上下文菜单

和其他任何程序一样，**上下文菜单**把可以对所选内容执行的操作放在手边。在 Object Mode 中，它就是 **Object** 菜单，会在指针所在的位置打开：

| | 如何打开 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 在 3D Viewport 上右键单击 |
| ![Android](img/icono-android.svg) 触屏 | 一根手指在 3D Viewport 上按住不动半秒 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 左软键打开菜单栏；在那里选择 Object 菜单 |

![在 3D Viewport 中打开的上下文菜单，选中的是 Leon：Transform、Set Origin、Apply、Snap、Cut、Copy、Paste、Duplicate Objects、Duplicate Linked、Join、Set Parent、Clear Parent、Delete，以及预制体的选项](img/objeto-menu-contextual.webp)

![Android](img/icono-android.svg) 选中 Leon 后，一根手指在 3D Viewport 上按住不动：半秒后手机振动，菜单打开。抬起手指时不会选中任何选项：之后再点选项。

![一根手指按在 3D Viewport 上不动；半秒后出现上下文菜单，抬起手指后菜单仍然开着](img/objeto-menu-contextual-dedo.webp)

| 选项 | 作用 | ![PC](img/icono-pc.svg) 快捷键 |
|---|---|---|
| **Cut** | 保存所选内容以便之后粘贴，并把它从场景中移除。 | <kbd>Ctrl</kbd> + <kbd>X</kbd> |
| **Copy** | 保存所选内容（连同子级）的副本，以便之后粘贴：可以粘贴到这个场景、另一个项目，或另一个打开着的 Whisk3D 窗口。 | <kbd>Ctrl</kbd> + <kbd>C</kbd> |
| **Paste** | 把最后复制或剪切的内容放到场景中原来的位置，并保持选中。 | <kbd>Ctrl</kbd> + <kbd>V</kbd> |
| **Duplicate Objects** | 为所选内容创建一个独立于原对象的副本，并让它处于抓取状态，方便你移动：单击（或按 <kbd>OK</kbd>）就把它留在当前位置。 | <kbd>Shift</kbd> + <kbd>D</kbd> |
| **Duplicate Linked** | 和 Duplicate 一样，但副本与原对象**共享网格**：改变其中一个的形状，两个都会改变。位置、旋转和缩放则各自独立。适合大量相同的东西，比如森林里的树或大厅里的椅子：可以一次编辑全部，而且占用的内存更少。 | <kbd>Alt</kbd> + <kbd>D</kbd> |
| **Delete** | 删除所选内容。会要求确认。 | <kbd>X</kbd> 或 <kbd>Delete</kbd> |

菜单里的其他选项都会在本教程中讲到：Transform（移动、旋转和缩放）、[Set Origin 和 Snap](#从哪里移动旋转和缩放)、[Apply](#缩放最好保持为-1)、[Set Parent 和 Clear Parent](#outliner场景中的对象)。**Join** 把多个网格合并成一个；**预制体**（组装好以便重复使用的对象）有自己的教程。

## 移动、旋转和缩放

对象的三种基本操作是**移动**、**旋转**和**缩放**。每一种都可以用 Gizmo、用键盘来完成，在触屏上还可以用工具栏上的命令。如果出了错，**Undo**（PC 上是 <kbd>Ctrl</kbd> + <kbd>Z</kbd>）可以撤销。

### Gizmo

**Gizmo** 就是出现在选中对象上的彩色箭头。每种颜色代表一个轴：**红色**是 X，**绿色**是 Y，**蓝色**是 Z，也就是朝上的那个轴。

Gizmo 有三种模式，每种操作一种。**在 Gizmo 上单击一下而不拖动，就会切换到下一种**：从 Move 到 Rotate，从 Rotate 到 Scale，再从 Scale 回到 Move。

![PC](img/icono-pc.svg) 用鼠标在 Gizmo 上单击一下，就会切换模式。

![Leon 身上的 Gizmo：每单击一次，就从移动的箭头变成旋转的半圆，再变成缩放的方形端点，然后回到箭头](img/objeto-gizmo-modos-raton.webp)

![Android](img/icono-android.svg) 在 Gizmo 上点一下，就会切换模式。

![手指每点一下，Gizmo 就从移动变成旋转，再变成缩放，然后回到移动](img/objeto-gizmo-modos-dedo.webp)

使用时，抓住 Gizmo 的某个部分并拖动。松开后，操作就完成了。

| 模式 | 抓住哪里 | 作用 |
|---|---|---|
| Move | 一个箭头 | 只沿这个轴移动。 |
| Move | 一个小方块 | 在一个平面上移动：蓝色小方块在 X 和 Y 上移动，不改变高度。 |
| Move | 中心的圆 | 自由移动，跟随指针。 |
| Rotate | 一个彩色半圆 | 绕这个轴旋转。 |
| Rotate | 白色圆环 | 面向视图旋转。 |
| Scale | 一个方形端点 | 沿这个轴拉伸或压缩。 |
| Scale | 中心的圆 | 在三个轴上均匀地放大或缩小。 |

![PC](img/icono-pc.svg) 用鼠标：在 Gizmo 的某个部分上单击并拖动。

![Leon 沿着红色箭头移动后回到原位；单击一下，Gizmo 切换到旋转，Leon 随蓝色半圆转动；再单击一下切换到缩放，Leon 被蓝色端点拉高，按 Ctrl+Z 撤销](img/objeto-gizmo-raton.webp)

![Android](img/icono-android.svg) 用手指：把手指放在 Gizmo 的某个部分上并拖动。如果放在背景上，就会环绕视图。

![一根手指拖动红色箭头，Leon 沿 X 移动；然后在旋转模式下拖动蓝色半圆，Leon 随之转动](img/objeto-gizmo-dedo.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，Gizmo 默认是关闭的：移动、旋转和缩放都用按键完成，下面会讲到。可以在 Overlays → Gizmo 中打开它，配合蓝牙鼠标使用。

### 用键盘或命令

用键盘时不需要瞄准任何东西：按下按键，对象就会跟随鼠标（在手机上则是跟随方向键），直到你确认为止。

| | 移动 | 旋转 | 缩放 | 只在一个轴上 | 确认 | 取消 |
|---|---|---|---|---|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>G</kbd> | <kbd>R</kbd> | <kbd>S</kbd> | <kbd>X</kbd> / <kbd>Y</kbd> / <kbd>Z</kbd> | 单击或 <kbd>Enter</kbd> | <kbd>Esc</kbd> 或右键单击 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>1</kbd> | <kbd>2</kbd> | <kbd>3</kbd> | <kbd>1</kbd> / <kbd>2</kbd> / <kbd>3</kbd> | <kbd>OK</kbd>（方向键中间的键） | <kbd>C</kbd> |

![PC](img/icono-pc.svg) 按 <kbd>G</kbd> 并移动鼠标：对象会跟着移动。如果按 <kbd>X</kbd>，它就只在 X 上移动；再按一次 <kbd>X</kbd> 则从 Global 切换到 Local。旋转也一样：按 <kbd>R</kbd> 再按 <kbd>Z</kbd>，就绕竖直的 Z 轴旋转；不指定轴时，则面向视图旋转。你也可以输入一个数字：<kbd>G</kbd>、<kbd>X</kbd>、<kbd>2</kbd> 和 <kbd>Enter</kbd> 会让它沿 X 移动 2 米。

![按 G 后 Leon 跟随鼠标；按 X 后它被约束在红色轴上，单击一下就停在那里；按 R 和 Z 它绕自身的轴转动，按 S 它变大；每一步都用单击确认](img/objeto-teclas-raton.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上：<kbd>1</kbd> 移动，<kbd>2</kbd> 旋转，<kbd>3</kbd> 缩放，用**方向键**来执行动作。操作进行期间，<kbd>1</kbd>、<kbd>2</kbd> 和 <kbd>3</kbd> 不再切换操作，而是选择 X、Y 或 Z 轴。例如，先按 <kbd>2</kbd> 再按 <kbd>3</kbd>，就绕竖直的 Z 轴旋转。<kbd>OK</kbd> 确认，<kbd>C</kbd> 取消。

![一部竖着的 Nokia N95：按 1 再按方向键，Leon 移动；按 2 和 3（Z 轴），它绕自身的轴转动；按 3，它变大；每一次都按 OK 确认](img/objeto-teclas-nokia.webp)

![Android](img/icono-android.svg) **在触屏上**没有键盘，可以使用上面讲过的 Gizmo，或者 3D Viewport 工具栏上的**命令**。点 **Move**、**Rotate** 或 **Scale**，然后在视图的任意位置拖动手指：对象会跟随手指。工具栏会变化：**绿色对勾**确认，**红色叉号**取消，**X**、**Y** 和 **Z** 按钮把动作限制在这些轴上。

![在工具栏上点 Move，手指在视图中拖动，Leon 跟着移动；点一下绿色对勾，操作完成](img/objeto-comandos-dedo.webp)

### 使用精确数值

要精确地工作，可以用数字来指定动作：把东西正好旋转 90°，用 `0.5` 把它缩小到一半，用 `2` 把它放大到正好两倍，或者把它移动 1.5 米。**Whisk3D 中的一个单位就是一米。**

操作进行期间，3D Viewport 的菜单栏会显示正在进行的操作和数值。

![PC](img/icono-pc.svg) **用键盘：**按 <kbd>G</kbd>、<kbd>R</kbd> 或 <kbd>S</kbd> 之后，输入数字并按 <kbd>Enter</kbd>。<kbd>R</kbd> <kbd>Z</kbd> <kbd>9</kbd> <kbd>0</kbd> <kbd>Enter</kbd> 让它绕竖直轴旋转 90°；<kbd>S</kbd> <kbd>0</kbd> <kbd>.</kbd> <kbd>5</kbd> <kbd>Enter</kbd> 把它缩小到一半，<kbd>S</kbd> <kbd>2</kbd> <kbd>Enter</kbd> 则放大到两倍；<kbd>G</kbd> <kbd>X</kbd> <kbd>1</kbd> <kbd>.</kbd> <kbd>5</kbd> <kbd>Enter</kbd> 让它沿 X 移动 1.5 米。

![Android](img/icono-android.svg) **在触屏上：**用工具栏上的 **Move**、**Rotate** 或 **Scale** 开始操作，然后点一下顶部显示数值的那个栏：会打开 Whisk3D 的数字键盘。输入数字，点 **OK**；再点绿色对勾完成操作。这个键盘也能计算，比如 `90/2`。

![在工具栏上点 Rotate，再点 Z；点一下顶部的栏，打开数字键盘，输入 90 并点 OK，Leon 正好转了 90 度；点绿色对勾完成操作](img/objeto-numero-dedo.webp)

Object Mode 的所有快捷键都在手册中：[移动、旋转和缩放](../editor/transformar.md)和 [Object Mode](../editor/modo-objeto.md)。

### 工具栏：最近使用的命令

![PC](img/icono-pc.svg) ![Android](img/icono-android.svg) 在 3D Viewport 的**工具栏**（底部的那个）上，有 Undo 和 Redo（箭头），后面是**你最近使用过的命令**，最后用的排在最前面。一开始是 Move、Rotate、Scale 和 Delete。如果你缩放了一个对象，Scale 就会移到最前面：你最常用的命令总是在手边。

![带有 Move、Rotate 和 Scale 的工具栏；缩放 Leon 之后，Scale 移到了最前面](img/objeto-barra-inferior.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，工具栏默认是隐藏的，好把屏幕留给场景：移动、旋转和缩放都用手机按键完成。当工具栏出现时（例如变换时显示的比例编辑工具栏），它的按钮要用**虚拟鼠标**来操作：用右软键打开虚拟鼠标，方向键移动它，<kbd>OK</kbd> 单击。

## 从哪里移动、旋转和缩放

### 原点

每个对象都有一个**原点**：Gizmo 就是从这个点出发的。变换以这个点为基准：对象绕着原点旋转，也以原点为中心放大或缩小。Properties 中显示的**位置**就是原点的位置。

在基本体中，原点位于中心。在导入的模型中，原点位于制作它的程序所留下的位置：Leon 的原点在他的脚下。

![一个选中的立方体的原点特写：中心的一个绿色点](img/objeto-origen.webp)

### 3D 光标

**3D 光标**是 3D Viewport 中那个带十字的圆。一开始它位于世界的中心。

![3D 光标的特写：一个带四条射线的绿色圆，像一个准星](img/objeto-cursor3d.webp)
 它有两个用途：

- 用 **Add** 添加的所有东西都出现在这里；
- 它可以作为旋转和缩放的中心，参见[轴心点](#轴心点)。

3D 光标用 **Snap** 菜单来移动，这个菜单也可以把对象移到光标处：

| | 如何打开 Snap 菜单 |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Shift</kbd> + <kbd>S</kbd> |
| 菜单 | Object → Snap（在 Edit Mode 中是 Mesh → Snap） |

| 选项 | 作用 |
|---|---|
| Cursor to Selected | 把 3D 光标移到所选内容的中心。在 Edit Mode 中，则是所选顶点、边或面的中心。 |
| Cursor to Active | 把 3D 光标移到活动对象的原点。 |
| Cursor to World Origin | 把它移回世界的中心。 |
| Cursor to Grid | 把它移到最近的栅格点上。 |
| Selection to Cursor | 把所选内容移到 3D 光标处。 |
| Selection to Cursor (Keep Offset) | 同上，但选中多个对象时会保持它们之间的距离：整组一起移过去。 |
| Selection to Active | 把所选内容移到活动对象处。 |
| Selection to Grid | 把每个选中的对象吸附到最近的栅格点上。 |

例如，要把一个盒子放到某人手中，或者把一个对象放到另一个对象所在的位置：先选中目标对象，执行 **Cursor to Selected**；然后选中要移动的对象，执行 **Selection to Cursor**。

![选中 Leon：按 Shift+S，选择 Cursor to Selected，3D 光标跳到他的脚下；然后选中盒子，按 Shift+S，选择 Selection to Cursor，盒子就出现在那里](img/objeto-cursor.webp)

### 轴心点

**轴心点**是旋转和缩放所选内容时围绕的中心。用 3D Viewport 栏上的轴心按钮来选择，它位于 **Global** 的左边（图标会随所选的轴心变化）：

| 轴心 | 旋转和缩放围绕哪里 |
|---|---|
| **Median Point** | 所有选中内容的中心。这是默认选项。只选中一个对象时，就是它的原点。 |
| **Active Element** | 活动对象（最后选中的那个）的原点：其他对象绕着它转。 |
| **Individual Origins** | 每个对象绕自己的原点：所有对象同时旋转，各自在原地转。 |
| **3D Cursor** | 3D 光标。 |

用 **3D Cursor**，可以让任何东西绕任何点旋转：把光标放到那里（用 Snap 菜单），然后旋转。适合把椅子摆在桌子周围、摆放钟表的指针，或者任何需要绕着某个东西转的物体。

![一排三个盒子旋转 90°：用 Median Point 时，它们一起绕中心转动；用 Individual Origins 时，每个都在原地转；用 3D Cursor 时，它们绕着放在旁边的光标转动](img/objeto-pivote.webp)

### Global 和 Local

在某个轴上移动、旋转或缩放时，这个轴可以是**世界**的轴，也可以是**对象**的轴。用 3D Viewport 栏上的 **Global** 按钮来选择：

| 方向 | 坐标轴 |
|---|---|
| **Global** | 世界的轴：始终不变，Z 朝上。这是默认选项。 |
| **Local** | 对象自身的轴，会随对象一起转动。 |
| **View** | 屏幕的轴：X 朝向侧面，Y 朝上。 |
| **Normal** | 在 Edit Mode 中，所选面朝向的方向。 |

对象被旋转过时，区别就看出来了。例如，Leon 旋转了 40°：在 **Global** 中，他的 Y 轴是世界的 Y 轴，所以相对于他面朝的方向，他是斜着移动的；在 **Local** 中，Y 轴随他一起转动，他就笔直地向前走。一辆在街角转了弯的汽车也是一样：要让它前进，就沿它的 Local 轴移动。

![PC](img/icono-pc.svg) 用键盘时，第二次按轴的字母会从 Global 切换到 Local：<kbd>G</kbd> <kbd>Y</kbd> 沿世界的 Y 移动，<kbd>G</kbd> <kbd>Y</kbd> <kbd>Y</kbd> 则沿对象的 Y 移动。在 Nokia N95 上，移动时按 <kbd>2</kbd> <kbd>2</kbd> 也一样。

![转了 40° 的 Leon：Gizmo 设为 Global 时，箭头沿着世界的栅格；设为 Local 时，箭头随他一起转动，绿色箭头让他笔直地朝面对的方向前进](img/objeto-local-global.webp)

### 更改原点

用 **Object → Set Origin**，可以在不移动网格的情况下改变 3D 网格原点的位置。具体做法以一扇需要绕铰链打开的门为例，在[更改原点：一扇门](puerta.md)教程中讲解。

## 属性

活动对象的**属性**位于 Properties 视口中。**Object** 选项卡（立方体图标）对所有对象都一样：最上面是 **Transform** 卡片，包括位置、旋转和缩放。这些数字可以手动输入：单击（或点一下）数字，然后输入。在 3D Viewport 中移动对象会改变这些数字，修改数字也会移动对象。

其他选项卡取决于对象的类型：灯光有灯光的选项卡，网格有材质和修改器的选项卡。

### 菱形：可以制作动画的属性

许多属性旁边都有一个**菱形**。菱形表示这个属性**可以制作动画**：单击菱形会在当前帧保存它的数值（一个*关键帧*），在绿色的菱形上再单击一下则会删除它。有了多个关键帧，属性就会随时间自动变化。

菱形的颜色表示属性的状态：**灰色**，还没有动画；亮起的**绿色**，已经有动画，并且在这一帧有关键帧；**淡绿色**，有动画，但这一帧没有关键帧。

![选中 Leon 时的 Properties：Transform 卡片中的位置、旋转和缩放，右侧那一列菱形被标了出来](img/objeto-propiedades.webp)

动画会在后面有专门的教程。

### 缩放最好保持为 1

一个建议：**让对象的缩放保持为 1**。用 Scale 放大或缩小对象很方便，但缩放过的对象之后可能会带来问题：子级会继承缩放，旋转时会变形；物理、碰撞和某些修改器也可能计算出错。

如果你缩放了一个对象，就**应用缩放**：它现在的大小会变成它的实际大小，缩放值回到 1，而看上去没有任何变化。

| | 如何应用缩放 |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Ctrl</kbd> + <kbd>A</kbd> → Scale |
| 菜单 | Object → Apply → Scale |

![一个缩放过的盒子：Properties 中的缩放是 2 和 0.5；按 Ctrl+A → Scale 后，缩放回到 1，盒子看上去一样](img/objeto-escala-raton.webp)

在同一个菜单中，**Location** 和 **Rotation** 对位置和旋转做同样的事，**All Transforms** 则同时应用这三项。

## Outliner：场景中的对象

**Outliner** 是场景中所有对象的列表。单击（或点一下）某一行就会选中那个对象，和在 3D Viewport 中一样。在 PC 上，它位于右上方。

一个对象可以**在另一个对象里面**：它会显示得更靠右，挂在那个对象下面。外面的是**父级**，里面的是**子级**。子级**继承父级的变换**：移动、旋转或缩放父级时，子级会跟着一起变。子级仍然可以单独移动，但始终是相对于父级的。

Leon 就是一个例子：他的网格，也就是你看到的四个部分，都是他的骨架 `leon_rig` 的子级。移动骨架，整个 Leon 都会移动。

![leon_rig 展开后的 Outliner：里面是它的四个网格，从 leon 到 leon.003，显示得更靠右](img/objeto-outliner.webp)

把一个对象放进另一个对象里，叫作**设置父级**：

| | 如何设置父级 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 在 Outliner 中，把子级那一行拖到父级那一行上松开。或者先选中子级，再（按住 <kbd>Shift</kbd>）选中父级，然后按 <kbd>Ctrl</kbd> + <kbd>P</kbd>。 |
| ![Android](img/icono-android.svg) 触屏 | 选中子级；打开工具栏上的 **Shift**，点一下父级；然后选择 Object 菜单 → Set Parent。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Outliner 为活动视口、选中子级时：按 <kbd>1</kbd>，**右**方向键把它放进上面那个对象里，**左**方向键把它移出来；再按一次 <kbd>1</kbd> 确认。 |
| 菜单 | Object → Set Parent；要移出来，用 Object → Clear Parent |

![PC](img/icono-pc.svg) 在视频中，一个盒子在 Outliner 中被拖到 `leon_rig` 上：它成了 `leon_rig` 的子级，移动 Leon 时，盒子也跟着移动。

![在 Outliner 中把盒子那一行拖到 leon_rig 上，它就到了里面；按 G 移动 Leon，盒子跟着他一起移动](img/objeto-emparentar-raton.webp)

## 对象的类型

所有对象都从 **Add** 菜单添加（3D Viewport 栏上的 **+** 按钮，在 PC 上也可以按 <kbd>Shift</kbd> + <kbd>A</kbd>）。新对象会出现在 3D 光标处，并保持选中状态。

![打开的 Add 菜单，里面有所有对象类型](img/objeto-tipos.webp)

基本的对象有这些：

| 对象 | 是什么 |
|---|---|
| [3D Mesh](../editor/objeto-malla.md) | 一个 3D 模型：一个角色、一座房子、一块石头。它由顶点、边和面组成，显示时带有材质和纹理。Add → Mesh 中的基本体（立方体、球体、圆柱体……）是用来开始建模的网格。 |
| [Camera](../editor/objeto-camara.md) | 游戏和渲染的视点。活动相机看到的，就是玩游戏时看到的，也是图像或视频中输出的内容。 |
| [Light](../editor/objeto-luz.md) | 照亮场景。有三种类型：Point（一个灯泡）、Sun（日光，光线平行）和 Spot（一个锥形，像手电筒）。 |
| [Armature](../editor/objeto-esqueleto.md) | 一串带动网格的骨骼，用于角色动画。Leon 就有一个：骨架走动时，网格会跟着走。 |
| [Sound](../editor/objeto-sonido.md) | 放在场景中的声音。它可以作为背景播放，像音乐一样，也可以从对象所在的位置发出。 |
| Empty | 没有形状的对象：场景中的一个点。可以作为父级把其他对象归为一组，或者用作参考点。 |
| Collection | 在 Outliner 中整理对象用的文件夹。 |

还有更多：路径、雾、粒子、用于碰撞的碰撞盒、游戏用的 2D 界面，以及用于优化的对象。以后还会陆续加入更多。本教程讲的是一般意义上的对象；每种对象都有自己的文章，逐一讲解它的属性。

## 所有对象共有的内容

除了变换和属性，任何对象都可以有：

- Lua **脚本**，为它在游戏中赋予行为：让它自己移动、响应按键、发生碰撞。后面会讲到。
- **动画**：任何带菱形的属性都可以制作动画。动画有自己的教程。
- **约束**：把它和另一个对象绑定在一起的规则，比如复制它的位置，或者始终朝向相机。在[约束](../editor/restricciones.md)中讲解。

有些类型还有自己特有的东西。例如，3D 网格有**修改器**，可以在不破坏形状的情况下改变它：像镜子一样镜像、重复、平滑。这些都在 [3D Mesh](../editor/objeto-malla.md) 中。

## 导入 3D 模型

除了在 Whisk3D 中建模，还可以导入在其他程序中制作的模型。支持的格式有：

| 格式 | 包含什么 |
|---|---|
| **GLB** 和 **glTF** | 网格、材质和纹理、骨架以及动画。这是最完整的格式：Leon 就是一个 .glb 文件。 |
| **OBJ** | 网格、材质和纹理（连同它的 .mtl）。没有骨架和动画。 |
| **FBX** | 网格、材质、骨架和动画。 |
| **STL** | 只有形状：三角形，没有材质和颜色。这是 3D 打印和 CAD 程序使用的格式。 |

从 **Add → Imports** 导入，先选择格式，再选择文件。

![PC](img/icono-pc.svg) 在 PC 上，也可以从文件夹中把文件拖到 3D Viewport 上松开。

![打开 Add → Imports → GLB；选择 leon.glb 后，Leon 出现在场景中，他的骨架和网格显示在 Outliner 中](img/objeto-importar.webp)

带骨架的模型导入后是一个**里面装着网格的骨架**：骨架是父级，网格是它的子级，就像在 [Outliner](#outliner场景中的对象) 中看到的那样。动画保存在骨架中，可以直接使用。

要把模型带到另一个程序或 3D 打印机，可以走相反的路：Properties → 软盘图标的选项卡 → **Export**，可以导出 OBJ、glTF、GLB 或 STL。

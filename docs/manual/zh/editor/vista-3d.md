# 3D Viewport

**3D Viewport** 是查看和编辑场景的视口：建模、移动对象、绘制、摆姿势和制作动画都在这里进行。和所有视口一样，它顶部是**菜单栏**，底部是**工具栏**。

![Object Mode 下选中立方体的 3D Viewport：顶部是完整的菜单栏](../tutoriales/img/vista-3d-barra.webp)

- **菜单栏**的每个按钮及其菜单：[3D Viewport 的菜单栏](vista-3d-barra.md)。
- **导航**（环绕、平移、缩放、各种视图、相机）：[在 3D Viewport 中导航](navegacion.md)，以及[在 3D Viewport 中导航](../tutoriales/navegacion.md)教程。
- **移动、旋转和缩放**，以及 Gizmo、轴心点和方向：[移动、旋转和缩放](transformar.md)。

## 模式

3D Viewport 有多种**模式**：单击、按键和菜单的作用取决于当前所处的模式。模式可以用菜单栏的第一个按钮（它显示当前模式）来选择，也可以用 <kbd>Tab</kbd>（Object ↔ Edit；在 Nokia N95 上是 <kbd>OK</kbd>）。

能选择哪些模式取决于**活动对象**。所有对象都有 **Object Mode**。3D 网格另外还有 Edit Mode 和三种绘制模式；骨架则有 Edit Mode 和 Pose Mode。如果是其他任何对象（灯光、相机、声音），唯一的模式就是 Object Mode，这个按钮也不会出现。

![选中立方体时的模式菜单：Object Mode、Edit Mode、Vertex Paint、Weight Paint 和 Texture Paint](../tutoriales/img/vista-3d-menu-modo.webp)

![选中骨架时的模式菜单：Object Mode、Edit Mode 和 Pose Mode](../tutoriales/img/vista-3d-modos-esqueleto.webp)

| 模式 | 适用对象 | 用途 |
|---|---|---|
| [Object Mode](modo-objeto.md) | 全部 | 处理整个对象：创建、移动、旋转、缩放、复制、设置父级和删除对象。 |
| [Edit Mode](modo-edicion.md) | 3D 网格和曲线 | 建模：移动、添加和删除网格的顶点、边和面。 |
| [Edit Mode](esqueletos.md#骨架的-edit-mode) | 骨架 | 搭建骨架：创建骨骼，并把它们摆到静止姿态的位置。 |
| [Pose Mode](esqueletos.md#pose-mode) | 骨架 | 为骨骼摆姿势并保存姿势：角色就是这样制作动画的。 |
| [Weight Paint](pintura.md#weight-paint) | 3D 网格 | 绘制每根骨骼对每个顶点的影响程度。 |
| [Vertex Paint](pintura.md#vertex-paint) | 3D 网格 | 直接在顶点上绘制颜色。 |
| [Texture Paint](pintura.md#texture-paint) | 3D 网格 | 在纹理上绘制，直接画在对象表面上。 |

### Object Mode

这是 Whisk3D 启动时所在的模式，也是所有对象都有的唯一模式。在这里处理的是整个对象：选择、移动、旋转、缩放、添加和删除对象。参见 [Object Mode](modo-objeto.md) 和 [Object Mode](../tutoriales/modo-objeto.md) 教程。

### 网格的 Edit Mode

编辑网格的**形状**：顶点、边和面。菜单栏上会出现 **Vertex / Edge / Face** 按钮（决定单击时选择哪种元素），以及 **Mesh** 和 **UV** 菜单。场景的其余部分仍然可见，但无法操作。

![Edit Mode 中的立方体：一个面被选中，显示为绿色，Gizmo 位于其上](../tutoriales/img/vista-3d-modo-edicion.webp)

### 骨架的 Edit Mode

搭建**骨架**：创建骨骼（从另一根骨骼的末端挤出）、移动骨骼并设置父级。这是骨架的**静止**姿态，也就是没有动画时的姿态。

![Edit Mode 中 Leon 的骨架：骨骼显示为蓝色，处于静止姿态](../tutoriales/img/vista-3d-modo-edicion-esqueleto.webp)

### Pose Mode

为骨架**摆姿势**：旋转和移动骨骼，但不改变骨架本身，网格会跟着骨骼动。保存在不同帧上的姿势，就构成了角色的动画。

![Pose Mode 中 Leon 的骨架，行走动画进行到一半：骨骼是弯曲的](../tutoriales/img/vista-3d-modo-pose.webp)

### Weight Paint

绘制**每根骨骼对网格每个部分的影响程度**：红色表示所选骨骼完全带动的部分，蓝色表示它不带动的部分。正是它让你弯曲手臂时，弯的是手臂而不是腰。

![Weight Paint 中 Leon 的一个网格：所选骨骼带动胸部（红色和黄色），其余部分都不受影响（蓝色）](../tutoriales/img/vista-3d-modo-pesos.webp)

### Vertex Paint

在网格的**顶点上绘制颜色**，不使用纹理。可以用来给简单的模型上色、手绘阴影，以及在着色器中制作效果。

![Vertex Paint 中的立方体：画笔从一侧划到另一侧，碰到的顶点被染成红色](../tutoriales/img/vista-3d-modo-vertices.webp)

### Texture Paint

**直接在纹理上**绘制，画在对象表面上，就像给玩偶上色一样。画好的内容用菜单栏上出现的软盘图标来保存。

![Texture Paint 中的 Leon：画笔在他的夹克上画出一道红色笔触](../tutoriales/img/vista-3d-modo-textura.webp)

在绘制模式中，工具栏上方会出现画笔栏（**Radius** 和 **Value**）。参见[绘制模式](pintura.md)。

## 工具栏

3D Viewport 底部有 **Undo** 和 **Redo**（箭头）、**最近使用的命令**（一开始是 Move、Rotate、Scale 和 Delete；用过的会移到前面），以及**比例编辑**（Proportional Editing）。移动、旋转或缩放时，工具栏会改变：绿色对勾确认，红色叉号取消，还有方向，以及把移动限制在相应轴上的 **X**、**Y** 和 **Z**。参见[移动、旋转和缩放](transformar.md#工具栏)。

![Android](../tutoriales/img/icono-android.svg) 在触屏上，工具栏还会多出 **Shift** 和 **Ctrl**：它们会保持开启（显示为绿色），起到这两个键的作用；以及 **View** 按钮（显示器图标），让手指在 Edit Mode 和各绘制模式中用来导航，而不是选择或绘制。

![Nokia](../tutoriales/img/icono-nokia.svg) 在 Nokia N95 上，工具栏默认是隐藏的，好把屏幕留给场景，需要时才会出现：在绘制模式中会出现画笔栏（**Radius** 和 **Value**）；在开启比例编辑的情况下移动、旋转或缩放时，会出现它的半径栏。它的按钮要用**虚拟鼠标**来操作：用右软键打开虚拟鼠标，方向键移动它，<kbd>OK</kbd> 单击。

## Gizmo 和 3D 光标

![选中的立方体，上面是 Gizmo（彩色箭头），左边是 3D 光标，各自用橙色圆圈和名称标出](../tutoriales/img/vista-3d-gizmo-cursor.webp)

- **Gizmo** 是所选内容上的彩色箭头：抓住它们就能移动、旋转和缩放。在 Gizmo 上单击一下，会从一种模式切换到另一种。参见 [Gizmo](transformar.md#gizmo)。
- **3D 光标**是那个带十字的圆：用 Add 添加的东西会出现在这里，它也可以作为轴心点。参见 [3D 光标](../tutoriales/modo-objeto.md#3d-光标)。

![Nokia](../tutoriales/img/icono-nokia.svg) 在 Nokia N95 上，Gizmo 默认是关闭的，因为屏幕很小，而且在那里是用按键来移动的。可以在 **Overlays → Gizmo** 中开启它，然后用虚拟鼠标来抓取。

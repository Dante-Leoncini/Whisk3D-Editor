# 3D Viewport 的菜单栏

![Object Mode 下 3D Viewport 的菜单栏，选中了一个立方体：Object Mode、轴心点、Global、磁铁、衰减编辑、View、Select、Add、Animation、Object、Overlays、Render 和场景](../tutoriales/img/vista-3d-barra-menus.webp)

如果菜单栏显示不全，可以滚动它：参见[菜单栏的选项显示不全时](interfaz.md#菜单栏的选项显示不全时)。

## 模式

![模式按钮](../tutoriales/img/boton-vista3d-modo.webp) 显示当前工作所在的模式，点击它可以选择另一个模式。出现哪些模式取决于活动对象：网格有 Edit Mode 和三种绘制模式；骨架有 Edit Mode 和 Pose Mode。如果是其他类型的对象（灯光、相机），这个按钮不会出现，因为唯一的模式就是 Object Mode。每种模式的说明见[模式](vista-3d.md#模式)。

![选中立方体时的模式菜单：Object Mode、Edit Mode、Vertex Paint、Weight Paint 和 Texture Paint](../tutoriales/img/vista-3d-menu-modo.webp)

![PC](../tutoriales/img/icono-pc.svg) <kbd>Tab</kbd> 在 Object Mode 和 Edit Mode 之间来回切换。![Nokia](../tutoriales/img/icono-nokia.svg) 在 N95 上按 <kbd>OK</kbd>。

在网格的 **Edit Mode** 中，旁边会出现 **Vertex / Edge / Face** 按钮：决定单击时选择哪种元素（参见 [Edit Mode](modo-edicion.md)）。

## 轴心点

![轴心点按钮](../tutoriales/img/boton-vista3d-pivote.webp) **轴心点**是变换发生的基准点：所选内容绕什么旋转、从哪里缩放。按钮的图标会随所选的轴心点而变化，所以一眼就能看出当前用的是哪一个。

![Transform Pivot Point 菜单：3D Cursor、Individual Origins、Median Point（已选中，显示为绿色）和 Active Element，每一项都有自己的图标](../tutoriales/img/vista-3d-menu-pivote.webp)

| 轴心点 | 从哪里旋转和缩放 |
|---|---|
| ![3D Cursor](../tutoriales/img/boton-pivote-cursor.webp) **3D Cursor** | 从 3D 光标。可以用来让东西绕任意一点旋转：把光标放到那里（用 [Snap 菜单](#吸附)），然后旋转。 |
| ![Individual Origins](../tutoriales/img/boton-pivote-individual.webp) **Individual Origins** | 每个对象从自己的原点：所有对象同时旋转，各自在原地转动。 |
| ![Median Point](../tutoriales/img/boton-pivote-medio.webp) **Median Point** | 从所有所选内容的中心。这是默认设置。 |
| ![Active Element](../tutoriales/img/boton-pivote-activo.webp) **Active Element** | 从活动对象（最后选中的那个）的原点：其他对象绕着它旋转。 |

每一种的视频示例，请参阅 [Object Mode](../tutoriales/modo-objeto.md#轴心点) 教程。

## 方向

![方向按钮](../tutoriales/img/boton-vista3d-orientacion.webp) 沿某个轴移动、旋转或缩放时（用 <kbd>X</kbd>、<kbd>Y</kbd>、<kbd>Z</kbd>，用 Gizmo，或者用 N95 的按键）所使用的坐标轴。按钮会显示所选的方向。

![Transform Orientations 菜单：Global、Local、Normal 和 View](../tutoriales/img/vista-3d-menu-orientacion.webp)

| 方向 | 坐标轴 |
|---|---|
| **Global** | 世界的坐标轴：始终不变，Z 朝上。这是默认设置。 |
| **Local** | 对象自身的坐标轴：随对象一起旋转。一辆转了向的汽车沿它的 Local 轴前进。 |
| **Normal** | 在 Edit Mode 中：所选面朝向的方向（Extrude 使用的就是它）。 |
| **View** | 屏幕的坐标轴：X 朝侧面，Y 朝上。 |

![PC](../tutoriales/img/icono-pc.svg) 变换过程中，第二次按下轴字母会从 Global 切换到 Local：<kbd>G</kbd> <kbd>Y</kbd> <kbd>Y</kbd>。

## 吸附

![磁铁按钮](../tutoriales/img/boton-vista3d-iman.webp) 开启**磁铁**（**Snap**）后，被移动的东西会吸附到某个地方：其他对象的顶点、边或面，或者它们的中心。开启时按钮显示为绿色。

![Snap 菜单：Enable、Snap Base、Snap Target，以及 Affect Move、Rotate、Scale 和 Include Active、Edited、Non-Edited 复选框](../tutoriales/img/vista-3d-menu-iman.webp)

| 选项 | 作用 | 快捷键 |
|---|---|---|
| Enable | 开启或关闭吸附。 | <kbd>Shift+Tab</kbd> |
| Snap Base | 被移动内容的哪个点去吸附：最近的点、中心、中位点或活动元素。 | 菜单 |
| Snap Target | 吸附到什么上：顶点、边、面，或它们的中心。使用 Face Project / Face Nearest 时，顶点会贴附在另一个表面上（用于重新拓扑）。 | 菜单 |
| Affect Move / Rotate / Scale | 吸附对哪些操作生效。 | 菜单 |
| Include Active / Edited / Non-Edited | 可以吸附到哪些对象上。 | 菜单 |

**3D 光标**用另一个菜单来移动，即光标的吸附菜单：<kbd>Shift</kbd> + <kbd>S</kbd>，或者 Object 菜单 → Snap（Cursor to Selected、Selection to Cursor……）。具体说明见 [Object Mode](../tutoriales/modo-objeto.md#3d-光标)。

## 衰减编辑

![衰减编辑按钮](../tutoriales/img/boton-vista3d-proporcional.webp) 使用**衰减编辑**（**Proportional Editing**）时，移动、旋转或缩放某个东西，附近的部分会平滑地跟着动：离得越近，动得越多。可以用来建模有机形状（地形、脸），而不必逐个移动顶点。开启时按钮显示为绿色。

![Proportional Editing 菜单：Enable、Connected Only、衰减的形状（Smooth、Sphere、Root、Inverse Square、Sharp、Linear、Constant、Random）和 Proportional Size](../tutoriales/img/vista-3d-menu-proporcional.webp)

| 选项 | 作用 | 快捷键 |
|---|---|---|
| Enable | 开启或关闭衰减编辑。 | <kbd>O</kbd> |
| Connected Only | 沿着边来计算距离：不会带动附近分离的部分。 | 菜单 |
| Falloff | 影响如何随距离分布：Smooth、Sphere、Root、Inverse Square、Sharp、Linear、Constant 或 Random。 | 菜单 |
| Proportional Size | 影响半径。变换过程中可以用滚轮或 <kbd>PageUp</kbd> / <kbd>PageDown</kbd> 调整。 | 菜单 / 滚轮 |

## View

![View 按钮](../tutoriales/img/boton-vista3d-vista.webp) 所有与**导航**有关的功能：从哪里观看场景。每个选项及其视频，请参阅[在 3D Viewport 中导航](../tutoriales/navegacion.md)教程。

![View 菜单：Cameras、Viewpoint、Frame Selected、Local View、Perspective/Ortho、Lock Orbit 和 Lock the camera to the view](../tutoriales/img/vista-3d-menu-vista.webp)

| 选项 | 作用 | ![PC](../tutoriales/img/icono-pc.svg) 快捷键 |
|---|---|---|
| Cameras | 通过活动相机观看，或把所选对象设为活动相机。 | <kbd>Num 0</kbd> / <kbd>Ctrl+Num 0</kbd> |
| Viewpoint | Front、Back、Right、Left、Top、Bottom。 | <kbd>Num 1</kbd>、<kbd>Num 3</kbd>、<kbd>Num 7</kbd>（加 <kbd>Ctrl</kbd> 为相反方向） |
| Frame Selected | 让视图以所选内容为中心，并拉近到正好容纳它。 | <kbd>Num .</kbd> |
| Local View | 视图中只保留所选内容，再执行一次则恢复。 | <kbd>/</kbd> |
| Perspective/Ortho | 在透视和正交之间来回切换。 | <kbd>Num 5</kbd> |
| Lock Orbit | 原本的环绕变成平移视图。 | — |
| Lock the camera to the view | 通过相机观看时：导航会移动相机。 | — |

## Select

![Select 按钮](../tutoriales/img/boton-vista3d-seleccionar.webp) 一次性选中全部、全不选、反选，或者选中框内的内容。

![Select 菜单：All、None、Invert 和 Box Select](../tutoriales/img/vista-3d-menu-seleccionar.webp)

| 选项 | 作用 | 快捷键 |
|---|---|---|
| All | 全部选中。 | <kbd>A</kbd> |
| None | 取消全部选择。 | <kbd>Alt+A</kbd> |
| Invert | 已选中的变为未选中，反之亦然。 | <kbd>Ctrl+I</kbd> |
| Box Select | 拖出一个框：从左往右拖（绿色）只选中完全位于框内的内容；从右往左拖（蓝色）只要碰到就会选中。按住 <kbd>Shift</kbd> 可加选。 | <kbd>B</kbd>，或在背景上拖动 |

在 Edit Mode 中还会多出 Select Linked、循环和路径（参见 [Edit Mode](modo-edicion.md)）。

## Add

![Add 按钮](../tutoriales/img/boton-vista3d-anadir.webp) 创建新对象，新对象会出现在 3D 光标处，并保持选中状态。仅在 Object Mode 中可用。

![打开的 Add 菜单，显示所有类型的对象](../tutoriales/img/vista-3d-menu-anadir.webp)

每种类型的说明见 [Object Mode → Add 菜单](modo-objeto.md#add-菜单)。![PC](../tutoriales/img/icono-pc.svg) <kbd>Shift</kbd> + <kbd>A</kbd> 会在鼠标所在的位置打开它。

在 Edit Mode 中，这里会改为显示 **Mesh**（建模操作，参见 [Edit Mode](modo-edicion.md)）和 **UV**（展开和投影 UV，参见 [UV Editor](editor-uv.md)）。在 Texture Paint 中会出现一个**软盘**图标，用来保存绘制的纹理；有未保存的绘制内容时，它会显示为绿色。

## Animation

![Animation 按钮](../tutoriales/img/boton-vista3d-animacion.webp) 所选内容的**关键帧**：在当前帧保存位置、旋转或缩放的数值，让动画从一个关键帧过渡到另一个关键帧。选中了东西时才会出现，在 Object Mode 和 Pose Mode 中可用，其选项取决于所选内容。

![选中立方体时的 Animation 菜单：Insert Keyframe、Insert Mesh Keyframe、Delete Keyframe、Clear Keyframe、Delete Mesh Layer 和 Motion Trail](../tutoriales/img/vista-3d-menu-animacion.webp)

| 选项 | 作用 | 快捷键 |
|---|---|---|
| Insert Keyframe | 一个用来选择保存什么的菜单：位置、旋转、缩放或全部。 | <kbd>I</kbd>（<kbd>Shift+I</kbd>：不询问，直接保存全部） |
| Insert Mesh Keyframe | 仅限 3D 网格：选择保存网格动画的哪一部分：顶点、法线或 UV。 | 菜单 |
| Delete Keyframe | 删除当前帧的关键帧。 | 菜单 |
| Clear Keyframe | 删除所选内容的整个动画。 | 菜单 |
| Delete Mesh Layer | 仅限 3D 网格：整个删除网格动画中的一层：**顶点**层、**法线**层（例如在没有光照的模型中）或 **UV** 层。至少会保留一个关键帧。 | 菜单 |
| Motion Trail | 在视图中绘制动画对象经过的路径。 | 菜单 |

### 在 Pose Mode 中

为骨架摆姿势时，这个菜单处理的是所选骨骼（以及活动骨骼）的关键帧：

![Pose Mode 中的 Animation 菜单，显示 Leon 的骨架：Insert Keyframe、Delete Keyframe、Clear Keyframe、Reset Pose 和 Motion Trail](../tutoriales/img/vista-3d-menu-animacion-pose.webp)

| 选项 | 作用 | 快捷键 |
|---|---|---|
| Insert Keyframe | 在当前帧保存所选骨骼的姿势：位置、旋转、缩放或全部。 | <kbd>I</kbd> |
| Delete Keyframe | 删除这些骨骼在当前帧的关键帧。可以用 <kbd>Ctrl+Z</kbd> 撤销。 | 菜单 |
| Clear Keyframe | 删除这些骨骼在活动动画中的所有关键帧。可以用 <kbd>Ctrl+Z</kbd> 撤销。 | 菜单 |
| Reset Pose | 所选骨骼（如果没有选中任何骨骼，则为全部骨骼）回到它们在 Edit Mode 中的位置、旋转和缩放。可以用 <kbd>Ctrl+Z</kbd> 撤销。 | 菜单 |
| Motion Trail | 在视图中绘制动画对象经过的路径。 | 菜单 |

动画有自己的教程和章节：[Timeline](linea-de-tiempo.md)。

## Object

![Object 按钮](../tutoriales/img/boton-vista3d-objeto.webp) 可以对所选内容执行的操作。它和在视图中右键单击打开的菜单相同（在触屏上则是按住一根手指不动）。选中了东西时才会出现。

![Object 菜单：Transform、Set Origin、Apply、Snap、Cut、Copy、Paste、Duplicate Objects、Duplicate Linked、Join、Set Parent、Clear Parent、Delete 以及预制体相关选项](../tutoriales/img/vista-3d-menu-objeto.webp)

每个选项的说明见 [Object Mode](modo-objeto.md)。在 Edit Mode 中，这个按钮会变成所选元素类型（Vertex、Edge 或 Face）的菜单；在 Pose Mode 中变成 Pose 菜单；编辑骨架时变成 Armature 菜单。

## Overlays

![Overlays 按钮](../tutoriales/img/boton-vista3d-superposiciones.webp) **Overlays**（叠加层）是绘制在场景之上、辅助工作的元素，不会出现在渲染结果中：地面、坐标轴、Gizmo、3D 光标。可以在这里一次性或逐个开启和关闭它们。

![Overlays 菜单：Show Overlays、Floor、X Axis、Y Axis、Origins、Gizmo、Objects、3D Cursor、Relationship Lines、Normals、Statistics、X-Ray 和 Black Letterbox](../tutoriales/img/vista-3d-menu-superposiciones.webp)

| 选项 | 作用 | 快捷键 |
|---|---|---|
| Show Overlays | 一次性开启或关闭所有辅助元素。 | <kbd>K</kbd> |
| Floor、X Axis、Y Axis、Origins、3D Cursor、Relationship Lines | 分别控制每种辅助元素。关系线把每个子级和它的父级连接起来。 | 菜单 |
| Gizmo | 移动、旋转和缩放的 Gizmo。在 Nokia N95 上默认关闭。 | 菜单 |
| Objects | 绘制哪些类型的对象：骨架、灯光、相机、空物体、粒子、曲线、碰撞盒（hitbox）。 | 菜单 |
| Normals | 绘制顶点或面的法线，并可设置其大小。 | 菜单 |
| Statistics | 每秒帧数、顶点数、面数、绘制调用次数和耗时。 | 菜单 |
| X-Ray | 在 Edit Mode 中，网格显示为半透明，可以选中背面的元素。 | 菜单 |
| Black Letterbox | 通过与屏幕宽高比不同的相机观看时，遮住镜头之外的部分。 | 菜单 |

## Render

![Render 按钮](../tutoriales/img/boton-vista3d-render.webp) **着色模式**：场景在视图中的绘制方式。它不会改变项目，只改变你工作时看到的样子。

![Render 菜单：Render Preview、Material Preview、Solid Preview、Wireframe Preview、ZBuffer Preview、Normal View、Alpha Preview、Motion vectors 和 Wireframe: Only Visible](../tutoriales/img/vista-3d-menu-render.webp)

![Leon 依次切换各种着色模式](../tutoriales/img/vista-3d-sombreado.webp)

| 模式 | 显示效果 |
|---|---|
| Render Preview | 使用场景中的灯光。如果开启了光线追踪，会逐步显示追踪出的图像。 |
| Material Preview | 默认模式：从视图方向打一盏中性灯光来显示材质和纹理，不依赖场景中的灯光。 |
| Solid Preview | 全部显示为灰色，用来看形状。 |
| Wireframe Preview | 只显示边。开启 **Wireframe: Only Visible** 时，只显示正面可见部分的边。 |
| ZBuffer Preview | 深度：近处亮，远处暗。 |
| Normal View | 用颜色显示法线。 |
| Alpha Preview | 用黑白显示透明度。 |
| Motion vectors | 每个像素自上一帧以来移动了多少（红 = X，绿 = Y）。与渲染的 `_vectores` 通道是同一张图。 |

![PC](../tutoriales/img/icono-pc.svg) <kbd>Z</kbd> 切换到下一个模式。![Nokia](../tutoriales/img/icono-nokia.svg) 在 N95 上按 <kbd>6</kbd>。

## 场景

![场景按钮](../tutoriales/img/boton-vista3d-escena.webp) 当前正在编辑的内容的名称：项目中的一个**场景**、**游戏**或**预制体**。点击它可以选择另一个、新建一个，或者把场景转换为游戏。

![场景菜单：当前场景、New Scene、New Game、New Prefab 和 Convert to Game](../tutoriales/img/vista-3d-menu-escena.webp)

场景、游戏和预制体的说明见[项目](proyecto.md)。

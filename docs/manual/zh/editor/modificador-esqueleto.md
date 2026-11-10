# Armature

**Armature** 修改器让网格跟随骨架的骨骼：当你为骨架摆姿势或制作动画时，网格会随之弯曲。正是它把一个静止的模型变成会走、会跳、会挥手的角色。

![Leon 网格完整的 Modifiers 选项卡，带有它的 Armature 修改器](../tutoriales/img/modificador-esqueleto.webp)

## 如何添加

1. 选中网格。
2. 在 Properties 中，打开 **Modifiers** 选项卡（扳手图标）。
3. 点击 **Add**，选择 **Armature**。

它在列表中以内部名称 **Armature** 显示，下方会展开一张包含其参数的卡片。和所有修改器一样，它不会破坏网格：静止姿势保持不变。你可以关闭它，用 **Remove** 移除它，或者用 **Apply Modifier** 烘焙它。

导入带骨架的 FBX 或 glTF 时，这个修改器已经添加好，并指向对应的骨架。

## 参数

| 参数 | 作用 |
|---|---|
| Display in Viewport | 关闭后网格不会变形：保持静止姿势。 |
| Display in Edit Mode | 关闭后，编辑顶点时网格保持静止，不变形。 |
| Target | 驱动网格的骨架。列表只显示场景中的骨架。设为 **None** 时不变形。 |
| Optimize Vertex Groups | 让每个顶点只保留一根骨骼，即对它影响最大的那根，并赋予完整的权重。由于中间的权重会丢失，执行前会要求确认。 |
| Apply Modifier | 把当前帧的姿势烘焙到网格中并移除修改器：网格会定格在这个姿势。骨架不受影响。此操作可以撤销。 |

## 网格如何知道由哪根骨骼驱动

靠的是**顶点组**。每个组都和**某根骨骼同名**，并指明这根骨骼驱动哪些顶点、影响多少：即**权重**，从 0（完全不影响）到 1（完全影响）。一个顶点可以属于多个组；比如在肘部，一半属于上臂，一半属于前臂，这样弯曲起来才平滑。

这些组位于 **3D Mesh** 选项卡的 **Vertex Groups** 卡片中：

| 按钮 | 作用 |
|---|---|
| Add Vertex Group / Rename | 创建一个组并为它命名。名称必须和骨骼的名称一字不差。 |
| Assign / Remove | 在 Edit Mode 中，把选中的顶点加入组（权重为 1），或把它们移出组。 |
| Select / Deselect | 显示这个组包含哪些顶点。 |

要精细调整权重，例如肩部和膝部的过渡，请使用 [Weight Paint](pintura.md#weight-paint)：它为选中的组绘制权重，从蓝色（不影响）到红色（完全影响）。

## 分步操作

1. 创建骨架（**Add → Armature**），并把骨骼摆放到模型内部（参见[骨架](esqueletos.md)）。
2. 给网格添加 **Armature** 修改器，并在 **Target** 中选择这个骨架。
3. 为每根骨骼创建一个同名的组，然后分配或绘制权重。
4. 选中骨架，切换到 **Pose Mode**，移动一根骨骼：网格会跟着动。

## 值得了解的事项

- Armature 在列表中的位置不会带来任何影响：它不和其他修改器一起计算，而是在每一帧绘制网格时使其变形。
- 它变形的是原始网格。如果还有其他已开启、会生成几何体的修改器（[Mirror](modificador-espejo.md)、[Array](modificador-array.md)、[Subdivision Surface](modificador-subdivision.md)、[Screw](modificador-screw.md)、[Boolean](modificador-booleano.md)），你看到的是生成出来的网格，它不会跟随骨骼。请在制作动画之前先应用它们。
- 应用其中任意一个修改器（Mirror 除外）都会清空整个列表，Armature 也会被移除。顶点组会保留下来：重新添加 Armature，并再次选择 Target 即可。

![Nokia](../tutoriales/img/icono-nokia.svg) 在 Nokia N95 上，每一帧都要让网格变形，这是有开销的，而且影响每个顶点的骨骼越多，开销越大。**Optimize Vertex Groups** 让每个顶点只保留一根骨骼，蒙皮会快很多；代价是关节弯曲时会显得更生硬。请在最后、权重都调好之后再这样做。

返回 [3D Mesh](objeto-malla.md)

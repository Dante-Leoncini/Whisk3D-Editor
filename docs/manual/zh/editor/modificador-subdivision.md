# Subdivision Surface

**Subdivision Surface** 修改器把每个面分割成更小的面，并且默认会让形状变圆滑。它让你可以用很少的多边形建模（编辑起来快，资源占用也小），同时看到平滑的结果：一个立方体会变得接近球体，一个低多边形角色会变得线条圆润。

![完整的 Modifiers 选项卡，立方体上加了 Subdivision Surface 修改器](../tutoriales/img/modificador-subdivision.webp)

## 如何添加

1. 选中网格。
2. 在 Properties 中，打开 **Modifiers** 选项卡（扳手图标）。
3. 点击 **Add**，选择 **Subdivision Surface**。

它在列表中以内部名称 **Subdivision Surface** 显示，下方会展开一张包含其参数的卡片。和所有修改器一样，它不会破坏网格：你可以关闭它，用 **Move Up** 和 **Move Down** 调整它的位置，用 **Remove** 移除它，或者用 **Apply Modifier** 烘焙它。

## 参数

| 参数 | 作用 |
|---|---|
| Display in Viewport | 关闭后不细分：视图中和渲染时都不会细分。 |
| Display in Edit Mode | 关闭后，编辑时显示的是未细分的网格，编辑起来更流畅；回到 Object Mode 后又会显示为平滑。 |
| Simple | 关闭时（默认），每一级都会平滑形状、让棱角变圆。开启时，只分割面而不移动顶点：形状不变，但顶点更多，例如可以用来更细致地绘制顶点颜色。 |
| Levels Viewport | 在视图中细分多少次，范围为 0 到 6（默认为 1）。每一级都会让面数乘以 4：一个有 6 个面的立方体，1 级时变成 24 个面，2 级时 96 个，3 级时 384 个。为 0 时不细分。 |
| Render | 执行 **Render Image** 或 **Render Animation** 时使用的级别（默认为 2）。这样视图保持轻快，最终图像则更加平滑。 |
| Apply Modifier | 按 **Levels Viewport**（而不是 Render 的级别）把细分烘焙到网格中。见下面的说明。 |

## 如何配合细分工作

- 四边形面的效果最好。一个三角形会被分成 3 个四边形，一个五边形面会被分成 5 个。
- 细分不会改变着色：平面着色的网格看起来依然是一个个平面。要让它看起来光滑，请在 Edit Mode 中选中这些面，然后使用 Face 菜单 → **Shade Smooth**。
- 要让某条边缘更分明，用 **Loop Cut and Slide** 在它附近加一道切线（参见 [Edit Mode](modo-edicion.md)）：离边缘越近，边缘就越硬。
- 在未封闭的网格上，开放的边缘会沿着边缘本身进行平滑。

## 顺序很重要

修改器按从上到下的顺序生效。把 [Mirror](modificador-espejo.md) 放在 Subdivision **上方**：这样整个模型会一次性平滑。如果 Mirror 在下方，每一半会分别平滑，中间会留下一道折痕。[Array](modificador-array.md) 也是一样：用 Merge 焊接的副本会一起平滑。

![Nokia](../tutoriales/img/icono-nokia.svg) 在 Nokia N95 上，每一级都很吃性能：1 级时要绘制的面就已经是原来的四倍，2 级时是十六倍。把 Levels Viewport 保持在 0 或 1，建模时关闭 Display in Edit Mode，把高级别留给 Render。

> [!NOTE]
> 对 Subdivision 执行 Apply Modifier 会烘焙列表中**所有**已开启的修改器，而不只是这一个，并清空列表：已关闭的修改器，以及不生成几何体的修改器（例如 [Armature](modificador-esqueleto.md)），也会一并移除。此操作可以撤销。

返回 [3D Mesh](objeto-malla.md)

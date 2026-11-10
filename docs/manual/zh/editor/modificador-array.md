# Array

**Array** 修改器把网格复制成一排，每个副本都相对前一个错开一段距离。凡是重复的东西都能用它：栅栏的立柱、楼梯的台阶、走廊的柱子，或者链条的链环。你只需建模一个部件，Array 会搭出其余部分；修改这个部件，所有副本都会跟着改变。

![完整的 Modifiers 选项卡，立方体上加了 Array 修改器](../tutoriales/img/modificador-array.webp)

## 如何添加

1. 选中网格。
2. 在 Properties 中，打开 **Modifiers** 选项卡（扳手图标）。
3. 点击 **Add**，选择 **Array**。

它在列表中显示为 **Array**，下方会展开一张包含其参数的卡片。和所有修改器一样，它不会破坏网格：你可以关闭它，用 **Move Up** 和 **Move Down** 调整它的位置，用 **Remove** 移除它，或者用 **Apply Modifier** 烘焙它。

## 参数

| 参数 | 作用 |
|---|---|
| Display in Viewport | 关闭后不计算副本：视图中和渲染时都不会出现。 |
| Display in Edit Mode | 关闭后，编辑时只显示原始部件。 |
| Count | 部件总数，包括原始部件（默认为 2）。设为 1 时只剩原始部件。 |
| Relative Offset | 按部件的**尺寸**错开每个副本。默认开启，Factor X 为 1：每个副本紧贴前一个，既没有空隙也不重叠。 |
| Factor X / Y / Z | 每个副本在该轴上错开自身尺寸的几倍：1 = 紧贴，2 = 中间留出一个部件大小的空隙，0.5 = 重叠一半。Y 是深度，Z 是高度，与 Object 选项卡中相同。只有开启 Relative Offset 时才会显示。 |
| Constant Offset | 按固定距离错开每个副本，与部件的尺寸无关。默认关闭。如果两种偏移都开启，它们会叠加。 |
| Distance X / Y / Z | 每个副本在各轴上错开的米数。只有开启 Constant Offset 时才会显示。 |
| Object Offset | 场景中的一个对象（用 Empty 最方便），它让每个副本比前一个多移动、多旋转、多缩放一点。见下文。设为 **None** 时不使用。 |
| Merge | 把相邻副本之间贴在一起的顶点焊接起来，使它们形成一个连续的表面：得到一整条栏杆，而不是一段段分开的栏杆。默认关闭。 |
| Merge Distance | 两个顶点相距多近才会被焊接（默认为 0.001 m）。只有开启 Merge 时才会显示。 |
| Apply Modifier | 把副本烘焙到网格中。见下面的说明。 |

## 示例

- **栅栏：**一根立柱（一个又窄又高的 Cube），Count 设为 10，Factor X 设为 3：十根立柱，每两根之间隔着两根立柱宽的空隙。
- **楼梯：**一级台阶（一个扁平的 Cube），Factor X 设为 0，Factor Y 设为 1，Factor Z 设为 1：每级台阶都比前一级往后一步、往上一步。

## Object Offset

选定一个对象后，每个副本都会重复该对象**相对于网格的位移、旋转和缩放**，并不断累积：第二个副本做一次，第三个做两次，依此类推。

- 如果 Empty 和网格在同一位置，并绕竖直轴旋转了 30°，每个副本就比前一个多转 30°：12 个副本正好转一整圈。
- 如果它还稍微高一点，每个副本就再升高一点：成了旋转楼梯。
- 如果它缩放为 0.9，每个副本都比前一个小一点：成了向内收拢的螺旋。

当 Empty 和网格在同一位置时，副本绕网格的原点旋转。要让它们排成一个圆，而不是在原地自转，请在 Edit Mode 中把部件移到远离原点的地方，并关闭 Relative Offset。移动或旋转该对象时，副本会立即更新。

## 顺序很重要

Relative Offset 测量的是从上方修改器**传下来的**网格：如果上方有 [Mirror](modificador-espejo.md)，它测量的是已经镜像后的对象。如果下方有 [Subdivision Surface](modificador-subdivision.md)，用 Merge 焊接的副本会作为一个整体被平滑；不用 Merge 时，每个副本分别平滑。

> [!NOTE]
> 对 Array 执行 Apply Modifier 会烘焙列表中**所有**已开启的修改器，而不只是 Array（使用视图中的数值），并清空列表：已关闭的修改器，以及不生成几何体的修改器（例如 [Armature](modificador-esqueleto.md)），也会一并移除。此操作可以撤销。

返回 [3D Mesh](objeto-malla.md)

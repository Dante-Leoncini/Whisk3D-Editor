# 3D Mesh

**3D 网格**是有形状的对象：它由顶点、边和面构成，是你建模、绘制和制作动画的对象。一个立方体、一个角色或整个场景都是网格。本文是其属性的参考；如何创建、移动对象以及设置父级，请参阅 [Object Mode](../tutoriales/modo-objeto.md) 教程。

## 选项卡

选中网格时，Properties 会显示下面这些选项卡，此外还有始终存在的 File / Render 和 Animation：

| 选项卡 | 内容 |
|---|---|
| Object | 所有对象共有的内容：位置、旋转、缩放、名称和可见性。 |
| Material | 网格的各个部分，以及每个部分的材质。 |
| 3D Mesh | 网格的数据：使用哪个网格、它的顶点组、UV 贴图和颜色层。 |
| Modifiers | 修改器，用来改变形状而不破坏它。 |
| Transform | 仅在 Edit Mode 中：所选内容的中心，可以用数字输入。 |
| Constraints | 参见 [Constraints](restricciones.md)。 |
| Scripts | 对象的 Lua 脚本。参见 [Whisk3D 中的 Lua](../lua/README.md)。 |

## Object 选项卡

![完整的 Object 选项卡，选中了 Leon 的一个网格](../tutoriales/img/objeto-malla-objeto.webp)

所有类型的对象都一样：相机、灯光、骨架和声音也有这个选项卡。

### Transform

| 属性 | 作用 |
|---|---|
| Location X, Y, Z ◆ | 对象所在的位置，单位为米。Z 是高度。 |
| Mode | 旋转的表示方式：**XYZ Euler**（三个以度为单位的角度，最容易读懂）、**Quaternion (WXYZ)** 或 **Axis Angle**。选择后两种时会出现 **Rotation W**。更改模式不会旋转对象：只是改变数字的显示方式。 |
| Rotation X, Y, Z ◆ | 绕各轴的旋转。 |
| Scale X, Y, Z ◆ | 各轴上的大小：1 是原始大小，2 是两倍，0.5 是一半。 |
| Name | 对象的名称，与 Outliner 中显示的相同。名称不能重复：如果已经存在，会在后面加上一个数字（`.001`）。 |
| Visible ◆ | 对象是否可见。和 Outliner 中的眼睛图标是同一个设置。 |
| Renderable ◆ | 是否出现在渲染结果中。取消勾选后，它在 3D Viewport 中仍然可见，但不会出现在图像或视频中。 |
| Relationship Lines | 连接对象与其父级的线。当一个对象有很多子级、这些线挡住视图时，可以把它关掉。 |
| Static | 标记该对象永远不会移动。只有在使用栅格法的 Culling 对象内部才有意义：静态对象只会被放进它所在的单元格一次，而不是每一帧都重新测量。 |

### Animation 和 Palette

- **Animation：**对象自身的动画（例如顶点动画）及其片段。参见 [Timeline](linea-de-tiempo.md)。
- **Palette：**对象使用哪个调色板。设为 **Same as parent**（默认）时继承父级的调色板；选择项目中的某个调色板，会把它以及挂在它下面的所有对象重新上色。

## Material 选项卡

![完整的 Material 选项卡：网格的各个部分，以及所选部分的材质](../tutoriales/img/objeto-malla-material.webp)

### Mesh Parts

一个网格可以分成多个**部分**，每个部分有自己的材质。例如一栋房子，可以把墙放在一个部分，窗户的玻璃放在另一个部分。

| 属性 | 作用 |
|---|---|
| 列表 | 网格的各个部分。选中的部分会显示在下方的 Material 卡片中。 |
| New Mesh Part | 创建一个空的部分。 |
| Assign | 把在 Edit Mode 中选中的面移到所选部分。 |
| Select / Deselect | 选中或取消选中该部分的面。必要时，Select 会切换到 Edit Mode 来显示它们。 |
| Delete / Rename | 删除该部分（它的面会归入上一个部分）或为它重命名。有多个部分时才会出现 Delete。 |
| Move Up / Move Down | 调整顺序。各部分按这个顺序绘制：最好把透明的部分放在最后。 |

### Material

材质决定了部分的外观：它的颜色、纹理，以及光线照在上面的效果。默认材质不能编辑：要修改任何内容，先用上方的选择器创建一个材质（**New Material**），这个选择器也可以用来选择项目中的其他材质。最常用的选项：

| 属性 | 作用 |
|---|---|
| Rename Material | 为材质重命名。 |
| Color | 该部分的纹理、**Base Color** 色块，以及纹理使用的 UV 贴图。 |
| Metallic、Roughness、Emission、Normal、Alpha | 每一项都可以是数值或纹理（**Number / Texture** 按钮）。Alpha 是透明度：小于 1，或者使用 Alpha 纹理时，材质就会变成透明的。 |
| Alpha Test | 直接裁掉 Alpha 值低的部分，不做混合：适用于栏杆、铁丝网或树叶。设为 0 时关闭。 |
| Transmission、IOR | 玻璃和折射。只有开启 **Ray Tracing**（File / Render 选项卡）时才有效。 |
| Lighting | 场景中的灯光是否照亮它。关闭后颜色看起来是平的，始终不变。如果图形后端支持着色器，会出现 **Shading**：Per vertex 或 Per pixel。 |
| Vertex Color | 使用在网格上绘制的颜色（参见 [Vertex Paint](pintura.md#vertex-paint)），而不是基础色。 |
| Filtering / Repeat | 近看时让纹理平滑（取消勾选时能看到像素）/ 像瓷砖一样重复纹理。 |
| Reflection | 环境反射，像镀铬一样。 |
| Texture Layers | 在第一张纹理之上最多再叠加四张纹理，每张都有自己的 UV 贴图和混合方式。 |
| Lines / Line Width | 绘制该部分的边，线宽以像素为单位。 |
| Culling | 不绘制背面。 |
| Test、Depth Bias、Pass Order | 高级深度选项，用于贴花以及叠放在其他物体上的东西。 |
| Advanced: custom shader | 一个按需绘制材质的文件：参见[手写着色器](shaders.md)。 |

如果对象带有物理（Add → Physics），它的 **Physics** 卡片也会出现在这个选项卡中。

## 3D Mesh 选项卡

![完整的 3D Mesh 选项卡：使用的网格、编辑（带有 Lock Normals 复选框）、顶点组（带有每根骨骼的名称）、UV 贴图和颜色](../tutoriales/img/objeto-malla-propiedades.webp)

### 3D Mesh

对象的形状是项目中的一个资源，多个对象可以**共享**同一个网格（Duplicate Linked 就是这样做的）：编辑其中一个，所有的都会改变。

| 属性 | 作用 |
|---|---|
| 选择器 | 对象使用的网格（有未保存的更改时带有 `*`）。可以在这里选择项目中的另一个网格，或选择 **New Copy**。 |
| Users | 场景中有多少个对象在使用这个网格。 |
| Make Single User | 网格被共享时才会出现：为对象创建它自己的副本，这样编辑时不会影响其他对象。 |

如果网格来自某个库，会显示提示“It belongs to a library (read-only): open the library to edit it”，面板中会改变它的选项也会消失。

### Editing

**Delete editing data** 适用于不会再建模的场景：它会去掉网格中供 Edit Mode 使用的数据，这样加载更快、占用的内存也更少（这在 Nokia N95 上很重要）。在此期间无法进入 Edit Mode。**Convert to editable mesh** 会重新计算这些数据。

勾选 **Lock Normals** 复选框后，在 Edit Mode 中移动顶点时，这个网格的法线不会重新计算。这样可以保留导入模型自带的法线，或者手动修好的法线。它默认关闭，每个网格各有自己的设置。

### Vertex Groups

**顶点组**是一份顶点列表，每个顶点带有一个权重。[骨架](objeto-esqueleto.md)就是靠它来知道每根骨骼驱动哪些顶点的：组的名称和骨骼的名称相同。权重用 [Weight Paint](pintura.md#weight-paint) 绘制。

| 属性 | 作用 |
|---|---|
| Add Vertex Group / Rename | 创建一个组，或为它重命名。 |
| Assign / Remove | 把在 Edit Mode 中选中的顶点加入组（权重为 1），或把它们移出组。 |
| Select / Deselect | 选中或取消选中组中的顶点，用来查看它包含哪些顶点。 |
| Delete / Move Up / Move Down | 删除该组，或在列表中移动它。 |

**UV Groups** 的用法相同，但它的权重是给 [UV Editor](editor-uv.md) 中的 2D 骨架用的。

### UV Maps

**UV 贴图**决定纹理如何贴在网格上。一个网格可以有多个 UV 贴图：例如，一个用于颜色纹理，另一个用于以不同方式重复的纹理层。材质中的每张纹理各自选择使用哪一个。

**Add UV Map** 创建一个，**Rename** 为它重命名；有多个时会出现 Delete、Move Up 和 Move Down。

### Color

**颜色层**保存用 Vertex Paint 在网格上绘制的颜色。**Add Color Layer** 创建一个；**Color Mode** 在 **Per-Vertex**（共享一个顶点的面也共享它的颜色）和 **Per-Corner**（每个面可以有自己的颜色，用于硬边）之间切换。Rename、Delete、Move Up 和 Move Down 与 UV 贴图中的相同。

## Modifiers 选项卡

![完整的 Modifiers 选项卡，立方体上加了 Subdivision Surface](../tutoriales/img/objeto-malla-modificadores.webp)

**修改器**改变网格但不会破坏它：你可以随时关闭、重新排序或删除它，原始网格始终保留着。

**Modifiers** 卡片包含列表（按从上到下的顺序生效）、**Add**（列出各种类型的菜单）、**Remove** 和 **Move Up / Move Down**。下方是所选修改器的卡片（以修改器名称为标题），其中有 **Display in Viewport**（关闭后不计算）、**Display in Edit Mode**（关闭后编辑更快，退出时重新计算）、它自己的属性，以及 **Apply Modifier**：把修改器烘焙到网格中，并从列表中移除。

每个修改器都有自己的文章：

| 修改器 | 用途 |
|---|---|
| [Screw](modificador-screw.md) | 让轮廓绕某个轴旋转：螺丝、花瓶、弹簧。 |
| [Mirror](modificador-espejo.md) | 镜像网格：只建模一半，另一半自动生成。 |
| [Array](modificador-array.md) | 成排复制：楼梯、栅栏、链条。 |
| [Subdivision Surface](modificador-subdivision.md) | 通过细分让网格更平滑。 |
| [Boolean](modificador-booleano.md) | 与另一个对象求并集、交集或差集。 |
| [Culling](modificador-culling.md) | 优化：不绘制从相机看不到的部分。 |
| [Armature](modificador-esqueleto.md) | 用骨架使网格变形。 |

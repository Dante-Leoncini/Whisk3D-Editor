# Armature

**骨架**是由**骨骼**组成的对象，用来为角色和会弯曲的东西制作动画。骨骼移动时，网格会跟着变形：要让一条手臂动起来，只需旋转几根骨骼，而不必移动成百上千个顶点。本文是其属性的参考；如何添加和移动对象，请参阅 [Object Mode](../tutoriales/modo-objeto.md) 教程。

Add → Armature 会创建一个只有一根骨骼的骨架，你可以接着往上添加更多骨骼。在 3D Viewport 中，每根骨骼都绘制为一条位于所有内容之上的蓝线，这样即使它在网格内部也能看到。

![完整的 Armature 选项卡，显示 Leon 的骨架：它的动画和骨骼](../tutoriales/img/objeto-esqueleto-propiedades.webp)

## 静止姿势和姿势

每根骨骼有两种状态：

- **静止姿势：**搭建骨架时骨骼所放的位置。这是网格未变形时的姿态（例如一个张开双臂的角色）。
- **姿势：**骨骼在动画某一帧中的状态：手臂抬起、头转过去。摆姿势不会改变静止姿势：随时都可以回到静止姿势。

## 模式

骨架有三种模式，在 3D Viewport 的 **Mode** 菜单中选择：

| 模式 | 用途 |
|---|---|
| Object Mode | 像任何对象一样，移动、旋转和缩放整个骨架。 |
| Edit Mode | 搭建骨架：从端点创建骨骼、移动骨骼、设置父子关系。会改变静止姿势。 |
| Pose Mode | 摆姿势和制作动画：旋转骨骼并保存关键帧。 |

![PC](../tutoriales/img/icono-pc.svg) <kbd>Tab</kbd> 在 Object Mode 和 Pose Mode 之间切换（在 Edit Mode 中按则切换到 Pose Mode）。

每种模式的工具和快捷键，请参阅手册中的[骨架](esqueletos.md)。

## 选项卡

| 选项卡 | 内容 |
|---|---|
| Object | 整个骨架的位置、旋转、缩放、名称和可见性，和所有对象一样：参见 [3D Mesh](objeto-malla.md#object-选项卡)。 |
| Armature | 动画和骨骼：即下面的两张卡片。 |
| Constraints | 参见 [Constraints](restricciones.md)。 |
| Scripts | 对象的 Lua 脚本。参见 [Whisk3D 中的 Lua](../lua/README.md)。 |

## Armature 选项卡

### Animation

骨架的动画（**片段**）：走、跑、跳。每个片段保存骨骼随时间变化的姿势。

| 属性 | 作用 |
|---|---|
| 列表 | 骨架的片段。选中的片段就是被编辑和播放的那个。 |
| New Animation | 创建一个空片段，骨骼处于静止姿势。 |
| Duplicate | 复制所选片段，用来制作一个变体而不改动原片段（有片段时才会出现）。 |
| Rename | 为片段重命名。同一个骨架中不能重名。 |
| Delete / Move Up / Move Down | 删除片段，或在列表中移动它。 |

如何制作动画：[Timeline](linea-de-tiempo.md)。

### Bones

骨架的骨骼列表。选中一根骨骼后，卡片会显示它的数据：

| 属性 | 作用 |
|---|---|
| Name | 骨骼的名称。更改名称时，骨架所属网格中对应的顶点组也会一起重命名，所以网格仍然会跟随它。 |
| Parent | 它挂在哪根骨骼下面（如果是根骨骼，则为 **None**）。移动父骨骼时，它的子骨骼也会跟着移动。菜单中不会列出这根骨骼本身，也不会列出挂在它下面的骨骼，以免形成循环。 |
| Connected | 只有当它有父骨骼时才会出现。勾选后，骨骼的起点会焊接在父骨骼的末端：移动其中一个，另一个也会动。如果骨骼原本是分开的，勾选后它会被移过去贴上。取消勾选后，它仍然是子骨骼，但端点是松开的（会显示一条虚线）。 |
| Pos X, Y, Z | 骨骼在当前姿势中的位置，相对于其父骨骼测量。 |
| Rot X, Y, Z | 它在当前姿势中旋转了多少度。 |
| Scale X, Y, Z | 它在当前姿势中的大小：1 是原始大小。 |

Pos、Rot 和 Scale 编辑的是**姿势**，修改会立即显示，但在插入关键帧之前（在 Pose Mode 中使用 Insert Keyframe），不会保存到动画中。

## 让网格跟随骨架

要让网格随骨骼变形，需要两样东西：

1. 网格上的 **Armature 修改器**（Properties → Modifiers 选项卡 → Add → Armature），并在 **Target** 中选择这个骨架。参见 [Armature（修改器）](modificador-esqueleto.md)。
2. 以骨骼名称命名的**顶点组**（Properties → 3D Mesh 选项卡 → [Vertex Groups](objeto-malla.md#vertex-groups)）。每个组指明其骨骼驱动哪些顶点、影响多少；权重用 [Weight Paint](pintura.md#weight-paint) 绘制。

对于刚性物体，例如手中的剑或一顶帽子，这些都不需要：只要一个 [Child Of](restricciones.md#child-of) 约束就够了，以骨架为来源，并选择手部或头部的骨骼。这比让网格变形轻量得多，在 Nokia N95 上差别很明显。

## 导入角色

Add → Imports → glTF 或 GLB 可以一次性导入一个绑定好的角色。以 Leon 为例：

- **网格**，作为骨架的子级；
- **骨架**，名称为文件名加上结尾的 `_rig`；
- **顶点组**，每根驱动网格的骨骼对应一个，带有文件中原有的权重；
- **动画**，作为 Animation 卡片中的片段；
- 如果带有动画，网格上还会已经加好 **Armature 修改器**，并指向它的骨架。

从列表中选择一个片段，在 Timeline 中点击 Play，角色就会动起来。

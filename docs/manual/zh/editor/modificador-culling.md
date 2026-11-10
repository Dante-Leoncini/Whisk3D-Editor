# Culling

**Culling** 修改器只绘制网格中从相机所在区域能看到的三角形。它使用预先计算好的可见性列表，每个区域一份，就像 PlayStation 1 的游戏那样（**PVS**，*潜在可见集*）。它适用于大型场景：整个关卡中，只绘制从你所在的走廊能看到的部分。

![Nokia](../tutoriales/img/icono-nokia.svg) 它是为 Nokia N95 设计的，在那上面每少画一个三角形，就多一分速度。切换区域几乎没有开销：列表只构建一次，从一个区域进入另一个区域时，只是换用另一份列表。不会有任何逐帧计算。

![完整的 Modifiers 选项卡，立方体上加了 Culling 修改器](../tutoriales/img/modificador-culling.webp)

## 如何添加

1. 选中场景的网格。
2. 在 Properties 中，打开 **Modifiers** 选项卡（扳手图标）。
3. 点击 **Add**，选择 **Culling**。

它在列表中以内部名称 **Oclusion** 显示，下方会展开一张包含其参数的卡片。它不会改动网格：只是选择绘制哪些三角形，所以网格保持完整，仍然可以编辑。因此它没有 Apply Modifier。

不要把它和 3D Viewport 的 Add 菜单中的 **Culling** 对象混淆：后者处理的是整个对象，而不是网格中的三角形。

## 数据从哪里来

这个修改器并不计算哪些部分可见：它从模型附带的一个文件中读取。对于从 `nivel.obj` 导入的网格，它会在同一文件夹中查找 `nivel.pvs.json`：

```json
{ "sectores": 2, "tris": [[0, 1], [2, 3]] }
```

`sectores` 是区域的数量，`tris` 为每个区域列出要绘制的三角形，按 `.obj` 中面的顺序从 0 开始编号。这个文件通常来自原版游戏或单独的工具。每个区域是一个**节点**：节点 1 是第一份列表，节点 0 绘制完整的网格。

## 参数

| 参数 | 作用 |
|---|---|
| Display in Viewport | 关闭后绘制完整的网格。 |
| Display in Edit Mode | 不起任何作用：在 Edit Mode 中，网格总是完整绘制。 |
| Method | **Triangles (PVS)** 使用 `.pvs.json` 中的列表。**BSP (pending)** 显示为灰色：目前还不能选择。 |
| Recalculate | 重新读取 `.pvs.json`，例如在重新生成它之后。会提示“PVS recalculated”，如果找不到文件，则提示“PVS: <model>.pvs.json is missing next to the source .obj”。 |
| （状态行） | 位于 Recalculate 下方：“4 nodes \| active node: 2”；没有文件时显示“no data (whole mesh)”；或者显示“sidecar not loaded (Recalculate)”。 |
| Path | 场景中的一条路径（**Add → Path**），例如相机移动的轨道。路径的每个节点是一个区域，引擎会自动选择离视点最近的节点。设为 **None** 时，区域需要手动设置或通过脚本设置。 |
| Only active camera | 开启时（默认），场景播放期间由活动相机决定；暂停时不会自动选择，节点需要手动设置。关闭时由视口的视图决定：你可以在场景中自由飞行，观察三角形如何出现和消失。 |
| Branches | 只有当路径有多条分支时才会出现，例如主路线和一条奖励路线。它选择哪些分支参与查找最近的节点；开启的分支显示为绿色。按钮上会显示例如“Branches: 3 (all)”或“Branches: 2/3”。 |
| Current node | 当前正在绘制的区域；0 = 完整的网格。可以手动更改，逐个检查每个区域。当由 Path 自动选择节点时，引擎会覆盖这个值。 |

在脚本中，`setSector(objeto, n)` 用于更改活动节点（0 = 完整的网格）。参见[粒子、雾和可见性](../lua/api-particulas-niebla.md)。

## 值得了解的事项

- 网格处于 Edit Mode 时会完整绘制，方便你编辑所有部分。
- 如果列表中还有其他已开启、会生成几何体的修改器（[Mirror](modificador-espejo.md)、[Array](modificador-array.md)、[Subdivision Surface](modificador-subdivision.md)……），Culling 就不会生效，所有内容都会绘制：`.pvs.json` 中的三角形编号对应的是原始网格。
- 列表是按编号来指定三角形的。如果你编辑网格，添加或删除了面，编号就会改变，列表可能会指向别的三角形：Culling 适用于已经完成的场景，而不是还在建模中的场景。

返回 [3D Mesh](objeto-malla.md)

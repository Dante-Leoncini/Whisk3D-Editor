# 3D Viewport

3D Viewport 是查看和编辑场景的地方。它有多种**模式**：单击、按键和菜单的作用取决于当前模式。你可以用顶部栏的模式选择器（只有当活动对象是网格或骨架时才会出现）或 <kbd>Tab</kbd> 来切换模式。

| 模式 | 适用对象 | 用途 |
|---|---|---|
| [Object Mode](modo-objeto.md) | 全部 | 创建、移动、旋转、缩放、复制、设置父级和删除整个对象。 |
| [Edit Mode](modo-edicion.md) | 网格和曲线 | 编辑顶点、边和面（建模）。 |
| [骨架的 Edit Mode](esqueletos.md#骨架的-edit-mode) | 骨架 | 创建和摆放骨骼。 |
| [Pose Mode](esqueletos.md#pose-mode) | 骨架 | 为骨骼摆姿势并制作动画。 |
| [Weight Paint](pintura.md#weight-paint) | 仅限 3D 网格 | 绘制每根骨骼对每个顶点的影响程度。 |
| [Vertex Paint](pintura.md#vertex-paint) | 仅限 3D 网格 | 直接在顶点上绘制颜色。 |
| [Texture Paint](pintura.md#texture-paint) | 仅限 3D 网格 | 直接在对象表面上绘制纹理。 |

## 3D Viewport 的顶部栏

| 按钮 | 说明 |
|---|---|
| 模式选择器 | 切换模式（Object、Edit、Pose、Paint…）。 |
| Vertex / Edge / Face | 仅在 Edit Mode 中：选择哪种元素。 |
| Transform Pivot Point | 旋转和缩放围绕的中心：3D Cursor、Individual Origins、Median Point 或 Active Element。附带 Lock Normals 复选框。 |
| Orientation | 变换使用的坐标轴：Global、Local、Normal（面的方向）或 View。 |
| Snap | 吸附选项。启用时显示为绿色（<kbd>Shift+Tab</kbd>）。 |
| Proportional Editing | 移动某个元素时，附近的部分会平滑地跟着移动。启用时显示为绿色（<kbd>O</kbd>）。 |
| View | 相机、视角、取景、局部视图、透视、锁定环绕。 |
| Select | 全选、全不选、反选、框选、关联选择、循环和路径。 |
| Add（+） | 仅在 Object Mode 中：创建新对象。 |
| Mesh | 仅在 Edit Mode 中：各种建模操作。 |
| Save Texture（软盘图标） | 仅在 Texture Paint 中。有未保存的绘制内容时显示为绿色。 |
| Animation（菱形图标） | 插入、删除和清除关键帧；运动轨迹。 |
| Context | 当前模式的菜单：Object、Vertex/Edge/Face、Armature 或 Pose。 |
| Overlays（眼睛图标） | 要绘制哪些辅助元素：地面、坐标轴、Gizmo、3D 光标、法线、统计信息、X 射线。 |
| Render（相机图标） | 着色模式。 |
| Scene | 选择要编辑项目中的哪个场景、游戏或预制体，也可以新建。 |
| UV | 仅在 Edit Mode 中：展开和投影 UV。 |

## 多个模式共用的菜单

### Select

| 功能 | 说明 | 快捷键 |
|---|---|---|
| All | 全部选中。 | <kbd>A</kbd> |
| None | 取消全部选择。 | <kbd>Alt+A</kbd> |
| Invert | 已选中的变为未选中，反之亦然。 | <kbd>Ctrl+I</kbd> |
| Box Select | 从左往右拖（绿色）只选中完全位于框内的元素；从右往左拖（蓝色）只要碰到就会选中。按住 <kbd>Shift</kbd> 可加选。 | <kbd>B</kbd> |
| Show selection | 显示或隐藏所选内容的轮廓。 | 菜单 |

### Render（着色模式）

| 功能 | 说明 |
|---|---|
| Render Preview | 使用场景中的灯光。如果启用了光线追踪，会逐步显示追踪出的图像。 |
| Material Preview | 默认模式：从视图方向打一盏中性灯光，不依赖场景中的灯光。 |
| Solid / Wireframe / ZBuffer Preview | 灰色实体、只显示边，或显示深度。 |
| Normal View / Alpha Preview | 用颜色显示法线，或用黑白显示透明度。 |
| Motion vectors | 每个像素自上一帧以来在屏幕上移动了多少（红 = X，绿 = Y；淡紫 = 静止，橄榄绿 = 背景）。与渲染的 `_vectores` 通道是同一张图。 |
| Wireframe: Only Visible | 线框视图只显示真正被绘制出来的部分。 |

### Overlays

| 功能 | 说明 | 快捷键 |
|---|---|---|
| Show Overlays | 一次性开启或关闭所有视觉辅助元素。 | <kbd>K</kbd> |
| Floor、X Axis、Y Axis、Origins、3D Cursor、Relationship Lines | 分别控制每种辅助元素。 | 菜单 |
| Gizmo | 移动/旋转/缩放的 Gizmo。 | 菜单 |
| Objects | 显示或隐藏骨架、灯光、相机、空物体、粒子、曲线、碰撞盒（hitbox）。 | 菜单 |
| Normals | 绘制顶点法线、自定义法线或面法线，并可设置其大小。 | 菜单 |
| Statistics | FPS、顶点数、面数、绘制调用次数和耗时。 | 菜单 |
| X-Ray | 正在编辑的网格显示为半透明，可以选中背面的元素。 | 菜单 |

### Snap

| 功能 | 说明 | 快捷键 |
|---|---|---|
| Enable | 移动的元素会吸附到顶点、边、面或它们的中心。 | <kbd>Shift+Tab</kbd> |
| Snap Base 和 Snap Target | 被移动元素的哪个点用来吸附（Closest、Center、Median、Active），以及吸附到什么上。 | 菜单 |
| Face Project / Face Nearest | 用于重新拓扑：让顶点贴附在另一个表面上。 | 菜单 |
| Affect Move / Rotate / Scale | 吸附对哪些操作生效。 | 菜单 |
| 光标的 Snap menu | Selection to Grid、Selection to Cursor 或 Selection to Active；Cursor to Selected、Cursor to World Origin、Cursor to Grid 或 Cursor to Active。 | <kbd>Shift+S</kbd> |

### Proportional Editing

| 功能 | 说明 | 快捷键 |
|---|---|---|
| Enable | 移动、旋转或缩放时，相邻的部分会根据距离跟着动。 | <kbd>O</kbd> |
| Connected Only | 沿着边来计算距离：不会带动分离的部分。 | 菜单 |
| Falloff | 影响范围的形状：Smooth、Sphere、Root、Inverse Square、Sharp、Linear、Constant、Random。 | 菜单 |
| Proportional Size | 影响半径。变换过程中可以用滚轮或 <kbd>PageUp</kbd> / <kbd>PageDown</kbd> 调整。 | 菜单 / 滚轮 |

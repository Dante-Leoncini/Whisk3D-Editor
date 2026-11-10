# Object Mode

| 功能 | 说明 | 使用方法 |
|---|---|---|
| Select | 选择一个对象。在空白处单击可取消选择。 | 单击；<kbd>Shift</kbd>+单击可加选或减选 |
| Move / Rotate / Scale | 参见[移动、旋转和缩放](transformar.md)。 | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>、Gizmo |
| Add | 在 3D 光标处创建一个新对象（参见 [Add 菜单](#add-菜单)）。 | <kbd>Shift+A</kbd> 或 + 按钮 |
| Apply | 把位置、旋转、缩放或全部烘焙到网格中（对象回到 0, 0, 0，没有旋转，缩放为 1）。 | <kbd>Ctrl+A</kbd> |
| Set Origin | Geometry to Origin、Origin to Geometry、Origin to 3D Cursor。 | Object 菜单 |
| Snap | 把 3D 光标移到所选内容、活动对象、世界原点或栅格上，或者把所选内容移到光标、活动对象或栅格上。 | <kbd>Shift+S</kbd> 或 Object 菜单 → Snap |
| Duplicate Objects | 独立的副本；创建后会被直接抓取，可以马上移动。 | <kbd>Shift+D</kbd> |
| Duplicate Linked | 与原对象共享网格的副本（编辑其中一个，两个都会改变）。 | <kbd>Alt+D</kbd> |
| Join | 把所选的网格合并到活动对象中。 | <kbd>Ctrl+J</kbd> |
| Set Parent | 所选对象成为活动对象的子级（可以选择是否保持变换）。 | <kbd>Ctrl+P</kbd> |
| Clear Parent | 移除父级（可以选择是否保持位置）。 | <kbd>Alt+P</kbd> |
| Delete | 删除所选对象（会要求确认）。 | <kbd>X</kbd> 或 <kbd>Delete</kbd> |
| Hide / Show | 切换活动对象的可见性。 | <kbd>H</kbd> |
| Create Prefab / Unpack Prefab | 把所选内容转换成可复用的预制体，或把一个实例重新拆开。 | Object 菜单 |
| Insert Keyframe | 在当前帧保存位置、旋转和/或缩放（通道菜单）。<kbd>Shift+I</kbd> 不询问，直接保存全部。 | <kbd>I</kbd> |
| Delete Keyframe / Clear Keyframe | 删除当前帧的关键帧，或删除整个动画。 | Animation 菜单 |
| Motion Trail | 绘制动画对象经过的路径。 | Animation 菜单 |
| 切换模式 | 网格：Object ↔ Edit。骨架：Object ↔ Pose。曲线：编辑节点。 | <kbd>Tab</kbd> |

## Add 菜单

仅在 Object Mode 中可用。新建的内容会出现在 3D 光标处，并保持选中状态。基本体会打开一个面板，用来调整尺寸。

| 功能 | 说明 |
|---|---|
| Mesh | Plane、Cube、Circle、UV Sphere、Cone、Cylinder（可设置大小、半径、段数、平滑着色）。还有 Vertex（一个独立的点，用来开始建模）和 Reference（一个带图片的平面，用于描摹）。 |
| Empty | 没有网格的对象，用于分组或作为参考点。 |
| Camera / Light / Fog | 场景的相机、灯光和雾。 灯光有 **Type**：Point、Sun（平行光）或 Spot（锥形，在视图中绘制）；添加菜单也直接提供 Sun 和 Spot。 **Intensity** 让光更强（可动画）；**Distance**（米）是点光源或聚光灯能照到的距离，也是锥形的长度（0 = 不限；光线追踪会在那里让光平滑消失）。选中灯光时，聚光灯显示锥形，点光源显示该距离的球体；太阳光始终显示一条朝照射方向延伸到地面的线。 |
| Sound | 放在场景中的声音（扬声器图标）。随时间线以**秒**为单位播放，而不是帧；勾选 **Positional (3D)** 后从它所在的位置发声（相机越近越响，从它所在的一侧传来）；不勾选则像音乐一样。把 .wav 拖到 3D 视图中会在那里创建一个。 |
| Armature | 带有一根骨骼的骨架，用于制作角色动画。 |
| Path | 一条有两个节点的曲线（像网格一样编辑：<kbd>E</kbd> 添加节点）。 |
| Collection | 用来整理对象的文件夹。 |
| UI | 游戏用的 2D 界面（在 2D Editor 中编辑）。 |
| Hitbox | 碰撞检测盒；如果有活动对象，它会作为该对象的子级创建，大小与之相同。 |
| Physics | 不创建对象：为活动对象添加一个刚体。 |
| Particles | 粒子发射器。 |
| LOD / Culling / Grid Cull | 优化用的对象：细节层级，以及隐藏看不见的部分。 |
| Clear Depth / Screen Rect | 绘制工具：清除深度，或裁剪屏幕上的某个区域。 |
| Prefab / Scene / Proxy W3D | 插入项目中的预制体、另一个场景，或外部库中的预制体。 |
| Imports | OBJ、FBX、glTF、GLB、STL 或 Whisk3D UI 界面。STL（3D 打印、CAD）以 Z 轴朝上，导入时会被旋转，使模型立起来，相同的顶点会被焊接在一起。 |
| Core (advanced) | Routines：手动搭建的绘制步骤列表（变换、绘制、纹理、灯光、雾、混合、深度、控制、Call Lua）。其中包括 **Routine loop**：把自己的步骤重复多次的例程（例如“平移、绘制”重复 20 次），效果像阵列，但速度更快。 |

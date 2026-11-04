# Properties

这个面板包含活动对象的全部数据，按选项卡（图标）组织。显示哪些选项卡取决于对象的类型。单击卡片的标题可以将其折叠。在数值字段中：单击可以输入（支持算式），拖动可以滑动调整数值。右侧的菱形图标用于添加或移除关键帧。

| 选项卡 | 内容 |
|---|---|
| File / Render（软盘图标） | **File**：打开、保存、另存为、保存版本、提取资源。**Render**：路径、尺寸、额外通道、背景、**Ray Tracing**（光线、采样、通道）、Render Image 和 Render Animation。**Game**：平台（Windows、Symbian、Linux .deb、AppImage、WebGL、Android）、窗口、方向、图标、物理、声音和 Build game。**Export**：OBJ、glTF、GLB。**Settings**：语言、抗锯齿、图形、皮肤、日志、缩放、**Keyboard Shortcuts…** 和 Save Changes。 |
| Object | 位置、旋转（Euler、四元数或轴角）、缩放、名称、可见、可渲染；对象的动画和调色板。 |
| Contextual | 取决于对象类型：网格部分和**材质**（纹理、光照、顶点颜色、透明、反射、法线贴图、混合、深度、纹理层、**Advanced: custom shader**（见[手写着色器](shaders.md)）；启用光线追踪时还有 Roughness、Metallic、Transmission 和 IOR）、灯光、相机、雾、粒子、物理、碰撞盒、骨架和骨骼、2D 元素、预制体、例程。 |
| 3D Mesh | 使用的网格（以及有多少对象共享它）、UV 贴图、颜色层、顶点组、UV 组。在 Library 中选中某个资源时：显示它的数据和存放位置（.w3d 内部或外部）。 |
| Modifiers | 参见[修改器](modificadores.md)。 |
| Transform | 仅在 Edit Mode 中：所选内容及其 UV 的中心，可以编辑。 |
| Armature 2D | 网格的 2D 骨架和骨骼。 |
| Constraints | Copy Location、Copy Rotation、Billboard（朝向相机）、Child Of。 |
| Scripts | 对象的 Lua 脚本及其属性（参见 [Lua 文档](../lua/README.md)）。 |
| Animation | 活动动画、混合层（Mix）、所选关键帧的精确数值，以及镜头切换。 |

## 例程

例程是在 Properties 中搭建的一系列绘制步骤（平移、旋转、设置纹理、绘制网格、调用另一个例程、Call Lua…）。这是在 N95 上绘制最快的方式。

| 功能 | 说明 | 快捷键 |
|---|---|---|
| 添加 / 移除步骤 | 按主题分组添加步骤，或移除选中的步骤。 | 按钮；<kbd>X</kbd> 或 <kbd>Delete</kbd> 移除 |
| Grab step | 用方向键重新排列该步骤；<kbd>Enter</kbd> 确认。 | <kbd>G</kbd> |
| Insert Keyframe | 在当前帧为该步骤的数值记录动画。 | <kbd>I</kbd> |
| Repeat | 整个列表连续运行多少次（例程循环）。1 = 普通例程。 | Repeat 字段 |
| Render mode / Use | 每种着色模式使用不同的列表，或所有模式共用同一个列表。 | 下拉框 |

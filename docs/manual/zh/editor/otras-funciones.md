# 3D Viewport 的其他功能

| 功能 | 说明 | 使用方法 |
|---|---|---|
| Camera View | 通过活动相机观看，并显示一个与渲染尺寸相同的取景框。在框内，用滚轮或双指捏合可以进行检视缩放，而不会移动相机。 | <kbd>Num0</kbd> |
| Local View | 隔离所选内容及其子级（父级不绘制：选择网格时不会显示其骨架的骨骼）；左上角会显示 **Local View**。再执行一次即可恢复。 | <kbd>/</kbd> |
| 3D Cursor | 新对象出现的位置；也可以用作轴心。用 Snap 菜单（<kbd>Shift+S</kbd>）来放置它。 | <kbd>Shift+S</kbd> |
| Ray Tracing | 具有柔和阴影、透明、反射（材质的粗糙度和金属度）、法线贴图和聚光灯的真实感渲染。在 Properties → Render 中启用，在 Render Preview 中显示。可在 **GPU**（Vulkan：NVIDIA、AMD 或 Intel；驱动支持时使用 RTX/RDNA2/Arc 光线追踪核心）或 CPU 上追踪。**Only the final image** 可隐藏逐步成像的过程。 材质的 **Transmission**（0 到 1）表示有多少是玻璃：光线按 **IOR** 弯折后穿过（1 = 无，1.33 水，1.5 玻璃），配合 **Roughness** 呈现磨砂效果；因 alpha 而透明的部分直接穿过（transmission 为 0 时，IOR 像以前一样弯折透明部分）。参见[手写着色器](shaders.md#与光线追踪的关系)；**Transparency layers** 限制光线穿过的透明层数（超过则为黑色）。不模拟焦散。 **New noise each image**（默认开启）每张图像都换一次噪点，这样动画中噪点不会粘在屏幕上。 | Properties → Render → Ray Tracing、Device |
| 降噪与放大 | **降噪器**清除少量采样的噪点：任何支持 Vulkan 的显卡上的 **GPU (A-Trous)**（实时）、**Open Image Denoise**（CPU）和 **OptiX**（NVIDIA）。**放大**（x2、x3、x4）以视图分辨率的一部分追踪，降噪后放大：帧率更高。全部为可选：机器上没有的就不会显示。 **Upscaler** 选择放大方式：Linear，或 **FSR 1**（AMD，开源）：边缘更锐利，任何显卡都能用（由 CPU 处理）。 | Properties → Render → Denoiser、Upscaling |
| 运动矢量与运动模糊 | **Motion vectors** 会多保存一张图像（`_vectores`），每个像素表示它自上一帧以来在屏幕上移动了多少（红 = X，绿 = Y；128 = 静止，每级 = 半个像素）。这是一个普通的 OpenGL 通道，用运动的颜色给每个顶点上色，所以在 OpenGL ES 1.1 上也能用。**Motion blur** 用这些矢量在 CPU 上沿运动方向模糊图像；**Shutter**（0 到 1）是相机在一帧中保持打开的比例（0.5 = 经典的 180°）。 | Properties → Render → Motion vectors、Motion blur |
| 光线追踪运动模糊 | 打开 **Ray Tracing** 后，**Ray traced motion blur** 会在快门的不同时刻追踪每一遍（Render Image 和 Render Animation）：移动的网格和相机在相邻帧之间插值。这是真正的模糊（阴影、反射、移动物体后面的东西）；请用足够的遍数，免得看出重影。**Shutter**（0.01 到 1，默认 0.5）和 **Position**：**Center of frame**、**Start of frame** 或 **End of frame**。它会取代基于矢量的模糊。 | Properties → Render → Ray traced motion blur |
| 相机景深 | 每台相机都有 **Depth of field**：**F-stop**（和真实镜头一样：越小越模糊）、**Focus distance**（米，可用关键帧菱形做动画）和 **Focus object**（焦点会自动跟随该物体）。相机上的橙色线和十字标出对焦平面。**Ray Tracing** 会从镜头真正地做出景深（从相机看时）；后期处理的 **Depth of field** 也可以用同一个焦点和同一个 F-stop。勾选后，它的选项会立刻出现。 | Properties → Camera → Depth of field |
| 后期处理 | **Render** 下面的一张卡片，和修改器堆栈一样：可以 **Add** 效果、调整顺序（**Move Up** / **Move Down**）和 **Remove**；每个效果都有自己的参数。**Film grain**（强度、大小、彩色）、用深度做的 **Depth of field**（清晰范围；勾选 **Use camera focus** 时使用活动相机的焦点和 F-stop，如果该相机关闭了景深就什么都不模糊；不勾选时，手动设置对焦距离和模糊）和用运动通道做的 **Motion blur**（快门）。这些是相机的效果：在 **Rendered** 视图中只有 **透过活动相机观看时**、并且只在 **取景框内** 才能看到（全屏游戏模式下则是整个屏幕），也会进入渲染出的图像。为游戏的实时运行设计：在 OpenGL ES 2.0 和 Vulkan 上全部在 GPU 上完成（深度、法线、运动和 Solid 视图也是）；在 OpenGL ES 1.1（N95）和 PC 默认的 OpenGL 上由 CPU 完成，卡片会提示（在 PC 上，**Settings → Graphics → OpenGL ES 2.0** 或 **Vulkan** 会把它移到 GPU 上）。它与光线追踪的运动模糊和景深相互独立：可以只用一个，也可以两个都用。 | Properties → Render → Post Processing |
| 图形 | 编辑器在同一程序中带有三个后端：**OpenGL**（固定管线，可在旧显卡上运行）、**OpenGL ES 2.0**（着色器）和 **Vulkan**。选择在重启后生效；如果 Vulkan 在该机器上无法运行，则继续使用 OpenGL。 | Properties → Settings → Graphics |
| 游戏模式 | 当“Game”动画处于 Play 状态时，单击和按键都会发送给游戏。<kbd>Esc</kbd> 暂停。 | 栏上的 Play / Stop 按钮 |

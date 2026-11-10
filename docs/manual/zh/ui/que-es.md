# Whisk3D UI 是什么

**Whisk3D UI** 是一个用于构建界面的 C++ 库：按钮、选项卡、下拉菜单、属性卡片、文本和图标。它有自己的仓库 [Whisk3D-UI](https://github.com/Dante-Leoncini/Whisk3D-UI)，采用 MIT 许可证。它构建在 [Whisk3D Core](../core/README.md) 之上，Whisk3D 的编辑器就是用它做的。

![Whisk3D 的架构：可选的 Editor 和 Whisk3D UI 位于 Whisk3D Core 之上，Core 通过各自的后端与每个系统通信](img/whisk3d-capas.webp)

## 在哪里运行

与 Core 相同的系统：**Windows**、**Linux**、**macOS**、**Android**、**浏览器**、**Nokia N95** 和 **N8**、**Sega Dreamcast**、**Sony PSP**、**PS Vita** 和 **Nintendo 3DS**。编辑器在所有这些系统上都会编译它。

UI 没有针对各系统的专属代码。它不调用 OpenGL：一切都用 `w3dEngine` 的函数绘制（[图形](../core/graficos.md)），其余的由各系统的后端完成。少数不同之处都在编译层面：

- 头文件包含 `GL/gl.h`（在 Symbian 上是 `GLES/gl.h`），是为了其中的类型（`GLfloat`、`GLshort`、`GLubyte`）。
- 在 Android 上，`loadColorsW3d` 会更新字节颜色表，但不更新 `float` 颜色表。
- 代码是 C++03，因为 Symbian 的编译器不认识 C++11：枚举放在一个 `struct` 里（`IconType::camera`），并用 `W3D_OVERRIDE` 代替 `override`。

## 为什么用图集而不用矢量

几乎所有界面库都用矢量绘制。很多旧设备不对矢量绘制做加速：界面由软件绘制，运行很慢。而 N95 这类设备真正会加速的是 3D：带纹理的三角形。

所以 Whisk3D UI 全部用从一张纹理（即**图集**）中取样的三角形来绘制。字母、卡片的圆角边框和图标都在同一张纹理里：整个界面只需一次 `BindTexture`。一段文本每个图集只需一次绘制调用，而不是每个字母一次。

代价是：字母是像素字体，只能按整数倍放大（[颜色和缩放](tema.md)），而且只有图集中画好的字母可用（[文本](texto.md)）。

## 里面有什么

| 文件夹 | 解决什么问题 |
|---|---|
| `core/` | 界面的尺寸及其缩放（`UI.h`）：[颜色和缩放](tema.md) |
| `theme/` | 调色板（`colores.h`）：[颜色和缩放](tema.md) |
| `draw/` | 2D 绘制（`glesdraw`）、矩形、图标和图集打包器：[绘制和图集](dibujo.md)、[图标](iconos.md) |
| `text/` | 位图字体、扩展图集，以及使用烘焙好的 TTF 字体的文本：[文本](texto.md) |
| `widgets/` | 带边框的卡片、按钮、选项卡、菜单和文本框：[按钮、选项卡和复选框](botones.md)、[下拉菜单](menus.md)、[输入和文本框](entrada.md) |
| `Propieties/` | 属性卡片及其各行：[卡片和属性](propiedades.md) |

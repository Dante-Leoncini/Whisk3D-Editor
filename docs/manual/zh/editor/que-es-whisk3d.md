# Whisk3D 是什么

Whisk3D 是一款从零开始打造的自由 **3D 编辑器和游戏引擎**。它的理念是用你手头的设备创造你想要的任何东西：它可以在 PC、手机上运行，也可以在游戏机和老旧设备上运行，不依赖现代硬件，也不依赖专有软件。

## 能做什么

使用 **Whisk3D Editor**，你可以：

- 进行 3D **建模**：由顶点、边和面构成的网格，以及不会破坏网格的[修改器](objeto-malla.md#modifiers-选项卡)。
- 为模型做**绑定**（rig）：搭建角色的[骨架](esqueletos.md)，并绘制每根骨骼带动的程度。
- **制作动画**：关键帧、曲线编辑器和骨架动画（[Timeline](linea-de-tiempo.md)）。
- 用点光源、太阳光和聚光灯为场景**布光**。
- **渲染**图像和视频，可以实时渲染，也可以用**光线追踪**（柔和阴影、反射和透明）。
- 直接在模型上**绘制**顶点颜色和纹理（[绘制模式](pintura.md)）。
- **制作游戏**：用 [Lua](../lua/README.md) 编写逻辑，使用内置的[脚本编辑器](editor-de-scripts.md)；在 [2D Editor](editor-2d.md) 中搭建界面（菜单、HUD），并直接在编辑器中为每个系统编译游戏。
- 以 OBJ、FBX、glTF、GLB 和 STL 格式导入和导出模型。

Whisk3D 还有自己的**自由格式**，用来保存项目和游戏：**`.w3d`**。`.w3d` 格式的游戏可以在任何能运行 Whisk3D 的系统上直接打开，也可以针对特定系统**编译**，以充分发挥其硬件性能。参见文章 [W3D：一种让电子游戏永不消亡的自由格式](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=zh)。

游戏也可以**不用编辑器**制作，直接在 **Whisk3D Core** 之上用 C 和 C++ 编程。这条路只适合已经会用 C 和 C++ 编程的有经验用户：参见 [Whisk3D Core 手册](../core/empezar.md)。

## 在哪里运行

| 系统 | |
|---|---|
| **PC** | Windows、Linux 和 macOS |
| **手机** | Android（2.3 及以上）以及 Nokia N95 和 N8（Symbian） |
| **浏览器** | 任何现代浏览器（WebGL） |
| **游戏机** | Sega Dreamcast、Sony PSP、PlayStation Vita 和 Nintendo 3DS |

## 它是怎样构成的

Whisk3D 由三个项目组成，一个叠在另一个之上，就像三个一起转动的齿轮：

- **Whisk3D Editor**（绿色齿轮）：编辑器，包含上面提到的一切。
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=zh)**（黄色齿轮）：负责**界面**：按钮、菜单、属性卡片、文本和图标。编辑器就是用它做的。参见 [Whisk3D UI 手册](../ui/README.md)。
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=zh)**（红色齿轮）：**抽象层**。Whisk3D UI 做的每一件事都要用到它。

在编辑器中制作的游戏或项目存在于编辑器**内部**，其逻辑用 Lua 编写。但 Editor 和 Whisk3D UI 都是**可选的**：你可以使用自己的工具。用 C 或 C++ 编写的游戏可以不用编辑器，只用 Whisk3D UI 来做界面；也可以**直接与 Core 对话**，使用自己的界面和工具。唯一不能缺少的是 Core。

![Whisk3D 的三层架构：最上层是 Editor（绿色），里面是你用 Lua 编写的游戏或项目；中间是 Whisk3D UI（黄色）；最下层是 Whisk3D Core（红色）。Editor 和 UI 是可选的：从外部，一个 C++ 游戏可以使用 UI，另一个可以直接使用 Core。Core 通过各自的后端与每个系统通信](img/whisk3d-capas.webp)

Core 是**秘密配方**。每个系统绘图、发声、读取文件和接收按键的方式都不一样：PC 使用 OpenGL，Dreamcast 有自己的图形芯片，PSP 和 3DS 有各自的库，浏览器使用 WebGL。编辑器和游戏都不直接与各个系统对话：它们总是用同样的方式向 Core 提出请求（“绘制这个网格”“播放这个声音”“打开这个文件”），而 Core 知道在每个系统上该怎么做。正因如此，**同一份代码**才能在差异如此之大的机器上运行；要支持一个新系统，只需教会 Core 与它对话，而不必重写编辑器。

它具体是如何做到的（图形、声音、文件、输入、网络、日志）：[Whisk3D Core 手册](../core/README.md)。

## 项目

所有工作都保存在一个 `.w3d` 项目文件中。文件里既有场景，也有各种资源：纹理（`texturas/`）、脚本（`scripts/`）、声音、场景、预制体（prefab）。资源可以存放在 .w3d **内部**，也可以作为磁盘上的独立文件存放在**外部**；“Save inside the .w3d”和“Save outside the .w3d”菜单可以把资源在两者之间移动。参见[项目](proyecto.md)。

> [!NOTE]
> 本手册介绍的是 Whisk3D 目前已有的功能。由于编辑器暂时还没有中文版，手册中的功能名称都是编辑器英文界面中显示的名称。手册中列出的按键均为默认设置，你可以在[键盘快捷键](atajos.md)中修改。

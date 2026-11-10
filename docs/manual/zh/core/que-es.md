# Whisk3D Core 是什么

**Whisk3D Core** 是把 Whisk3D 与每台机器隔开的那一层。它是秘密武器：多亏了它，同一份代码可以在 **Windows**、**Linux**、**macOS**、**Android**、**浏览器**（WebGL）、**Nokia N95** 和 **N8**（Symbian）、**Sega Dreamcast**、**Sony PSP**、**PS Vita** 和 **Nintendo 3DS** 上运行。

Core 是一个 C++ 库，有自己的仓库 [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core)，采用 MIT 许可证。它的设计灵感来自 **RenderWare** 等经典引擎：上层是一套小而稳定的 API，下层为每个系统提供不同的实现。

![Whisk3D 的三层架构：最上面是 Editor（绿色），里面是你的 Lua 游戏或项目；中间是 Whisk3D UI（黄色）；最下面是 Whisk3D Core（红色）。Editor 和 UI 是可选的：从外部来看，一个 C++ 游戏可以使用 UI，另一个可以直接使用 Core。Core 通过各自的后端与每个系统通信](img/whisk3d-capas.webp)

## 思路：提出请求，而不是亲自去做

一台装有现代显卡的 PC、一部 2007 年的手机和一台 Dreamcast，绘制方式毫无相似之处。PC 使用 OpenGL 或 Vulkan；PSP 有自己的芯片（GE）和一个库（sceGu）；3DS 使用 citro3d；N95 使用 OpenGL ES 1.1。如果游戏要逐一调用它们，就得把游戏写上十一遍。

**抽象层**解决了这个问题。游戏不绘制，而是**请求**。它说“用这张纹理和这盏灯绘制这些三角形”，而了解该系统的那部分，也就是**后端**，知道该怎么做。声音、文件、日志和网络也是如此：一种请求方式，每个系统一套实现。

本项目的规矩：

- **上层代码从不调用系统。** 不调用 OpenGL，不调用 sceGu，也不调用 Symbian。一切都通过 Core 的函数。
- **头文件不暴露系统的类型。** 纹理是一个 `unsigned int`，各项功能用自己的枚举（`w3dEngine::DepthTest`、`w3dEngine::Blend`），而不是 OpenGL 的。这样使用 Core 的人不依赖任何图形头文件。
- **每个系统只提供最少的部分。** 也就是它的启动部分（窗口、GPU 上下文），以及放不进共享代码的东西。在编辑器的仓库中，这些位于 `platform/<sistema>/` 以及 `#ifdef W3D_PSP`、`#ifdef W3D_3DS` 等代码块中。

一个使用 Whisk3D Core 编写的游戏小例子，可以在所有系统上运行：

```cpp
#include "graphics/w3dGraphics.h"   // 绘制
#include "graphics/w3dTexture.h"    // 纹理
#include "io/w3dFilesystem.h"       // 文件
#include "base/w3dlog.h"            // 日志
#include "audio/W3dAudio.h"         // 声音

namespace gfx = w3dEngine;

// 一个文件：它可以在磁盘上、在 .w3d 里、在 APK 里，或者打包在游戏中
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// 一张纹理：由 stb（或 Symbian 的 ICL）解码，由后端上传
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// 一个声音：由 Core 混音；输出由各个系统负责
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// 绘制：一次也不调用 OpenGL、sceGu 或 citro3d
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## 里面有什么

Core 的每个文件夹都是一个模块：

| 文件夹 | 解决什么问题 |
|---|---|
| `graphics/` | 绘制 API（`w3dEngine`）、纹理、屏幕和各个后端：[图形](graficos.md) |
| `audio/` | 音效、音乐和音量：[声音和音乐](sonido.md) |
| `io/` | 文件、容器（.w3d、zip、pak）、网格和资源：[文件](archivos.md) |
| `base/` | 日志、保存的配置以及所有模块共用的部分：[日志](logs.md) |
| `input/` | 触屏、手柄、倾斜、振动和剪贴板：[输入](entrada.md) |
| `net/` | 联网游戏：[网络](red.md) |
| `physics/` | 物理、碰撞、碰撞盒、布娃娃和寻路：[物理](fisica.md) |
| `script/` | Lua：[Lua 脚本](lua.md) |
| `math/` | 向量、矩阵和四元数：[数学、场景和格式](formatos.md) |
| `objects/`、`animation/` | 场景：对象、网格、材质、灯光、相机和动画：[数学、场景和格式](formatos.md) |
| `tools/` | 分派代码生成器、独立性测试和打包工具 |

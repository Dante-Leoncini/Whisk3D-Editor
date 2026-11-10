# 直接使用 Core

面向使用 C 或 C++ 编程、想不借助编辑器制作游戏的人。你需要：

- **Core 的源代码。** 包含目录是 Core 的根目录，再加上 `base/`、`graphics/`、`io/` 和 `thirdparty/`。
- **一个 `graphics/` 后端**，即对应系统的那个（例如 `graphics/opengl/w3dGraphics.cpp`），以及 `base/w3dlog.cpp`。然后是要用到的模块（音频、文件、物理……）。
- **一个窗口及其上下文。** 示例使用 SDL2。使用着色器后端时，需要在创建上下文之后调用一次 `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)`。

最少需要的代码，取自 `simple_cube` 示例：

```cpp
gfx::Viewport(0, 0, w, h);
gfx::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
gfx::MatrixMode(gfx::Projection);  gfx::LoadIdentity();
gfx::Perspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
gfx::MatrixMode(gfx::ModelView);   gfx::LoadIdentity();
gfx::Translatef(0.0f, 0.0f, -5.0f);
gfx::Rotatef(angulo, 0.0f, 1.0f, 0.0f);
gfx::EnableArray(gfx::VertexArray); gfx::VertexPointer3f(0, cubeVerts);
gfx::EnableArray(gfx::NormalArray); gfx::NormalPointer3b(cubeNormals);
gfx::DrawTriangles(36, cubeIdx);
```

示例正在陆续上传到 [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples)：`simple_cube`（一个带一盏灯的立方体）、`textured_cube`（带一张由 `LoadTexture` 加载的纹理）和 `normal_map_cube`（用 DOT3 实现的假凹凸）。那里还有 `game/w3drun`，它是一个运行时，可以脱离编辑器运行在编辑器中制作的游戏（场景、界面和 Lua）。

目前的状态，直说吧：

- **Core 还不能用于生产环境。** 它仍在重写，经常变动。
- **它内部还带有编辑器的逻辑**（编辑模式、修改器、渲染模式），正在逐步移除。`tools/prueba_independencia.sh` 用来衡量它离独立使用还差多少。
- **示例是为旧版的文件夹结构编写的：** 它们的 CMake 引用的是 Core 根目录下的 `w3dGraphics.cpp`，而它现在位于 `graphics/opengl/`。可能需要调整路径。
- **接下来的计划：** 骨架、一个类似 BSP 的地图系统、可选的物理、更多复古平台后端、更多示例和文档。

有疑问可以到 [Telegram](https://t.me/Whisk3D) 群组提问。

如果不需要深入到 C++，捷径是使用编辑器：用 [Lua](../lua/README.md) 编写游戏，再由 [Properties](../editor/propiedades.md) 中的 **Game** 卡片为每个系统编译。它底层用的也是同一个 Core。

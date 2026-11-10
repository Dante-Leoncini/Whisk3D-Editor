# Using the Core directly

For people who program in C or C++ and want a game without the editor. You need:

- **The Core sources.** The include directories are the Core's root plus `base/`, `graphics/`, `io/` and `thirdparty/`.
- **A `graphics/` backend**, the one for your system (for example `graphics/opengl/w3dGraphics.cpp`), and `base/w3dlog.cpp`. Then, the modules you use (audio, files, physics...).
- **A window with its context.** The examples use SDL2. With the shader backend you have to call `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` once, after creating the context.

The bare minimum, from the `simple_cube` example:

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

The examples are being uploaded to [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples): `simple_cube` (a cube with a light), `textured_cube` (with a texture loaded by `LoadTexture`) and `normal_map_cube` (fake relief with DOT3). There you'll also find `game/w3drun`, the runtime that runs a game made in the editor (scene, interface and Lua) without the editor.

The current state, plainly:

- **The Core isn't production-ready yet.** It's still being rewritten and changes often.
- **It has editor logic inside** (edit mode, modifiers, render modes) that is being taken out. `tools/prueba_independencia.sh` measures how far it is from being usable on its own.
- **The examples were written for an earlier version** of the folder layout: their CMake names `w3dGraphics.cpp` at the Core's root, and today it's in `graphics/opengl/`. You may need to adjust the paths.
- **What's coming:** armatures, a BSP-style system for maps, optional physics, more retro backends, more examples and documentation.

Questions go to the [Telegram](https://t.me/Whisk3D) group.

If you don't need to go down to C++, the short path is the editor: you program the game in [Lua](../lua/README.md) and the **Game** card in [Properties](../editor/propiedades.md) builds it for each system. Underneath it uses the same Core.

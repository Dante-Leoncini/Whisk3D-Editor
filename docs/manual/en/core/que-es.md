# What is Whisk3D Core

**Whisk3D Core** is the layer that separates Whisk3D from each machine. It's the secret ingredient: thanks to it, the same code runs on **Windows**, **Linux**, **macOS**, **Android**, the **browser** (WebGL), the **Nokia N95** and **N8** (Symbian), the **Sega Dreamcast**, the **Sony PSP**, the **PS Vita** and the **Nintendo 3DS**.

The Core is a C++ library with its own repository, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), under the MIT license. Its design is inspired by classic engines like **RenderWare**: a small, stable API on top and a different implementation for each system underneath.

![Whisk3D's architecture in three layers: at the top the Editor (green), with your game or Lua project inside; in the middle Whisk3D UI (yellow); at the bottom Whisk3D Core (red). The Editor and the UI are optional: from outside, a C++ game can use the UI and another one the Core directly. The Core talks to each system through its own backend](img/whisk3d-capas.webp)

## The idea: ask, don't do

A PC with a modern graphics card, a phone from 2007 and a Dreamcast draw in ways that have nothing in common. The PC speaks OpenGL or Vulkan; the PSP has its own chip (the GE) and a library (sceGu); the 3DS uses citro3d; the N95, OpenGL ES 1.1. If the game called each one of them, it would have to be written eleven times.

An **abstraction layer** solves that. The game doesn't draw: it **asks**. It says "draw these triangles with this texture and this light", and the piece that knows that system, the **backend**, knows how to do it. The same goes for sound, files, the log and networking: a single way to ask and one implementation per system.

The house rules:

- **The code on top never calls the system.** Not OpenGL, not sceGu, not Symbian. Everything goes through Core functions.
- **The headers don't expose system types.** A texture is an `unsigned int`, and the capabilities are the Core's own enums (`w3dEngine::DepthTest`, `w3dEngine::Blend`), not OpenGL's. That way whoever uses the Core doesn't depend on any graphics header.
- **Each system contributes the minimum.** Its startup (the window, the GPU context) and whatever has no place in the shared code. In the editor's repository that lives in `platform/<sistema>/` and in `#ifdef W3D_PSP`, `#ifdef W3D_3DS`, etc. blocks.

A small example of a game written to use Whisk3D Core, which works on every system:

```cpp
#include "graphics/w3dGraphics.h"   // drawing
#include "graphics/w3dTexture.h"    // textures
#include "io/w3dFilesystem.h"       // files
#include "base/w3dlog.h"            // log
#include "audio/W3dAudio.h"         // sound

namespace gfx = w3dEngine;

// a file: it can be on disk, inside the .w3d, inside the APK or packed into the game
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// a texture: stb (or Symbian's ICL) decodes it and the backend uploads it
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// a sound: the Core mixes it; the output belongs to each system
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// drawing: not a single call to OpenGL, sceGu or citro3d
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## What's inside

Each folder of the Core is a module:

| Folder | What it handles |
|---|---|
| `graphics/` | The drawing API (`w3dEngine`), textures, the screen and the backends: [Graphics](graficos.md) |
| `audio/` | Effects, music and volume: [Sound and music](sonido.md) |
| `io/` | Files, containers (.w3d, zip, pak), meshes and resources: [Files](archivos.md) |
| `base/` | The log, saved settings and what everything shares: [Logs](logs.md) |
| `input/` | Touch, gamepads, tilt, vibration and clipboard: [Input](entrada.md) |
| `net/` | Network play: [Networking](red.md) |
| `physics/` | Physics, collisions, hitboxes, ragdolls and navigation: [Physics](fisica.md) |
| `script/` | Lua: [Lua scripts](lua.md) |
| `math/` | Vectors, matrices and quaternions: [Math, scene and formats](formatos.md) |
| `objects/`, `animation/` | The scene: objects, meshes, materials, lights, cameras and animations: [Math, scene and formats](formatos.md) |
| `tools/` | The dispatch generator, the independence test and the packer |

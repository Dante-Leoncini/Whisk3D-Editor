# What is Whisk3D

Whisk3D is a free, open-source **3D editor and game engine**, built from scratch. The idea is to be able to create whatever you want with whatever you have: it runs on a PC, on a phone and also on consoles and old devices, without depending on modern hardware or proprietary software.

## What you can do

With **Whisk3D Editor** you can:

- **Model** in 3D: meshes with vertices, edges and faces, and [modifiers](objeto-malla.md#the-modifiers-tab) that work without destroying the mesh.
- **Rig** them: build a character's [armature](esqueletos.md) and paint how much each bone moves.
- **Animate**: keyframes, the curve editor and armature animations ([Timeline](linea-de-tiempo.md)).
- **Light** the scene with point lights, sun lights and spotlights.
- **Render** images and videos, in real time or with **ray tracing** (soft shadows, reflections and transparency).
- **Paint** vertex colors and textures, directly on the model ([Paint modes](pintura.md)).
- **Make games**: the logic is written in [Lua](../lua/README.md), with the built-in [script editor](editor-de-scripts.md); interfaces (menus, HUD) are built in the [2D Editor](editor-2d.md), and the game is compiled for each system from the editor itself.
- Import and export models in OBJ, FBX, glTF, GLB and STL.

Whisk3D also has its **own open format** for saving projects and games: **`.w3d`**. A game in `.w3d` can be opened directly on any system where Whisk3D runs, or **compiled** for a specific system, to get the most out of its hardware. See the article [W3D: an open format so video games never die](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=en).

Games can also be made **without the editor**, programming directly on top of **Whisk3D Core** in C and C++. That path is only for experienced users who already know how to program in C and C++: see the [Whisk3D Core manual](../core/empezar.md).

## Where it runs

| System | |
|---|---|
| **PC** | Windows, Linux and macOS |
| **Phones** | Android (2.3 and later) and the Nokia N95 and N8 (Symbian) |
| **Browser** | Any modern browser (WebGL) |
| **Consoles** | Sega Dreamcast, Sony PSP, PlayStation Vita and Nintendo 3DS |

## How it's built

Whisk3D is three projects, one on top of the other, like three gears that turn together:

- **Whisk3D Editor** (the green gear): the editor, with everything above.
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=en)** (the yellow one): takes care of the **interface**: the buttons, the menus, the property cards, the text and the icons. The editor is made with it. See the [Whisk3D UI manual](../ui/README.md).
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=en)** (the red one): the **abstraction layer**. Whisk3D UI uses it for everything it does.

A game or a project made in the editor lives **inside** the editor, with its logic written in Lua. But the Editor and Whisk3D UI are **optional**: you can use your own tools. A game in C or C++ can use Whisk3D UI for its interface, without the editor, or talk **directly to the Core**, with its own interface and its own tools. The only thing that can't be missing is the Core.

![The Whisk3D architecture in three layers: at the top the Editor (green), with your Lua game or project inside; in the middle Whisk3D UI (yellow); at the bottom Whisk3D Core (red). The Editor and the UI are optional: from outside, one C++ game can use the UI and another the Core directly. The Core talks to each system through its own backend](img/whisk3d-capas.webp)

The Core is the **secret ingredient**. Each system draws, plays sound, reads files and receives key presses in its own way: a PC uses OpenGL, the Dreamcast has its own graphics chip, the PSP and the 3DS their own libraries, the browser WebGL. Neither the editor nor the games talk to each system: they ask the Core for things, always in the same way ("draw this mesh", "play this sound", "open this file"), and the Core knows how to do it on each one. That's why the **same code** works on such different machines, and adding a new system means teaching the Core to talk to it, not rewriting the editor.

How it does it, in full detail (graphics, sound, files, input, networking, logs): [Whisk3D Core manual](../core/README.md).

## The project

All your work is saved in a `.w3d` project file. It holds the scene and also the resources: textures (`texturas/`), scripts (`scripts/`), sounds, scenes, prefabs. A resource can live **inside** the .w3d or **outside**, as a separate file on disk; the "Save inside the .w3d" and "Save outside the .w3d" menus move it from one place to the other. See [The project](proyecto.md).

> [!NOTE]
> This manual describes what Whisk3D can do today. The function names are the ones shown in the editor in English. The keys listed are the defaults: they can be changed in [Keyboard Shortcuts](atajos.md).

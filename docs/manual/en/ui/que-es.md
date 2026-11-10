# What is Whisk3D UI

**Whisk3D UI** is a C++ library for building interfaces: buttons, tabs, drop-down menus, property cards, text and icons. It has its own repository, [Whisk3D-UI](https://github.com/Dante-Leoncini/Whisk3D-UI), under the MIT license. It's built on top of [Whisk3D Core](../core/README.md), and the Whisk3D editor is made with it.

![Whisk3D's architecture: the Editor and Whisk3D UI, both optional, on top of Whisk3D Core, which talks to each system through its own backend](img/whisk3d-capas.webp)

## Where it runs

On the same systems as the Core: **Windows**, **Linux**, **macOS**, **Android**, the **browser**, the **Nokia N95** and **N8**, the **Sega Dreamcast**, the **Sony PSP**, the **PS Vita** and the **Nintendo 3DS**. The editor compiles it on all of them.

The UI has no system-specific code. It doesn't call OpenGL: it draws everything with the `w3dEngine` functions ([Graphics](../core/graficos.md)) and each system's backend does the rest. The little that changes is at compile time:

- The headers include `GL/gl.h`, or `GLES/gl.h` on Symbian, for the types (`GLfloat`, `GLshort`, `GLubyte`).
- On Android, `loadColorsW3d` updates the byte color table but not the `float` one.
- The code is C++03, because the Symbian compiler doesn't know C++11: enums go inside a `struct` (`IconType::camera`) and `W3D_OVERRIDE` is used instead of `override`.

## Why atlases and not vectors

Almost every interface library draws with vectors. Many old devices don't accelerate that: the interface is drawn in software and runs slowly. What devices like the N95 do accelerate is 3D: textured triangles.

That's why Whisk3D UI draws everything with triangles that read from a texture, an **atlas**. The letters, the rounded edges of the cards and the icons are in the same texture: a single `BindTexture` is enough for the whole interface. A text is drawn with one call per atlas, not one per letter.

The price: the letters are pixel art and grow in integer multiples ([Colors and scale](tema.md)), and only the letters drawn in the atlas exist ([Text](texto.md)).

## What's inside

| Folder | What it handles |
|---|---|
| `core/` | The interface measurements and their scale (`UI.h`): [Colors and scale](tema.md) |
| `theme/` | The color palette (`colores.h`): [Colors and scale](tema.md) |
| `draw/` | 2D drawing (`glesdraw`), rectangles, icons and the atlas packer: [Drawing and atlas](dibujo.md), [Icons](iconos.md) |
| `text/` | The bitmap font, the extended atlas and text with baked TTF fonts: [Text](texto.md) |
| `widgets/` | Bordered cards, buttons, tabs, menus and text boxes: [Buttons, tabs and checkboxes](botones.md), [Drop-down menus](menus.md), [Input and text boxes](entrada.md) |
| `Propieties/` | Property cards and their rows: [Cards and properties](propiedades.md) |

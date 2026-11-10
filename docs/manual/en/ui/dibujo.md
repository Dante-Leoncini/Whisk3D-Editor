# Drawing and atlas

## How it draws

Everything goes through `w3dEngine` ([Graphics](../core/graficos.md)). The UI draws triangles (`DrawTrianglesArray`, `DrawTrianglesByte`) and lines, with the vertices in `float`. It doesn't draw with `TRIANGLE_STRIP` or with `short` vertices: the N95's driver doesn't draw them in the 2D phase of the frame.

Coordinates are screen pixels, with Y pointing down and the origin at the top left. Each widget is drawn at the origin of the current matrix. To position it you use `Translatef`, between `PushMatrix` and `PopMatrix`.

## The `glesdraw` functions

`draw/glesdraw.h` is the lowest-level drawing, the one the others use:

| Function | What it draws |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | A quad: 4 `GLshort` vertices in strip order, with their 8 UVs. It turns it into two triangles |
| `W3dDrawElemsB(verts, uvs, idx, n)` | A small mesh with `GLubyte` indices, up to 64 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | Up to 21 lines, with 3-component `GLshort` vertices |
| `W3dDrawLinesF(verts, idx, n)` | Lines with 3-component `float` vertices, 40 vertices per draw |

The vertices converted to `float` go to a fixed 256-slot store that's used as a ring, never to the stack: the N95's driver reads the pointers late.

`W3dPantallaAlto` is the screen height. Whoever uses the UI sets it when the size changes, and it's used to convert the interface's Y to that of `Scissor` and `Viewport`, which count from the bottom.

## Nodes and rectangles

`Object2D` (`draw/object2D.h`) is a node with children (`Childrens`): it has a size, color, opacity and `visible`. `Render()` draws it and then its children. Deleting it deletes the children.

`Rec2D` (`draw/rectangle.h`) is a single-color rectangle. `SetSize(x, y, w, h)` positions it and `SetColor(r, g, b)` sets its color. It doesn't read the texture: it's drawn with `Texture2D` turned off.

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false: with the current color, not its own
gfx::Enable(gfx::Texture2D);
```

## Cards

`Card` (`widgets/card.h`) is the rounded-corner rectangle of buttons, menus and property cards. It's a **9-patch**: 16 vertices that stretch the center and keep the corners at `2 * GlobalScale`. The corners and the center come from a corner of `font.png`. `CalcCardUV(anchoAtlas, altoAtlas)` computes those coordinates: it's called once, with the texture's size.

| Function | What it does |
|---|---|
| `Resize(w, h)` | Changes the size |
| `RenderObject(propio)` | Draws the fill |
| `RenderBorder(propio)` | Draws only the border |
| `RenderObjectTab()`, `RenderBorderTab()` | The same, with a straight bottom edge: for tabs |

With `propio` set to `true` it uses its own color (`color` and `opacity`); with `false`, the current color.

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## The skin atlas

A single texture for the whole interface. `font.png`, 128x128, holds the letters, the card borders and the old icons. With that alone, the UI works: `CrearIconos` takes the icons from inside `font.png`.

The new icons are separate files, in `atlas/iconos/`. At startup the editor builds a bigger texture (`main/io/SkinAtlas.cpp`): it puts the whole `font.png` at (0,0), so its coordinates don't change, and packs each icon into the leftover space. Then it tells the UI where each one ended up, with `CrearIconosDesde`. Building that atlas belongs to the editor; the packer and the icon table belong to the UI. See [Icons](iconos.md).

## The packer

`W3dAtlasPack` (`draw/W3dAtlasPacker.h`) combines several RGBA images into one. It doesn't touch the GPU: pixels go in and a canvas comes out, which the caller uploads. It works just as well for a game: a level's textures combined into one mean fewer texture changes.

```cpp
std::vector<W3dAtlasItem> items(n);       // for each image: rgba, w, h
items[0].fijo = true;                     // this one goes exactly where requested
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // 2 pixels of padding between images
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x, items[i].y: where each one ended up
}
```

It tries larger and larger canvases: 128x128, 256x128, 256x256, 512x256, 512x512 and 1024x512. If they don't fit even in the last one, it returns `false`. The loose images go in rows, from left to right, skipping the fixed ones. The padding between images keeps linear filtering from mixing one with its neighbor.

## Leftovers from before

`draw/sprites.h` (`Sprite`) and `draw/image.h` (`Image`) aren't included by anyone. They use C++11 initializers, which don't compile on Symbian.

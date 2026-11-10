# Icons

## The files

Each icon is a **10x10** PNG in `res/Skins/Whisk3D/atlas/iconos/`, in the editor's repository. Today there are 106. The pixels are white on transparent: the icon takes the color it's drawn with. That way the same icon works in gray, in white or in the accent color.

## The enum and the names

`draw/icons.h` has the `IconType` enum and the count, `ICON_TOTAL` (106). In `draw/icons.cpp`, `KNombres` gives each one's name, in the same order: it's the file name without `.png`.

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, or -1 if it doesn't exist
```

Almost all of them are named the same as in the enum. The exception is `candadoAbierto`, which is the file `candado_abierto.png`.

Many belong to the editor: modeling tools, object types, animation curves. The general ones are there too: `arrow` and `arrowRight` (the fold arrows), `visible` and `hidden`, `carpeta`, `guardar`, `borrar`, `mas` and `menos`, `lista` and `cuadricula`, `candado`, `notifOk` and `notifError`, `tijera`, `copiar` and `pegar`.

## The coordinates

`IconsUV` is a `std::vector<IconUV*>`, with each icon's coordinates in the atlas (`uvs`, 8 numbers). It's filled in one of two ways:

- `CrearIconos(ancho, alto)`: with the table of where each icon is inside `font.png` (`TablaLegacy`, in `icons.cpp`). This is the way without the `atlas/iconos/` folder.
- `CrearIconosDesde(rects, ancho, alto)`: with your own list of rectangles, the one from the built atlas. A rectangle with `x = -1` uses the one from `font.png`: an icon without its PNG still shows up.

`SetIconScale(escala)` adjusts `IconSizeGS` and the two meshes: `IconMesh`, the quad of an icon, and `IconLineMesh`, a quad one line tall, for the lines of trees.

## Drawing an icon

With the atlas bound, at the current origin:

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` and `GroupPropertie` take the icon as an `int` from the enum, or `-1` for none.

## Adding one

1. The 10x10 PNG, in `atlas/iconos/`.
2. A line at the end of the `IconType` enum, and add one to `ICON_TOTAL`.
3. Its name at the end of `KNombres`.
4. A line at the end of `TablaLegacy`, with the `font.png` icon that replaces it if the PNG is missing.

Without step 4, the icon has no coordinates when there's no built atlas.

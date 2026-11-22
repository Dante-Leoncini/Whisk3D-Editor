# Icons

## The files

Each icon is a **10x10** PNG in `res/Skins/Whisk3D/atlas/icons/`, in the editor's repository. Today there are 106. The pixels are white on transparent: the icon takes the color it's drawn with. That way the same icon works in gray, in white or in the accent color.

## The enum and the names

`draw/icons.h` has the `IconType` enum and the count, `ICON_TOTAL` (106). In `draw/icons.cpp`, `KNames` gives each one's name, in the same order: it's the file name without `.png`.

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta, or -1 if it doesn't exist
```

Almost all of them are named the same as in the enum. The exception is `lockOpen`, which is the file `candado_abierto.png`.

Many belong to the editor: modeling tools, object types, animation curves. The general ones are there too: `arrow` and `arrowRight` (the fold arrows), `visible` and `hidden`, `folder`, `save`, `trash`, `plus` and `minus`, `list` and `grid`, `lock`, `notifOk` and `notifError`, `scissors`, `copy` and `paste`.

## The coordinates

`IconsUV` is a `std::vector<IconUV*>`, with each icon's coordinates in the atlas (`uvs`, 8 numbers). It's filled in one of two ways:

- `CreateIcons(width, height)`: with the table of where each icon is inside `font.png` (`TableLegacy`, in `icons.cpp`). This is the way without the `atlas/icons/` folder.
- `CreateIconsFrom(rects, width, height)`: with your own list of rectangles, the one from the built atlas. A rectangle with `x = -1` uses the one from `font.png`: an icon without its PNG still shows up.

`SetIconScale(scale)` adjusts `IconSizeGS` and the two meshes: `IconMesh`, the quad of an icon, and `IconLineMesh`, a quad one line tall, for the lines of trees.

## Drawing an icon

With the atlas bound, at the current origin:

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`, `Tab`, `PopupMenu` and `GroupPropertie` take the icon as an `int` from the enum, or `-1` for none.

## Adding one

1. The 10x10 PNG, in `atlas/icons/`.
2. A line at the end of the `IconType` enum, and add one to `ICON_TOTAL`.
3. Its name at the end of `KNames`.
4. A line at the end of `TableLegacy`, with the `font.png` icon that replaces it if the PNG is missing.

Without step 4, the icon has no coordinates when there's no built atlas.

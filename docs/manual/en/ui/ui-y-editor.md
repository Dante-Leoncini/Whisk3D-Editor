# What belongs to the UI and what to the editor

The editor is built with Whisk3D UI, but much of what you see in it is its own. The UI's parts are in `libs/WhiskUI/`; the editor's, in `main/` of the Whisk3D repository.

## The UI's

- 2D drawing, bordered cards and rectangles.
- The icon table and the atlas packer.
- The bitmap font, the extended atlas and text with baked TTF fonts.
- The color palette and the measurements, with their scale.
- Buttons, tabs, checkboxes and drop-down menus.
- Property cards and their rows, with the animation diamond.
- The state of text boxes and how they're drawn.

## The editor's

| What | Where |
|---|---|
| The viewports: splitting the screen, their borders, their menu bars and toolbars | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| The properties panel: which cards there are, their tabs, clicking and dragging on rows | `main/ui/ViewPorts/Properties.cpp` |
| Modal windows: the color picker, the file browser, confirmations, the progress bar, the on-screen keyboards | `main/ui/ViewPorts/PopUp/` |
| Notifications | `main/ui/ViewPorts/Notificaciones.cpp` |
| The panels' scroll bars | `main/ui/ViewPorts/ScrollBar.cpp` |
| Tooltips. The text is stored in `Button::ayuda` | `W3dAyudaRender`, in `main/ui/ViewPorts/LayoutInput.cpp` |
| Which key, click or tap goes to each widget, and the N95's keys | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| The text boxes' focus functions | `main/ui/ViewPorts/NumInput.cpp` |
| Translations, `T()`. The UI only has the `W3dMenuTraducir` hook | `main/config/W3dLang.cpp` |
| Starting up the UI | `main/app/W3dInitUI.cpp` |
| Building the skin atlas with the separate icons | `main/io/SkinAtlas.cpp` |
| Recalculating the scale | `SetGlobalScale`, in `main/ui/ViewPorts/ViewPorts.cpp` |
| Reading `skin.ini` on PC and Android | `loadColors`, in `main/app/main.cpp` |
| The editor's own colors: the floor, the axes, the normals | `main/ui/W3dColors.cpp` |
| The skin files: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| The extended atlas generator | `tools/fuente_ext.py` |
| The [2D Editor](../editor/editor-2d.md) and games' `.w3dui` interfaces: texts, images, rectangles, containers, 9-slice, buttons and videos | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

The 2D Editor draws text with `W3dTextAtlas`, which belongs to the UI. Baking a TTF at runtime, with stb_truetype, belongs to the editor (`main/io/Fuente2D.cpp`).

## In the UI, but the editor's

Things that are in `libs/WhiskUI/` and still belong to the editor:

- `PropListMeshParts` reads meshes, armatures, animations, constraints and scripts, and calls `RebindMaterialMeshPart()` from the properties panel.
- `PropColor` stores the palette and the object the color comes from (`palRef`, `palObj`), for the color picker.
- `GroupProperties` is the properties panel's global list of cards.
- `core/UI.h` has editor state: Edit Mode's touch view mode (`g_viewEditMode`), mouse movement (`dx`, `dy`), the Alt, Shift and Ctrl keys (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) and the viewport measurements (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- Most of the icons are editor tools.

What's left to separate, and the state of the library: the end of [Using the UI directly](empezar.md).

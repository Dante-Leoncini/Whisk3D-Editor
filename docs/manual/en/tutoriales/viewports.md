# Viewports

The Whisk3D window is divided into **viewports**: panels, each with its own editor (the 3D Viewport, the Outliner, Properties...). You can split them, resize them and choose what each one shows. It works the same with a mouse, with your finger on a touch screen and with a Nokia's keypad.

## The menu bar and the corner button

Every viewport has a **menu bar** at the top (green in the image) with that editor's menus and buttons. The **corner button** (orange) is always at the top left: you use it to change or modify the viewport.

![The four Whisk3D viewports, with each one's menu bar highlighted in green and the corner button in orange](img/viewports-barras.webp)

## Resize

Put your finger (or the mouse) on the corner button and **drag it**. That moves the left and top borders of that viewport, and the ones next to it adjust. Here, the bottom-right one is dragged in circles and all three change size:

![A finger drags the corner button of the bottom-right viewport in circles, and the three viewports change size](img/viewports-arrastrar.webp)

> [!TIP]
> If you tap the button and release it without dragging, its menu opens. On PC you can also drag the border between two viewports.

## The corner menu

Tapping the corner button opens this menu:

![The corner button's menu: the viewport types at the top and, below the line, Expand, Split in Columns, Split in Rows and Maximize](img/viewports-menu.webp)

At the top are the **viewport types**: choosing one turns the viewport into that editor. Below the line is what you can do with it:

| Option | What it does |
|---|---|
| Expand | The viewport takes the space of the one next to it, which closes (if the one next to it is split, that whole group closes). |
| Split in Columns | Splits it in two, side by side. |
| Split in Rows | Splits it in two, one above the other. |
| Maximize | Enlarges it to fill the whole window. While it's maximized, the menu only has **Minimize**, which puts it back in its place. |

### Change the type

The 3D Viewport turned into a UV Editor:

![The corner button of the 3D Viewport is tapped, UV Editor is chosen and the viewport becomes a UV Editor](img/viewports-cambiar-tipo.webp)

### Split in Columns and Split in Rows

![Split in Columns: the 3D Viewport is split into two views, side by side](img/viewports-dividir-columnas.webp)

![Split in Rows: the 3D Viewport is split into two views, one above the other](img/viewports-dividir-filas.webp)

### Expand

The Outliner takes the space of Properties:

![Expand is chosen in the Outliner and it also takes up the space of Properties](img/viewports-expandir.webp)

### Maximize and Minimize

![The 3D Viewport is maximized to fill the whole window and then, with Minimize, goes back to its place](img/viewports-maximizar.webp)

## When the bar doesn't fit

If the viewport is narrow, its bar doesn't fit entirely. The same happens with the toolbar at the bottom of the 3D Viewport. To see the rest:

- **With your finger:** drag it sideways.
- **With the mouse:** hold down the mouse button on the bar and drag, or turn the **wheel** over it. In the 3D Viewport the wheel zooms, so drag its bar instead.
- **With the keyboard:** <kbd>M</kbd> on PC, or the **left soft key** on the Nokia, enters the bar of the active viewport. Go through it with the left and right arrows; each menu opens when you reach it. Move through the menu with up and down, and choose with OK (<kbd>Enter</kbd> on PC). The bar scrolls by itself to show where you are.

![A finger drags the Outliner's bar sideways and the buttons that didn't fit appear](img/viewports-barra-dedo.webp)

![The mouse pointer over the Outliner's bar: with the wheel, the bar scrolls](img/viewports-barra-rueda.webp)

On a Nokia N95: the left soft key enters the bar and the arrows go through it.

![The Nokia N95 interface: the left soft key opens the Mode menu and the right arrow goes through the menus on the bar, which scrolls by itself](img/viewports-barra-nokia.webp)

## Viewport types

| Viewport | What it's for |
|---|---|
| [3D Viewport](../editor/vista-3d.md) | Where you see and edit the scene: model, move objects, paint, pose and animate. |
| [Outliner](../editor/outliner.md) | The list of everything in the scene. |
| [Library](../editor/outliner.md#library-view) | The project's resources: meshes, materials, textures, animations, sounds, scripts and more. |
| [Properties](../editor/propiedades.md) | The data of the active object, in tabs. |
| [UV Editor](../editor/editor-uv.md) | How the texture sits on the mesh. |
| [Timeline](../editor/linea-de-tiempo.md) | The animation: keyframes and curves. |
| [2D Editor](../editor/editor-2d.md) | Game interfaces, HUDs and 2D games. |
| [Console](../editor/consola.md) | The engine log: errors, warnings and information. |
| [IDE](../editor/editor-de-scripts.md) | The editor for Lua scripts and shaders. |

Each viewport will get its own tutorial with more detail. In the meantime, the [Whisk3D manual](../editor/README.md) explains each one.

Whisk3D keeps growing: over time new viewports will be added and the current ones may change. They will all work the same way, with the bar at the top and the button in the corner.

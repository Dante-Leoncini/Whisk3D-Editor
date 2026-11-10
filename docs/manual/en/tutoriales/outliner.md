# The Outliner

> [!NOTE]
> ![Nokia](img/icono-nokia.svg) The Nokia N95 supports a mouse and keyboard (over Bluetooth). The Nokia parts of this page explain how to use it without a mouse or keyboard, with just the phone's keys.

The **Outliner** is the list of everything in the scene: objects, cameras, lights and collections. It's one of the viewports covered in [Viewports](viewports.md); on PC it's at the top right. From here you select objects, hide them, reorder them, put some inside others and rename them.

What you select in the Outliner is also selected in the 3D Viewport, and vice versa. The last one you selected is the **active object**: the one Properties shows.

![Android](img/icono-android.svg) **On Android** the Outliner isn't on the screen: that space is left for the 3D Viewport and Properties. To use it, turn a viewport into an Outliner (or split one) with the corner button, as in [Change the type](viewports.md#change-the-type). You can also work without the Outliner: you select objects by tapping them in the 3D Viewport, and Properties has the name and the **Visible** and **Renderable** checkboxes.

![Nokia](img/icono-nokia.svg) **On the Nokia N95** the Outliner isn't on the screen: only two viewports fit. While you're organizing the scene, turn Properties into an Outliner: with the **left soft key** you enter its tabs and with the left arrow you reach the corner button's menu; choose Outliner there. To go back, the left soft key in the Outliner opens that menu directly. With the **green call key** ![](img/tecla-verde.svg) you make it the active viewport: everything that follows for the N95 is with the green border on the Outliner.

## What each row shows

Each row is an object. The **arrow** collapses or expands what it has inside; the **icon** tells its type (mesh, camera, light, collection...); next comes the **name**, in green if it's the active object and in a darker green if it's selected; and on the right, the **eye** and the **camera**. An object that's inside another one appears further to the right, hanging from it, and a faded row is a hidden object.

In a new project, at the top is the **Constructor**, a routine that sets up the screen, and below it the **Collection** with the camera, the light and the cube. A collection is a folder for organizing things.

## Select

| | How to select |
|---|---|
| ![PC](img/icono-pc.svg) PC | A click on the row selects only that object. <kbd>Shift</kbd> + click selects all the rows from the active one to that one. <kbd>Ctrl</kbd> + click adds or removes one. With the mouse over the Outliner, the up and down arrows go to the previous or the next row. |
| ![Android](img/icono-android.svg) Touch screen | A tap on the row. With **Shift** or **Ctrl** turned on in the 3D Viewport's toolbar, the tap does the same as with the keyboard. Dragging your finger scrolls the list. |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | The up and down arrows go to the previous or the next row, and the list scrolls by itself to follow it. |
| Menu | Select → Select All, Deselect All or Invert Selection |

![A click on Camera selects it; Shift + click on Cube selects from Camera to Cube; a click on Light leaves only the light; Ctrl + click on Cube adds it and another Ctrl + click removes it](img/outliner-seleccionar-raton.webp)

## Collapse and expand

When an object has others inside, its arrow shows or hides them. Collapsing what you're not using keeps the list short.

| | How to collapse and expand |
|---|---|
| ![PC](img/icono-pc.svg) PC | A click on the arrow. With the mouse over the Outliner, the keyboard's right arrow expands the active object and the left one collapses it; if it was already collapsed, the left one goes up to the one containing it and collapses it. |
| ![Android](img/icono-android.svg) Touch screen | A tap on the arrow. |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Right expands the active object and left collapses it; if it was already collapsed, left goes up to the one containing it and collapses it. |

![A click on the Collection arrow hides Camera, Light and Cube; another click shows them again](img/outliner-plegar-raton.webp)

## Hide: the eye and the camera

- The **eye** shows or hides the object in the 3D Viewport. A hidden object doesn't show up in the render either. If it has others inside, they're hidden along with it and their rows look faded.
- The **camera** decides whether it shows up in the **render**. When it's off, you still see the object while you work, but it doesn't appear in the images or videos you render.

Clicking them doesn't change the selection.

| | The eye | The camera |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | Click on the eye, or <kbd>H</kbd> (with the mouse over the Outliner) for the active object | Click on the camera |
| ![Android](img/icono-android.svg) Touch screen | A tap on the eye | A tap on the camera |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>2</kbd>, the active object's | <kbd>3</kbd>, the active object's |
| Menu | Element → Hide/Show (the active object's) | — |

![A click on Cube's eye closes it and the cube disappears from the 3D Viewport; another click shows it again. A click on Cube's camera turns it off, and the cube is still visible](img/outliner-ocultar-raton.webp)

Both are also in Properties, in the Transform card: **Visible** and **Renderable**. They have their diamond, so they can be animated: for example, to make something appear at a certain moment in the video.

## Reorder and put one object inside another

You can change the order of the list, and you can put an object **inside** another one: it becomes its **child**, and when the parent moves, rotates or scales, the child goes along with it. This is what [Object Mode](modo-objeto.md#the-outliner-the-objects-in-the-scene) calls **parenting**. The object doesn't move from its place in the scene: only its place in the list changes.

![PC](img/icono-pc.svg) **With the mouse:** grab the row and drag it. Over the top or bottom edge of another row a **green line** appears: when you drop it, the object ends up before or after that row, at its same level. Over the middle of another row, that row is highlighted: when you drop it, the object ends up **inside** it. Dropped below the last row, it comes out of everything that contained it. Near the top or bottom edge, the list scrolls by itself, so you can reach any row in a long list.

![The Light row is dragged above Camera and the green line puts it first; then Camera is dropped in the middle of the Cube row and ends up inside it, shifted to the right](img/outliner-mover-raton.webp)

![Android](img/icono-android.svg) **With your finger:** hold your finger still on the row for a second, until the phone vibrates: the row is grabbed. Then drag it and drop it just like with the mouse. If you let go without moving it, the object's menu opens. Without that wait, dragging your finger scrolls the list.

![Nokia](img/icono-nokia.svg) **On the Nokia N95:** select the row with the arrows and press <kbd>1</kbd>. The row turns green, and now the arrows move it:

- ![Nokia](img/icono-nokia.svg) **Up** and **down** move it up or down among the ones at its same level.
- ![Nokia](img/icono-nokia.svg) **Right** puts it inside the one above it, at its same level.
- ![Nokia](img/icono-nokia.svg) **Left** takes it out of the one containing it: it ends up after it, at its level.
- ![Nokia](img/icono-nokia.svg) <kbd>1</kbd> again (or <kbd>OK</kbd>) leaves it there. <kbd>C</kbd> cancels and puts it back where it was.

![Nokia](img/icono-nokia.svg) The whole move is undone in one go, with <kbd>C</kbd> + left. It's useful, for example, to put a light before the objects it lights.

![A Nokia N95 with the Outliner active: with 1 the Light row turns green, the up arrow moves it above Camera and 1 leaves it there; with 2 the cube disappears from the 3D Viewport and comes back; with 3 the camera in its row turns off](img/outliner-nokia.webp)

## Find the active object and frame it

In a large scene the list is long, and selecting something in the 3D Viewport doesn't move it: its row may end up out of view. To find it in the list, or to see the object in the 3D Viewport (**frame it**, as in [Frame the selection](navegacion.md#frame-the-selection)):

| | In the Outliner | In the 3D Viewport |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num .</kbd> with the mouse over the Outliner (**Show active**): the list scrolls until the selection is in the middle. If it's inside something collapsed it has no row: expand it first. | <kbd>Num .</kbd> with the mouse over the 3D Viewport |
| ![Android](img/icono-android.svg) Touch screen | Drag the list with your finger | Double tap with two fingers on the 3D Viewport |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | The list follows the row you select with the arrows by itself | A quick press of <kbd>0</kbd>, even if the active viewport is the Outliner |
| Menu | — | View → Frame Selected, on the 3D Viewport's bar |

![A scene with dozens of objects: one is selected in the 3D Viewport and its row isn't visible in the Outliner; with the mouse over the Outliner, Num . scrolls the list and the green row ends up in the middle](img/outliner-activo-raton.webp)

## Rename

| | How to rename |
|---|---|
| ![PC](img/icono-pc.svg) PC | Double click on the name, or <kbd>F2</kbd> for the active object: the row turns into a text field. Type the new name and confirm with <kbd>Enter</kbd> or with a click outside; <kbd>Esc</kbd> leaves it as it was. |
| ![Android](img/icono-android.svg) Touch screen | Double tap on the name: the on-screen keyboard opens. |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | With the left soft key you enter the bar and with the right arrow you reach Element → Rename. Type with T9 and confirm with <kbd>OK</kbd>. |
| Menu | Element → Rename |

![Double click on Cube's name: it turns into a field, Box is typed and Enter confirms it; then Light is selected, F2, Lamp is typed and Enter](img/outliner-renombrar-raton.webp)

Two objects in the same scene can't have the same name: if the name is already in use, Whisk3D adds a number to it and lets you know. If a script referred to the object by name, it's updated by itself.

## Duplicate and delete

| | Duplicate | Delete |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Shift</kbd> + <kbd>D</kbd>; <kbd>Alt</kbd> + <kbd>D</kbd> for Duplicate Linked | <kbd>X</kbd> or <kbd>Delete</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Element → Duplicate | <kbd>C</kbd> |
| Menu | Element → Duplicate or Duplicate Linked | Element → Delete |

From the Outliner, the copy ends up right on top of the original, without following the mouse as in the 3D Viewport: move it afterwards. Deleting asks for confirmation, and both can be undone.

## The Outliner menu bar

| Button | What it has |
|---|---|
| **View options** (the eye) | What is shown: the Constructor, the other routines, and the eye and camera columns (**Visibility icons** and **Render icons**). |
| **Element** (the cube) | Delete, Rename, Move, Hide/Show, Duplicate, Duplicate Linked, Create Prefab and Unpack Prefab. |
| **Select** | Select All, Deselect All and Invert Selection. |
| The scene | Which scene, game or prefab of the project is being edited ([Scenes, games and prefabs](outliner.md#scenes-games-and-prefabs)). |

![PC](img/icono-pc.svg) A right click on a row selects it and opens the same menu right there, at the pointer.

![Android](img/icono-android.svg) With your finger, hold the row until it vibrates and let go without moving it: the same menu opens.

## The Library

The project's **resources** (meshes, materials, textures, animations, sounds, scripts, prefabs...) aren't in the Outliner: they have their own viewport, the **Library**, which you choose with the corner button. It shows thumbnails in folders, tells you which resources are used and which aren't, and you can drag a resource onto the 3D Viewport: a mesh creates an object, and a material is applied to whatever is underneath. It's explained in [Library view](outliner.md#library-view); the rest of the Outliner, in its [manual](outliner.md) and in [Shortcuts](../editor/atajos.md#outliner).

## Library view

| Function | Description |
|---|---|
| Resources | Meshes, materials, textures, animations, prefabs, scenes, sounds, scripts, [shaders](../editor/shaders.md), fonts and videos, in folders. A material or a shader dragged onto a mesh in the 3D Viewport shows on the part under the pointer before you drop it. Green check mark = in use; red cross = orphan; `*` = unsaved. |
| Filter / List or grid | Show only one type; view thumbnails. |
| Drag a resource | Onto a folder (moves it), onto the 3D Viewport (mesh = new object, material or texture = assigned to what you drop it on, prefab = instance) or onto a Properties drop-down. |
| New Folder / Move to Folder | Organize (folders are only for sorting). |
| Duplicate Resource / Select Users / Purge Orphans | Copy, find out who uses it, delete what nobody uses. |
| Inside the .w3d / External | From Properties (Resource card): moves the resource into or out of the project. |

## Scenes, games and prefabs

The button with the scene's name (in the Outliner and in the 3D Viewport) lists the project's scenes, games and prefabs. From there you create new ones, choose the start scene and turn a scene into a game (or the other way around).

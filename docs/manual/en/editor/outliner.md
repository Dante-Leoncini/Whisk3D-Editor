# Outliner

The list of everything in the scene, and also the project's resource library. On Android it isn't shown (you work with the 3D Viewport and Properties).

## View menu (eye)

| Function | Description |
|---|---|
| Scene / Library / Libraries | What is listed: the scene tree, all of the project's resources, or those of another linked .w3d (read-only). |
| Constructor / Routines | Show or hide the Constructor routine (what runs only once at the start) and the other routines. |
| Visibility icons / Render icons | Show or hide the eye and camera columns. |
| Grid | In Library: thumbnails instead of a list. |
| Link .w3d... / Unlink | Uses another project as a library, or releases it. |

## Scene view

| Function | Description | How to use it |
|---|---|---|
| Select | Picks the object. <kbd>Shift</kbd>+click takes a range; <kbd>Ctrl</kbd>+click adds or removes. | Click |
| Expand / collapse | Shows or hides the children. | Arrow; <kbd>Left</kbd> / <kbd>Right</kbd> |
| Visible / Renderable | The eye hides the object (it can be animated); the camera excludes it from the render. | Click on the eye / the camera |
| Reorder and parent | Drop before, after or onto another row (as a child). | Drag the row |
| Move (reorder) | With the keyboard: up/down reorders, left takes it out of its parent, right puts it under the previous one, <kbd>Enter</kbd> confirms. | <kbd>G</kbd> |
| Rename | The row turns into a text field. | Double click or <kbd>F2</kbd> |
| Duplicate / Duplicate Linked | As in the 3D Viewport. | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Hide / Show | Toggles visibility. | <kbd>H</kbd> |
| Delete | With confirmation. | <kbd>X</kbd> or <kbd>Delete</kbd> |
| Create / Unpack Prefab | From the Object menu. | Right click |
| Show active | Scrolls the list to the active object. | <kbd>Num.</kbd> |
| Scroll the list | Wheel, or drag (also with your finger). | Wheel / drag |

## Library view

| Function | Description |
|---|---|
| Resources | Meshes, materials, textures, animations, prefabs, scenes, sounds, scripts, [shaders](shaders.md), fonts and videos, in folders. A material or a shader dragged onto a mesh in the 3D Viewport shows on the part under the pointer before you drop it. Green check mark = in use; red cross = orphan; `*` = unsaved. |
| Filter / List or grid | Show only one type; view thumbnails. |
| Drag a resource | Onto a folder (moves it), onto the 3D Viewport (mesh = new object, material or texture = assigned to what you drop it on, prefab = instance) or onto a Properties drop-down. |
| New Folder / Move to Folder | Organize (folders are only for sorting). |
| Duplicate Resource / Select Users / Purge Orphans | Copy, find out who uses it, delete what nobody uses. |
| Inside the .w3d / External | From Properties (Resource card): moves the resource into or out of the project. |

## Scenes, games and prefabs

The button with the scene's name (in the Outliner and in the 3D Viewport) lists the project's scenes, games and prefabs. From there you create new ones, choose the start scene and turn a scene into a game (or the other way around).

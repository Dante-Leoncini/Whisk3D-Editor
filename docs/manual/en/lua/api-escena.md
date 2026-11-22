# The scene: find, create and destroy

| Function | What it does |
|---|---|
| `find(name [, root]) → obj \| nil` | Finds an object by its name in the outliner (or only inside `root`). |
| `selected() → { obj, … }` | The objects selected in the editor (for [buttons](scripts-y-objetos.md#buttons) that work as editor add-ons). |
| `getName(obj) → text` | The object's name. |
| `getType(obj) → text` | What it is: `"mesh"`, `"light"`, `"camera"`, `"empty"`, `"armature"`, `"text"`, `"image"`, `"button"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` if it doesn't exist). |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | Creates a copy of a prefab at that position (rotY in degrees). `"Libreria/Escena"` creates the one from a linked library. With `parent` it hangs from that object (position and rotation relative to it). |
| `destroy(obj) → bool` | Deletes the object at the end of the frame. |
| `importW3D(path) → obj \| nil` | Adds another .w3d file to the scene while the game is running. |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | Loads or unloads a part of the level by hand (streaming), or sets it back to automatic. |
| `isLoaded(obj) → bool` | Whether that part is already loaded. |

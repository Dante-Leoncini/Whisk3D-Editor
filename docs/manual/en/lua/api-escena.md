# The scene: find, create and destroy

| Function | What it does |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Finds an object by its name in the outliner (or only inside `raiz`). |
| `nombre(obj) → texto` | The object's name. |
| `tipo(obj) → texto` | What it is: `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` if it doesn't exist). |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | Creates a copy of a prefab at that position (rotY in degrees). `"Libreria/Escena"` creates the one from a linked library. With `padre` it hangs from that object (position and rotation relative to it). |
| `destruir(obj) → bool` | Deletes the object at the end of the frame. |
| `importarW3D(ruta) → obj \| nil` | Adds another .w3d file to the scene while the game is running. |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | Loads or unloads a part of the level by hand (streaming), or sets it back to automatic. |
| `cargado(obj) → bool` | Whether that part is already loaded. |

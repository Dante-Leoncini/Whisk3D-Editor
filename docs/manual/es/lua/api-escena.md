# La escena: buscar, crear y destruir

| Función | Qué hace |
|---|---|
| `find(name [, root]) → obj \| nil` | Busca un objeto por su nombre en el outliner (o solo dentro de `root`). |
| `selected() → { obj, … }` | Los objetos elegidos en el editor (para los [botones](scripts-y-objetos.md#botones) que funcionan como complementos del editor). |
| `getName(obj) → text` | El nombre del objeto. |
| `getType(obj) → text` | Qué es: `"mesh"`, `"light"`, `"camera"`, `"empty"`, `"armature"`, `"text"`, `"image"`, `"button"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` si no existe). |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | Crea una copia de un prefab en esa posición (rotY en grados). `"Libreria/Escena"` crea la de una librería vinculada. Con `parent` cuelga de ese objeto (posición y giro relativos a él). |
| `destroy(obj) → bool` | Borra el objeto al final del cuadro. |
| `importW3D(path) → obj \| nil` | Agrega a la escena otro archivo .w3d mientras el juego corre. |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | Carga o descarga a mano una parte del nivel (streaming), o la devuelve a automático. |
| `isLoaded(obj) → bool` | Si esa parte ya está cargada. |

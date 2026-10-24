# La escena: buscar, crear y destruir

| Función | Qué hace |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Busca un objeto por su nombre en el outliner (o solo dentro de `raiz`). |
| `nombre(obj) → texto` | El nombre del objeto. |
| `tipo(obj) → texto` | Qué es: `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` si no existe). |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | Crea una copia de un prefab en esa posición (rotY en grados). `"Libreria/Escena"` crea la de una librería vinculada. Con `padre` cuelga de ese objeto (posición y giro relativos a él). |
| `destruir(obj) → bool` | Borra el objeto al final del cuadro. |
| `importarW3D(ruta) → obj \| nil` | Agrega a la escena otro archivo .w3d mientras el juego corre. |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | Carga o descarga a mano una parte del nivel (streaming), o la devuelve a automático. |
| `cargado(obj) → bool` | Si esa parte ya está cargada. |

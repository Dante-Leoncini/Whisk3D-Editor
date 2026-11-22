# Luces y cámara

| Función | Qué hace |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | El color de una luz. |
| `getEnergy(light) → e`, `setEnergy(light, e)` | Su brillo, de 0 a 1 (para que titile o se apague de a poco). |
| `setActiveCamera(cam)` | Cambia la cámara desde la que se ve el juego. |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | Ángulo de visión y distancias de recorte. |
| `cameraXZ() → fx, fy, rx, ry` | Hacia dónde mira la cámara sobre el piso, en X e Y: adelante (`fx, fy`) y a la derecha (`rx, ry`), para mover al personaje según la cámara. (El nombre es de cuando el piso era XZ.) |
| `screenOf(obj) → x, y, front` | Dónde aparece un objeto 3D en la pantalla (para poner una etiqueta 2D encima). |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | Hacer que la cámara siga una curva (riel). |
| `resolution() → width, height` | Tamaño real de la imagen 3D en píxeles. |

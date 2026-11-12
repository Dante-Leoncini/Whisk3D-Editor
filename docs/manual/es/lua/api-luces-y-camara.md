# Luces y cámara

| Función | Qué hace |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | El color de una luz. |
| `energia(luz) → e`, `setEnergia(luz, e)` | Su brillo, de 0 a 1 (para que titile o se apague de a poco). |
| `setCamaraActiva(cam)` | Cambia la cámara desde la que se ve el juego. |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | Ángulo de visión y distancias de recorte. |
| `camaraXZ() → fx, fy, rx, ry` | Hacia dónde mira la cámara sobre el piso, en X e Y: adelante (`fx, fy`) y a la derecha (`rx, ry`), para mover al personaje según la cámara. (El nombre es de cuando el piso era XZ.) |
| `pantallaDe(obj) → x, y, delante` | Dónde aparece un objeto 3D en la pantalla (para poner una etiqueta 2D encima). |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | Hacer que la cámara siga una curva (riel). |
| `resolucion() → ancho, alto` | Tamaño real de la imagen 3D en píxeles. |

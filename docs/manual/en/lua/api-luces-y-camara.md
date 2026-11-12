# Lights and camera

| Function | What it does |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | The color of a light. |
| `energia(luz) → e`, `setEnergia(luz, e)` | Its brightness, from 0 to 1 (to make it flicker or fade out). |
| `setCamaraActiva(cam)` | Changes the camera the game is seen from. |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | Field of view and clipping distances. |
| `camaraXZ() → fx, fy, rx, ry` | Where the camera is looking on the floor plane, in X and Y: forward (`fx, fy`) and to the right (`rx, ry`), to move the character relative to the camera. (The name is from when the floor was XZ.) |
| `pantallaDe(obj) → x, y, delante` | Where a 3D object appears on the screen (to put a 2D label on top of it). |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | Make the camera follow a curve (rail). |
| `resolucion() → ancho, alto` | Actual size of the 3D image in pixels. |

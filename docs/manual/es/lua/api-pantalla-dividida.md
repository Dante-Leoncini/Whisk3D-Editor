# Pantalla dividida (dos jugadores en el mismo equipo)

| Función | Qué hace |
|---|---|
| `setViews(n [, "side" \| "up"])` | `2` divide la pantalla en dos (una al lado de la otra o una arriba de la otra); `1` vuelve a la pantalla entera. La interfaz 2D (el HUD) se dibuja una vez, encima de las dos. |
| `setCameraView(i, camera)` | La cámara de la vista `i` (1 o 2). Sin poner, la activa. |
| `view() → i, n` | La vista que se está dibujando (1 o 2; 0 fuera del dibujo) y cuántas hay. |
| `beforeView(i)` | (La define tu script.) El motor la llama antes de dibujar cada vista, en el script que llamó a `setViews`. Ahí das vuelta hacia ESA cámara lo que mira a la cámara (carteles, partículas) o escondés lo que esa vista no tiene que ver. No muevas nada que avance con el tiempo: eso va en `update`. |

Cada mitad es una imagen entera con su cámara: el cuadro del juego (el aspecto de la cámara) se parte en dos.

## Un mando para cada uno

`button`, `buttonPressed`, `stick` y `trigger` aceptan el **número de mando** (1 a 4, en el orden en que se movieron
por primera vez): `button("a", 2)`, `stick("left", 1)`. Sin número, cualquiera (todos juntos), como siempre. El teclado
no tiene número: repartilo vos (por ejemplo, el jugador 1 con las flechas y los números, el 2 con WASD y las letras;
en un Nokia con teclado QWERTY las letras también llegan).

```lua
function update(dt)
  local x1, y1 = stick("left", 1)
  local x2, y2 = stick("left", 2)
  if key("w") then y2 = -1 end
  move(jugador1, x1 * dt * 5, y1 * dt * 5, 0)
  move(jugador2, x2 * dt * 5, y2 * dt * 5, 0)
end
```

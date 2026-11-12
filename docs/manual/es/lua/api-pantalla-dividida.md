# Pantalla dividida (dos jugadores en el mismo equipo)

| Función | Qué hace |
|---|---|
| `setVistas(n [, "lado" \| "arriba"])` | `2` divide la pantalla en dos (una al lado de la otra o una arriba de la otra); `1` vuelve a la pantalla entera. La interfaz 2D (el HUD) se dibuja una vez, encima de las dos. |
| `setVistaCamara(i, camara)` | La cámara de la vista `i` (1 o 2). Sin poner, la activa. |
| `vista() → i, n` | La vista que se está dibujando (1 o 2; 0 fuera del dibujo) y cuántas hay. |
| `antesDeVista(i)` | (La define tu script.) El motor la llama antes de dibujar cada vista, en el script que llamó a `setVistas`. Ahí das vuelta hacia ESA cámara lo que mira a la cámara (carteles, partículas) o escondés lo que esa vista no tiene que ver. No muevas nada que avance con el tiempo: eso va en `actualizar`. |

Cada mitad es una imagen entera con su cámara: el cuadro del juego (el aspecto de la cámara) se parte en dos.

## Un mando para cada uno

`boton`, `botonApretado`, `stick` y `gatillo` aceptan el **número de mando** (1 a 4, en el orden en que se movieron
por primera vez): `boton("a", 2)`, `stick("izq", 1)`. Sin número, cualquiera (todos juntos), como siempre. El teclado
no tiene número: repartilo vos (por ejemplo, el jugador 1 con las flechas y los números, el 2 con WASD y las letras;
en un Nokia con teclado QWERTY las letras también llegan).

```lua
function actualizar(dt)
  local x1, y1 = stick("izq", 1)
  local x2, y2 = stick("izq", 2)
  if tecla("w") then y2 = -1 end
  mover(jugador1, x1 * dt * 5, y1 * dt * 5, 0)
  mover(jugador2, x2 * dt * 5, y2 * dt * 5, 0)
end
```

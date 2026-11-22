# Cuándo llama el motor a tu script

Tu script define funciones con nombres especiales y el motor las llama solo, en el momento justo. Todas son opcionales: escribí solo las que necesites.

| Función | Cuándo se llama |
|---|---|
| `start()` | Una vez, al darle Play (o apenas se crea el objeto con `instantiate`). Sirve para buscar objetos y preparar variables. |
| `update(dt)` | En cada cuadro, mientras el objeto esté **visible** y en la escena activa. `dt` son los segundos que pasaron desde el cuadro anterior (entre 0.001 y 0.1). |
| `onEnter(other, myHitbox, otherHitbox)` | Cuando un hitbox empieza a tocar a otro. |
| `onStay(other, myHitbox, otherHitbox)` | En cada cuadro mientras siguen tocándose. |
| `onExit(other, myHitbox, otherHitbox)` | Cuando dejan de tocarse (también si uno se oculta o se desactiva). |
| `onTouch(other, impulse, x, y, z, nx, ny, nz)` | Cuando dos **cuerpos rígidos** chocan. `x, y, z` es el punto de contacto y `nx, ny, nz` la dirección hacia el otro. |

Los eventos de hitbox los reciben los scripts del hitbox y los de su objeto padre. Si el otro es un cuerpo rígido, `otherHitbox` llega como `nil`.

## Mover cosas con dt

Multiplicá siempre las velocidades por `dt`. Así el juego va igual de rápido en una PC potente y en un teléfono lento:

```lua
function update(dt)
  move(self(), 3 * dt, 0, 0)   -- 3 unidades por segundo hacia +X
end
```

## Orden dentro de un cuadro

1. Física (velocidades, cuerpos rígidos) y los eventos de choque: `onTouch`, `onExit`, `onEnter`, `onStay`.
2. `update(dt)` de todos los scripts, en el orden del outliner.
3. Animaciones (escena, clips, esqueletos).
4. Fin del cuadro: se aplican los `destroy()` pendientes, entran los objetos instanciados y se hace el `changeScene()` pedido.

## Al detener el juego

Al apretar Stop se cierran todos los scripts, se borra lo compartido y la escena vuelve a como estaba antes del Play. Si editás un script mientras el juego corre, ese objeto lo recarga: sus variables vuelven a cero y se llama otra vez a `start()`.

# When the engine calls your script

Your script defines functions with special names, and the engine calls them by itself at the right moment. They're all optional: write only the ones you need.

| Function | When it's called |
|---|---|
| `inicio()` | Once, when you press Play (or as soon as the object is created with `instanciar`). Use it to find objects and set up variables. |
| `actualizar(dt)` | Every frame, while the object is **visible** and in the active scene. `dt` is the number of seconds since the previous frame (between 0.001 and 0.1). |
| `alEntrar(otro, miHitbox, suHitbox)` | When a hitbox starts touching another one. |
| `alQuedarse(otro, miHitbox, suHitbox)` | Every frame while they keep touching. |
| `alSalir(otro, miHitbox, suHitbox)` | When they stop touching (also if one of them is hidden or disabled). |
| `alTocar(otro, impulso, x, y, z, nx, ny, nz)` | When two **rigid bodies** collide. `x, y, z` is the contact point and `nx, ny, nz` the direction toward the other one. |

Hitbox events are received by the hitbox's scripts and by its parent object's scripts. If the other one is a rigid body, `suHitbox` arrives as `nil`.

## Moving things with dt

Always multiply speeds by `dt`. That way the game runs just as fast on a powerful PC as on a slow phone:

```lua
function actualizar(dt)
  mover(yo(), 3 * dt, 0, 0)   -- 3 units per second toward +X
end
```

## Order within a frame

1. Physics (velocities, rigid bodies) and the collision events: `alTocar`, `alSalir`, `alEntrar`, `alQuedarse`.
2. `actualizar(dt)` of every script, in outliner order.
3. Animations (scene, clips, armatures).
4. End of the frame: pending `destruir()` calls are applied, instantiated objects come in, and the requested `cambiarEscena()` takes place.

## When the game stops

When you press Stop, all scripts are closed, the shared space is cleared and the scene goes back to how it was before Play. If you edit a script while the game is running, that object reloads it: its variables go back to zero and `inicio()` is called again.

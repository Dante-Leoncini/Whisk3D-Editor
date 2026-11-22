# Cuerpos rígidos, choques y hitboxes

| Función | Qué hace |
|---|---|
| `physicsVel`, `physicsAngVel`, `physicsImpulse`, `physicsForce` | Velocidad, giro, empujones y fuerzas de un cuerpo rígido. |
| `physicsGravity([g])`, `physicsActive(obj, bool)`, `physicsAwake(obj)` | Gravedad (9.81 de fábrica), prender/apagar la física de un objeto. |
| `physicsRay(ox, oy, oz, dx, dy, dz, max) → dist, nx, ny, nz, obj` | Tira un rayo y dice contra qué choca. |
| `contacts() → list` | Todos los choques de este cuadro. |
| `colGround(mesh, x, y, z)`, `colWall(...)`, `colRay(...)` | Choque contra los triángulos de una malla (pisar el suelo, no atravesar paredes). |
| `hitboxActive`, `setHitboxActive`, `setHitboxSize`, `hitboxInside`, `hitboxTouching` | Manejar hitboxes y preguntar qué hay adentro. |
| `showHitboxes(bool)` | Dibuja los hitboxes para depurar. |

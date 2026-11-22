# Física simple (velocidad y rebotes)

| Función | Qué hace |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | Le das una velocidad y el motor lo mueve solo. |
| `accelerate(obj, factor [, tope])` | Lo acelera multiplicando su velocidad. |
| `box(obj [, x, y, z])` | El tamaño de su caja de choque: en 3D, el ancho (X), el fondo (Y) y el alto (Z); en 2D, el ancho y el alto en píxeles. |
| `bounce(obj, other)`, `bounceOn(obj, area [, sides])`, `bounceInside(obj, area)` | Rebotes contra otro objeto o contra los bordes de un área (devuelven `true` si rebotó). |

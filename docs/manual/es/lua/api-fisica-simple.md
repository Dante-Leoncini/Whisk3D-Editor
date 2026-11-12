# Física simple (velocidad y rebotes)

| Función | Qué hace |
|---|---|
| `velocidad(obj, vx [, vy, vz])` / `velocidad(obj) → vx, vy, vz` | Le das una velocidad y el motor lo mueve solo. |
| `acelerar(obj, factor [, tope])` | Lo acelera multiplicando su velocidad. |
| `caja(obj [, x, y, z])` | El tamaño de su caja de choque: en 3D, el ancho (X), el fondo (Y) y el alto (Z); en 2D, el ancho y el alto en píxeles. |
| `rebotar(obj, otro)`, `rebotarEn(obj, area [, lados])`, `rebotarDentro(obj, area)` | Rebotes contra otro objeto o contra los bordes de un área (devuelven `true` si rebotó). |

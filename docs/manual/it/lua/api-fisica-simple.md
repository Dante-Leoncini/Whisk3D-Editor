# Fisica semplice (velocità e rimbalzi)

| Funzione | Cosa fa |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | Gli dai una velocità e il motore lo muove da solo. |
| `accelerate(obj, factor [, tope])` | Lo accelera moltiplicando la sua velocità. |
| `box(obj [, x, y, z])` | La dimensione del suo box di collisione: in 3D, la larghezza (X), la profondità (Y) e l'altezza (Z); in 2D, la larghezza e l'altezza in pixel. |
| `bounce(obj, other)`, `bounceOn(obj, area [, sides])`, `bounceInside(obj, area)` | Rimbalzi contro un altro oggetto o contro i bordi di un'area (restituiscono `true` se è rimbalzato). |

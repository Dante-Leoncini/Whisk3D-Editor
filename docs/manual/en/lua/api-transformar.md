# Move, rotate and scale in 3D

| Function | What it does |
|---|---|
| `posicion(obj) → x, y, z` | Where it is. |
| `setPosicion(obj, x, y, z)` | Puts it in an exact place (ignores physics). |
| `mover(obj, dx, dy, dz)` | Moves it from where it is (use it with `dt`). |
| `rotacion(obj) → x, y, z` | Its rotation in degrees. |
| `setRotacion(obj, x, y, z [, orden])` | Exact rotation, the same one as in the Transform card. `orden` chooses the order in which the axes are applied, for example `"ZXY"` (first the heading around the vertical, like a vehicle). |
| `rotacionX(obj)`, `rotacionY(obj)`, `rotacionZ(obj) → degrees` | The rotation of a single axis. `rotacionZ` is the turn around the vertical. |
| `setRotacionX(obj, g)`, `setRotacionY(obj, g)`, `setRotacionZ(obj, g)` | Changes a single axis; the other two stay the same. For example, `setRotacionZ(obj, rotacionZ(obj) + 90 * dt)` spins it around the vertical. |
| `girar(obj, dx, dy, dz)` | Rotates it around its own axes. |
| `girarHacia(obj, dx, dy [, factor])` | Turns it smoothly toward a direction on the floor (X and Y). |
| `mirarA(obj, x, y, z)` | Points it at a point (the front of a camera). |
| `escala(obj) → sx, sy, sz` | Its scale. |
| `setEscala(obj, s [, sy, sz])` | Exact scale; with a single number it scales uniformly. |
| `escalar(obj, f)` | Multiplies the current scale. |
| `puntoMundo(obj, x, y, z) → x, y, z` | Converts a point of the object to world coordinates. |
| `visible(obj) → bool`, `setVisible(obj, bool)` | Shows or hides it. A hidden object stops receiving `actualizar`. |
| `setTinte(obj, r, g, b [, a])` | Changes its color (values from 0 to 1). |

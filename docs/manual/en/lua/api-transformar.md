# Move, rotate and scale in 3D

| Function | What it does |
|---|---|
| `getPosition(obj) → x, y, z` | Where it is. |
| `setPosition(obj, x, y, z)` | Puts it in an exact place (ignores physics). |
| `move(obj, dx, dy, dz)` | Moves it from where it is (use it with `dt`). |
| `getRotation(obj) → x, y, z` | Its rotation in degrees. |
| `setRotation(obj, x, y, z [, order])` | Exact rotation, the same one as in the Transform card. `order` chooses the order in which the axes are applied, for example `"ZXY"` (first the heading around the vertical, like a vehicle). |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → degrees` | The rotation of a single axis. `getRotationZ` is the turn around the vertical. |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | Changes a single axis; the other two stay the same. For example, `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` spins it around the vertical. |
| `rotate(obj, dx, dy, dz)` | Rotates it around its own axes. |
| `rotateTowards(obj, dx, dy [, factor])` | Turns it smoothly toward a direction on the floor (X and Y). |
| `lookAt(obj, x, y, z)` | Points it at a point (the front of a camera). |
| `getScale(obj) → sx, sy, sz` | Its scale. |
| `setScale(obj, s [, sy, sz])` | Exact scale; with a single number it scales uniformly. |
| `scale(obj, f)` | Multiplies the current scale. |
| `worldPoint(obj, x, y, z) → x, y, z` | Converts a point of the object to world coordinates. |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | Shows or hides it. A hidden object stops receiving `update`. |
| `setTint(obj, r, g, b [, a])` | Changes its color (values from 0 to 1). |

# Mover, rotar y escalar en 3D

| Función | Qué hace |
|---|---|
| `getPosition(obj) → x, y, z` | Dónde está. |
| `setPosition(obj, x, y, z)` | Lo pone en un lugar exacto (ignora la física). |
| `move(obj, dx, dy, dz)` | Lo corre desde donde está (usalo con `dt`). |
| `getRotation(obj) → x, y, z` | Su rotación en grados. |
| `setRotation(obj, x, y, z [, order])` | Rotación exacta, la misma de la tarjeta Transformar. Con `order` se elige en qué orden se aplican los ejes, por ejemplo `"ZXY"` (primero el rumbo sobre la vertical, como un vehículo). |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → degrees` | La rotación de un solo eje. `getRotationZ` es el giro sobre la vertical. |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | Cambia un solo eje; los otros dos quedan igual. Por ejemplo, `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` lo hace girar sobre la vertical. |
| `rotate(obj, dx, dy, dz)` | Lo gira sobre sus propios ejes. |
| `rotateTowards(obj, dx, dy [, factor])` | Lo va girando suave hacia una dirección del piso (X e Y). |
| `lookAt(obj, x, y, z)` | Lo apunta hacia un punto (el frente de una cámara). |
| `getScale(obj) → sx, sy, sz` | Su escala. |
| `setScale(obj, s [, sy, sz])` | Escala exacta; con un solo número escala parejo. |
| `scale(obj, f)` | Multiplica la escala actual. |
| `worldPoint(obj, x, y, z) → x, y, z` | Pasa un punto del objeto a coordenadas del mundo. |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | Lo muestra u oculta. Un objeto oculto deja de recibir `update`. |
| `setTint(obj, r, g, b [, a])` | Le cambia el color (valores de 0 a 1). |

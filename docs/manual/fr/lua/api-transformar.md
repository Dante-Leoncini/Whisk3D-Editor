# Déplacer, faire pivoter et mettre à l'échelle en 3D

| Fonction | Ce qu'elle fait |
|---|---|
| `posicion(obj) → x, y, z` | Où il se trouve. |
| `setPosicion(obj, x, y, z)` | Le place à un endroit précis (ignore la physique). |
| `mover(obj, dx, dy, dz)` | Le décale depuis sa position actuelle (utilisez-la avec `dt`). |
| `rotacion(obj) → x, y, z` | Sa rotation en degrés. |
| `setRotacion(obj, x, y, z [, orden])` | Rotation exacte, la même que celle de la carte Transformer. `orden` permet de choisir dans quel ordre les axes sont appliqués, par exemple `"ZXY"` (d'abord le cap autour de la verticale, comme un véhicule). |
| `rotacionX(obj)`, `rotacionY(obj)`, `rotacionZ(obj) → degrés` | La rotation d'un seul axe. `rotacionZ` est la rotation autour de la verticale. |
| `setRotacionX(obj, g)`, `setRotacionY(obj, g)`, `setRotacionZ(obj, g)` | Change un seul axe ; les deux autres restent pareils. Par exemple, `setRotacionZ(obj, rotacionZ(obj) + 90 * dt)` le fait tourner autour de la verticale. |
| `girar(obj, dx, dy, dz)` | Le fait tourner sur ses propres axes. |
| `girarHacia(obj, dx, dy [, factor])` | Le fait tourner en douceur vers une direction au sol (X et Y). |
| `mirarA(obj, x, y, z)` | L'oriente vers un point (l'avant d'une caméra). |
| `escala(obj) → sx, sy, sz` | Son échelle. |
| `setEscala(obj, s [, sy, sz])` | Échelle exacte ; avec un seul nombre, met à l'échelle uniformément. |
| `escalar(obj, f)` | Multiplie l'échelle actuelle. |
| `puntoMundo(obj, x, y, z) → x, y, z` | Convertit un point de l'objet en coordonnées du monde. |
| `visible(obj) → bool`, `setVisible(obj, bool)` | L'affiche ou le masque. Un objet masqué ne reçoit plus `actualizar`. |
| `setTinte(obj, r, g, b [, a])` | Change sa couleur (valeurs de 0 à 1). |

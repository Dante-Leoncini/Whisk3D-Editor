# Déplacer, faire pivoter et mettre à l'échelle en 3D

| Fonction | Ce qu'elle fait |
|---|---|
| `getPosition(obj) → x, y, z` | Où il se trouve. |
| `setPosition(obj, x, y, z)` | Le place à un endroit précis (ignore la physique). |
| `move(obj, dx, dy, dz)` | Le décale depuis sa position actuelle (utilisez-la avec `dt`). |
| `getRotation(obj) → x, y, z` | Sa rotation en degrés. |
| `setRotation(obj, x, y, z [, order])` | Rotation exacte, la même que celle de la carte Transformer. `order` permet de choisir dans quel ordre les axes sont appliqués, par exemple `"ZXY"` (d'abord le cap autour de la verticale, comme un véhicule). |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → degrés` | La rotation d'un seul axe. `getRotationZ` est la rotation autour de la verticale. |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | Change un seul axe ; les deux autres restent pareils. Par exemple, `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` le fait tourner autour de la verticale. |
| `rotate(obj, dx, dy, dz)` | Le fait tourner sur ses propres axes. |
| `rotateTowards(obj, dx, dy [, factor])` | Le fait tourner en douceur vers une direction au sol (X et Y). |
| `lookAt(obj, x, y, z)` | L'oriente vers un point (l'avant d'une caméra). |
| `getScale(obj) → sx, sy, sz` | Son échelle. |
| `setScale(obj, s [, sy, sz])` | Échelle exacte ; avec un seul nombre, met à l'échelle uniformément. |
| `scale(obj, f)` | Multiplie l'échelle actuelle. |
| `worldPoint(obj, x, y, z) → x, y, z` | Convertit un point de l'objet en coordonnées du monde. |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | L'affiche ou le masque. Un objet masqué ne reçoit plus `update`. |
| `setTint(obj, r, g, b [, a])` | Change sa couleur (valeurs de 0 à 1). |

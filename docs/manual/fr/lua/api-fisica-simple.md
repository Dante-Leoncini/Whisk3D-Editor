# Physique simple (vitesse et rebonds)

| Fonction | Ce qu'elle fait |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | Vous lui donnez une vitesse et le moteur le déplace tout seul. |
| `accelerate(obj, factor [, tope])` | L'accélère en multipliant sa vitesse. |
| `box(obj [, x, y, z])` | La taille de sa boîte de collision : en 3D, la largeur (X), la profondeur (Y) et la hauteur (Z) ; en 2D, la largeur et la hauteur en pixels. |
| `bounce(obj, other)`, `bounceOn(obj, area [, sides])`, `bounceInside(obj, area)` | Rebonds contre un autre objet ou contre les bords d'une zone (renvoient `true` s'il a rebondi). |

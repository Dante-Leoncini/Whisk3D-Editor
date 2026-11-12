# Physique simple (vitesse et rebonds)

| Fonction | Ce qu'elle fait |
|---|---|
| `velocidad(obj, vx [, vy, vz])` / `velocidad(obj) → vx, vy, vz` | Vous lui donnez une vitesse et le moteur le déplace tout seul. |
| `acelerar(obj, factor [, tope])` | L'accélère en multipliant sa vitesse. |
| `caja(obj [, x, y, z])` | La taille de sa boîte de collision : en 3D, la largeur (X), la profondeur (Y) et la hauteur (Z) ; en 2D, la largeur et la hauteur en pixels. |
| `rebotar(obj, otro)`, `rebotarEn(obj, area [, lados])`, `rebotarDentro(obj, area)` | Rebonds contre un autre objet ou contre les bords d'une zone (renvoient `true` s'il a rebondi). |

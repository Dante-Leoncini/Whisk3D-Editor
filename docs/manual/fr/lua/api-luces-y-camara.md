# Lumières et caméra

| Fonction | Ce qu'elle fait |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | La couleur d'une lumière. |
| `getEnergy(light) → e`, `setEnergy(light, e)` | Sa luminosité, de 0 à 1 (pour la faire scintiller ou s'éteindre progressivement). |
| `setActiveCamera(cam)` | Change la caméra depuis laquelle on voit le jeu. |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | Angle de vue et distances de découpe. |
| `cameraXZ() → fx, fy, rx, ry` | La direction dans laquelle la caméra regarde sur le sol, en X et Y : vers l'avant (`fx, fy`) et vers la droite (`rx, ry`), pour déplacer le personnage selon la caméra. (Le nom date de l'époque où le sol était XZ.) |
| `screenOf(obj) → x, y, front` | L'endroit où un objet 3D apparaît à l'écran (pour placer une étiquette 2D au-dessus). |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | Faire suivre une courbe (rail) à la caméra. |
| `resolution() → width, height` | Taille réelle de l'image 3D en pixels. |

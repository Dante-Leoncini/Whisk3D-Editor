# Luci e camera

| Funzione | Cosa fa |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | Il colore di una luce. |
| `getEnergy(light) → e`, `setEnergy(light, e)` | La sua luminosità, da 0 a 1 (per farla tremolare o spegnerla gradualmente). |
| `setActiveCamera(cam)` | Cambia la camera da cui si vede il gioco. |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | Angolo di visione e distanze di clipping. |
| `cameraXZ() → fx, fy, rx, ry` | Verso dove guarda la camera sul pavimento, in X e Y: in avanti (`fx, fy`) e a destra (`rx, ry`), per muovere il personaggio in base alla camera. (Il nome risale a quando il pavimento era XZ.) |
| `screenOf(obj) → x, y, front` | Dove appare un oggetto 3D sullo schermo (per metterci sopra un'etichetta 2D). |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | Far seguire alla camera una curva (binario). |
| `resolution() → width, height` | Dimensione reale dell'immagine 3D in pixel. |

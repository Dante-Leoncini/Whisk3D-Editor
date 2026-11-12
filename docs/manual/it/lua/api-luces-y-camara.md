# Luci e camera

| Funzione | Cosa fa |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | Il colore di una luce. |
| `energia(luz) → e`, `setEnergia(luz, e)` | La sua luminosità, da 0 a 1 (per farla tremolare o spegnerla gradualmente). |
| `setCamaraActiva(cam)` | Cambia la camera da cui si vede il gioco. |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | Angolo di visione e distanze di clipping. |
| `camaraXZ() → fx, fy, rx, ry` | Verso dove guarda la camera sul pavimento, in X e Y: in avanti (`fx, fy`) e a destra (`rx, ry`), per muovere il personaggio in base alla camera. (Il nome risale a quando il pavimento era XZ.) |
| `pantallaDe(obj) → x, y, delante` | Dove appare un oggetto 3D sullo schermo (per metterci sopra un'etichetta 2D). |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | Far seguire alla camera una curva (binario). |
| `resolucion() → ancho, alto` | Dimensione reale dell'immagine 3D in pixel. |

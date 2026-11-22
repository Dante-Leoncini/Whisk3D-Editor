# Interfaccia 2D e schermo

| Funzione | Cosa fa |
|---|---|
| `screen() → width, height` | Dimensione della tela. |
| `screenMargin() → mx, my` | Quanto schermo avanza per lato attorno a una tela di dimensione fissa, in unità della tela, fino ai bordi reali (anche le bande dell'inquadratura sono schermo). In verticale con una tela 4:3: spostare un indicatore di `my` verso l'alto lo attacca al bordo superiore. |
| `screenFrame() → x, y, width, height` | Il riquadro dell'immagine 3D (l'inquadratura della camera, senza le bande) in unità della tela, dal suo centro. |
| `captureScreen(name [, withHud]) → path` | Copia l'immagine 3D di questo fotogramma (alla fine della scena; con `withHud`, dopo l'HUD) in una texture. Il percorso (`"pantalla:<name>"`) serve per `setTexture`, un materiale o il passo "Bind texture" di una routine. |
| `captureUV(name) → u0, v0, u1, v1` | La parte della texture catturata con l'immagine dritta, per `setUVRect` (o `nil` se la copia non è ancora stata fatta: si fa quando si disegna il fotogramma). |
| `safeArea() → x, y, w, h` | La parte dello schermo senza notch (non usarla in `start`). |
| `getScale() → f` | Senza argomenti: fattore per adattare le dimensioni allo schermo (lato minore / 480). |
| `getPosPx(obj) → x, y`, `setPosPx(obj, x, y)` | Posizione di un elemento 2D (un `nil` lascia invariato quell'asse). |
| `getSizePx(obj) → w, h`, `setSizePx(obj, w, h)` | Dimensione di un elemento 2D. |
| `uiBox(obj) → x, y, w, h` | Il rettangolo che occupa davvero (x, y = centro). |
| `setText(obj, value)`, `setSize(text, px)` | Cambia il testo di un testo o di un pulsante, e la dimensione del carattere. |
| `setTexture(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Cambia l'immagine, ne ritaglia una parte (con u1 < u0 o v1 < v0, specchiata), o anima un foglio di fotogrammi. |
| `setOpacity(obj, 0..1)`, `fade(obj, objetivo, paso)` | Trasparenza e dissolvenze. |
| `setRot2D(obj, degrees)` | Ruota un elemento 2D. |
| `collide(a, b) → bool` | Se due elementi 2D si sovrappongono sullo schermo. |
| `limit(obj, area [, axis])`, `inside(area, x [, y])` | Mantenere un elemento dentro un altro, o chiedere se un punto è dentro. |
| `quit()` | Chiude il gioco (nell'editor non fa nulla). |

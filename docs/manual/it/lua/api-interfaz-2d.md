# Interfaccia 2D e schermo

| Funzione | Cosa fa |
|---|---|
| `pantalla() → ancho, alto` | Dimensione della tela. |
| `margenPantalla() → mx, my` | Quanto schermo avanza per lato attorno a una tela di dimensione fissa, in unità della tela, fino ai bordi reali (anche le bande dell'inquadratura sono schermo). In verticale con una tela 4:3: spostare un indicatore di `my` verso l'alto lo attacca al bordo superiore. |
| `marcoPantalla() → x, y, ancho, alto` | Il riquadro dell'immagine 3D (l'inquadratura della camera, senza le bande) in unità della tela, dal suo centro. |
| `capturarPantalla(nombre [, conHud]) → ruta` | Copia l'immagine 3D di questo fotogramma (alla fine della scena; con `conHud`, dopo l'HUD) in una texture. Il percorso (`"pantalla:<nombre>"`) serve per `setTextura`, un materiale o il passo "Bind texture" di una routine. |
| `capturaUV(nombre) → u0, v0, u1, v1` | La parte della texture catturata con l'immagine dritta, per `setUVRect` (o `nil` se la copia non è ancora stata fatta: si fa quando si disegna il fotogramma). |
| `areaSegura() → x, y, w, h` | La parte dello schermo senza notch (non usarla in `inicio`). |
| `escala() → f` | Senza argomenti: fattore per adattare le dimensioni allo schermo (lato minore / 480). |
| `posPx(obj) → x, y`, `setPosPx(obj, x, y)` | Posizione di un elemento 2D (un `nil` lascia invariato quell'asse). |
| `tamPx(obj) → w, h`, `setTamPx(obj, w, h)` | Dimensione di un elemento 2D. |
| `cajaUI(obj) → x, y, w, h` | Il rettangolo che occupa davvero (x, y = centro). |
| `setTexto(obj, valor)`, `setTam(texto, px)` | Cambia il testo di un testo o di un pulsante, e la dimensione del carattere. |
| `setTextura(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Cambia l'immagine, ne ritaglia una parte (con u1 < u0 o v1 < v0, specchiata), o anima un foglio di fotogrammi. |
| `setOpacidad(obj, 0..1)`, `fundir(obj, objetivo, paso)` | Trasparenza e dissolvenze. |
| `setRot2D(obj, grados)` | Ruota un elemento 2D. |
| `chocan(a, b) → bool` | Se due elementi 2D si sovrappongono sullo schermo. |
| `limitar(obj, area [, eje])`, `dentro(area, x [, y])` | Mantenere un elemento dentro un altro, o chiedere se un punto è dentro. |
| `salir()` | Chiude il gioco (nell'editor non fa nulla). |

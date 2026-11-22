# Interface 2D et écran

| Fonction | Ce qu'elle fait |
|---|---|
| `screen() → width, height` | Taille du canevas. |
| `screenMargin() → mx, my` | La place d'écran qui reste de chaque côté autour d'un canevas de taille fixe, en unités du canevas, jusqu'aux bords réels (les bandes du cadrage font aussi partie de l'écran). En portrait avec un canevas 4:3 : remonter une jauge de `my` la colle au bord supérieur. |
| `screenFrame() → x, y, width, height` | Le cadre de l'image 3D (le cadrage de la caméra, sans les bandes) en unités du canevas, depuis son centre. |
| `captureScreen(name [, withHud]) → path` | Copie dans une texture le rendu 3D de cette image (à la fin de la scène ; avec `withHud`, après le HUD). Le chemin (`"pantalla:<name>"`) sert pour `setTexture`, un matériau ou l'étape « Bind texture » d'une routine. |
| `captureUV(name) → u0, v0, u1, v1` | La partie de la texture capturée qui contient l'image à l'endroit, pour `setUVRect` (ou `nil` si la copie n'a pas encore été faite : elle est faite quand l'image est dessinée). |
| `safeArea() → x, y, w, h` | La partie de l'écran sans encoche (à ne pas utiliser dans `start`). |
| `getScale() → f` | Sans arguments : facteur pour adapter les tailles à l'écran (petit côté / 480). |
| `getPosPx(obj) → x, y`, `setPosPx(obj, x, y)` | Position d'un élément 2D (un `nil` laisse cet axe inchangé). |
| `getSizePx(obj) → w, h`, `setSizePx(obj, w, h)` | Taille d'un élément 2D. |
| `uiBox(obj) → x, y, w, h` | Le rectangle qu'il occupe réellement (x, y = centre). |
| `setText(obj, value)`, `setSize(text, px)` | Change le texte d'un texte ou d'un bouton, et la taille de police. |
| `setTexture(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Change l'image, en découpe une partie (en miroir si u1 < u0 ou v1 < v0) ou anime une planche d'images. |
| `setOpacity(obj, 0..1)`, `fade(obj, objetivo, paso)` | Transparence et fondus. |
| `setRot2D(obj, degrees)` | Fait pivoter un élément 2D. |
| `collide(a, b) → bool` | Si deux éléments 2D se chevauchent à l'écran. |
| `limit(obj, area [, axis])`, `inside(area, x [, y])` | Garder un élément à l'intérieur d'un autre, ou demander si un point est à l'intérieur. |
| `quit()` | Ferme le jeu (dans l'éditeur, ne fait rien). |

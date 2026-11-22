# Interfaz 2D y pantalla

| Función | Qué hace |
|---|---|
| `screen() → width, height` | Tamaño del lienzo. |
| `screenMargin() → mx, my` | Cuánta pantalla sobra por lado alrededor de un lienzo de tamaño fijo, en unidades del lienzo, hasta los bordes reales (las bandas del encuadre también son pantalla). En vertical con un lienzo 4:3: correr un medidor `my` para arriba lo pega al borde de arriba. |
| `screenFrame() → x, y, width, height` | El cuadro de la imagen 3D (el encuadre de la cámara, sin las bandas) en unidades del lienzo, desde su centro. |
| `captureScreen(name [, withHud]) → path` | Copia la imagen 3D de este cuadro (al terminar la escena; con `withHud`, después del HUD) a una textura. La ruta (`"pantalla:<name>"`) sirve para `setTexture`, un material o el paso "Bind texture" de una rutina. |
| `captureUV(name) → u0, v0, u1, v1` | La parte de la textura capturada con la imagen derecha, para `setUVRect` (o `nil` si la copia todavía no se hizo: se hace al dibujar el cuadro). |
| `safeArea() → x, y, w, h` | La parte de la pantalla sin notch (no usar en `start`). |
| `getScale() → f` | Sin argumentos: factor para adaptar tamaños a la pantalla (lado menor / 480). |
| `getPosPx(obj) → x, y`, `setPosPx(obj, x, y)` | Posición de un elemento 2D (un `nil` deja ese eje igual). |
| `getSizePx(obj) → w, h`, `setSizePx(obj, w, h)` | Tamaño de un elemento 2D. |
| `uiBox(obj) → x, y, w, h` | El rectángulo que ocupa de verdad (x, y = centro). |
| `setText(obj, value)`, `setSize(text, px)` | Cambia el texto de un texto o botón, y el tamaño de letra. |
| `setTexture(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Cambia la imagen, recorta una parte (con u1 < u0 o v1 < v0, espejada), o anima una hoja de cuadros. |
| `setOpacity(obj, 0..1)`, `fade(obj, objetivo, paso)` | Transparencia y fundidos. |
| `setRot2D(obj, degrees)` | Rota un elemento 2D. |
| `collide(a, b) → bool` | Si dos elementos 2D se superponen en pantalla. |
| `limit(obj, area [, axis])`, `inside(area, x [, y])` | Mantener un elemento adentro de otro, o preguntar si un punto está adentro. |
| `quit()` | Cierra el juego (en el editor no hace nada). |

# Interface 2D e tela

| Função | O que faz |
|---|---|
| `screen() → width, height` | Tamanho do canvas. |
| `screenMargin() → mx, my` | Quanto de tela sobra de cada lado ao redor de um canvas de tamanho fixo, em unidades do canvas, até as bordas reais (as faixas do enquadramento também são tela). Na vertical, com um canvas 4:3: mover um medidor `my` para cima o encosta na borda de cima. |
| `screenFrame() → x, y, width, height` | O quadro da imagem 3D (o enquadramento da câmera, sem as faixas) em unidades do canvas, a partir do centro dele. |
| `captureScreen(name [, withHud]) → path` | Copia a imagem 3D deste quadro (ao terminar a cena; com `withHud`, depois do HUD) para uma textura. O caminho (`"pantalla:<name>"`) serve para `setTexture`, para um material ou para o passo "Bind texture" de uma rotina. |
| `captureUV(name) → u0, v0, u1, v1` | A parte da textura capturada com a imagem na orientação certa, para `setUVRect` (ou `nil` se a cópia ainda não foi feita: ela é feita ao desenhar o quadro). |
| `safeArea() → x, y, w, h` | A parte da tela sem notch (não usar em `start`). |
| `getScale() → f` | Sem argumentos: fator para adaptar tamanhos à tela (lado menor / 480). |
| `getPosPx(obj) → x, y`, `setPosPx(obj, x, y)` | Posição de um elemento 2D (um `nil` deixa esse eixo como está). |
| `getSizePx(obj) → w, h`, `setSizePx(obj, w, h)` | Tamanho de um elemento 2D. |
| `uiBox(obj) → x, y, w, h` | O retângulo que ele ocupa de verdade (x, y = centro). |
| `setText(obj, value)`, `setSize(text, px)` | Muda o texto de um texto ou botão, e o tamanho da letra. |
| `setTexture(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Muda a imagem, recorta uma parte (com u1 < u0 ou v1 < v0, espelhada) ou anima uma folha de quadros. |
| `setOpacity(obj, 0..1)`, `fade(obj, objetivo, paso)` | Transparência e fades. |
| `setRot2D(obj, degrees)` | Gira um elemento 2D. |
| `collide(a, b) → bool` | Se dois elementos 2D se sobrepõem na tela. |
| `limit(obj, area [, axis])`, `inside(area, x [, y])` | Manter um elemento dentro de outro, ou perguntar se um ponto está dentro. |
| `quit()` | Fecha o jogo (no editor não faz nada). |

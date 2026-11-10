# Interface 2D e tela

| Função | O que faz |
|---|---|
| `pantalla() → ancho, alto` | Tamanho do canvas. |
| `margenPantalla() → mx, my` | Quanto de tela sobra de cada lado ao redor de um canvas de tamanho fixo, em unidades do canvas, até as bordas reais (as faixas do enquadramento também são tela). Na vertical, com um canvas 4:3: mover um medidor `my` para cima o encosta na borda de cima. |
| `marcoPantalla() → x, y, ancho, alto` | O quadro da imagem 3D (o enquadramento da câmera, sem as faixas) em unidades do canvas, a partir do centro dele. |
| `capturarPantalla(nombre [, conHud]) → ruta` | Copia a imagem 3D deste quadro (ao terminar a cena; com `conHud`, depois do HUD) para uma textura. O caminho (`"pantalla:<nombre>"`) serve para `setTextura`, para um material ou para o passo "Bind texture" de uma rotina. |
| `capturaUV(nombre) → u0, v0, u1, v1` | A parte da textura capturada com a imagem na orientação certa, para `setUVRect` (ou `nil` se a cópia ainda não foi feita: ela é feita ao desenhar o quadro). |
| `areaSegura() → x, y, w, h` | A parte da tela sem notch (não usar em `inicio`). |
| `escala() → f` | Sem argumentos: fator para adaptar tamanhos à tela (lado menor / 480). |
| `posPx(obj) → x, y`, `setPosPx(obj, x, y)` | Posição de um elemento 2D (um `nil` deixa esse eixo como está). |
| `tamPx(obj) → w, h`, `setTamPx(obj, w, h)` | Tamanho de um elemento 2D. |
| `cajaUI(obj) → x, y, w, h` | O retângulo que ele ocupa de verdade (x, y = centro). |
| `setTexto(obj, valor)`, `setTam(texto, px)` | Muda o texto de um texto ou botão, e o tamanho da letra. |
| `setTextura(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Muda a imagem, recorta uma parte (com u1 < u0 ou v1 < v0, espelhada) ou anima uma folha de quadros. |
| `setOpacidad(obj, 0..1)`, `fundir(obj, objetivo, paso)` | Transparência e fades. |
| `setRot2D(obj, grados)` | Gira um elemento 2D. |
| `chocan(a, b) → bool` | Se dois elementos 2D se sobrepõem na tela. |
| `limitar(obj, area [, eje])`, `dentro(area, x [, y])` | Manter um elemento dentro de outro, ou perguntar se um ponto está dentro. |
| `salir()` | Fecha o jogo (no editor não faz nada). |

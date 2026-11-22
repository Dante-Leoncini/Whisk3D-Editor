# Mover, rotacionar e escalar em 3D

| Função | O que faz |
|---|---|
| `getPosition(obj) → x, y, z` | Onde ele está. |
| `setPosition(obj, x, y, z)` | Coloca-o em um lugar exato (ignora a física). |
| `move(obj, dx, dy, dz)` | Desloca-o a partir de onde está (use com `dt`). |
| `getRotation(obj) → x, y, z` | A sua rotação em graus. |
| `setRotation(obj, x, y, z [, order])` | Rotação exata, a mesma do cartão Transformar. Com `order` se escolhe em que ordem os eixos são aplicados, por exemplo `"ZXY"` (primeiro o rumo em torno da vertical, como um veículo). |
| `getRotationX(obj)`, `getRotationY(obj)`, `getRotationZ(obj) → graus` | A rotação de um só eixo. `getRotationZ` é o giro em torno da vertical. |
| `setRotationX(obj, g)`, `setRotationY(obj, g)`, `setRotationZ(obj, g)` | Muda um só eixo; os outros dois ficam iguais. Por exemplo, `setRotationZ(obj, getRotationZ(obj) + 90 * dt)` faz ele girar em torno da vertical. |
| `rotate(obj, dx, dy, dz)` | Gira-o sobre os seus próprios eixos. |
| `rotateTowards(obj, dx, dy [, factor])` | Vai girando-o suavemente em direção a uma direção do chão (X e Y). |
| `lookAt(obj, x, y, z)` | Aponta-o para um ponto (a frente de uma câmera). |
| `getScale(obj) → sx, sy, sz` | A sua escala. |
| `setScale(obj, s [, sy, sz])` | Escala exata; com um só número, escala por igual. |
| `scale(obj, f)` | Multiplica a escala atual. |
| `worldPoint(obj, x, y, z) → x, y, z` | Passa um ponto do objeto para coordenadas do mundo. |
| `isVisible(obj) → bool`, `setVisible(obj, bool)` | Mostra-o ou oculta-o. Um objeto oculto deixa de receber `update`. |
| `setTint(obj, r, g, b [, a])` | Muda a sua cor (valores de 0 a 1). |

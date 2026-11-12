# Luzes e câmera

| Função | O que faz |
|---|---|
| `color(luz) → r, g, b`, `setColor(luz, r, g, b)` | A cor de uma luz. |
| `energia(luz) → e`, `setEnergia(luz, e)` | O seu brilho, de 0 a 1 (para piscar ou se apagar aos poucos). |
| `setCamaraActiva(cam)` | Muda a câmera pela qual o jogo é visto. |
| `lenteDe(cam) → fov, cerca, lejos`, `setLente(cam [, fov, cerca, lejos])` | Ângulo de visão e distâncias de recorte. |
| `camaraXZ() → fx, fy, rx, ry` | Para onde a câmera olha sobre o chão, em X e Y: para a frente (`fx, fy`) e para a direita (`rx, ry`), para mover o personagem de acordo com a câmera. (O nome é de quando o chão era XZ.) |
| `pantallaDe(obj) → x, y, delante` | Onde um objeto 3D aparece na tela (para colocar uma etiqueta 2D em cima). |
| `setRiel(cam, curva [, offset])`, `setRielNodo`, `setMiradaRiel`, `rielDe` | Fazer a câmera seguir uma curva (trilho). |
| `resolucion() → ancho, alto` | Tamanho real da imagem 3D em pixels. |

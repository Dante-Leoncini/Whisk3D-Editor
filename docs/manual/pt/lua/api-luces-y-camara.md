# Luzes e câmera

| Função | O que faz |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | A cor de uma luz. |
| `getEnergy(light) → e`, `setEnergy(light, e)` | O seu brilho, de 0 a 1 (para piscar ou se apagar aos poucos). |
| `setActiveCamera(cam)` | Muda a câmera pela qual o jogo é visto. |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | Ângulo de visão e distâncias de recorte. |
| `cameraXZ() → fx, fy, rx, ry` | Para onde a câmera olha sobre o chão, em X e Y: para a frente (`fx, fy`) e para a direita (`rx, ry`), para mover o personagem de acordo com a câmera. (O nome é de quando o chão era XZ.) |
| `screenOf(obj) → x, y, front` | Onde um objeto 3D aparece na tela (para colocar uma etiqueta 2D em cima). |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | Fazer a câmera seguir uma curva (trilho). |
| `resolution() → width, height` | Tamanho real da imagem 3D em pixels. |

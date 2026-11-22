# Física simples (velocidade e quiques)

| Função | O que faz |
|---|---|
| `getVelocity(obj, vx [, vy, vz])` / `getVelocity(obj) → vx, vy, vz` | Você dá uma velocidade, e o motor o move sozinho. |
| `accelerate(obj, factor [, tope])` | Acelera-o multiplicando a sua velocidade. |
| `box(obj [, x, y, z])` | O tamanho da sua caixa de colisão: em 3D, a largura (X), a profundidade (Y) e a altura (Z); em 2D, a largura e a altura em pixels. |
| `bounce(obj, other)`, `bounceOn(obj, area [, sides])`, `bounceInside(obj, area)` | Quiques contra outro objeto ou contra as bordas de uma área (devolvem `true` se quicou). |

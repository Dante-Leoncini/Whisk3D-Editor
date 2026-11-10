# Física

Tudo feito em casa, em C++03, sem motor externo e sem pedir memória a cada quadro. Se ninguém a usa, não custa nada: cada passo sai na primeira linha.

- **W3dFisica:** velocidade e quique de caixas alinhadas aos eixos. Nada mais.
- **W3dRigido:** caixas orientadas com massa, gravidade, impulsos com atrito e quique, raycast e eventos de contato.
- **W3dColMalla:** andar sobre os triângulos de um cenário (rampas, degraus) e ser barrado pelas paredes, com uma grade para não verificar milhares de triângulos.
- **W3dHitbox:** caixas que só avisam (`alEntrar`, `alQuedarse`, `alSalir`, `alTocar`).
- **W3dRagdoll:** bonecos de pano, e cinemática inversa de dois ossos e FABRIK.
- **W3dNav:** o caminho de um ponto a outro sobre uma grade do chão (A* e depois esticado).

Por que importa: o Play do editor e o jogo compilado chamam o mesmo passo com o mesmo `dt`, então o jogo se comporta igual nos dois, no PC e no N95. Em Lua: [Física simples](../lua/api-fisica-simple.md) e [Corpos rígidos, colisões e hitboxes](../lua/api-cuerpos-rigidos.md).

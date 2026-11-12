# Eixos, unidades e objetos

- **3D:** nos scripts, o eixo **Z aponta para cima**, igual ao painel de Propriedades: X e Y são o chão (a largura e a profundidade) e Z é a altura. O que você vê no painel é o que o script lê e escreve. As posições, rotações e escalas são relativas ao pai do objeto. As rotações são em **graus**.
- **2D (tela e UI):** pixels do canvas, com o (0, 0) no **centro**; +x para a direita e +y para **baixo**.
- **Objetos:** as funções que buscam ou devolvem objetos (`yo`, `buscar`, `objeto`…) dão um valor que não tem campos nem métodos: você o passa para outras funções. Dois objetos podem ser comparados com `==`. Se o objeto foi destruído, ele se comporta como `nil`.
- **Tolerância:** quase todas as funções aceitam `nil` ou um objeto do tipo errado sem quebrar nada: as que leem devolvem 0, `""` ou `false`, e as que escrevem não fazem nada.
- **Índices:** como em Lua, quase tudo começa em 1 (vértices, dedos, camadas, passos de rotina). A exceção são as memórias das rotinas (`memoria`), que vão de 0 a 255.

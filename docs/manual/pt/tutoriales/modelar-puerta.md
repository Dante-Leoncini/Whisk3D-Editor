# Modelar uma porta

Vamos fazer o nosso primeiro modelo simples: uma porta! Para começar, serve o cubo que vem na cena, que mede 2 × 2 × 2 metros.

Eu (Dante) uso muito as portas para criar espaços: é algo que aprendi estudando construção. As portas quase sempre medem 2 metros de altura, e uma pessoa (normalmente) é mais baixa que uma porta. Com uma porta na cena, você tem na hora a escala de todo o resto.

## Apoiá-la no chão

O cubo tem o centro no chão, então metade dele fica embaixo. Vamos subi-lo 1 metro no eixo Z para que fique apoiado. Com o cubo selecionado:

![PC](img/icono-pc.svg) Aperte <kbd>G</kbd>: o cubo segue o mouse. Depois <kbd>Z</kbd>: ele fica preso ao eixo Z (aparece a linha azul). Digite `1` e aperte <kbd>Enter</kbd>.

![Com G o cubo segue o mouse; com Z fica na linha azul do eixo vertical; com 1 sobe exatamente 1 metro e Enter o deixa ali, apoiado no chão](img/modelar-subir-raton.webp)

![Android](img/icono-android.svg) Toque em **Mover** na barra de ferramentas, depois em **Z**, um toque na barra de cima para o teclado numérico, `1`, **OK** e o visto verde.

![Toca-se em Mover e em Z na barra de ferramentas; um toque na barra de cima abre o teclado numérico, 1 e OK sobem o cubo e o visto conclui; embaixo aparece o painel Mover, que se expande com um toque e se fecha com Aceitar](img/modelar-subir-dedo.webp)

![Nokia](img/icono-nokia.svg) <kbd>1</kbd> (mover), <kbd>3</kbd> (o eixo Z), o **lápis** mantido e <kbd>1</kbd>, e <kbd>OK</kbd>.

![Um Nokia N95: o 1 começa a mover, o 3 o deixa no eixo Z, o lápis e o 1 digitam o valor e o OK confirma](img/modelar-subir-nokia.webp)

Como digitar os números em cada dispositivo está em [Com números exatos](modo-objeto.md#com-números-exatos).

### Corrigir a última operação

Depois de mover, rotacionar ou escalar, aparece embaixo à esquerda um painel pequeno, **Ajustar a última operação**, no lugar da barra de ferramentas. Recolhido, ele só diz o que você fez (por exemplo, **Mover**); um toque ou um clique no título o expande:

![O painel Mover expandido: X 0, Y 0 e Z 1 metro, a orientação Global, a caixa de edição proporcional e os botões Cancelar e Aceitar](img/modelar-ficha.webp)

Cada valor pode ser corrigido: se em vez de 1 metro você queria 1,2, toca em Z, digita `1.2` e a operação é refeita com o valor novo. **Aceitar** o fecha e a barra de ferramentas volta; **Cancelar** desfaz a operação.

## Dar forma

Há duas formas de modelar a porta.

### A mais fácil, que eu não recomendo: escalar o objeto

No Modo Objeto, escale o cubo no eixo X e no eixo Y até ele ter a forma da porta. Por exemplo, <kbd>S</kbd> <kbd>X</kbd> `0.45` <kbd>Enter</kbd> (90 cm de largura) e <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd> (10 cm de espessura).

Não a recomendo porque o objeto fica com uma escala diferente em cada eixo (0,45, 0,05 e 1), e o ideal é que ele tenha escala **1** nos três eixos (veja [Melhor com a escala em 1](modo-objeto.md#melhor-com-a-escala-em-1)). Se você chegar a fazer assim, **sempre aplique a transformação**: <kbd>Ctrl</kbd> + <kbd>A</kbd> → **Escalar** (ou Objeto → Aplicar → Escalar). A forma não muda e a escala volta para 1.

![No Modo Objeto, S X 0.45 e S Y 0.05 deixam o cubo com forma de porta, e as Propriedades mostram a escala diferente em cada eixo; Ctrl+A e Escalar a aplicam e a escala volta para 1](img/modelar-escalar-raton.webp)

![Android](img/icono-android.svg) Em uma tela touch: **Escalar**, **X**, a barra de cima, `0.45` e OK, e o mesmo com **Y** e `0.05`. Depois, **Objeto → Aplicar → Escalar**.

![Escalar em X com 0.45 e em Y com 0.05 pela barra de ferramentas e pelo teclado numérico; depois Objeto, Aplicar, Escalar, e a escala volta para 1](img/modelar-escalar-dedo.webp)

### A que eu recomendo: o Modo Edição

Com o cubo selecionado, entre no Modo Edição: aperte <kbd>Tab</kbd> ou escolha **Modo Edição** no botão do modo da Viewport 3D. Ao entrar, todos os vértices ficam selecionados.

No Modo Edição você seleciona **vértices**, **arestas** e **faces**: isso é o básico. E, igual ao Modo Objeto, eles podem ser movidos, rotacionados e escalados (veja [Modo Edição](../editor/modo-edicion.md)).

Com tudo selecionado, diminua em X e em Y até ter a porta do jeito que você quer. Em Z não é preciso, porque ela já mede 2 metros! Como o que muda é a malha e não o objeto, a escala continua em 1.

**No olho, com o gizmo:** um clique (ou um toque) no centro do gizmo o passa para Rotacionar, e outro para **Escalar**. Pegue a ponta vermelha (X) e arraste-a em direção ao centro até a largura que você quiser; depois a verde (Y), até ela ficar fininha como uma porta.

![No Modo Edição, dois cliques no centro do gizmo o passam para escalar; a ponta vermelha é arrastada em direção ao centro e o cubo se estreita; depois a verde, e ele fica fino como uma porta](img/modelar-gizmo-raton.webp)

![O mesmo com o dedo: dois toques no centro do gizmo e as pontas vermelha e verde arrastadas em direção ao centro](img/modelar-gizmo-dedo.webp)

**Com números:** <kbd>S</kbd>, <kbd>X</kbd>, `0.45`, <kbd>Enter</kbd> e <kbd>S</kbd>, <kbd>Y</kbd>, `0.05`, <kbd>Enter</kbd>.

![Com Tab o cubo passa para o Modo Edição, tudo em verde; S, X, 0.45 e Enter o estreitam, e S, Y, 0.05 e Enter o afinam; a escala do objeto continua em 1](img/modelar-edicion-raton.webp)

![Android](img/icono-android.svg) Em uma tela touch: o botão do modo → **Modo Edição**; depois **Escalar**, **X**, um toque na barra de cima, `0.45`, **OK** e o visto, e o mesmo com **Y** e `0.05`.

![O botão do modo passa para o Modo Edição; Escalar, X, a barra de cima, 0.45 e OK o estreitam; o mesmo com Y e 0.05](img/modelar-edicion-dedo.webp)

![Nokia](img/icono-nokia.svg) No Nokia N95: <kbd>OK</kbd> passa para o Modo Edição. <kbd>3</kbd> (escalar), <kbd>1</kbd> (o eixo X), o lápis mantido e `0` <kbd>*</kbd> `45` (o <kbd>*</kbd> é o ponto), e <kbd>OK</kbd>. O mesmo com <kbd>2</kbd> (o eixo Y) e `0.05`.

![Um Nokia N95: OK passa para o Modo Edição; 3 e 1 escalam em X, o lápis e 0.45 digitam o valor e OK confirma; o mesmo com 3, 2 e 0.05](img/modelar-edicion-nokia.webp)

## Com medidas exatas

Se em vez de fazer no olho você quer, por exemplo, uma porta de exatamente **90 cm de largura**: no botão **Vértice / Aresta / Face** escolha **Face** e clique em uma das laterais. Sabemos que o cubo mede 2 metros, então precisamos tirar o que sobra para que fique com 0,9: é preciso deslocar essa lateral **1,1 metro**. <kbd>G</kbd> <kbd>X</kbd> `-1.1` <kbd>Enter</kbd>.

![No Modo Edição escolhe-se Face, um clique na lateral direita seleciona só ela, e G, X, -1.1 e Enter a deslocam: o cubo fica com 0,9 metro de largura](img/modelar-exacto-raton.webp)

![Com o dedo: Face no botão de seleção, um toque na lateral, Mover, X, a barra de cima, -1.1 e OK; o cubo fica com 0,9 metro de largura](img/modelar-exacto-dedo.webp)

Ao mover, o eixo **X** trava o movimento em um só eixo, para que o cubo não se deforme:

- ![PC](img/icono-pc.svg) Com a tecla <kbd>X</kbd>.
- ![Android](img/icono-android.svg) Com um toque em **X** na barra de ferramentas. O `-1.1` é digitado com o teclado numérico, tocando na barra de cima.
- ![Nokia](img/icono-nokia.svg) Com <kbd>1</kbd> enquanto move. O número é digitado com o lápis mantido: <kbd>#</kbd> muda o sinal e <kbd>*</kbd> é o ponto. A face é selecionada com o mouse virtual (a tecla de seleção direita, as setas e <kbd>OK</kbd>).

Para a espessura, com tudo selecionado, <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>, como antes.

E já temos o nosso primeiro modelo 3D! Parabéns.

No próximo tutorial vamos fazer a porta abrir a partir da dobradiça: [Mudar a origem: uma porta](puerta.md).

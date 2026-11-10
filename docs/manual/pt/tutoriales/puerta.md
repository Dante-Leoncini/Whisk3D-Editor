# Mudar a origem: uma porta

Neste tutorial você vai fazer uma porta abrir a partir da dobradiça. Para isso é preciso mudar a sua **origem**: o ponto ao redor do qual o objeto gira (veja [A origem](modo-objeto.md#a-origem) no tutorial do Modo Objeto).

Partimos da porta do tutorial anterior, [Modelar uma porta](modelar-puerta.md), com a origem no centro. Se você a fez deslocando uma lateral, a origem ficou deslocada: **Objeto → Definir Origem → Origem para Geometria** a devolve ao centro.

## A porta gira errado

Experimente girá-la. A porta gira sobre o seu centro, como uma porta giratória, e não a partir da dobradiça:

![PC](img/icono-pc.svg) Com o gizmo: um clique no centro dele o passa para **Rotacionar**, e o anel azul a gira sobre o eixo vertical.

![Um clique no centro do gizmo o passa para Rotacionar; o anel azul é arrastado e a porta gira sobre o seu centro, como uma porta giratória, e volta](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) Com o dedo: um toque no centro do gizmo o passa para **Rotacionar**, e o anel azul é arrastado.

![O dedo toca no centro do gizmo e o passa para Rotacionar; depois arrasta o anel azul e a porta gira sobre o seu centro, e volta](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) No Nokia N95 o gizmo vem desligado: <kbd>2</kbd> (rotacionar), <kbd>3</kbd> (o eixo Z, o vertical) e as setas; <kbd>OK</kbd> confirma.

![Um Nokia N95: 2 e 3 começam a rotacionar sobre o eixo vertical; a seta direita gira a porta sobre o seu centro, a esquerda a faz voltar e OK confirma](img/puerta-gira-mal-nokia.webp)

Ela gira errado porque a sua origem está no centro. Vamos levá-la até a dobradiça.

## Levar a origem até a dobradiça

1. **Entre no Modo Edição.** Um clique no botão do modo e outro em **Modo Edição** (no PC, também com <kbd>Tab</kbd>).

   ![O botão do modo aberto, com Modo Edição destacado](img/puerta-paso-1.webp)

2. **Escolha as faces.** No Modo Edição aparece o botão **Vértice / Aresta / Face**: um clique e escolha **Face**.

   ![O botão Vértice, Aresta, Face aberto, com Face destacado](img/puerta-paso-2.webp)

3. **Selecione a face da dobradiça.** Um clique na face estreita da lateral da porta, do lado onde vai a dobradiça: ela fica verde.

   ![A face estreita da lateral da porta, selecionada em verde e marcada com um círculo](img/puerta-paso-3.webp)

4. **Leve o cursor 3D até ali.** No menu **Malha**, **Ímã** → **Cursor para o selecionado**: o cursor 3D fica sobre a dobradiça.

   ![O menu Malha aberto, com o submenu Ímã e Cursor para o selecionado destacado](img/puerta-paso-4.webp)

5. **Volte para o Modo Objeto.** Um clique no botão do modo e outro em **Modo Objeto** (ou <kbd>Tab</kbd>).

   ![O botão do modo aberto, com Modo Objeto destacado; o cursor 3D já está na dobradiça](img/puerta-paso-5.webp)

6. **Leve a origem até o cursor 3D.** No menu **Objeto**, **Definir Origem** → **Origem para Cursor 3D**. A porta não se move: quem se move é a origem dela, e com ela o gizmo, que fica na dobradiça.

   ![O menu Objeto aberto, com o submenu Definir Origem e Origem para Cursor 3D destacado](img/puerta-paso-6.webp)

## Agora gira certo

Gire-a de novo com o gizmo: agora ela abre e fecha a partir da dobradiça.

![Com a origem na dobradiça, o anel azul do gizmo abre a porta até ela ficar de lado e a fecha de novo](img/puerta-gira-bien-raton.webp)

![Com o dedo, o anel azul abre a porta a partir da dobradiça e a fecha de novo](img/puerta-gira-bien-dedo.webp)

![Um Nokia N95: 2, 3 e a seta direita abrem a porta a partir da dobradiça; a esquerda a fecha e OK confirma](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) Em uma tela touch, os passos acima são iguais, com toques: os menus são abertos e as opções escolhidas com um toque.

![Nokia](img/icono-nokia.svg) No Nokia N95, <kbd>OK</kbd> passa do Modo Objeto para o Modo Edição e os menus são abertos com a soft esquerda.

## As opções de Definir Origem

| Opção | O que faz |
|---|---|
| **Origem para Geometria** | Leva a origem ao centro da malha. |
| **Origem para Cursor 3D** | Leva a origem para onde está o cursor 3D. |
| **Geometria para Origem** | O contrário: deixa a origem e move a malha até o centro dela ficar sobre a origem. |

No próximo tutorial fazemos a maçaneta: [Modelar a maçaneta](modelar-picaporte.md).

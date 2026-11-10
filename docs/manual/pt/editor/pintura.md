# Modos de pintura

> [!NOTE]
> Os três modos de pintura são **só para malhas 3D**: não aparecem com câmeras, luzes, armações, curvas nem elementos 2D. Enquanto você pinta, o clique não muda a seleção; para pintar outro objeto, escolha-o no Outliner.

## O que é comum aos três

| Função | Descrição | Como usar |
|---|---|---|
| Pintar | Pinta só as faces de frente que estão visíveis (com Raio-X, também as de trás). Cada traço é um passo de desfazer. | Clicar e arrastar |
| Raio | O tamanho do pincel em pixels. Ao mudá-lo, aparece um círculo no centro da vista com o tamanho real. | Barra arrastável sobre a barra de ferramentas |
| Valor | A intensidade do pincel. | Barra arrastável |
| Queda | Como a borda do pincel é suavizada; dá para desenhar uma curva própria. | Botão de queda |
| Vista (touch) | Ligado: o dedo navega. Desligado: o dedo pinta. | Botão em forma de monitor |

## Pintura de pesos

Pinta quanto um osso influencia cada vértice (azul = nada, vermelho = tudo). É o que faz a malha se deformar bem com a armação.

| Função | Descrição |
|---|---|
| Modo do pincel | Somar (+), Subtrair (−) ou Igualar (=, define o valor exato). |
| Grupo de vértices | Qual grupo (osso) é pintado; dá para adicionar grupos. Se não houver nenhum, o primeiro traço cria um. |
| Marcas | Mostra um quadradinho por vértice; o pincel atua só sobre eles. |
| Só o selecionado | Protege o que não está selecionado (aparece escurecido). |

## Pintura de vértices

Pinta cores sobre os vértices de uma **malha 3D** (não serve para imagens nem elementos 2D: para isso existe a pintura de textura). É muito leve e é a forma recomendada de dar cor no N95.

| Função | Descrição |
|---|---|
| Cor | Abre o seletor de cor (RGB, HSV, hexadecimal ou um índice da paleta). |
| Por vértice / por canto | Por vértice, a cor é compartilhada entre as faces; por canto, cada face pode ter a sua (bordas duras). |
| Camada de cor | Escolher ou criar camadas de cor. |
| Marcas / Só o selecionado | Igual à pintura de pesos. |

## Pintura de textura

Pinta diretamente sobre o objeto 3D, e o traço vai parar na sua textura.

| Função | Descrição |
|---|---|
| Textura automática | Se a malha não tiver textura, ela é criada sozinha no primeiro traço: um material próprio, UV automáticas e uma imagem de 1024×1024 (256×256 no N95) da cor do material. |
| Cor, queda, raio, valor | Como nos outros modos. |
| Salvar textura | A pintura fica na memória até ser salva. O disquete fica verde se houver mudanças não salvas. Opções: Salvar, Salvar como…, Salvar no w3d e Salvar fora do w3d… |

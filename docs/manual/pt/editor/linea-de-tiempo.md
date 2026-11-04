# Linha do Tempo

Mostra e edita a animação: folha de keyframes (dope sheet) ou editor de curvas. Mostra o que está animado no que está **selecionado**.

## Barra superior

| Função | Descrição |
|---|---|
| Transporte | Ir para o início, keyframe anterior, quadro anterior, reproduzir para trás, reproduzir / pausa, próximo quadro, próximo keyframe, ir para o fim. |
| Início / Fim / FPS | Intervalo e velocidade da animação (clique para digitar). |
| Animação | Qual animação é editada (cena, clipes, armações, objetos, flipbooks, Jogo, Mix) e criar novas. |
| Auto Key | Em vermelho: cada transform grava keyframes sozinho. |
| Curvas / Dope Sheet | Alterna entre a folha de keyframes e o editor de curvas. |
| Selecionar, Key, Vista, Pivô | Menus de seleção, operações sobre keyframes, enquadramento e painel lateral, e a partir de onde o <kbd>S</kbd> escala (centro ou quadro atual). |

## Navegar no tempo

| Função | Descrição | Atalho |
|---|---|---|
| Mudar de quadro | Clique ou arraste sobre os números de quadro (scrub). O quadro atual (verde) pode ser digitado. | Clique |
| Quadro anterior / próximo | Um quadro para trás ou para frente. | <kbd>Esquerda</kbd> / <kbd>Direita</kbd> |
| Keyframe anterior / próximo | Pula para o keyframe mais próximo. | <kbd>Baixo</kbd> / <kbd>Cima</kbd> |
| Ir para o início / para o fim | Primeiro ou último quadro. | <kbd>Home</kbd> / <kbd>End</kbd> |
| Reproduzir / Pausa | De qualquer painel. | <kbd>Espaço</kbd> |
| Zoom e deslocar | Roda = zoom (em curvas, <kbd>Ctrl</kbd> = só tempo, <kbd>Shift</kbd> = só valor); botão do meio = deslocar. | Roda / botão do meio |
| Enquadrar seleção | Ajusta a vista ao que está selecionado. | <kbd>Num.</kbd> |

## Editar keyframes (dope sheet e curvas)

| Função | Descrição | Atalho |
|---|---|---|
| Selecionar | Clique em um losango, ou em uma linha para pegar todos os seus keyframes; <kbd>Shift</kbd> adiciona. | Clique; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Mover keyframes | Desloca os keyframes no tempo (também arrastando um losango). | <kbd>G</kbd> |
| Escalar keyframes | Estica ou comprime a partir do pivô. | <kbd>S</kbd> |
| Rotacionar alças | Só em curvas. | <kbd>R</kbd> |
| Duplicar | Copia os keyframes e os deixa presos ao ponteiro. | <kbd>Shift+D</kbd> |
| Excluir | Exclui os keyframes (ou as curvas das linhas) selecionados. | <kbd>X</kbd> |
| Interpolação | Constante, linear ou Bezier. | <kbd>T</kbd> |
| Tipo de alça | Livre, alinhada, vetor, automática, auto limitada (só curvas). | <kbd>V</kbd> |
| Euler inteligente | Remove as voltas a mais nas rotações selecionadas. | Menu Key |
| Ocultar uma curva | O olho de cada linha a oculta. | Clique no olho |
| Valores exatos | Quadro, valor e alças do keyframe escolhido. | Propriedades → Animação → Keyframe |

## Sons

| Função | Descrição | Atalho |
|---|---|---|
| Faixa do som | Cada objeto Som selecionado tem sua faixa: onde começa e quanto dura, em **segundos** (mudar os FPS não a move). Com loop, repete até o final. | — |
| Mover | Arrastar a faixa, ou <kbd>G</kbd> sem keyframes escolhidos (move todos os sons selecionados). <kbd>Ctrl</kbd> encaixa nos frames. | Arrastar / <kbd>G</kbd> |
| Cortar | Pegar uma ponta da faixa. | Arrastar uma ponta |
| Forma de onda / Linha de volume | Menu View: a forma de onda e a curva do volume dentro da faixa (a faixa ocupa três linhas). Sem elas é só uma faixa. | Menu View |

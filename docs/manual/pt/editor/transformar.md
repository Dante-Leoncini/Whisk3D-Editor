# Mover, rotacionar e escalar

Funciona igual no Modo Objeto, no Modo Edição, no Pose e na edição de ossos.

| Função | Descrição | Atalho |
|---|---|---|
| Mover (transladar) | O que está selecionado segue o ponteiro. | <kbd>G</kbd> |
| Rotacionar | Gira ao redor do pivô. Outro <kbd>R</kbd> alterna entre giro livre (trackball) e orbital. | <kbd>R</kbd> |
| Escalar | Aumenta ou diminui a partir do pivô. | <kbd>S</kbd> |
| Restringir a um eixo | Durante o transform. Apertar de novo alterna Global → Local → Vista → livre. | <kbd>X</kbd> / <kbd>Y</kbd> / <kbd>Z</kbd> |
| Restringir a um plano | Exclui esse eixo (move nos outros dois). | <kbd>Shift+X</kbd> / <kbd>Shift+Y</kbd> / <kbd>Shift+Z</kbd> |
| Valor exato | Digitar um número ou uma conta (`2*3`, `(1+0.5)/2`). <kbd>-</kbd> inverte o sinal, <kbd>Backspace</kbd> apaga. | Números |
| Confirmar | Aceita a operação. | Clique esquerdo, <kbd>Enter</kbd>, visto verde |
| Cancelar | Tudo volta a ficar como estava. | <kbd>Esc</kbd>, clique direito, X vermelho |
| Passos finos | Move, rotaciona ou escala aos poucos. | Setas |

Enquanto dura, a barra superior mostra o que está sendo feito, por exemplo "Mover X 1.5 em X global". Em telas touch, tocar nessa barra abre um teclado numérico.

## O gizmo

O gizmo aparece sobre o pivô no Modo Objeto e no Modo Edição (liga e desliga em Sobreposições → Gizmo).

| Função | Descrição |
|---|---|
| Gizmo de mover | Setas = um eixo; quadrados = um plano; círculo central = livre. |
| Gizmo de rotacionar | Um semicírculo colorido por eixo, um anel branco para girar de frente para a vista, e o interior para girar livremente. |
| Gizmo de escalar | Pontas quadradas por eixo; o círculo central escala por igual. |
| Trocar de gizmo | Um clique (ou toque) rápido sobre o gizmo, sem arrastar, passa de Mover para Rotacionar, para Escalar e de volta para Mover. |

## Ajustar a última operação

Depois de mover, rotacionar ou escalar (no Modo Objeto ou Edição), aparece embaixo à esquerda um painel com o que foi feito: os valores X, Y, Z (ou o ângulo), a orientação e a edição proporcional com suas opções. Mudar um valor refaz a operação com o valor novo. **Cancelar** a desfaz e **Aceitar** a mantém. Um clique fora do painel também aceita. Enquanto o painel está aberto, a barra de ferramentas da Viewport 3D fica oculta.

O painel tem borda verde porque é a janela com o foco. Se não couber na metade da altura da tela, ele encolhe para essa metade e rola arrastando o dedo (sem tocar nas opções), com a roda ou com as setas. Com o teclado ou o celular: as setas percorrem as linhas, <kbd>OK</kbd> / <kbd>Enter</kbd> edita o valor sem fechar o painel, **Aceitar**, <kbd>Esc</kbd> ou <kbd>Delete</kbd> (o <kbd>C</kbd> no N95) o fecham mantendo a mudança, e só **Cancelar** a desfaz.

## Barra de ferramentas

Tem Desfazer e Refazer, as últimas ações usadas e o botão de edição proporcional. Durante um transform, mostra o visto (aceitar), o X (cancelar), a orientação e os eixos X / Y / Z (dois eixos ligados = um plano). Em telas touch, depois de extrudar aparece **Repetir**, que aceita e extruda de novo.

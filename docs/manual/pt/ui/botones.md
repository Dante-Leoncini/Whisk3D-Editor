# Botões, abas e caixas de seleção

## Botão

`Button` (`widgets/Button.h`) é um cartão com texto e um ícone opcional à esquerda. A borda escura é o que o faz parecer um botão. Sob o mouse (`hover`), a borda fica branca e o fundo clareia um pouco.

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // a largura acompanha o texto
guardar->Resize(200);                                         // nunca mais largo que isto

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // onde ficou, para o mouse
guardar->hover = guardar->Contains(mx, my);
```

`Render` desenha na origem atual e não sabe onde ficou na tela: `sx` e `sy` são definidos por quem o desenha, e `Contains` compara com eles. O terceiro argumento do construtor é `adaptar`: com `false`, o botão ocupa toda a largura que `Resize` lhe dá.

| Campo | O que faz |
|---|---|
| `centrado` | Ícone e texto no meio, como em Aceitar ou Cancelar |
| `cuadrado` | A largura igual à altura: botões só de ícone |
| `altoRenglon` | A altura de uma linha de propriedades, em vez da de uma barra |
| `caretMenu` | Uma setinha à direita: avisa que abre um menu |
| `tinte` | A cor do fundo, em vez de `gris` |
| `colorTexto`, `colorBorde` | A cor do texto e do ícone, e a da borda |
| `focoMenu` | Em foco pelo teclado: borda e texto em `accent` |
| `alpha` | A opacidade do fundo e da borda, para que se veja o que está atrás |
| `iconFlip` | O ícone espelhado: 1 horizontal, 2 vertical |
| `barra` | Um número de 0 a 1: o botão se preenche até ali, em `accentDark`, com o texto no meio |
| `relleno` | Um quadrado dessa cor por dentro: para escolher uma cor |
| `editField` | Um `TextField`: o botão é desenhado como caixa de texto, para renomear no lugar |
| `visible` | Com `false`, não é desenhado |

Há campos que o botão guarda e não desenha: quem os usa é quem o contém. `desplegable` diz que ele abre um menu. `separador` pede uma linha antes dele, numa barra. `rol` serve para encontrá-lo numa barra sem depender da ordem. `ayuda` é o texto da dica que aparece ao deixar o mouse em cima; a dica não é da UI.

A altura é `UIBotonAltura()` ([Cores e escala](tema.md)), ou `RenglonHeightGS + 2 * GlobalScale` com `altoRenglon`. Nunca é mais estreito que a altura de uma linha.

## Aba

`Tab` (`widgets/Tab.h`) é um botão sem a borda de baixo: ela se funde com o que tem embaixo. A ativa (`activa`) é um pouco mais alta e da cor do conteúdo; as outras, mais escuras. O texto fica em `accent` se ela tiver o foco do teclado (`foco`), em branco se for a ativa ou estiver sob o mouse, e em cinza nos outros casos.

Usa-se como o botão: `Resize(maxW)`, `Render()` e `Contains(mx, my)`, com `sx` e `sy` definidos por quem a desenha.

## Caixas de seleção e amostras de cor

A aparência de uma caixa de seleção é decidida num único lugar, `widgets/card.h`, e assim a de um menu e a de um cartão não podem ser diferentes.

| Função | O que faz |
|---|---|
| `DibujarCheckbox(lado, prendida)` | Desenha a caixa e, se estiver marcada, a marca de seleção |
| `DibujarMuestraColor(lado, rgba)` | Desenha um quadrado dessa cor, com os cantos da caixa de seleção |
| `CheckboxLado()` | O lado: a altura de uma linha de texto |
| `CheckboxColorCaja(prendida)`, `CheckboxColorTilde()` | As cores da caixa e da marca |
| `CheckboxTilde()` | O caractere da marca, `✔` |

Marcada, a caixa é `grisUI` e a marca, da cor do fundo. Desmarcada, a caixa é da cor do fundo e não tem marca.

```cpp
DibujarCheckbox(CheckboxLado(), sonidoPrendido);
```

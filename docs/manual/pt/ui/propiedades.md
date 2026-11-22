# Cartões e propriedades

## O cartão

`GroupPropertie` (`Propieties/GroupPropertie.h`) é um cartão com um título e linhas. Cada linha é uma propriedade, uma classe que herda de `PropertieBase` e aponta para uma variável sua: o cartão a mostra e a altera ali mesmo.

```cpp
GroupPropertie* player = new GroupPropertie("Jugador");
player->icon = IconType::gamepad;        // opcional: entre a seta e o título

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
player->properties.push_back(vel);

player->Resize(width, 0);    // a altura é calculada pelo cartão
player->Render();            // na origem atual
```

O cartão é dono das suas linhas: ao ser apagado, ele as apaga.

- O título leva uma seta para recolhê-lo (`open`). Com `arrow = false`, não tem: para uma janela flutuante.
- Embaixo há duas colunas: o rótulo à esquerda e o valor à direita. `valueWidth` é a parte da largura que fica com o valor: 0.5 por padrão.
- `reserveKeyBtn = true` deixa, à direita de tudo, uma coluna para o losango de animação.
- Quase todas as linhas têm a altura de uma linha de texto mais um `gap`. Um número, uma caixa de seleção ou uma cor sem variável (`value` em `NULL`) não ocupa espaço nem é desenhado.
- `visible = false` esconde o cartão inteiro.

A linha escolhida tem o rótulo em `accent`, o valor em branco e a borda branca; enquanto ela é editada, o valor e a borda passam para `accent`. A linha sob o mouse fica em branco: quem a marca é quem contém o cartão, com `PropHoverGroup` e `PropHoverRow`.

## Escolher e editar

`selectIndex` é a linha escolhida: `-2` nenhuma, `-1` o título, de `0` em diante uma linha.

| Função | O que faz |
|---|---|
| `NextSelect()`, `PrevSelect()` | Passam para a linha de baixo ou de cima. Pulam os espaços e as linhas sem variável. Retornam `true` ao sair do cartão, para passar ao seguinte |
| `EnterPropertieSelect()` | No título, recolhe ou expande o cartão. Numa linha, a edita |
| `button_up()`, `button_down()`, `button_left()`, `button_right()` | Durante a edição, vão para a linha |
| `Cancel()` | Deixa a linha como estava |

`editing` diz se há uma linha em edição. Com isso, as setas mudam o valor ou movem a escolha:

```cpp
if (player->editing) player->button_up();   // muda o valor
else player->PrevSelect();                    // a linha de cima
```

O mouse fica por conta de quem contém o cartão: ele não sabe onde ficou na tela. Em cima estão a borda (`borderGS`) e o título (`RowHeightGS + gapGS`). Cada linha mede o que o seu `Resize(width)` retorna.

## As linhas

| Classe | O que é | Como se edita |
|---|---|---|
| `PropFloat` | Um número | Com as setas, ou digitado |
| `PropBool` | Uma caixa de seleção, encostada à direita | OK a marca ou desmarca |
| `PropColor` | Um quadrado da cor, encostado à direita | OK entra em edição e guarda a cor, caso se cancele |
| `PropButton` | Um botão | OK chama a sua ação |
| `PropButtonRow` | Vários botões numa linha | Esquerda e direita escolhem um; OK o pressiona |
| `PropText` | Uma caixa de texto | OK a coloca em foco para escrever |
| `PropListMeshParts` | Uma lista com rolagem | Cima e baixo escolhem |
| `PropLabel` | Um texto para ler, em uma ou várias linhas | Não pode ser escolhida |
| `PropSeparator` | Uma linha | Não pode ser escolhida |
| `PropGap` | Um espaço de um `gap` | Não pode ser escolhida |
| `PropSelectText` | Uma caixa com seu rótulo, sem valor | Ainda não faz nada |

### Número

```cpp
PropFloat* life = new PropFloat("Vida");
life->value = &lifePlayer;
life->SetRange(0, 100);           // não passa disso
life->integer = true;              // é mostrado e avança em inteiros
life->fineStep = 1;               // cima e baixo
life->thickStep = 10;            // esquerda e direita
life->onChange = OnChangeLife;   // void AlCambiarVida()
```

| Campo | O que faz |
|---|---|
| `unit` | Um texto colado ao número, como `°`. É o segundo argumento do construtor |
| `fineStep`, `thickStep` | O que as setas somam: 0.01 e 0.1 por padrão |
| `integer` | Mostra o número arredondado, e as setas partem do arredondado. Sem isso, ele tem quatro casas decimais |
| `centered` | O número no meio da sua caixa |
| `arrows` | Desenha `<` e `>` dos lados. O clique nelas fica por conta do contêiner |
| `options`, `nOptions` | Com `integer`: o número é um índice e é mostrado o nome dessa opção, traduzido com `W3dMenuTranslate` |
| `accelerates` | Esquerda e direita começam de 1 em 1 e, se repetidas, sobem até 10 |
| `dragStep` | Quanto muda por pixel ao arrastar. O arraste é feito pelo contêiner |

`Set(v)` atribui o valor dentro do intervalo e chama `onChange`. As setas passam por ali.

Para digitá-lo:

```cpp
life->StartEditText();   // mostra o valor, todo selecionado
// as teclas vão para g_activeTextField: veja Entrada e caixas de texto
NumEditCommit();               // Enter: aplica o valor
NumEditCancel();               // Esc: fica como estava
```

`NumEditActive()` diz se há um número em edição. Um texto que não é um número vale 0.

### Caixa de seleção

`value` aponta para um `bool`. `onChange` é chamado cada vez que ele muda. O rótulo ocupa todo o espaço à sua esquerda e, se não couber, é cortado com `..`.

### Cor

`value` aponta para quatro `float` (RGBA). A UI desenha o quadrado; o seletor de cor não é da UI. Durante a edição, as setas movem só o primeiro componente.

### Botão

`action` é a função que ele chama. `gray = true` o atenua e ele não faz nada. `hidden = true` não ocupa linha.

Com `withLabel = true`, a linha é um nome e um botão: o nome à esquerda, da largura do seu texto, até 55 % da linha, e o botão em todo o resto. `labelChars` fixa a largura do nome, para alinhar várias linhas. Com `button->dropdown`, ele leva a setinha de um menu. Além disso:

- `AddPrefix(action, help)` coloca um quadrado antes do botão, para mostrar uma cor.
- `AddExtra(icon, action, help)` coloca até dois botões pequenos, só de ícone, à direita. `slotsExtras` reserva espaço para eles, para que fiquem alinhados entre as linhas. `ExtraInX(x)` diz qual ficou nesse x, para o clique.

### Linha de botões

```cpp
PropButtonRow* row = new PropButtonRow();
row->Add("Aceptar", Accept);
row->Add("Cancelar", Cancel);
player->properties.push_back(row);
```

Os botões dividem a largura em partes iguais. Os invisíveis são pulados, e os outros dividem o espaço deles. `Fire(i)` pressiona o botão `i`: o contêiner o chama quando o clique cai nessa parte.

### Caixa de texto

```cpp
PropText* name = new PropText("Nombre", "Jugador 1");
// name->field.text tem o que foi escrito
```

`onClick`: em vez de editar, chama essa função, por exemplo para abrir um explorador de arquivos. `error = true` pinta de vermelho a borda da linha escolhida, até que se escreva algo. `hidden = true` não ocupa linha. OK a coloca em foco com `TextFieldFocus`, que hoje está definida no editor. Para escrever, veja [Entrada e caixas de texto](entrada.md).

### Texto

`PropLabel("texto")` é uma linha. `PropLabel("texto", true)` quebra nos espaços para caber na largura; uma palavra que sozinha não cabe fica inteira. `red = true`, para um erro. `hidden = true` não ocupa linha.

### Lista

`PropListMeshParts` mostra `rowsMax` linhas, 3 por padrão, com uma barra pequena se houver mais. A escolhida sempre fica à vista. `Cancel` volta `selectIndex` ao valor que tinha ao entrar em edição.

Ainda não serve para uma lista qualquer: conforme o `mode`, lê uma lista da cena (as partes de uma malha, seus mapas UV, suas camadas de cor, os modificadores, os grupos de vértices, os ossos, as animações, as restrições, os scripts) ou a pede a ganchos de funções do editor (`MixRow*`, `RoutineRow*`, `PostRow*`).

## O losango de animação

Uma linha animável (`animProp` de 0 em diante, em `PropFloat`, `PropBool`, `PropColor` e `PropButton`) mostra um losango à direita se o cartão tiver `reserveKeyBtn`. Sua cor indica o estado:

| Estado | Cor | O que quer dizer |
|---|---|---|
| 0 | Cinza | Sem animação |
| 1 | `accent`, a meia-luz | Animada, sem keyframe aqui |
| 2 | `accent` | Há um keyframe aqui |
| 3 | Verde | O valor mudou e não foi salvo |

O losango é da UI; o estado que ele tem, não. Ele pergunta o estado a um gancho que o programa instala, e o clique chama o outro:

```cpp
W3dKeyframeState = MiState;   // int MiEstado(int prop, int comp): de 0 a 3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp): põe ou tira o keyframe
```

Sem ganchos, o losango fica cinza. O cartão não chama `W3dKeyframeToggle`: quem o chama é quem trata o clique. `g_kfFocusProp` aponta para a linha cujo losango tem o foco do teclado e o marca com uma borda branca. `W3dRenderDiamondKey(state)` desenha um losango avulso.

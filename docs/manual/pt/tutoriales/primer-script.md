# O seu primeiro script: um cubo que gira

Neste tutorial você vai escrever o seu primeiro script de **Lua**: um que faz o cubo girar.

![O Whisk3D com o IDE embaixo, onde ficava a Linha do Tempo, com o script rotate.lua escrito; à direita, as Propriedades do cubo com o cartão do script: a caixa rotate e a speed em 90](img/script-ide.webp)

## 1. O IDE

Neste tutorial você não vai usar a Linha do Tempo: transforme-a no IDE, o editor de scripts. Toque no botão do canto da Linha do Tempo (no canto superior esquerdo) e escolha **IDE**.

O IDE começa vazio. Para escrever com mais conforto, aumente-o: arraste o mesmo botão do canto para cima.

![O mouse clica no botão do canto da Linha do Tempo, escolhe IDE no menu e a Linha do Tempo vira o IDE; depois arrasta o mesmo botão para cima e o IDE aumenta](img/script-cambiar-ide.webp)

> [!NOTE]
> Também é possível usar outros editores ou IDEs externos e carregar os scripts. Os scripts podem ficar fora ou dentro dos projetos W3D.

## 2. Escreva o script

O script tem duas partes: em cima, as **propriedades**, e embaixo, a **função** que faz o cubo girar.

### As propriedades

```lua
properties = {
  rotate   = true,   -- se o cubo gira ou não
  speed    = 90,     -- quantos graus gira por segundo
}
```

- **`properties`** são os valores do script que podem ser mudados no painel de Propriedades, um por linha. `rotate = true` é um valor **booleano** (verdadeiro ou falso: `true` ou `false`), e no painel aparece como uma caixa de seleção. `speed = 90` é um número, e aparece como um campo numérico. O que você escreve aqui é o valor **padrão**: cada objeto que usar o script pode ter o seu.
- O que vem depois de `--` é um **comentário**: o Lua não o lê, é para você.

### A função

```lua
function update(dt)
  if rotate then
    setRotationZ(self(), getRotationZ(self()) + speed * dt)
  end
end
```

Uma **função** é um bloco de código com um nome: começa com `function nome()` e termina com `end`. O que ela tem dentro não é feito quando você a escreve, e sim cada vez que alguém **chama** a função.

O Whisk3D tem **funções reservadas**: se o seu script tem uma função com um desses nomes, o motor a chama sozinho, no momento certo. Por exemplo:

- **`start()`**: uma vez só, ao dar Play. Serve para preparar as coisas.
- **`update(dt)`**: a **cada quadro**, enquanto o jogo roda.

Há mais, como as que avisam quando um objeto colide com outro. Estão todas no manual de Lua: [Quando o motor chama o seu script](../lua/ciclo-del-script.md).

`update` é ideal para um videogame. Um jogo desenha a tela muitas vezes por segundo, e cada uma dessas imagens é um **quadro**: tudo o que se move, se move um pouquinho a cada quadro. Aqui, a cada quadro o cubo gira um pouco mais.

- Ela roda enquanto o objeto está **visível**: se você o ocultar, o script dele para de rodar até que ele volte a aparecer.
- `dt` são os segundos que passaram desde o quadro anterior. Multiplicar a velocidade por `dt` faz o cubo girar 90 graus **por segundo**, quer o jogo rode a 30 ou a 60 quadros por segundo.

O que ela faz dentro:

**`if rotate then` … `end`** é uma **condição**. Um `if` ("se") faz o código que tem dentro só se o que vem entre `if` e `then` for verdadeiro. Ele pode ter um `elseif` ("se não for verdade, faz isto se…"), que testa outra condição, e um `else` ("senão"), que é feito quando nenhuma se cumpriu; os dois são opcionais. Sempre termina com `end`. Por exemplo:

```lua
if puntos >= 100 then
  premio = "ouro"
elseif puntos >= 50 then
  premio = "prata"
else
  premio = "nada"
end
```

Com 120 pontos o prêmio é ouro; com 70, prata, e com 10, nada. Só é feita a primeira parte que se cumpre. (Em Lua, `elseif` se escreve tudo junto.)

No script, o `if` não tem `elseif` nem `else`: se **rotate** for verdadeiro (a caixa está marcada), o cubo gira; se for falso, nada acontece e a função termina. `rotate` lê o valor que esse objeto tem no painel. Toda propriedade se usa assim, pelo nome; também dá para lê-la com `property("rotate")`. Como é lido a cada quadro, se você o mudar com o jogo rodando, o script fica sabendo na mesma hora.

- **`self()`** é o objeto que tem o script: o cubo. **`getRotationZ(self())`** dá a rotação dele em graus em torno do Z (o eixo vertical, o mesmo Z que você vê no painel de Propriedades), e **`setRotationZ`** dá a ele uma nova: a mesma, um pouco mais. Assim o cubo gira como um pião.

`self`, `getRotationZ` e `setRotationZ` também são funções, mas ao contrário: quem as traz é o Whisk3D e quem as chama é você. Elas estão no [manual de Lua](../lua/README.md).

### O script completo

Toque dentro do IDE e escreva tudo junto:

```lua
properties = {
  rotate   = true,   -- se o cubo gira ou não
  speed    = 90,     -- quantos graus gira por segundo
}

function update(dt)
  if rotate then
    setRotationZ(self(), getRotationZ(self()) + speed * dt)
  end
end
```

## 3. Salve-o

Primeiro salve o projeto, se ainda não o salvou: o script só pode ser salvo dentro do projeto se o projeto já estiver salvo. Em Propriedades, na aba do **disquete** (o cartão **Arquivo**), escreva um **Nome**, por exemplo `rotate-cubo`, e toque em **Salvar**. Ele é salvo na **Pasta** que o cartão mostra.

Depois, na barra de menus do IDE, **Arquivo → Salvar como...**:

1. Em **Nome**, coloque `rotate.lua`.
2. Marque **Dentro do .w3d**: o script fica salvo dentro do projeto, no mesmo arquivo `.w3d`. A **Pasta** passa a ser uma de dentro do projeto: **(raiz)** é a principal.
3. Toque em **Salvar**.

![O formulário Salvar como... do IDE com o nome rotate.lua e Dentro do .w3d marcado](img/script-guardar.webp)

Se você não marcar **Dentro do .w3d**, o script é salvo como um arquivo avulso, fora do projeto, na **Pasta** que você escolher. Isso serve, por exemplo, para editá-lo com outro programa.

![O mesmo formulário sem marcar Dentro do .w3d: o script é salvo como um arquivo avulso, na Pasta](img/script-guardar-afuera.webp)

## 4. Transforme a cena em um jogo

Uma **cena** é para modelar e animar; os scripts rodam em um **jogo**. Um jogo não tem animações próprias: as animações são feitas em uma cena ou em um modelo 3D com armação, e são importadas no jogo para serem usadas lá. Na barra de menus da Viewport 3D (ou do Outliner), toque no botão da cena, o que mostra o nome dela, e escolha **Converter em jogo**.

![O menu do botão da cena aberto na barra de menus da Viewport 3D, com o ponteiro sobre Converter em jogo](img/script-convertir-juego.webp)

**Converter em jogo** não muda a sua cena: faz uma cópia, que é um jogo, com tudo do jeito que aparece no quadro atual da Linha do Tempo. As animações da cena não passam para o jogo: esse quadro fica gravado como a posição e o estado inicial do jogo. A cena original continua igual, com as suas animações, e você a encontra de novo no menu do botão da cena.

Agora você está no jogo: o botão da cena mostra um joystick, e na barra de menus da Viewport 3D aparece **Reproduzir**.

## 5. Coloque-o no cubo

1. Selecione o **Cubo** do jogo (no Outliner ou na Viewport 3D).
2. Em Propriedades, vá para a aba do **joystick**.
3. No cartão **Controle**, toque em **Adicionar**: abre-se um menu com os scripts do projeto. Escolha `rotate.lua`.

![O menu de Adicionar aberto no cartão Controle: rotate.lua, o script do projeto, e Carregar .lua...](img/script-anadir.webp)

O script aparece na lista do cartão Controle, e embaixo é adicionado o cartão dele, **rotate.lua**, com as propriedades que você escreveu: a caixa **rotate** (marcada) e a **speed** (90).

Na lista, à direita do script, há um **olho**: tocando nele, o script é desligado (o motor não o usa) sem tirá-lo do objeto nem ocultar o objeto. Outro toque o liga de novo.

![O cartão Controle com rotate.lua na lista e os botões Adicionar e Remover; embaixo, o cartão rotate.lua com a caixa rotate marcada e a speed em 90](img/script-propiedades.webp)

### Um script de fora

Se você escreveu o script com outro editor, ou o salvou fora do `.w3d`, toque em **Adicionar → Carregar .lua...**. Abre-se o explorador, **Carregar script**: escolha o arquivo e toque em **Abrir**.

![O explorador Carregar script aberto na pasta onde está rotate.lua, com rotate.lua selecionado](img/script-cargar-lua.webp)

## 6. Dê Play

Toque em **Reproduzir**: o cubo gira. Enquanto o jogo roda, a barra de menus da Viewport 3D mostra **Parar** e **Pausar**.

Com o jogo rodando, experimente mudar as propriedades no cartão **rotate.lua**:

- coloque **speed** em 360: ele gira quatro vezes mais rápido;
- desmarque **rotate**: ele fica parado. Marque de novo e ele continua.

Para terminar, toque em **Parar**: o cubo volta a ficar como estava antes de dar Play.

![Reproduzir: o cubo gira; a speed passa para 360 e ele gira mais rápido; rotate é desmarcada e ele fica parado; Parar: o cubo volta ao seu lugar, com a speed em 90 e rotate marcada](img/script-jugar.webp)

> [!NOTE]
> Tudo o que acontece com o jogo rodando se perde ao tocar em **Parar**: o cubo volta ao seu lugar, e **rotate** e **speed** voltam aos valores que tinham antes de dar Play. Para mudá-los de verdade, mude-os com o jogo parado.

## Para continuar

- [Lua no Whisk3D](../lua/README.md): o manual de Lua, com todas as funções.
- [Um script é um componente de um objeto](../lua/scripts-y-objetos.md): as propriedades (também listas, textos e objetos da cena).
- [Quando o motor chama o seu script](../lua/ciclo-del-script.md): `update`, `start` e as outras.

## Baixe o exemplo

Aqui está o projeto pronto, para você testar ou comparar com o seu: **[rotate-cube.w3d](ejemplos/rotate-cube.w3d)**. Abra-o no Whisk3D e toque em **Reproduzir**. O script está dentro do `.w3d` e aparece no IDE.

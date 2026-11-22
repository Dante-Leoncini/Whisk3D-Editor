# Cores e escala

## A paleta

As cores da UI são papéis, não cores fixas: o enum `ColorID` (`theme/colors.h`).

| Papel | Para que é usado |
|---|---|
| `background` | O fundo. O quadrado de uma caixa de seleção desmarcada e a borda de um botão em repouso |
| `white` | O texto e a borda do que está sob o mouse ou escolhido |
| `accent` | O foco: a borda dos menus, o que está sendo editado, a opção ativa, o preenchimento de um controle deslizante |
| `accentDark` | O fundo do texto selecionado e a barra de um botão numérico |
| `black` | A linha de `PropSeparator` |
| `gray` | O corpo dos cartões, dos botões e dos menus |
| `headerColor` | A linha destacada de um menu e a trilha de um controle deslizante |
| `blackTransparent` | Preto a 25 %. A UI não o usa |
| `grayUI` | O texto normal e o quadrado de uma caixa de seleção marcada |
| `grayLine` | As linhas que separam grupos nos menus |

`ColorList` as tem em `float` e `ColorListUbyte`, em bytes. `SetColorID` define uma delas como cor de desenho, com um alfa opcional:

```cpp
SetColorID(ColorID::accent);         // opaca
SetColorID(ColorID::grayUI, 0.5f);   // com meia opacidade
```

`loadColorsW3d(path)` as lê de um arquivo de texto, uma cor por linha: o nome e os quatro componentes de 0 a 1, como `accent 0.0 1.0 0.0 1.0`. Ignora os nomes que não conhece, assim o mesmo `skin.ini` pode trazer cores de outro programa. Retorna `false` se não conseguiu abrir o arquivo. Sem o arquivo, ficam as cores do código (`theme/colors.cpp`); o skin do Whisk3D as muda e, por exemplo, o `accent` passa a ser verde.

Algumas cores estão escritas no código e não vêm da paleta: o verde do losango de uma mudança não salva, o vermelho de um `PropLabel` de erro, a borda vermelha de um `PropText` incompleto e o cinza de um `PropButton` desativado.

## A escala

As medidas base ficam em `core/UI.h`, em pixels:

| Medida | Valor |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RowHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` as multiplica, sempre por um inteiro: assim as letras de pixels continuam nítidas. Por exemplo, 1 na tela de 240 pixels do N95 e 3 num PC.

Cada medida tem sua versão já multiplicada, com `GS` no final: `RowHeightGS`, `gapGS`, `borderGS`, `bordersGS` (o dobro da borda), `CharacterWidthGS`, `LetterHeightGS`... São essas que os widgets usam.

A UI não traz a função que as recalcula: a do editor é `SetGlobalScale`, em `main/ui/ViewPorts/ViewPorts.cpp`. Com a UI sozinha, faz-se assim:

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RowHeightGS = RowHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

Os tamanhos de cada widget são calculados no seu `Resize`: depois de mudar a escala, é preciso chamá-lo de novo em cada cartão, botão e menu.

## Compacto ou touch

`UIButtonHeight()` dá a altura de um botão e `UIBarPadding()`, o espaço acima e abaixo de um botão numa barra. Os dois dependem de `UITouch`:

- `false`, o modo compacto, pensado para o N95: o botão mede uma linha de texto mais as bordas, menos `2 * GlobalScale`, e a barra deixa `GlobalScale` acima e abaixo.
- `true`: o botão e a barra somam um `gap`, para caber o dedo. As bordas não engrossam.

`UITouch` começa em `false` e hoje nada a ativa.

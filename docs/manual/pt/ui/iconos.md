# Ícones

## Os arquivos

Cada ícone é um PNG de **10x10** em `res/Skins/Whisk3D/atlas/icons/`, no repositório do editor. Hoje são 106. Os pixels são brancos sobre fundo transparente: o ícone assume a cor com que é desenhado. Assim, o mesmo ícone serve em cinza, em branco ou na cor de destaque.

## O enum e os nomes

`draw/icons.h` tem o enum `IconType` e a quantidade, `ICON_TOTAL` (106). Em `draw/icons.cpp`, `KNames` dá o nome de cada um, na mesma ordem: é o nome do arquivo sem `.png`.

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta, ou -1 se não existir
```

Quase todos têm o mesmo nome que no enum. A exceção é `lockOpen`, que é o arquivo `candado_abierto.png`.

Muitos são do editor: ferramentas de modelagem, tipos de objeto, curvas de animação. Os gerais também estão lá: `arrow` e `arrowRight` (as setas de recolher), `visible` e `hidden`, `folder`, `save`, `trash`, `plus` e `minus`, `list` e `grid`, `lock`, `notifOk` e `notifError`, `scissors`, `copy` e `paste`.

## As coordenadas

`IconsUV` é um `std::vector<IconUV*>`, com as coordenadas de cada ícone no atlas (`uvs`, 8 números). Ele é preenchido de duas formas:

- `CreateIcons(width, height)`: com a tabela de onde fica cada ícone dentro de `font.png` (`TableLegacy`, em `icons.cpp`). É a forma sem a pasta `atlas/icons/`.
- `CreateIconsFrom(rects, width, height)`: com uma lista própria de retângulos, a do atlas montado. Um retângulo com `x = -1` usa o de `font.png`: um ícone sem o seu PNG continua aparecendo.

`SetIconScale(scale)` ajusta `IconSizeGS` e as duas malhas: `IconMesh`, o quadrado de um ícone, e `IconLineMesh`, um quadrado da altura de uma linha de texto, para as linhas das árvores.

## Desenhar um ícone

Com o atlas definido, na origem atual:

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`, `Tab`, `PopupMenu` e `GroupPropertie` recebem o ícone como um `int` do enum, ou `-1` para nenhum.

## Adicionar um

1. O PNG de 10x10, em `atlas/icons/`.
2. Uma linha no final do enum `IconType`, e somar um a `ICON_TOTAL`.
3. O nome dele no final de `KNames`.
4. Uma linha no final de `TableLegacy`, com o ícone de `font.png` que o substitui se o PNG faltar.

Sem a linha do passo 4, o ícone fica sem coordenadas quando não há atlas montado.

# Couleurs et échelle

## La palette

Les couleurs de l'UI sont des rôles, pas des couleurs fixes : l'enum `ColorID` (`theme/colores.h`).

| Rôle | À quoi il sert |
|---|---|
| `background` | Le fond. La boîte d'une case décochée et la bordure d'un bouton au repos |
| `blanco` | Le texte et la bordure de ce qui est sous la souris ou choisi |
| `accent` | Le focus : la bordure des menus, ce qui est en cours de modification, l'option active, le remplissage d'une glissière |
| `accentDark` | Le fond du texte sélectionné et la barre d'un bouton numérique |
| `negro` | La ligne de `PropSeparator` |
| `gris` | Le corps des cartes, des boutons et des menus |
| `headerColor` | La ligne en surbrillance d'un menu et la piste d'une glissière |
| `negroTransparente` | Noir à 25 %. L'UI ne l'utilise pas |
| `grisUI` | Le texte normal et la boîte d'une case cochée |
| `grisLinea` | Les lignes qui séparent les groupes dans les menus |

`ListaColores` les contient en `float` et `ListaColoresUbyte` en octets. `SetColorID` en définit un comme couleur de dessin, avec un alpha facultatif :

```cpp
SetColorID(ColorID::accent);         // opaque
SetColorID(ColorID::grisUI, 0.5f);   // à demi-opacité
```

`loadColorsW3d(ruta)` les lit dans un fichier texte, une couleur par ligne : le nom et les quatre composantes de 0 à 1, comme `accent 0.0 1.0 0.0 1.0`. Elle ignore les noms qu'elle ne connaît pas, ce qui permet au même `skin.ini` de contenir des couleurs d'un autre programme. Elle renvoie `false` si elle n'a pas pu ouvrir le fichier. Sans le fichier, ce sont les couleurs du code qui restent (`theme/colores.cpp`) ; le skin de Whisk3D les change, et par exemple l'`accent` devient vert.

Certaines couleurs sont écrites dans le code et ne viennent pas de la palette : le vert du losange d'une modification non enregistrée, le rouge d'un `PropLabel` d'erreur, la bordure rouge d'un `PropText` incomplet et le gris d'un `PropButton` désactivé.

## L'échelle

Les mesures de base sont dans `core/UI.h`, en pixels :

| Mesure | Valeur |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` les multiplie, toujours par un entier : ainsi, les lettres en pixels restent nettes. Par exemple, 1 sur l'écran de 240 pixels du N95 et 3 sur un PC.

Chaque mesure a sa version déjà multipliée, avec `GS` à la fin : `RenglonHeightGS`, `gapGS`, `borderGS`, `bordersGS` (le double de la bordure), `CharacterWidthGS`, `LetterHeightGS`... Ce sont celles qu'utilisent les widgets.

L'UI ne fournit pas la fonction qui les recalcule : celle de l'éditeur est `SetGlobalScale`, dans `main/ui/ViewPorts/ViewPorts.cpp`. Pour l'UI seule, on procède ainsi :

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

Les tailles de chaque widget sont calculées dans son `Resize` : après avoir changé l'échelle, il faut l'appeler de nouveau sur chaque carte, bouton et menu.

## Compact ou tactile

`UIBotonAltura()` donne la hauteur d'un bouton et `UIBarPadding()` l'espace au-dessus et en dessous d'un bouton dans une barre. Les deux dépendent de `UITactil` :

- `false`, le mode compact, pensé pour le N95 : le bouton mesure une ligne de texte plus les bordures, moins `2 * GlobalScale`, et la barre laisse `GlobalScale` au-dessus et en dessous.
- `true` : le bouton et la barre ajoutent un `gap`, pour laisser de la place au doigt. Les bordures ne s'épaississent pas.

`UITactil` démarre à `false` et, aujourd'hui, rien ne l'active.

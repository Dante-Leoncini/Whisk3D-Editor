# Boutons, onglets et cases à cocher

## Bouton

`Button` (`widgets/Button.h`) est une carte avec du texte et une icône facultative à gauche. C'est la bordure sombre qui lui donne l'aspect d'un bouton. Sous la souris (`hover`), la bordure devient blanche et le fond s'éclaircit un peu.

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // la largeur suit le texte
guardar->Resize(200);                                         // jamais plus large que ceci

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // où il a été placé, pour la souris
guardar->hover = guardar->Contains(mx, my);
```

`Render` dessine à l'origine courante et ne sait pas où le bouton s'est retrouvé à l'écran : `sx` et `sy` sont définis par celui qui le dessine, et `Contains` compare avec eux. Le troisième argument du constructeur est `adaptar` : à `false`, le bouton occupe toute la largeur que lui donne `Resize`.

| Champ | Ce qu'il fait |
|---|---|
| `centrado` | Icône et texte au milieu, comme dans Accepter ou Annuler |
| `cuadrado` | La largeur égale à la hauteur : boutons avec seulement une icône |
| `altoRenglon` | La hauteur d'une ligne de propriétés, au lieu de celle d'une barre |
| `caretMenu` | Une petite flèche à droite : indique qu'il ouvre un menu |
| `tinte` | La couleur du fond, au lieu de `gris` |
| `colorTexto`, `colorBorde` | La couleur du texte et de l'icône, et celle de la bordure |
| `focoMenu` | Focus au clavier : bordure et texte en `accent` |
| `alpha` | L'opacité du fond et de la bordure, pour laisser voir ce qu'il y a derrière |
| `iconFlip` | L'icône en miroir : 1 horizontal, 2 vertical |
| `barra` | Un nombre de 0 à 1 : le bouton se remplit jusque-là, en `accentDark`, avec le texte au milieu |
| `relleno` | Un carré de cette couleur à l'intérieur : pour choisir une couleur |
| `editField` | Un `TextField` : le bouton est dessiné comme un champ de texte, pour renommer sur place |
| `visible` | À `false`, il n'est pas dessiné |

Certains champs sont conservés par le bouton sans qu'il les dessine : c'est son conteneur qui les utilise. `desplegable` indique qu'il ouvre un menu. `separador` demande une ligne avant lui, dans une barre. `rol` sert à le retrouver dans une barre sans dépendre de l'ordre. `ayuda` est le texte de l'infobulle qui apparaît quand on laisse la souris dessus ; l'infobulle ne fait pas partie de l'UI.

La hauteur est `UIBotonAltura()` ([Couleurs et échelle](tema.md)), ou `RenglonHeightGS + 2 * GlobalScale` avec `altoRenglon`. Il n'est jamais plus étroit qu'une ligne de texte.

## Onglet

`Tab` (`widgets/Tab.h`) est un bouton sans la bordure du bas : il se fond avec ce qui se trouve en dessous. L'onglet actif (`activa`) est un peu plus haut et de la couleur du contenu ; les autres sont plus sombres. Le texte est en `accent` s'il a le focus du clavier (`foco`), en blanc s'il est actif ou sous la souris, et en gris sinon.

Il s'utilise comme le bouton : `Resize(maxW)`, `Render()` et `Contains(mx, my)`, avec `sx` et `sy` définis par celui qui le dessine.

## Cases à cocher et échantillons de couleur

L'aspect d'une case à cocher est décidé à un seul endroit, `widgets/card.h`, ce qui empêche celle d'un menu et celle d'une carte d'être différentes.

| Fonction | Ce qu'elle fait |
|---|---|
| `DibujarCheckbox(lado, prendida)` | Dessine la boîte et, si elle est cochée, la coche |
| `DibujarMuestraColor(lado, rgba)` | Dessine un carré de cette couleur, avec les coins de la case |
| `CheckboxLado()` | Le côté : la hauteur d'une ligne de texte |
| `CheckboxColorCaja(prendida)`, `CheckboxColorTilde()` | Les couleurs de la boîte et de la coche |
| `CheckboxTilde()` | Le caractère de la coche, `✔` |

Cochée, la boîte est en `grisUI` et la coche de la couleur du fond. Décochée, la boîte est de la couleur du fond et n'a pas de coche.

```cpp
DibujarCheckbox(CheckboxLado(), sonidoPrendido);
```

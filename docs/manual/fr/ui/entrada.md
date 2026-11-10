# Entrées et champs de texte

## L'UI ne lit pas les événements

Whisk3D UI n'ouvre pas de fenêtres et n'écoute ni la souris, ni le clavier, ni l'écran tactile. Les événements sont reçus par le programme, sur presque tous les systèmes via SDL2 ([Entrées](../core/entrada.md) du Core), et il transmet à chaque widget ce qui le concerne :

| Événement | À qui il va |
|---|---|
| La souris ou le doigt se déplace | `PopupMenu::MouseMove(x, y)` ; le `hover` des boutons et des onglets ; `PropHoverGroup` et `PropHoverFila` dans les cartes |
| Clic ou toucher | `PopupMenu::Click(x, y)` ; `Button::Contains` et `Tab::Contains` ; dans une carte, la ligne que calcule le programme |
| Molette | `PopupMenu::Wheel(dir)` |
| Flèches | `button_up`, `button_down`, `button_left` et `button_right` des menus, des cartes et des lignes |
| OK ou Enter | `PopupMenu::Enter()`, `GroupPropertie::EnterPropertieSelect()` |
| Annuler ou Esc | `PopupMenu::Cerrar()`, `GroupPropertie::Cancel()` |
| Une lettre | Le champ de texte qui a le focus |

Les coordonnées sont des pixels d'écran, avec le Y vers le bas.

## Sans souris

Tout peut s'utiliser avec quatre flèches, OK et Annuler : c'est ainsi qu'on l'utilise sur le N95. Ce que l'UI prévoit pour cela :

- Les cartes se parcourent ligne par ligne et sautent ce qui ne peut pas être choisi.
- Les menus se parcourent avec les flèches, et l'option en surbrillance ne sort jamais de la vue.
- Les nombres se changent avec les flèches, sans les taper.
- Le focus du clavier est visible : `focoMenu` sur les boutons et `foco` sur les onglets, en `accent`, et la bordure blanche de `g_kfFocoProp` sur un losange.

Quelle touche du téléphone correspond à chacune, les touches programmables ou le OK, cela ne relève pas de l'UI : c'est l'éditeur et chaque système qui en font la traduction.

## Écran tactile

- Un toucher est un clic.
- Le toucher qui ouvre un sous-menu ne choisit rien dedans ([Menus déroulants](menus.md)).
- `UITactil` agrandit les boutons et les barres pour le doigt ([Couleurs et échelle](tema.md)). Aujourd'hui, rien ne l'active.

Faire glisser pour faire défiler, maintenir appuyé ou toucher deux fois : c'est à celui qui utilise l'UI de le gérer.

## Champs de texte

`TextField` (`widgets/TextField.h`) est l'état d'un champ de texte : le texte (`text`), la position du curseur (`caret`) et le fait que tout soit sélectionné ou non (`selectAll`). Il ne dessine pas et ne lit pas les touches : ce n'est qu'une donnée.

| Fonction | Ce qu'elle fait |
|---|---|
| `SetText(t)` | Définit le texte, avec le curseur à la fin |
| `SelectAll()` | Sélectionne tout : la première touche le remplace |
| `InsertChar(c)` | Écrit un octet à la position du curseur |
| `Backspace()`, `DelForward()` | Effacent vers l'arrière ou vers l'avant |
| `CaretIzq()`, `CaretDer()` | Déplacent le curseur. Si tout est sélectionné, retirent la sélection |

Effacer et déplacer le curseur traitent une lettre de plusieurs octets, comme un `ñ` ou une lettre cyrillique, comme une seule. Pour en écrire une, on passe chaque octet à `InsertChar`.

Un seul champ a le focus à la fois : `g_textFieldActivo`, ou `NULL` s'il n'y en a aucun. Les touches vont à celui-là.

```cpp
TextField nombre;
nombre.SetText("Jugador 1");
nombre.SelectAll();
g_textFieldActivo = &nombre;

// à chaque lettre
if (g_textFieldActivo) g_textFieldActivo->InsertChar(c);

// au dessin, avec la couleur du texte en place
RenderTextField(nombre, g_textFieldActivo == &nombre, textAlign::left, 200);
```

`RenderTextField(caja, foco, alineación, ancho)` le dessine à l'origine courante :

- Sans focus, c'est le texte.
- Avec le focus et tout sélectionné, le texte est en blanc sur `accentDark`.
- Avec le focus et sans sélection, le curseur apparaît : un petit trait qui clignote toutes les 530 ms.

`TextFieldParpadeoTick()` renvoie `true` quand le trait change : c'est le moment de redessiner.

`TextField.h` déclare aussi `TextFieldEnfocar`, `TextFieldCancelar` (Esc : revient au texte d'avant la prise de focus), `TextFieldInputChar` et `TextFieldFinDeCuadro`. Aujourd'hui, elles sont définies dans l'éditeur, dans `main/ui/ViewPorts/NumInput.cpp` : l'UI seule ne les fournit pas.

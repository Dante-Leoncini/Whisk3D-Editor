# Cartes et propriétés

## La carte

`GroupPropertie` (`Propieties/GroupPropertie.h`) est une carte avec un titre et des lignes. Chaque ligne est une propriété, une classe qui hérite de `PropertieBase` et pointe vers une de vos variables : la carte l'affiche et la change directement.

```cpp
GroupPropertie* jugador = new GroupPropertie("Jugador");
jugador->icono = IconType::gamepad;        // facultatif : entre la flèche et le titre

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
jugador->properties.push_back(vel);

jugador->Resize(ancho, 0);    // la hauteur est calculée par la carte
jugador->Render();            // à l'origine courante
```

La carte possède ses lignes : quand on la supprime, elle les supprime.

- Le titre porte une flèche pour la replier (`open`). Avec `flecha = false`, il n'en a pas : pour une fenêtre flottante.
- En dessous, il y a deux colonnes : l'étiquette à gauche et la valeur à droite. `anchoValores` est la part de la largeur que prend la valeur : 0.5 par défaut.
- `reservaKeyBtn = true` réserve tout à droite une colonne pour le losange d'animation.
- Presque toutes les lignes mesurent une ligne de texte plus un `gap`. Un nombre, une case à cocher ou une couleur sans variable (`value` à `NULL`) n'occupe pas de place et n'est pas dessiné.
- `visible = false` cache toute la carte.

La ligne choisie a l'étiquette en `accent`, la valeur en blanc et la bordure blanche ; pendant la modification, la valeur et la bordure passent en `accent`. La ligne sous la souris est en blanc : c'est le conteneur de la carte qui la marque, avec `PropHoverGroup` et `PropHoverFila`.

## Choisir et modifier

`selectIndex` est la ligne choisie : `-2` aucune, `-1` le titre, à partir de `0` une ligne.

| Fonction | Ce qu'elle fait |
|---|---|
| `NextSelect()`, `PrevSelect()` | Passent à la ligne du dessous ou du dessus. Sautent les espaces et les lignes sans variable. Renvoient `true` en sortant de la carte, pour passer à la suivante |
| `EnterPropertieSelect()` | Sur le titre, replie ou déplie la carte. Sur une ligne, la modifie |
| `button_up()`, `button_down()`, `button_left()`, `button_right()` | Pendant la modification, elles vont à la ligne |
| `Cancel()` | Remet la ligne comme elle était |

`editando` indique si une ligne est en cours de modification. Avec cela, les flèches changent la valeur ou déplacent le choix :

```cpp
if (jugador->editando) jugador->button_up();   // change la valeur
else jugador->PrevSelect();                    // la ligne du dessus
```

La souris est gérée par le conteneur de la carte : celle-ci ne sait pas où elle s'est retrouvée à l'écran. En haut, il y a la bordure (`borderGS`) et le titre (`RenglonHeightGS + gapGS`). Chaque ligne mesure ce que renvoie son `Resize(ancho)`.

## Les lignes

| Classe | Ce que c'est | Comment elle se modifie |
|---|---|---|
| `PropFloat` | Un nombre | Avec les flèches, ou en le tapant |
| `PropBool` | Une case à cocher, collée à droite | OK la coche ou la décoche |
| `PropColor` | Un carré de la couleur, collé à droite | OK passe en modification et garde la couleur, au cas où on annule |
| `PropButton` | Un bouton | OK appelle son action |
| `PropButtonRow` | Plusieurs boutons sur une ligne | Gauche et droite en choisissent un ; OK l'actionne |
| `PropText` | Un champ de texte | OK lui donne le focus pour écrire |
| `PropListMeshParts` | Une liste avec défilement | Haut et bas choisissent |
| `PropLabel` | Un texte à lire, sur une ou plusieurs lignes | Ne se choisit pas |
| `PropSeparator` | Une ligne | Ne se choisit pas |
| `PropGap` | Un espace d'un `gap` | Ne se choisit pas |
| `PropSelectText` | Une boîte avec son étiquette, sans valeur | Ne fait rien pour l'instant |

### Nombre

```cpp
PropFloat* vida = new PropFloat("Vida");
vida->value = &vidaJugador;
vida->SetRango(0, 100);           // ne dépasse pas ces bornes
vida->entero = true;              // s'affiche et avance par entiers
vida->stepFino = 1;               // haut et bas
vida->stepGrueso = 10;            // gauche et droite
vida->onChange = AlCambiarVida;   // void AlCambiarVida()
```

| Champ | Ce qu'il fait |
|---|---|
| `unit` | Un texte collé au nombre, comme `°`. C'est le deuxième argument du constructeur |
| `stepFino`, `stepGrueso` | Ce qu'ajoutent les flèches : 0.01 et 0.1 par défaut |
| `entero` | Affiche le nombre arrondi, et les flèches partent de l'arrondi. Sans cela, il a quatre décimales |
| `centrado` | Le nombre au milieu de sa boîte |
| `flechas` | Dessine `<` et `>` sur les côtés. Le clic sur elles est géré par le conteneur |
| `opciones`, `nOpciones` | Avec `entero` : le nombre est un index et c'est le nom de cette option qui s'affiche, traduit avec `W3dMenuTraducir` |
| `acelera` | Gauche et droite commencent par pas de 1 et, si elles se répètent, montent jusqu'à 10 |
| `dragStep` | De combien la valeur change par pixel quand on fait glisser. Le glissement est géré par le conteneur |

`Set(v)` affecte la valeur dans la plage et appelle `onChange`. Les flèches passent par là.

Pour le taper :

```cpp
vida->IniciarEdicionTexto();   // affiche la valeur, entièrement sélectionnée
// les touches vont à g_textFieldActivo : voir Entrées et champs de texte
NumEditCommit();               // Enter : l'applique
NumEditCancel();               // Esc : reste comme avant
```

`NumEditActivo()` indique si un nombre est en cours de modification. Un texte qui n'est pas un nombre vaut 0.

### Case à cocher

`value` pointe vers un `bool`. `onChange` est appelée à chaque changement. L'étiquette occupe toute la place à sa gauche et, si elle ne tient pas, elle est coupée avec `..`.

### Couleur

`value` pointe vers quatre `float` (RGBA). L'UI dessine le carré ; le sélecteur de couleur ne fait pas partie de l'UI. Pendant la modification, les flèches ne déplacent que la première composante.

### Bouton

`action` est la fonction qu'il appelle. `gris = true` l'estompe et il ne fait rien. `oculto = true` n'occupe pas de ligne.

Avec `conLabel = true`, la ligne est un nom et un bouton : le nom à gauche, de la largeur de son texte, jusqu'à 55 % de la ligne, et le bouton sur tout le reste. `labelChars` fixe la largeur du nom, pour aligner plusieurs lignes. Avec `button->desplegable`, il porte la petite flèche d'un menu. En outre :

- `AgregarPrefijo(accion, ayuda)` place un carré avant le bouton, pour afficher une couleur.
- `AgregarExtra(icono, accion, ayuda)` place jusqu'à deux petits boutons avec seulement une icône à droite. `slotsExtras` leur réserve de la place, pour qu'ils restent alignés d'une ligne à l'autre. `ExtraEnX(x)` indique lequel se trouve à cet x, pour le clic.

### Ligne de boutons

```cpp
PropButtonRow* fila = new PropButtonRow();
fila->Agregar("Aceptar", Aceptar);
fila->Agregar("Cancelar", Cancelar);
jugador->properties.push_back(fila);
```

Les boutons se partagent la largeur à parts égales. Les invisibles sont sautés et les autres se partagent leur place. `Disparar(i)` actionne le bouton `i` : c'est le conteneur qui l'appelle quand le clic tombe dans cette partie.

### Champ de texte

```cpp
PropText* nombre = new PropText("Nombre", "Jugador 1");
// nombre->field.text contient ce qui a été écrit
```

`onClick` : au lieu de passer en modification, il appelle cette fonction, par exemple pour ouvrir un explorateur de fichiers. `error = true` colore en rouge la bordure de la ligne choisie, jusqu'à ce que quelque chose soit écrit. `oculto = true` n'occupe pas de ligne. OK lui donne le focus avec `TextFieldEnfocar`, qui est aujourd'hui définie dans l'éditeur. Pour écrire, voir [Entrées et champs de texte](entrada.md).

### Texte

`PropLabel("texto")` est une ligne. `PropLabel("texto", true)` coupe aux espaces pour tenir dans la largeur ; un mot qui ne tient pas à lui seul reste entier. `rojo = true`, pour une erreur. `oculto = true` n'occupe pas de ligne.

### Liste

`PropListMeshParts` affiche `filasMax` lignes, 3 par défaut, avec une petite barre s'il y en a plus. La ligne choisie reste toujours visible. `Cancel` remet `selectIndex` à la valeur qu'il avait en entrant en modification.

Elle ne sert pas encore pour une liste quelconque : selon `modo`, elle lit une liste de la scène (les parties d'un maillage, ses cartes UV, ses calques de couleur, les modificateurs, les groupes de sommets, les os, les animations, les contraintes, les scripts) ou la demande à des points d'accroche de fonctions de l'éditeur (`MixFila*`, `RutinaFila*`, `PostFila*`).

## Le losange d'animation

Une ligne animable (`animProp` à partir de 0, dans `PropFloat`, `PropBool`, `PropColor` et `PropButton`) affiche un losange à droite si la carte a `reservaKeyBtn`. Sa couleur indique l'état :

| État | Couleur | Ce que cela veut dire |
|---|---|---|
| 0 | Gris | Pas d'animation |
| 1 | `accent`, atténué | Animée, sans image clé ici |
| 2 | `accent` | Il y a une image clé ici |
| 3 | Vert | La valeur a changé et n'a pas été enregistrée |

Le losange fait partie de l'UI ; son état, non. Il le demande à un point d'accroche que le programme installe, et le clic appelle l'autre :

```cpp
W3dKeyframeEstado = MiEstado;   // int MiEstado(int prop, int comp) : de 0 à 3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp) : ajoute ou retire l'image clé
```

Sans points d'accroche, le losange reste gris. La carte n'appelle pas `W3dKeyframeToggle` : c'est celui qui gère le clic qui l'appelle. `g_kfFocoProp` pointe vers la ligne dont le losange a le focus du clavier, et le marque d'une bordure blanche. `W3dRenderRomboKey(estado)` dessine un losange isolé.

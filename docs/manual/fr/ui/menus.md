# Menus déroulants

`PopupMenu` (`widgets/PopupMenu.h`) est une liste flottante d'options : le menu d'une barre, un menu contextuel, un sélecteur. Chaque option (`MenuItem`) a un texte et peut avoir :

- une **icône** à gauche,
- un **sous-menu**, avec une flèche à droite,
- une **case à cocher**, qui affiche et change un `bool`,
- une **glissière**, qui modifie un `float` entre deux valeurs,
- un **raccourci**, à droite et en gris pâle (« Shift D »),
- `green` : c'est l'option active, avec l'icône et le texte en `accent`,
- `gray` : pointe vers un `bool` ; tant qu'il vaut `false`, l'option apparaît à demi-opacité et ne répond pas,
- `lineUp` : une ligne qui la sépare du groupe du dessus.

Le menu peut avoir un **titre** (`title`) : il est en blanc, avec une ligne en dessous.

## Le construire avec une table

La façon recommandée est une table de `MenuDef` : le texte et ce que fait chaque option se trouvent sur la même ligne. Il n'y a pas de numéro à relier à un `switch` ailleurs.

```cpp
static bool grid = true;
static PopupMenu* recent = NULL;
static void New() { /* ... */ }
static void Open() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // texte     action  raccourci icône                    sous-menu   case
    { "Nuevo",   New,  "Ctrl N", ICON(IconType::more) },
    { "Abrir",   Open,  "Ctrl O", ICON(IconType::folder) },
    { "Recientes", NULL, NULL,     ICON(IconType::list),  &recent },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grid },
};

recent = new PopupMenu();          // le sous-menu doit exister avant la construction
PopupMenu* archivo = new PopupMenu();
archivo->title = "Archivo";
archivo->Build(ARCHIVO, 4);
```

`ICON(x)` ajoute 1 à l'icône : dans une table, 0 signifie « pas d'icône ». L'id de chaque option est sa ligne.

Deux points d'accroche facultatifs, que le programme installe :

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

Avec `W3dMenuTranslate`, les textes de la table sortent traduits lors de la construction du menu. Avec `W3dMenuShortcutText`, un raccourci peut être un nom qui commence par `@`, et le point d'accroche renvoie la touche correspondante. Sans ce point d'accroche, un raccourci avec `@` n'est pas affiché.

## Le construire à la main

```cpp
PopupMenu* view = new PopupMenu();
view->Add("Frente", 0, IconType::camera);
view->AddCheck("Mostrar piso", 1, &showFloor);
view->AddFloat("Tamaño", 2, &size, 0.0f, 4.0f);
MenuItem* it = view->Add("Alambre", 3);
it->green = isWire;
it->lineUp = true;
view->action = OnPick;              // void AlElegir(int id)
```

`Add` renvoie le `MenuItem*`, pour en changer le reste. Quand une option est choisie, `Run(id)` appelle son `action` si elle en a une et, sinon, l'`action(id)` du menu. `Clear()` supprime toutes les options, pour les menus qui changent.

## L'ouvrir

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

Il s'ouvre au point que vous lui passez : le pointeur, ou le bord d'un bouton. S'il ne tient pas, il se décale vers l'intérieur de l'écran. `Open` ne touche pas à `OpenMenu` : c'est celui qui l'ouvre qui le définit. `Close()` le remet à `NULL`.

À l'ouverture, il se mesure :

- La largeur est celle de la ligne la plus longue, avec son texte et sa colonne de droite : le raccourci, la flèche, la case à cocher ou la glissière. Si une ligne a une icône, toutes lui laissent la place, pour que les textes restent alignés.
- Il n'est jamais plus large que l'écran. S'il ne tient pas, le texte de chaque ligne est coupé avant sa colonne de droite : les cases à cocher restent visibles.
- Il n'est jamais plus haut que l'écran. S'il a plus de lignes, il affiche celles qui tiennent, avec une barre de défilement à droite. Il se fait défiler avec la molette (`Wheel`).

Un sous-menu s'ouvre à côté de sa ligne : à droite s'il tient, sinon à gauche. S'il ne tient d'aucun côté, il se rétrécit et laisse visible une colonne du menu de derrière, pour pouvoir passer la souris sur une autre option.

## Souris et doigt

```cpp
// au déplacement
if (OpenMenu) OpenMenu->MouseMove(mx, my);

// au clic ou au toucher
if (OpenMenu) {
    PopupMenu* m = OpenMenu;
    int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}

// au dessin, avec une projection plein écran
if (OpenMenu) OpenMenu->Render();
```

`Click` renvoie l'id choisi, `-1` si rien n'a été choisi ou `-2` s'il a ouvert un sous-menu.

- Passer la souris met la ligne en surbrillance et ouvre son sous-menu, sans clic.
- Si la souris s'éloigne de plus de `60 * GlobalScale` pixels, le menu se ferme tout seul. Avec `closeFar = false`, non : pour un menu qui ne s'est pas ouvert sous la souris.
- Un clic à l'extérieur le ferme.
- Une option ordinaire ferme le menu. Une case à cocher change et le laisse ouvert, pour en cocher plusieurs. Un clic sur une glissière fixe la valeur selon l'endroit où il tombe.
- Maintenir appuyé sur une glissière et faire glisser change la valeur en direct. Celui qui gère la souris passe chaque mouvement à `MenuSliderDragMove(mx)`, tant que `MenuSliderDragActive()` vaut `true`, et appelle `MenuSliderDragRelease()` au relâchement.
- Sur un écran tactile, le toucher qui ouvre un sous-menu ne choisit rien dedans, même si le sous-menu s'est retrouvé sous le doigt. Il faut un deuxième toucher.
- Ce qui est choisi dans un sous-menu s'exécute dans ce sous-menu : chaque menu a ses propres ids.

## Clavier

Le focus appartient au sous-menu ouvert le plus profond :

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // l'option en surbrillance, ou NULL
```

| Fonction | Ce qu'elle fait |
|---|---|
| `button_up()`, `button_down()` | Déplacent la surbrillance, en revenant au début à la fin. N'ouvrent pas de sous-menus |
| `OpenSubmenuCurrent()` | Ouvre le sous-menu de l'option en surbrillance et met en surbrillance sa première option |
| `FitSlider(+1)`, `FitSlider(-1)` | Déplacent la glissière d'un vingtième de sa plage |
| `Enter()` | Comme un clic sur l'option en surbrillance, et renvoie la même chose. Appelée sur `OpenMenu`, elle arrive d'elle-même au sous-menu le plus profond |
| `Close()` | Ferme le menu et ses sous-menus |

Au clavier, l'option en surbrillance reste toujours visible. Quelle touche appelle quoi, c'est celui qui utilise le menu qui le décide.

`MenuScreenW` et `MenuScreenH` doivent avoir la taille de l'écran : c'est avec eux que le menu se place.

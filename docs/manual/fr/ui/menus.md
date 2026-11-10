# Menus déroulants

`PopupMenu` (`widgets/PopupMenu.h`) est une liste flottante d'options : le menu d'une barre, un menu contextuel, un sélecteur. Chaque option (`MenuItem`) a un texte et peut avoir :

- une **icône** à gauche,
- un **sous-menu**, avec une flèche à droite,
- une **case à cocher**, qui affiche et change un `bool`,
- une **glissière**, qui modifie un `float` entre deux valeurs,
- un **raccourci**, à droite et en gris pâle (« Shift D »),
- `verde` : c'est l'option active, avec l'icône et le texte en `accent`,
- `gris` : pointe vers un `bool` ; tant qu'il vaut `false`, l'option apparaît à demi-opacité et ne répond pas,
- `lineaArriba` : une ligne qui la sépare du groupe du dessus.

Le menu peut avoir un **titre** (`titulo`) : il est en blanc, avec une ligne en dessous.

## Le construire avec une table

La façon recommandée est une table de `MenuDef` : le texte et ce que fait chaque option se trouvent sur la même ligne. Il n'y a pas de numéro à relier à un `switch` ailleurs.

```cpp
static bool grilla = true;
static PopupMenu* recientes = NULL;
static void Nuevo() { /* ... */ }
static void Abrir() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // texte     action  raccourci icône                    sous-menu   case
    { "Nuevo",   Nuevo,  "Ctrl N", ICONO(IconType::mas) },
    { "Abrir",   Abrir,  "Ctrl O", ICONO(IconType::carpeta) },
    { "Recientes", NULL, NULL,     ICONO(IconType::lista),  &recientes },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grilla },
};

recientes = new PopupMenu();          // le sous-menu doit exister avant la construction
PopupMenu* archivo = new PopupMenu();
archivo->titulo = "Archivo";
archivo->Construir(ARCHIVO, 4);
```

`ICONO(x)` ajoute 1 à l'icône : dans une table, 0 signifie « pas d'icône ». L'id de chaque option est sa ligne.

Deux points d'accroche facultatifs, que le programme installe :

```cpp
W3dMenuTraducir = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuAtajoTexto = MiAtajo;      // std::string MiAtajo(const std::string& atajo)
```

Avec `W3dMenuTraducir`, les textes de la table sortent traduits lors de la construction du menu. Avec `W3dMenuAtajoTexto`, un raccourci peut être un nom qui commence par `@`, et le point d'accroche renvoie la touche correspondante. Sans ce point d'accroche, un raccourci avec `@` n'est pas affiché.

## Le construire à la main

```cpp
PopupMenu* vista = new PopupMenu();
vista->Agregar("Frente", 0, IconType::camera);
vista->AgregarCheck("Mostrar piso", 1, &verPiso);
vista->AgregarFloat("Tamaño", 2, &tamanio, 0.0f, 4.0f);
MenuItem* it = vista->Agregar("Alambre", 3);
it->verde = esAlambre;
it->lineaArriba = true;
vista->action = AlElegir;              // void AlElegir(int id)
```

`Agregar` renvoie le `MenuItem*`, pour en changer le reste. Quand une option est choisie, `Ejecutar(id)` appelle son `accion` si elle en a une et, sinon, l'`action(id)` du menu. `Limpiar()` supprime toutes les options, pour les menus qui changent.

## L'ouvrir

```cpp
archivo->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
MenuAbierto = archivo;
```

Il s'ouvre au point que vous lui passez : le pointeur, ou le bord d'un bouton. S'il ne tient pas, il se décale vers l'intérieur de l'écran. `Abrir` ne touche pas à `MenuAbierto` : c'est celui qui l'ouvre qui le définit. `Cerrar()` le remet à `NULL`.

À l'ouverture, il se mesure :

- La largeur est celle de la ligne la plus longue, avec son texte et sa colonne de droite : le raccourci, la flèche, la case à cocher ou la glissière. Si une ligne a une icône, toutes lui laissent la place, pour que les textes restent alignés.
- Il n'est jamais plus large que l'écran. S'il ne tient pas, le texte de chaque ligne est coupé avant sa colonne de droite : les cases à cocher restent visibles.
- Il n'est jamais plus haut que l'écran. S'il a plus de lignes, il affiche celles qui tiennent, avec une barre de défilement à droite. Il se fait défiler avec la molette (`Wheel`).

Un sous-menu s'ouvre à côté de sa ligne : à droite s'il tient, sinon à gauche. S'il ne tient d'aucun côté, il se rétrécit et laisse visible une colonne du menu de derrière, pour pouvoir passer la souris sur une autre option.

## Souris et doigt

```cpp
// au déplacement
if (MenuAbierto) MenuAbierto->MouseMove(mx, my);

// au clic ou au toucher
if (MenuAbierto) {
    PopupMenu* m = MenuAbierto;
    int id = m->Click(mx, my);
    if (id >= 0) m->Ejecutar(id);
}

// au dessin, avec une projection plein écran
if (MenuAbierto) MenuAbierto->Render();
```

`Click` renvoie l'id choisi, `-1` si rien n'a été choisi ou `-2` s'il a ouvert un sous-menu.

- Passer la souris met la ligne en surbrillance et ouvre son sous-menu, sans clic.
- Si la souris s'éloigne de plus de `60 * GlobalScale` pixels, le menu se ferme tout seul. Avec `cerrarLejos = false`, non : pour un menu qui ne s'est pas ouvert sous la souris.
- Un clic à l'extérieur le ferme.
- Une option ordinaire ferme le menu. Une case à cocher change et le laisse ouvert, pour en cocher plusieurs. Un clic sur une glissière fixe la valeur selon l'endroit où il tombe.
- Maintenir appuyé sur une glissière et faire glisser change la valeur en direct. Celui qui gère la souris passe chaque mouvement à `MenuSliderDragMover(mx)`, tant que `MenuSliderDragActivo()` vaut `true`, et appelle `MenuSliderDragSoltar()` au relâchement.
- Sur un écran tactile, le toucher qui ouvre un sous-menu ne choisit rien dedans, même si le sous-menu s'est retrouvé sous le doigt. Il faut un deuxième toucher.
- Ce qui est choisi dans un sous-menu s'exécute dans ce sous-menu : chaque menu a ses propres ids.

## Clavier

Le focus appartient au sous-menu ouvert le plus profond :

```cpp
PopupMenu* m = MenuAbierto;
while (m->submenuAbierto && m->submenuAbierto->abierto) m = m->submenuAbierto;
MenuItem* it = m->ItemActual();      // l'option en surbrillance, ou NULL
```

| Fonction | Ce qu'elle fait |
|---|---|
| `button_up()`, `button_down()` | Déplacent la surbrillance, en revenant au début à la fin. N'ouvrent pas de sous-menus |
| `AbrirSubmenuActual()` | Ouvre le sous-menu de l'option en surbrillance et met en surbrillance sa première option |
| `AjustarSlider(+1)`, `AjustarSlider(-1)` | Déplacent la glissière d'un vingtième de sa plage |
| `Enter()` | Comme un clic sur l'option en surbrillance, et renvoie la même chose. Appelée sur `MenuAbierto`, elle arrive d'elle-même au sous-menu le plus profond |
| `Cerrar()` | Ferme le menu et ses sous-menus |

Au clavier, l'option en surbrillance reste toujours visible. Quelle touche appelle quoi, c'est celui qui utilise le menu qui le décide.

`MenuPantallaW` et `MenuPantallaH` doivent avoir la taille de l'écran : c'est avec eux que le menu se place.

# Qu'est-ce que Whisk3D ?

Whisk3D est un **éditeur 3D et moteur de jeu** libre, créé de zéro. L'idée est de pouvoir créer ce que vous voulez avec ce que vous avez : il fonctionne sur un PC, sur un téléphone et aussi sur des consoles et de vieux appareils, sans dépendre de matériel moderne ni de logiciel propriétaire.

## Ce que l'on peut faire

Avec **Whisk3D Editor**, vous pouvez :

- **Modéliser** en 3D : des maillages avec des sommets, des arêtes et des faces, et des [modificateurs](objeto-malla.md#longlet-modificateurs) qui travaillent sans détruire le maillage.
- Leur créer un **rig** : construire l'[armature](esqueletos.md) d'un personnage et peindre l'influence de chaque os.
- **Animer** : keyframes, éditeur de courbes et animations par armature ([Timeline](linea-de-tiempo.md)).
- **Éclairer** la scène avec des lumières ponctuelles, des soleils et des spots.
- **Faire le rendu** d'images et de vidéos, en temps réel ou par **lancer de rayons** (ombres douces, reflets et transparences).
- **Peindre** les couleurs des sommets et les textures, directement sur le modèle ([Modes de peinture](pintura.md)).
- **Créer des jeux** : la logique s'écrit en [Lua](../lua/README.md), avec l'[éditeur de scripts](editor-de-scripts.md) intégré ; les interfaces (menus, HUD) se construisent dans l'[Éditeur 2D](editor-2d.md), et le jeu se compile pour chaque système depuis l'éditeur lui-même.
- Importer et exporter des modèles en OBJ, FBX, glTF, GLB et STL.

Whisk3D a en plus son **propre format libre** pour enregistrer les projets et les jeux : le **`.w3d`**. Un jeu en `.w3d` peut s'ouvrir directement sur n'importe quel système où fonctionne Whisk3D, ou être **compilé** pour un système précis, afin de tirer le meilleur parti de son matériel. Voir l'article [W3D : un format libre pour que les jeux vidéo ne meurent pas](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=fr).

Les jeux peuvent aussi se faire **sans l'éditeur**, en programmant directement sur **Whisk3D Core** en C et en C++. Cette voie est réservée aux utilisateurs expérimentés, qui savent déjà programmer en C et en C++ : voir le [Manuel de Whisk3D Core](../core/empezar.md).

## Où il fonctionne

| Système | |
|---|---|
| **PC** | Windows, Linux et macOS |
| **Téléphones** | Android (à partir de la 2.3) et les Nokia N95 et N8 (Symbian) |
| **Navigateur** | N'importe quel navigateur moderne (WebGL) |
| **Consoles** | Sega Dreamcast, Sony PSP, PlayStation Vita et Nintendo 3DS |

## Comment il est construit

Whisk3D, ce sont trois projets posés l'un sur l'autre, comme trois engrenages qui tournent ensemble :

- **Whisk3D Editor** (l'engrenage vert) : l'éditeur, avec tout ce qui précède.
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=fr)** (le jaune) : se charge de l'**interface** : les boutons, les menus, les cartes de propriétés, le texte et les icônes. L'éditeur est construit avec elle. Voir le [Manuel de Whisk3D UI](../ui/README.md).
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=fr)** (le rouge) : la **couche d'abstraction**. Whisk3D UI l'utilise pour tout ce qu'elle fait.

Un jeu ou un projet créé dans l'éditeur vit **à l'intérieur** de l'éditeur, avec sa logique écrite en Lua. Mais l'Editor et Whisk3D UI sont **facultatifs** : on peut utiliser ses propres outils. Un jeu en C ou en C++ peut utiliser Whisk3D UI pour son interface, sans l'éditeur, ou s'adresser **directement au Core**, avec sa propre interface et ses propres outils. La seule chose indispensable, c'est le Core.

![L'architecture de Whisk3D en trois couches : en haut l'Editor (vert), avec votre jeu ou projet en Lua à l'intérieur ; au milieu Whisk3D UI (jaune) ; en bas Whisk3D Core (rouge). L'Editor et l'UI sont facultatifs : de l'extérieur, un jeu en C++ peut utiliser l'UI et un autre directement le Core. Le Core parle à chaque système par son propre backend](img/whisk3d-capas.webp)

Le Core est l'**ingrédient secret**. Chaque système dessine, joue les sons, lit les fichiers et reçoit les touches à sa manière : un PC utilise OpenGL, la Dreamcast a sa propre puce graphique, la PSP et la 3DS leurs propres bibliothèques, le navigateur WebGL. Ni l'éditeur ni les jeux ne s'adressent à chaque système : ils demandent les choses au Core, toujours de la même façon (« dessine ce maillage », « joue ce son », « ouvre ce fichier »), et le Core sait comment le faire sur chacun. C'est pourquoi le **même code** fonctionne sur des machines si différentes, et ajouter un nouveau système consiste à apprendre au Core à lui parler, pas à réécrire l'éditeur.

Comment il s'y prend, en détail (les graphismes, le son, les fichiers, les entrées, le réseau, les logs) : [Manuel de Whisk3D Core](../core/README.md).

## Le projet

Tout le travail est enregistré dans un fichier de projet `.w3d`. Il contient la scène, mais aussi les ressources : textures (`texturas/`), scripts (`scripts/`), sons, scènes, prefabs. Une ressource peut se trouver **dans** le .w3d ou **en dehors**, comme fichier séparé sur le disque ; les menus « Enregistrer dans le .w3d » et « Enregistrer hors du .w3d » la font passer de l'un à l'autre. Voir [Le projet](proyecto.md).

> [!NOTE]
> Ce manuel décrit ce que Whisk3D sait faire aujourd'hui. Les noms des fonctions sont ceux affichés dans l'éditeur en français. Les touches indiquées sont celles par défaut : vous pouvez les modifier dans [Raccourcis clavier](atajos.md).

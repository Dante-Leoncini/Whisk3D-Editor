# 3D Viewport

Le 3D Viewport est l'endroit où l'on voit et modifie la scène. Il fonctionne selon différents **modes** : l'effet des clics, des touches et des menus dépend du mode actuel. Le mode se choisit avec le sélecteur de la barre supérieure (il n'apparaît que si l'objet actif est un maillage ou une armature) ou avec <kbd>Tab</kbd>.

| Mode | Pour quels objets | À quoi il sert |
|---|---|---|
| [Object Mode](modo-objeto.md) | Tous | Créer, déplacer, faire pivoter, mettre à l'échelle, dupliquer, apparenter et supprimer des objets entiers. |
| [Edit Mode](modo-edicion.md) | Maillages et courbes | Modifier les sommets, les arêtes et les faces (modéliser). |
| [Edit Mode (armature)](esqueletos.md#edit-mode-armature) | Armatures | Créer et placer les os. |
| [Pose Mode](esqueletos.md#pose-mode) | Armatures | Poser et animer les os. |
| [Weight Paint](pintura.md#weight-paint) | Maillages 3D uniquement | Peindre l'influence de chaque os sur chaque sommet. |
| [Vertex Paint](pintura.md#vertex-paint) | Maillages 3D uniquement | Peindre des couleurs directement sur les sommets. |
| [Texture Paint](pintura.md#texture-paint) | Maillages 3D uniquement | Peindre sur la texture, directement sur l'objet. |

## Barre supérieure du 3D Viewport

| Bouton | Description |
|---|---|
| Sélecteur de mode | Change de mode (Object, Edit, Pose, Paint…). |
| Vertex / Edge / Face | Uniquement en Edit Mode : ce que l'on sélectionne. |
| Transform Pivot Point | Ce autour de quoi on fait pivoter et met à l'échelle : 3D Cursor, Individual Origins, Median Point ou Active Element. Avec la case Lock Normals. |
| Orientation | Les axes de la transformation : Global, Local, Normal (la direction de la face) ou View. |
| Snap | Options d'aimantation. Le bouton devient vert si elle est active (<kbd>Shift+Tab</kbd>). |
| Proportional Editing | Quand on déplace quelque chose, ce qui est proche suit en douceur. Le bouton devient vert si elle est active (<kbd>O</kbd>). |
| View | Caméras, points de vue, cadrage, vue locale, perspective, verrouillage de l'orbite. |
| Select | Tout, aucun, inverser, boîte, lié, boucles et chemins. |
| Add (+) | Uniquement en Object Mode : créer de nouveaux objets. |
| Mesh | Uniquement en Edit Mode : les opérations de modélisation. |
| Save Texture (disquette) | Uniquement en Texture Paint. Vert s'il y a de la peinture non enregistrée. |
| Animation (losange) | Insérer, supprimer et effacer des keyframes ; trajectoire de mouvement. |
| Context | Le menu du mode : Object, Vertex/Edge/Face, Armature ou Pose. |
| Overlays (œil) | Les aides affichées : sol, axes, gizmo, curseur 3D, normales, statistiques, rayons X. |
| Render (caméra) | Le mode d'ombrage. |
| Scene | Choisir quelle scène, quel jeu ou quel prefab du projet modifier, et en créer de nouveaux. |
| UV | Uniquement en Edit Mode : déplier et projeter les UV. |

## Menus communs à plusieurs modes

### Select

| Fonction | Description | Raccourci |
|---|---|---|
| All | Sélectionne tout. | <kbd>A</kbd> |
| None | Désélectionne tout. | <kbd>Alt+A</kbd> |
| Invert | Ce qui était sélectionné est désélectionné, et inversement. | <kbd>Ctrl+I</kbd> |
| Box Select | Glisser de gauche à droite (vert) ne prend que ce qui est entièrement à l'intérieur ; de droite à gauche (bleu), il suffit d'effleurer. <kbd>Shift</kbd> ajoute. | <kbd>B</kbd> |
| Show selection | Affiche ou masque le contour de la sélection. | Menu |

### Render (modes d'ombrage)

| Fonction | Description |
|---|---|
| Render Preview | Avec les lumières de la scène. Si le Ray Tracing est actif, affiche l'image tracée petit à petit. |
| Material Preview | Celui par défaut : une lumière neutre venant de la vue, sans dépendre des lumières de la scène. |
| Solid / Wireframe / ZBuffer Preview | Gris uni, seulement les arêtes, ou la profondeur. |
| Normal View / Alpha Preview | Les normales en couleur, ou la transparence en noir et blanc. |
| Motion vectors | De combien chaque pixel a bougé à l'écran depuis l'image précédente (rouge = X, vert = Y ; lavande = immobile, olive = le fond). La même image que la passe `_vectores` du rendu. |
| Wireframe: Only Visible | La vue filaire n'affiche que ce qui est réellement dessiné. |

### Overlays

| Fonction | Description | Raccourci |
|---|---|---|
| Show Overlays | Active ou désactive toutes les aides visuelles d'un coup. | <kbd>K</kbd> |
| Floor, X Axis, Y Axis, Origins, 3D Cursor, Relationship Lines | Chaque aide séparément. | Menu |
| Gizmo | Le gizmo de déplacement, de rotation et de mise à l'échelle. | Menu |
| Objects | Afficher ou masquer armatures, lumières, caméras, empties, particules, courbes, hitbox. | Menu |
| Normals | Dessine les normales de sommet, personnalisées ou de face, avec leur taille. | Menu |
| Statistics | FPS, sommets, faces, appels de dessin et temps. | Menu |
| X-Ray | Le maillage en édition apparaît semi-transparent et l'on peut choisir les éléments situés derrière. | Menu |

### Snap

| Fonction | Description | Raccourci |
|---|---|---|
| Enable | Ce que l'on déplace s'aimante aux sommets, aux arêtes, aux faces ou à leurs centres. | <kbd>Shift+Tab</kbd> |
| Snap Base et Snap Target | Quel point de l'élément déplacé s'aimante (Closest, Center, Median, Active) et à quoi. | Menu |
| Face Project / Face Nearest | Pour la retopologie : les sommets se posent sur une autre surface. | Menu |
| Affect Move / Rotate / Scale | Les opérations sur lesquelles agit l'aimantation. | Menu |
| Menu Snap du curseur | Selection to Grid, to Cursor ou to Active ; Cursor to Selected, to World Origin, to Grid ou to Active. | <kbd>Shift+S</kbd> |

### Proportional Editing

| Fonction | Description | Raccourci |
|---|---|---|
| Enable | Quand on déplace, fait pivoter ou met à l'échelle, les éléments voisins suivent selon la distance. | <kbd>O</kbd> |
| Connected Only | Mesure la distance le long des arêtes : n'entraîne pas les pièces séparées. | Menu |
| Falloff | Forme de l'influence : Smooth, Sphere, Root, Inverse Square, Sharp, Linear, Constant, Random. | Menu |
| Proportional Size | Le rayon d'influence. Pendant la transformation, il se modifie avec la molette ou avec <kbd>PageUp</kbd> / <kbd>PageDown</kbd>. | Menu / molette |

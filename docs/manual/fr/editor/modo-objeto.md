# Mode Objet

| Fonction | Description | Utilisation |
|---|---|---|
| Sélectionner | Choisit un objet. Un clic dans le vide désélectionne. | Clic ; <kbd>Shift</kbd>+clic ajoute ou retire |
| Déplacer / Tourner / Échelle | Voir [Déplacer, faire pivoter et mettre à l'échelle](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Ajouter | Crée un nouvel objet au curseur 3D (voir [menu Ajouter](#menu-ajouter)). | <kbd>Shift+A</kbd> ou bouton + |
| Appliquer | Applique (« cuit ») la position, la rotation, l'échelle ou le tout dans le maillage (l'objet reste en 0, 0, 0, sans rotation et à l'échelle 1). | <kbd>Ctrl+A</kbd> |
| Définir l'origine | Géométrie à l'origine, Origine à la géométrie, Origine au curseur 3D. | Menu Objet |
| Aimantation | Amène le curseur 3D sur la sélection, sur l'objet actif, à l'origine du monde ou sur la grille, et amène la sélection sur le curseur, sur l'objet actif ou sur la grille. | <kbd>Shift+S</kbd> ou menu Objet → Aimantation |
| Dupliquer objets | Copie indépendante ; elle reste saisie pour être déplacée. | <kbd>Shift+D</kbd> |
| Dupliquer lié | Copie qui partage le maillage avec l'original (modifier l'une modifie les deux). | <kbd>Alt+D</kbd> |
| Joindre | Réunit les maillages sélectionnés dans l'objet actif. | <kbd>Ctrl+J</kbd> |
| Définir parent | La sélection devient enfant de l'objet actif (en conservant ou non la transformation). | <kbd>Ctrl+P</kbd> |
| Effacer parent | Retire le parent (en conservant ou non la position). | <kbd>Alt+P</kbd> |
| Supprimer | Supprime les objets sélectionnés (demande une confirmation). | <kbd>X</kbd> ou <kbd>Delete</kbd> |
| Masquer / Afficher | Alterne la visibilité de l'objet actif. | <kbd>H</kbd> |
| Créer prefab / Dépaqueter prefab | Convertit la sélection en un prefab réutilisable, ou sépare de nouveau une instance. | Menu Objet |
| Insérer image clé | Enregistre la position, la rotation et/ou l'échelle à la frame actuelle (menu des canaux). <kbd>Shift+I</kbd> enregistre tout sans demander. | <kbd>I</kbd> |
| Supprimer / Effacer images clés | Supprime la keyframe de la frame actuelle ou toute l'animation. | Menu Animation |
| Trace de mouvement | Dessine le chemin parcouru par les objets animés. | Menu Animation |
| Changer de mode | Maillage : Objet ↔ Édition. Armature : Objet ↔ Pose. Courbe : édition des nœuds. | <kbd>Tab</kbd> |

## Menu Ajouter

Uniquement en Mode Objet. Le nouvel élément apparaît au curseur 3D et reste sélectionné. Les primitives ouvrent un panneau pour ajuster leurs dimensions.

| Fonction | Description |
|---|---|
| Maillage | Plan, Cube, Cercle, Sphère UV, Cône, Cylindre (avec taille, rayon, segments, ombrage lisse). Aussi Sommet (un point isolé pour commencer à modéliser) et Référence (un plan avec une image, pour décalquer). |
| Vide | Un objet sans maillage, pour regrouper ou comme point de référence. |
| Caméra / Lumière / Brouillard | Caméra, lumière et brouillard de la scène. La lumière a un **Type** : Ponctuelle, Soleil (directionnelle) ou Spot (un cône, dessiné dans la vue) ; Ajouter propose aussi Soleil et Spot. **Intensité** la rend plus forte (animable) ; **Distance** (m) indique jusqu'où porte une lumière ponctuelle ou un spot, et la longueur de son cône (0 = sans limite ; le lancer de rayons l'éteint en douceur à cette distance). Quand la lumière est sélectionnée, un spot montre son cône et une lumière ponctuelle une sphère de cette distance ; un soleil montre toujours une ligne jusqu'au sol, vers où il éclaire. |
| Son | Un son placé dans la scène (icône de haut-parleur). Il joue avec la timeline en **secondes**, pas en frames ; avec **Positionnel (3D)** il s'entend depuis sa position (plus fort quand la caméra est proche, du côté où il est) ; sinon c'est comme une musique. Déposer un .wav dans la vue 3D en crée un à cet endroit. |
| Armature | Une armature avec un os, pour animer des personnages. |
| Chemin | Une courbe à deux nœuds (elle se modifie comme un maillage : <kbd>E</kbd> ajoute des nœuds). |
| Collection | Un dossier pour ranger les objets. |
| UI | Une interface 2D pour les jeux (elle se modifie dans l'Éditeur 2D). |
| Hitbox | Une boîte de détection de collisions ; s'il y a un objet actif, elle naît comme son enfant, à sa taille. |
| Physique | Ne crée pas d'objet : ajoute un corps rigide à l'objet actif. |
| Particules | Un émetteur de particules. |
| LOD / Culling / Grid Cull | Objets d'optimisation : niveaux de détail et masquage de ce qui n'est pas visible. |
| Effacer Z / Rectangle écran | Outils de dessin : effacer la profondeur ou découper une zone de l'écran. |
| Prefab / Scène / Proxy W3D | Insérer un prefab du projet, une autre scène, ou un prefab d'une bibliothèque externe. |
| Imports | OBJ, FBX, glTF, GLB, STL ou une interface Whisk3D UI. Le STL (impression 3D, CAO), qui a Z vers le haut, est importé pivoté pour qu'il soit debout, avec les sommets en double soudés. |
| Noyau (avancé) | Routines : listes d'étapes de dessin construites à la main (transformer, dessiner, textures, lumières, brouillard, mélange, profondeur, contrôle, Appeler Lua). Comprend **Boucle de routine** : une routine qui répète ses étapes plusieurs fois (par exemple « translater, dessiner » 20 fois), comme un array mais plus rapide. |

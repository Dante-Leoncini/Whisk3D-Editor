# Object Mode

| Fonction | Description | Utilisation |
|---|---|---|
| Select | Choisit un objet. Un clic dans le vide désélectionne. | Clic ; <kbd>Shift</kbd>+clic ajoute ou retire |
| Move / Rotate / Scale | Voir [Déplacer, faire pivoter et mettre à l'échelle](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Add | Crée un nouvel objet au curseur 3D (voir [menu Add](#menu-add)). | <kbd>Shift+A</kbd> ou bouton + |
| Apply | Applique (« cuit ») la position, la rotation, l'échelle ou le tout dans le maillage (l'objet reste en 0, 0, 0, sans rotation et à l'échelle 1). | <kbd>Ctrl+A</kbd> |
| Set Origin | Geometry to Origin, Origin to Geometry, Origin to 3D Cursor. | Menu Object |
| Duplicate Objects | Copie indépendante ; elle reste saisie pour être déplacée. | <kbd>Shift+D</kbd> |
| Duplicate Linked | Copie qui partage le maillage avec l'original (modifier l'une modifie les deux). | <kbd>Alt+D</kbd> |
| Join | Réunit les maillages sélectionnés dans l'objet actif. | <kbd>Ctrl+J</kbd> |
| Set Parent | La sélection devient enfant de l'objet actif (en conservant ou non la transformation). | <kbd>Ctrl+P</kbd> |
| Clear Parent | Retire le parent (en conservant ou non la position). | <kbd>Alt+P</kbd> |
| Delete | Supprime les objets sélectionnés (demande une confirmation). | <kbd>X</kbd> ou <kbd>Delete</kbd> |
| Hide / Show | Alterne la visibilité de l'objet actif. | <kbd>H</kbd> |
| Create Prefab / Unpack Prefab | Convertit la sélection en un prefab réutilisable, ou sépare de nouveau une instance. | Menu Object |
| Insert Keyframe | Enregistre la position, la rotation et/ou l'échelle à la frame actuelle (menu des canaux). <kbd>Shift+I</kbd> enregistre tout sans demander. | <kbd>I</kbd> |
| Delete / Clear Keyframe | Supprime la keyframe de la frame actuelle ou toute l'animation. | Menu Animation |
| Motion Trail | Dessine le chemin parcouru par les objets animés. | Menu Animation |
| Changer de mode | Maillage : Object ↔ Edit. Armature : Object ↔ Pose. Courbe : édition des nœuds. | <kbd>Tab</kbd> |

## Menu Add

Uniquement en Object Mode. Le nouvel élément apparaît au curseur 3D et reste sélectionné. Les primitives ouvrent un panneau pour ajuster leurs dimensions.

| Fonction | Description |
|---|---|
| Mesh | Plane, Cube, Circle, UV Sphere, Cone, Cylinder (avec taille, rayon, segments, ombrage lisse). Aussi Vertex (un point isolé pour commencer à modéliser) et Reference (un plan avec une image, pour décalquer). |
| Empty | Un objet sans maillage, pour regrouper ou comme point de référence. |
| Camera / Light / Fog | Caméra, lumière et brouillard de la scène. La lumière a un **Type** : Point, Soleil (directionnelle) ou Spot (un cône, dessiné dans la vue) ; Ajouter propose aussi Soleil et Spot. **Intensity** la rend plus forte (animable) ; **Distance** (m) indique jusqu'où porte une lumière ponctuelle ou un spot, et la longueur de son cône (0 = sans limite ; le lancer de rayons l'éteint en douceur à cette distance). Quand la lumière est sélectionnée, un spot montre son cône et une lumière ponctuelle une sphère de cette distance ; un soleil montre toujours une ligne jusqu'au sol, vers où il éclaire. |
| Son | Un son placé dans la scène (icône de haut-parleur). Il joue avec la timeline en **secondes**, pas en frames ; avec **Positional (3D)** il s'entend depuis sa position (plus fort quand la caméra est proche, du côté où il est) ; sinon c'est comme une musique. Déposer un .wav dans la vue 3D en crée un à cet endroit. |
| Armature | Une armature avec un os, pour animer des personnages. |
| Path | Une courbe à deux nœuds (elle se modifie comme un maillage : <kbd>E</kbd> ajoute des nœuds). |
| Collection | Un dossier pour ranger les objets. |
| UI | Une interface 2D pour les jeux (elle se modifie dans le 2D Editor). |
| Hitbox | Une boîte de détection de collisions ; s'il y a un objet actif, elle naît comme son enfant, à sa taille. |
| Physics | Ne crée pas d'objet : ajoute un corps rigide à l'objet actif. |
| Particles | Un émetteur de particules. |
| LOD / Culling / Grid Cull | Objets d'optimisation : niveaux de détail et masquage de ce qui n'est pas visible. |
| Clear Depth / Screen Rect | Outils de dessin : effacer la profondeur ou découper une zone de l'écran. |
| Prefab / Scene / Proxy W3D | Insérer un prefab du projet, une autre scène, ou un prefab d'une bibliothèque externe. |
| Imports | OBJ, FBX, glTF, GLB ou une interface Whisk3D UI. |
| Core (advanced) | Routines : listes d'étapes de dessin construites à la main (transformer, dessiner, textures, lumières, brouillard, mélange, profondeur, contrôle, Call Lua). Comprend **Routine loop** : une routine qui répète ses étapes plusieurs fois (par exemple « translater, dessiner » 20 fois), comme un array mais plus rapide. |

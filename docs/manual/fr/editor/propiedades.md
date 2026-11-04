# Properties

Le panneau qui regroupe toutes les données de l'objet actif, organisé en onglets (icônes). Les onglets affichés dépendent du type d'objet. Un clic sur le titre d'une carte la replie. Dans les champs numériques : cliquez pour saisir une valeur (les calculs sont acceptés), faites glisser pour faire varier la valeur. Le losange de droite ajoute ou retire une keyframe.

| Onglet | Contenu |
|---|---|
| File / Render (disquette) | **File** : Open project, Save, Save As, Save version, Extract assets. **Render** : Path, taille, passes supplémentaires, Background, **Ray Tracing** (Rays, Samples, Passes), Render Image et Render Animation. **Game** : Platform (Windows, Symbian, Linux .deb, AppImage, WebGL, Android), Window mode, Orientation, Icon, physique, son et Build game. **Export** : OBJ, glTF, GLB. **Settings** : Language, Antialiasing, Graphics, Skin, logs, Scale, **Keyboard Shortcuts…** et Save Changes. |
| Object | Location, Rotation (Euler, quaternion ou axe-angle), Scale, Name, Visible, Renderable ; animations et palette de l'objet. |
| Contextual | Dépend du type : parties de maillage et **matériau** (texture, éclairage, couleur de sommet, transparence, reflets, normal map, mélange, profondeur, couches de texture, **Advanced: custom shader** — voir [Shaders écrits](shaders.md) ; Roughness, Metallic, Transmission et IOR avec le Ray Tracing), lumière, caméra, brouillard, particules, physique, hitbox, armature et os, éléments 2D, prefabs, routines. |
| 3D Mesh | Le maillage utilisé (et le nombre d'objets qui le partagent), cartes UV, couches de couleur, groupes de sommets, groupes UV. Avec une ressource choisie dans Library : ses données et son emplacement (dans le .w3d ou en dehors). |
| Modifiers | Voir [Modificateurs](modificadores.md). |
| Transform | Uniquement en Edit Mode : centre de la sélection et de ses UV, modifiables. |
| Armature 2D | Armatures et os 2D du maillage. |
| Constraints | Copy Location, Copy Rotation, Billboard (regarder la caméra), Child Of. |
| Scripts | Les scripts Lua de l'objet et leurs propriétés (voir le [document Lua](../lua/README.md)). |
| Animation | L'animation active, les couches de mélange (Mix), les valeurs exactes de la keyframe choisie et les coupes de caméra. |

## Routines

Une routine est une liste d'étapes de dessin (translater, faire pivoter, lier une texture, dessiner un maillage, appeler une autre routine, Call Lua…) que l'on construit dans Properties. C'est la façon la plus rapide de dessiner sur le N95.

| Fonction | Description | Raccourci |
|---|---|---|
| Ajouter / retirer une étape | Ajoute des étapes regroupées par thème, ou retire celle qui est choisie. | Boutons ; <kbd>X</kbd> ou <kbd>Delete</kbd> la retire |
| Grab step | Réordonne l'étape avec les flèches ; <kbd>Enter</kbd> confirme. | <kbd>G</kbd> |
| Insert Keyframe | Anime les valeurs de l'étape à la frame actuelle. | <kbd>I</kbd> |
| Repeat | Le nombre de fois que la liste s'exécute d'affilée (Routine loop). 1 = routine normale. | Champ Repeat |
| Render mode / Use | Une liste différente par mode d'ombrage, ou la même pour tous. | Listes déroulantes |

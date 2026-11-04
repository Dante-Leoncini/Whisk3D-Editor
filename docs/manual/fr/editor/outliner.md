# Outliner

La liste de tout ce qui se trouve dans la scène, et aussi la bibliothèque de ressources du projet. Sur Android, il n'est pas affiché (on travaille avec le 3D Viewport et Properties).

## Menu View (œil)

| Fonction | Description |
|---|---|
| Scene / Library / bibliothèques | Ce qui est listé : l'arbre de la scène, toutes les ressources du projet, ou celles d'un autre .w3d lié (lecture seule). |
| Constructor / Routines | Affichent ou masquent la routine Constructor (ce qui s'exécute une seule fois au démarrage) et les autres routines. |
| Visibility icons / Render icons | Affichent ou masquent les colonnes de l'œil et de la caméra. |
| Grid | Dans Library : des miniatures au lieu d'une liste. |
| Link .w3d… / Unlink | Utilise un autre projet comme bibliothèque, ou le détache. |

## Vue Scene

| Fonction | Description | Utilisation |
|---|---|---|
| Select | Choisit l'objet. <kbd>Shift</kbd>+clic prend une plage ; <kbd>Ctrl</kbd>+clic ajoute ou retire. | Clic |
| Déplier / replier | Affiche ou masque les enfants. | Flèche ; <kbd>Left</kbd> / <kbd>Right</kbd> |
| Visible / Renderable | L'œil masque l'objet (c'est animable) ; la caméra l'exclut du rendu. | Clic sur l'œil / la caméra |
| Réordonner et apparenter | Déposer avant, après ou sur une autre ligne (comme enfant). | Faire glisser la ligne |
| Move (reorder) | Au clavier : haut/bas réordonne, gauche sort du parent, droite place sous la ligne précédente, <kbd>Enter</kbd> confirme. | <kbd>G</kbd> |
| Rename | La ligne devient un champ de texte. | Double clic ou <kbd>F2</kbd> |
| Duplicate / Duplicate Linked | Comme dans le 3D Viewport. | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Hide / Show | Alterne la visibilité. | <kbd>H</kbd> |
| Delete | Avec confirmation. | <kbd>X</kbd> ou <kbd>Delete</kbd> |
| Create Prefab / Unpack Prefab | Depuis le menu Object. | Clic droit |
| Show active | Fait défiler la liste jusqu'à l'objet actif. | <kbd>Num.</kbd> |
| Faire défiler la liste | Molette, ou glisser (aussi au doigt). | Molette / glisser |

## Vue Library

| Fonction | Description |
|---|---|
| Ressources | Maillages, matériaux, textures, animations, prefabs, scènes, sons, scripts, [shaders](shaders.md), polices et vidéos, rangés dans des dossiers. Un matériau ou un shader glissé sur un maillage du 3D Viewport s'affiche sur la partie sous le pointeur avant d'être relâché. Coche verte = utilisée ; croix rouge = orpheline ; `*` = non enregistrée. |
| Filter / liste ou Grid | Afficher un seul type ; voir des miniatures. |
| Faire glisser une ressource | Vers un dossier (la déplace), vers le 3D Viewport (maillage = nouvel objet, matériau ou texture = assigné à l'élément touché, prefab = instance) ou vers une liste déroulante de Properties. |
| New Folder / Move to Folder | Organiser (les dossiers servent uniquement à ranger). |
| Duplicate Resource / Select Users / Purge Orphans | Copier, trouver qui l'utilise, supprimer ce que personne n'utilise. |
| Inside the .w3d / External | Depuis Properties (carte Resource) : déplace la ressource dans le projet ou hors de celui-ci. |

## Scènes, jeux et prefabs

Le bouton portant le nom de la scène (dans l'Outliner et dans le 3D Viewport) liste les scènes, les jeux et les prefabs du projet. C'est là que l'on en crée de nouveaux, que l'on choisit la scène de départ et que l'on convertit une scène en jeu (ou l'inverse).

# Éditeur UV

Montre comment la texture se pose sur le maillage. Pour modifier, le maillage doit être en **Mode Édition dans la Vue 3D** ; sinon, l'éditeur affiche seulement la texture. Le mode de l'Éditeur UV lui est propre : il ne change pas le mode de la Vue 3D. Les outils pour déplier et projeter les UV se trouvent dans la Vue 3D (Mode Édition, touche <kbd>U</kbd>).

## Navigation et barre supérieure

| Fonction | Description | Utilisation |
|---|---|---|
| Zoom / Déplacer la vue | Zoome vers le pointeur / déplace la vue. | Molette / bouton du milieu ou flèches ; tactile : 1 doigt fait défiler, 2 doigts zooment |
| Mode | Mode Objet, Mode Édition, Peinture de poids, Éditer les os et Mode Pose (ces deux derniers si le maillage a une armature 2D). | Bouton de mode, <kbd>Tab</kbd> |
| Sommet / Arête / Face | Ce que l'on sélectionne en Mode Édition. | Bouton |
| Pivot | Curseur, Origines individuelles ou Point médian. | Bouton |
| Curseur | Curseur sur la sélection, Sélection au curseur, Curseur au centre. | Menu |
| Vue | Synchroniser la sélection (la sélection UV est la même que celle du maillage), Répéter texture (mosaïque), Afficher Chrome UV, Cadrer la sélection, Tout cadrer. | Menu ; <kbd>Num.</kbd> |
| Texture | La texture affichée : Nouvelle texture…, Auto (partie active), ou n'importe quelle texture du projet. | Menu |
| Nouvelle texture | Crée une image (nom, taille, couleur, transparence ; Vierge, Grille UV ou Grille de couleurs), l'enregistre dans le projet et l'assigne au matériau. | Texture → Nouvelle texture… |
| Mipmap | Affiche un niveau de mipmap précis, pour le vérifier. | Bouton Mipmap |
| Éditer | Entre dans la peinture de texture sur l'image et en sort. | Bouton Éditer |
| Enregistrer (disquette, toujours visible) | Enregistrer, Enregistrer sous…, Enregistrer hors du .w3d…, Enregistrer dans le .w3d, Exporter disposition UV… | Menu |
| Exporter disposition UV | Enregistre un PNG à fond transparent : faces semi-transparentes (plus opaques là où elles se superposent) et lignes opaques. On choisit le nom, le dossier et la résolution (512 par défaut, 256 sur le N95, ou la taille de la texture affichée). | Enregistrer → Exporter disposition UV… |
| Animation | Insérer, supprimer ou effacer des keyframes de la couche UV, ou de la pose 2D. | Menu |

## Mode Objet (UV)

Un clic choisit ce que l'on modifie : une armature 2D (si l'on touche un os) ou la géométrie. <kbd>Tab</kbd> entre en édition de l'élément choisi.

## Mode Édition (UV)

| Fonction | Description | Raccourci |
|---|---|---|
| Sélectionner | Sommets, arêtes ou faces UV. Sans synchronisation, la Vue 3D filtre : seules les faces sélectionnées en 3D sont visibles. | Clic ; <kbd>Shift</kbd>+clic |
| Tout / Aucun / Inverser | Sur ce qui est visible. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Sélectionner lié | L'îlot UV sous le pointeur (<kbd>Shift</kbd> ajoute). | <kbd>L</kbd> |
| Sélection par boîte | Rectangle ; <kbd>Shift</kbd> ajoute. | <kbd>B</kbd> |
| Déplacer / Tourner / Échelle | Transforme les UV. <kbd>X</kbd> / <kbd>Y</kbd> verrouillent un axe ; on peut taper la valeur (1.0 = une tuile). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Placer le curseur 2D | Place le curseur à l'endroit cliqué. | Clic droit |
| Insérer image clé | Enregistre la couche UV dans l'animation de sommets active. | <kbd>I</kbd> |

## Peinture de poids (UV)

Peint les poids des os 2D sur la carte UV, avec les mêmes contrôles que dans la Vue 3D (mode +/−/=, groupe, repères, atténuation, sélection uniquement). <kbd>Ctrl</kbd>+clic sur un os le choisit pour la peinture.

## Éditer les os et Mode Pose (armature 2D)

| Fonction | Description | Raccourci |
|---|---|---|
| Ajouter armature 2D / os | Crée l'armature 2D du maillage (avec son groupe UV) ou ajoute un os. | Menu Ajouter |
| Sélectionner | Une extrémité ou l'os entier ; le faire glisser le déplace. | Clic ; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>L</kbd> / <kbd>B</kbd> |
| Déplacer / Tourner / Échelle | Dans Éditer les os, change la position de repos ; en Pose, pose l'os (les UV pondérées suivent l'os). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Extruder / Dupliquer / Supprimer | Uniquement dans Éditer les os. | <kbd>E</kbd> / <kbd>Shift+D</kbd> / <kbd>X</kbd> |
| Définir parent / Effacer parent | Garder décalage ou Connecté / déconnecter. | <kbd>Ctrl+P</kbd> / <kbd>Alt+P</kbd> |
| Insérer image clé | En Pose : menu des canaux, ou tout avec <kbd>Shift+I</kbd>. | <kbd>I</kbd> |

## Peinture de texture (UV)

Avec le bouton **Éditer**, on peint directement sur l'image, avec couleur, atténuation, rayon et force. On enregistre depuis la disquette.

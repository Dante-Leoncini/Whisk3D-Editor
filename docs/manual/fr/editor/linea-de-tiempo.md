# Timeline

Affiche et modifie l'animation : feuille de keyframes (dope sheet) ou éditeur de courbes. Elle montre ce qui est animé dans la **sélection**.

## Barre supérieure

| Fonction | Description |
|---|---|
| Transport | Aller au début, keyframe précédente, frame précédente, lecture à l'envers, lecture / pause, frame suivante, keyframe suivante, aller à la fin. |
| Début / Fin / FPS | Plage et vitesse de l'animation (cliquez pour saisir une valeur). |
| Animation | L'animation que l'on modifie (scène, clips, armatures, objets, flipbooks, Jeu, Mix) et la création de nouvelles. |
| Auto Key | En rouge : chaque transformation enregistre des keyframes automatiquement. |
| Courbes / Feuille d'exposition | Bascule entre la feuille de keyframes et l'éditeur de courbes. |
| Sélectionner, Clé, Vue, Pivot | Menus de sélection, opérations sur les keyframes, cadrage et panneau latéral, et point depuis lequel <kbd>S</kbd> met à l'échelle (centre ou frame actuelle). |

## Naviguer dans le temps

| Fonction | Description | Raccourci |
|---|---|---|
| Changer de frame | Clic ou glisser sur les numéros de frame (scrub). La frame actuelle (en vert) peut être saisie au clavier. | Clic |
| Image précédente / Image suivante | Une frame en arrière ou en avant. | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Image clé précédente / Image clé suivante | Saute à la keyframe la plus proche. | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Aller au début / Aller à la fin | Première ou dernière frame. | <kbd>Home</kbd> / <kbd>End</kbd> |
| Lecture / Pause | Depuis n'importe quel panneau. | <kbd>Space</kbd> |
| Zoom et défilement | Molette = zoom (dans les courbes, <kbd>Ctrl</kbd> = temps seulement, <kbd>Shift</kbd> = valeur seulement) ; bouton du milieu = défilement. | Molette / bouton du milieu |
| Cadrer la sélection | Ajuste la vue à la sélection. | <kbd>Num.</kbd> |

## Modifier les keyframes (dope sheet et courbes)

| Fonction | Description | Raccourci |
|---|---|---|
| Sélectionner | Clic sur un losange, ou sur une ligne pour prendre toutes ses keyframes ; <kbd>Shift</kbd> ajoute. | Clic ; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Déplacer les keyframes | Décale les keyframes dans le temps (aussi en faisant glisser un losange). | <kbd>G</kbd> |
| Mettre les keyframes à l'échelle | Étire ou comprime depuis le pivot. | <kbd>S</kbd> |
| Tourner les poignées | Uniquement dans les courbes. | <kbd>R</kbd> |
| Dupliquer | Copie les keyframes et les laisse saisies. | <kbd>Shift+D</kbd> |
| Supprimer | Supprime les keyframes (ou les courbes des lignes) sélectionnées. | <kbd>X</kbd> |
| Interpolation | Constante, Linéaire ou Bézier. | <kbd>T</kbd> |
| Type de poignée | Libre, Alignée, Vecteur, Automatique, Auto limitée (courbes uniquement). | <kbd>V</kbd> |
| Euler intelligent | Supprime les tours en trop dans les rotations sélectionnées. | Menu Clé |
| Masquer une courbe | L'œil de chaque ligne la masque. | Clic sur l'œil |
| Valeurs exactes | Frame, valeur et poignées de la keyframe choisie. | Propriétés → Animation → Image clé |

## Sons

| Fonction | Description | Raccourci |
|---|---|---|
| Bande du son | Chaque objet Son sélectionné a sa bande : où il commence et combien il dure, en **secondes** (changer les FPS ne la déplace pas). Avec une boucle, elle se répète jusqu'à la fin. | — |
| Déplacer | Glisser la bande, ou <kbd>G</kbd> sans keyframe choisie (déplace tous les sons sélectionnés). <kbd>Ctrl</kbd> aimante aux frames. | Glisser / <kbd>G</kbd> |
| Couper | Saisir une extrémité de la bande. | Glisser une extrémité |
| Forme d'onde / Ligne de volume | Menu Vue : la forme d'onde et la courbe du volume dans la bande (la bande prend trois lignes). Sans elles, c'est juste une bande. | Menu Vue |

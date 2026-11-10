# Déplacer, faire pivoter et mettre à l'échelle

Fonctionne de la même façon en Mode Objet, en Mode Édition, en Pose et dans l'édition des os.

| Fonction | Description | Raccourci |
|---|---|---|
| Déplacer (translater) | La sélection suit le pointeur. | <kbd>G</kbd> |
| Tourner | Tourne autour du pivot. Un nouvel appui sur <kbd>R</kbd> alterne entre rotation libre (trackball) et orbitale. | <kbd>R</kbd> |
| Échelle | Agrandit ou réduit depuis le pivot. | <kbd>S</kbd> |
| Contraindre à un axe | Pendant la transformation. Appuyer à nouveau fait passer de Global → Local → Vue → libre. | <kbd>X</kbd> / <kbd>Y</kbd> / <kbd>Z</kbd> |
| Contraindre à un plan | Exclut cet axe (déplace sur les deux autres). | <kbd>Shift+X</kbd> / <kbd>Shift+Y</kbd> / <kbd>Shift+Z</kbd> |
| Valeur exacte | Tapez un nombre ou un calcul (`2*3`, `(1+0.5)/2`). <kbd>-</kbd> inverse le signe, <kbd>Backspace</kbd> efface. | Chiffres |
| Confirmer | Valide l'opération. | Clic gauche, <kbd>Enter</kbd>, coche verte |
| Annuler | Tout revient comme avant. | <kbd>Esc</kbd>, clic droit, croix rouge |
| Pas fins | Déplace, fait pivoter ou met à l'échelle petit à petit. | Flèches |

Pendant l'opération, la barre supérieure affiche ce qui est en cours, par exemple « Déplacer X 1.5 selon X global ». En tactile, toucher cette barre ouvre un pavé numérique.

## Le gizmo

Le gizmo apparaît sur le pivot en Mode Objet et en Mode Édition (il s'active et se désactive dans Superpositions → Gizmo).

| Fonction | Description |
|---|---|
| Gizmo de déplacement | Flèches = un axe ; carrés = un plan ; cercle central = libre. |
| Gizmo de rotation | Un demi-cercle de couleur par axe, un anneau blanc pour tourner face à la vue, et l'intérieur pour tourner librement. |
| Gizmo de mise à l'échelle | Des pointes carrées par axe ; le cercle central met à l'échelle uniformément. |
| Changer de gizmo | Un clic de la souris ou un toucher du doigt sur le gizmo, sans glisser, passe de Déplacer à Tourner, puis à Échelle, et revient à Déplacer. Fonctionne de la même façon sur PC, dans le navigateur et sur les écrans tactiles. |

## Ajuster la dernière opération

Après un déplacement, une rotation ou une mise à l'échelle (en Mode Objet ou en Mode Édition), un panneau apparaît en bas à gauche avec ce qui a été fait : les valeurs X, Y, Z (ou l'angle), l'orientation et l'édition proportionnelle avec ses options. Modifier une valeur refait l'opération avec la nouvelle valeur. **Annuler** l'annule et **Accepter** la conserve. Un clic hors du panneau valide aussi. Tant que le panneau est ouvert, la barre d'outils de la Vue 3D est masquée.

Le panneau a une bordure verte, car c'est la fenêtre qui a le focus. S'il ne tient pas dans la moitié de la hauteur de l'écran, il est réduit à cette moitié et défile en faisant glisser le doigt (sans toucher les options), avec la molette ou avec les flèches. Au clavier ou sur le téléphone : les flèches parcourent les lignes, <kbd>OK</kbd> / <kbd>Enter</kbd> modifie la valeur sans fermer le panneau, **Accepter**, <kbd>Esc</kbd> ou <kbd>Delete</kbd> (la touche <kbd>C</kbd> sur le N95) le ferment en conservant la modification, et seul **Annuler** l'annule.

## Barre d'outils

Elle contient Annuler et Rétablir, les dernières actions utilisées et le bouton d'édition proportionnelle. Pendant une transformation, elle affiche la coche (valider), la croix (annuler), l'orientation et les axes X / Y / Z (deux axes activés = un plan). En tactile, après une extrusion, **Répéter** apparaît : il valide et extrude à nouveau.

# Raccourcis clavier

Toutes les fonctions des tableaux ci-dessous peuvent être réassignées, et chacune peut avoir **une ou plusieurs** combinaisons (ou aucune). Cela fonctionne sur PC, sur Android (avec un clavier physique) et sur le Web. Sur le N95, la fenêtre ne peut pas capturer les touches : il faut alors modifier le fichier `atajos.ini` à la main.

## Comment modifier un raccourci

1. Ouvrez **Propriétés → onglet Fichier/Rendu → carte Réglages → Raccourcis clavier…**
2. Choisissez le **Contexte** (Général, Vue 3D, Pendant une transformation, Éditeur UV, Timeline, Outliner, Éditeur 2D, Propriétés, Éditeur de scripts). Les flèches gauche/droite le changent aussi.
3. Choisissez la fonction dans la liste et touchez **Ajouter raccourci** (ou touchez à nouveau la ligne choisie). Le message « Appuyez sur une touche… » apparaît.
4. Appuyez sur la nouvelle combinaison (par exemple <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> annule sans rien changer.

| Bouton | Ce qu'il fait |
|---|---|
| Ajouter raccourci | Ajoute une combinaison à la fonction. Si une autre fonction du même contexte l'utilisait, elle lui est retirée et un avertissement s'affiche. |
| Effacer | Laisse la fonction sans raccourci. |
| Réinitialiser | Rend à la fonction son raccourci par défaut. |
| Tout réinitialiser | Toutes les fonctions reviennent à leurs raccourcis par défaut. |

Les menus affichent à côté de chaque option le raccourci **configuré** : si vous réassignez une fonction, le menu affiche la nouvelle combinaison. Sur le N95, les menus affichent la touche du téléphone (par exemple <kbd>1</kbd> pour Déplacer ou <kbd>C</kbd> pour Supprimer) ; si un clavier Bluetooth est connecté, ils affichent la touche de ce clavier lorsqu'ils la reconnaissent.

Les combinaisons modifiées s'affichent en vert. Quand on retire à une fonction son raccourci par défaut, cette touche devient libre : elle ne fait plus rien dans ce contexte.

Les raccourcis de chaque contexte s'appliquent quand le pointeur se trouve sur ce panneau ; ceux de **Général** s'appliquent partout. Ceux de **Pendant une transformation** s'appliquent pendant que l'on déplace, fait pivoter ou met à l'échelle quelque chose, et ils sont prioritaires. Dans l'éditeur de scripts, les lettres écrivent toujours : on ne peut y réassigner que des combinaisons avec <kbd>Ctrl</kbd>, <kbd>Alt</kbd> ou les touches F.

## Le fichier shortcuts.ini

Les modifications sont enregistrées automatiquement dans `res/shortcuts.ini`, à côté de `config.ini`. Seules les fonctions modifiées y figurent, et le fichier peut être modifié à la main :

```ini
# fonction = combinaison, autre combinaison   (vide = aucun raccourci)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Noms des touches : lettres et chiffres tels quels, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Modificateurs : `Ctrl+`, `Shift+`, `Alt+`.

## Raccourcis par défaut

<!-- Les tableaux ci-dessous, jusqu'à ATAJOS-FIN, sont générés par tools/genmanual_atajos.py à partir du code : ne pas les modifier à la main. -->

<!--ATAJOS-INICIO-->

### Général (n'importe quel panneau)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Enregistrer projet | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Annuler | <kbd>Ctrl+Z</kbd> | <kbd>C+Gauche</kbd> | `global.undo` |
| Rétablir | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Droite</kbd> | `global.redo` |
| Mode Objet / Édition | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Basculer aimantation | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menu aimantation (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Lecture / Pause | <kbd>Space</kbd> | <kbd>Vert+Soft gauche</kbd> | `global.play` |
| Barre de menus | <kbd>M</kbd> | <kbd>Soft gauche</kbd> | `global.menuBar` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Effacer parent | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Plein écran | <kbd>F11</kbd> | — | `global.screen` |
| Rendu image | <kbd>F12</kbd> | — | `global.render` |
| Rendu animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Vue 3D

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | <kbd>Crayon+Haut</kbd> | `3d.selectAll` |
| Tout désélectionner | <kbd>Alt+A</kbd> | <kbd>Crayon+Haut</kbd> | `3d.deselect` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Sélection par boîte | <kbd>B</kbd> | — | `3d.box` |
| Sélectionner lié | <kbd>L</kbd> | — | `3d.linked` |
| Ajouter | <kbd>Shift+A</kbd> | — | `3d.add` |
| Appliquer | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Tourner | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Effacer position (Pose) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Effacer rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Rétrécir/Gonfler / Effacer échelle | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Dupliquer lié | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Masquer / Afficher | <kbd>H</kbd> | — | `3d.hide` |
| Joindre | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Insérer image clé | <kbd>I</kbd> | — | `3d.keyframe` |
| Insérer image clé (tout) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Édition proportionnelle | <kbd>O</kbd> | — | `3d.proportional` |
| Superpositions | <kbd>K</kbd> | — | `3d.overlays` |
| Vue locale | <kbd>/</kbd> | — | `3d.viewLocal` |
| Cadrer la sélection | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Vue de face | <kbd>Num1</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.viewFront` |
| Vue arrière | <kbd>Ctrl+Num1</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.viewBack` |
| Vue de droite | <kbd>Num3</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.viewRight` |
| Vue de gauche | <kbd>Ctrl+Num3</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.viewLeft` |
| Vue de dessus | <kbd>Num7</kbd> | <kbd>#+Haut</kbd> | `3d.viewTop` |
| Vue de dessous | <kbd>Ctrl+Num7</kbd> | <kbd>#+Bas</kbd> | `3d.viewBottom` |
| Perspective / Orthographique | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Rouler à gauche | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Rouler à droite | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Vue caméra | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Définir caméra active | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Extruder | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| Nouvelle face depuis sommets | <kbd>F</kbd> | — | `3d.face` |
| Recalculer normales | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Coupe en boucle et glisser | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connecter chemin de sommets | <kbd>J</kbd> | — | `3d.connect` |
| Déchirer | <kbd>V</kbd> | — | `3d.rip` |
| Glisser sommets | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Séparer | <kbd>P</kbd> | — | `3d.separate` |
| Fusionner | <kbd>M</kbd> | — | `3d.merge` |
| Menu contextuel | <kbd>W</kbd> | — | `3d.contextMenu` |
| Trianguler | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| Menu UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Pendant une transformation (déplacer / faire pivoter / mettre à l'échelle)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Confirmer | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Annuler | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| Axe X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Axe Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Axe Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Exclure X (plan) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Exclure Y (plan) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Exclure Z (plan) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Rayon proportionnel + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Rayon proportionnel - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### Éditeur UV

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `uv.selectAll` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Sélectionner lié | <kbd>L</kbd> | — | `uv.linked` |
| Sélection par boîte | <kbd>B</kbd> | — | `uv.box` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Tourner | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Insérer image clé | <kbd>I</kbd> | — | `uv.keyframe` |
| Insérer image clé (tout) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Extruder | <kbd>E</kbd> | — | `uv.extrude` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Cadrer la sélection | <kbd>Num.</kbd> | — | `uv.focus` |

### Timeline

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `tl.selectAll` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Tourner les poignées | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Supprimer | <kbd>X</kbd> | — | `tl.delete` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Type de poignée | <kbd>V</kbd> | — | `tl.handle` |
| Cadrer la sélection | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Image précédente | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Image suivante | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Image clé précédente | <kbd>Down</kbd> | — | `tl.prevKey` |
| Image clé suivante | <kbd>Up</kbd> | — | `tl.nextKey` |
| Aller au début | <kbd>Home</kbd> | — | `tl.start` |
| Aller à la fin | <kbd>End</kbd> | — | `tl.end` |

### Outliner

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `out.selectAll` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Déplacer (réordonner) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Renommer | <kbd>F2</kbd> | — | `out.rename` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Dupliquer lié | <kbd>Alt+D</kbd> | — | `out.instance` |
| Masquer / Afficher | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Afficher l'actif | <kbd>Num.</kbd> | — | `out.focus` |

### Éditeur 2D

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `2d.selectAll` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Tourner | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Cadrer la sélection | <kbd>Num.</kbd> | — | `2d.focus` |
| Vue du jeu | <kbd>Num0</kbd> | — | `2d.gameView` |

### Propriétés (étapes d'une routine)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Saisir étape | <kbd>G</kbd> | — | `props.grab` |
| Supprimer étape | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Insérer image clé | <kbd>I</kbd> | — | `props.keyframe` |

### Éditeur de scripts

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans shortcuts.ini |
|---|---|---|---|
| Enregistrer script | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Annuler | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Copier | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Couper | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Coller | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Tout sélectionner | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Rétablir | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Rechercher | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Remplacer | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## Touches fixes (non réassignables)

Certaines touches ne figurent pas dans la liste, car elles dépendent de ce que l'on est en train de faire : les flèches pour naviguer dans les menus et les panneaux, <kbd>Enter</kbd> et <kbd>Esc</kbd> dans les menus et les fenêtres, les flèches de l'Outliner pendant le déplacement d'une ligne, le clavier de l'IDE pendant la saisie, et le clavier du téléphone sur le N95 (voir [Navigation](navegacion.md)).

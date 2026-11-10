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

## Le fichier atajos.ini

Les modifications sont enregistrées automatiquement dans `res/atajos.ini`, à côté de `config.ini`. Seules les fonctions modifiées y figurent, et le fichier peut être modifié à la main :

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

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Enregistrer projet | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Annuler | <kbd>Ctrl+Z</kbd> | <kbd>C+Gauche</kbd> | `global.deshacer` |
| Rétablir | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Droite</kbd> | `global.rehacer` |
| Mode Objet / Édition | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Basculer aimantation | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menu aimantation (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Lecture / Pause | <kbd>Space</kbd> | <kbd>Vert+Soft gauche</kbd> | `global.play` |
| Barre de menus | <kbd>M</kbd> | <kbd>Soft gauche</kbd> | `global.barraMenu` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Effacer parent | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Plein écran | <kbd>F11</kbd> | — | `global.pantalla` |
| Rendu image | <kbd>F12</kbd> | — | `global.render` |
| Rendu animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Vue 3D

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | <kbd>Crayon+Haut</kbd> | `3d.seleccionarTodo` |
| Tout désélectionner | <kbd>Alt+A</kbd> | <kbd>Crayon+Haut</kbd> | `3d.deseleccionar` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Sélection par boîte | <kbd>B</kbd> | — | `3d.caja` |
| Sélectionner lié | <kbd>L</kbd> | — | `3d.vinculado` |
| Ajouter | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Appliquer | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Tourner | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Effacer position (Pose) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Effacer rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Rétrécir/Gonfler / Effacer échelle | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Dupliquer lié | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Masquer / Afficher | <kbd>H</kbd> | — | `3d.ocultar` |
| Joindre | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Insérer image clé | <kbd>I</kbd> | — | `3d.keyframe` |
| Insérer image clé (tout) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Édition proportionnelle | <kbd>O</kbd> | — | `3d.proporcional` |
| Superpositions | <kbd>K</kbd> | — | `3d.overlays` |
| Vue locale | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Cadrer la sélection | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Vue de face | <kbd>Num1</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.vistaFrente` |
| Vue arrière | <kbd>Ctrl+Num1</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.vistaAtras` |
| Vue de droite | <kbd>Num3</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.vistaDerecha` |
| Vue de gauche | <kbd>Ctrl+Num3</kbd> | <kbd>#+Gauche/Droite</kbd> | `3d.vistaIzquierda` |
| Vue de dessus | <kbd>Num7</kbd> | <kbd>#+Haut</kbd> | `3d.vistaArriba` |
| Vue de dessous | <kbd>Ctrl+Num7</kbd> | <kbd>#+Bas</kbd> | `3d.vistaAbajo` |
| Perspective / Orthographique | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Rouler à gauche | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Rouler à droite | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Vue caméra | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Définir caméra active | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Extruder | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Nouvelle face depuis sommets | <kbd>F</kbd> | — | `3d.cara` |
| Recalculer normales | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Coupe en boucle et glisser | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connecter chemin de sommets | <kbd>J</kbd> | — | `3d.conectar` |
| Déchirer | <kbd>V</kbd> | — | `3d.rip` |
| Glisser sommets | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Séparer | <kbd>P</kbd> | — | `3d.separar` |
| Fusionner | <kbd>M</kbd> | — | `3d.merge` |
| Menu contextuel | <kbd>W</kbd> | — | `3d.menuContexto` |
| Trianguler | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| Menu UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Pendant une transformation (déplacer / faire pivoter / mettre à l'échelle)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Confirmer | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Annuler | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| Axe X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Axe Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Axe Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Exclure X (plan) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Exclure Y (plan) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Exclure Z (plan) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Rayon proportionnel + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Rayon proportionnel - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### Éditeur UV

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Sélectionner lié | <kbd>L</kbd> | — | `uv.vinculado` |
| Sélection par boîte | <kbd>B</kbd> | — | `uv.caja` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Tourner | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Insérer image clé | <kbd>I</kbd> | — | `uv.keyframe` |
| Insérer image clé (tout) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Extruder | <kbd>E</kbd> | — | `uv.extruir` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Cadrer la sélection | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Timeline

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Tourner les poignées | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Supprimer | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Type de poignée | <kbd>V</kbd> | — | `tl.handle` |
| Cadrer la sélection | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Image précédente | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Image suivante | <kbd>Right</kbd> | — | `tl.frameSig` |
| Image clé précédente | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Image clé suivante | <kbd>Up</kbd> | — | `tl.keySig` |
| Aller au début | <kbd>Home</kbd> | — | `tl.inicio` |
| Aller à la fin | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Déplacer (réordonner) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Renommer | <kbd>F2</kbd> | — | `out.renombrar` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Dupliquer lié | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Masquer / Afficher | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Afficher l'actif | <kbd>Num.</kbd> | — | `out.encuadrar` |

### Éditeur 2D

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Tout sélectionner | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Tout désélectionner | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Inverser la sélection | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Déplacer | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Tourner | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Échelle | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Dupliquer | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Supprimer | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Cadrer la sélection | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Vue du jeu | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Propriétés (étapes d'une routine)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Saisir étape | <kbd>G</kbd> | — | `props.agarrar` |
| Supprimer étape | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insérer image clé | <kbd>I</kbd> | — | `props.keyframe` |

### Éditeur de scripts

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Enregistrer script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Annuler | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copier | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Couper | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Coller | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Tout sélectionner | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Rétablir | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Rechercher | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Remplacer | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Touches fixes (non réassignables)

Certaines touches ne figurent pas dans la liste, car elles dépendent de ce que l'on est en train de faire : les flèches pour naviguer dans les menus et les panneaux, <kbd>Enter</kbd> et <kbd>Esc</kbd> dans les menus et les fenêtres, les flèches de l'Outliner pendant le déplacement d'une ligne, le clavier de l'IDE pendant la saisie, et le clavier du téléphone sur le N95 (voir [Navigation](navegacion.md)).

# Raccourcis clavier

Toutes les fonctions des tableaux ci-dessous peuvent être réassignées, et chacune peut avoir **une ou plusieurs** combinaisons (ou aucune). Cela fonctionne sur PC, sur Android (avec un clavier physique) et sur le Web. Sur le N95, la fenêtre ne peut pas capturer les touches : il faut alors modifier le fichier `atajos.ini` à la main.

## Comment modifier un raccourci

1. Ouvrez **Properties → onglet File/Render → carte Settings → Keyboard Shortcuts…**
2. Choisissez le **Context** (General, 3D Viewport, During a transform, UV Editor, Timeline, Outliner, 2D Editor, Properties, Script Editor). Les flèches gauche/droite le changent aussi.
3. Choisissez la fonction dans la liste et touchez **Add shortcut** (ou touchez à nouveau la ligne choisie). Le message « Press a key… » apparaît.
4. Appuyez sur la nouvelle combinaison (par exemple <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> annule sans rien changer.

| Bouton | Ce qu'il fait |
|---|---|
| Add shortcut | Ajoute une combinaison à la fonction. Si une autre fonction du même contexte l'utilisait, elle lui est retirée et un avertissement s'affiche. |
| Clear | Laisse la fonction sans raccourci. |
| Reset | Rend à la fonction son raccourci par défaut. |
| Reset all | Toutes les fonctions reviennent à leurs raccourcis par défaut. |

Les menus affichent à côté de chaque option le raccourci **configuré** : si vous réassignez une fonction, le menu affiche la nouvelle combinaison. Sur le N95, les menus affichent la touche du téléphone (par exemple <kbd>1</kbd> pour Move ou <kbd>C</kbd> pour Delete) ; si un clavier Bluetooth est connecté, ils affichent la touche de ce clavier lorsqu'ils la reconnaissent.

Les combinaisons modifiées s'affichent en vert. Quand on retire à une fonction son raccourci par défaut, cette touche devient libre : elle ne fait plus rien dans ce contexte.

Les raccourcis de chaque contexte s'appliquent quand le pointeur se trouve sur ce panneau ; ceux de **General** s'appliquent partout. Ceux de **During a transform** s'appliquent pendant que l'on déplace, fait pivoter ou met à l'échelle quelque chose, et ils sont prioritaires. Dans le Script Editor, les lettres écrivent toujours : on ne peut y réassigner que des combinaisons avec <kbd>Ctrl</kbd>, <kbd>Alt</kbd> ou les touches F.

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

### General (n'importe quel panneau)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Save project | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Undo | <kbd>Ctrl+Z</kbd> | <kbd>C+Left</kbd> | `global.deshacer` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Right</kbd> | `global.rehacer` |
| Object / Edit Mode | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Toggle Snap | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Snap menu | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Play / Pause | <kbd>Space</kbd> | <kbd>Green+Left soft</kbd> | `global.play` |
| Menu bar | <kbd>M</kbd> | <kbd>Left soft</kbd> | `global.barraMenu` |
| Parent | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Clear Parent | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Fullscreen | <kbd>F11</kbd> | — | `global.pantalla` |
| Render Image | <kbd>F12</kbd> | — | `global.render` |
| Render Animation | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### 3D Viewport

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | <kbd>Pencil+Up</kbd> | `3d.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | <kbd>Pencil+Up</kbd> | `3d.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Box Select | <kbd>B</kbd> | — | `3d.caja` |
| Select Linked | <kbd>L</kbd> | — | `3d.vinculado` |
| Add | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Apply | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Clear Location (Pose) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Clear Rotation (Pose) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Shrink/Fatten / Clear Scale | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Duplicate | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Hide / Show | <kbd>H</kbd> | — | `3d.ocultar` |
| Join | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Insert Keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Proportional Editing | <kbd>O</kbd> | — | `3d.proporcional` |
| Overlays | <kbd>K</kbd> | — | `3d.overlays` |
| Local View | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Front View | <kbd>Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaFrente` |
| Back View | <kbd>Ctrl+Num1</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaAtras` |
| Right View | <kbd>Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaDerecha` |
| Left View | <kbd>Ctrl+Num3</kbd> | <kbd>#+Left/Right</kbd> | `3d.vistaIzquierda` |
| Top View | <kbd>Num7</kbd> | <kbd>#+Up</kbd> | `3d.vistaArriba` |
| Bottom View | <kbd>Ctrl+Num7</kbd> | <kbd>#+Down</kbd> | `3d.vistaAbajo` |
| Perspective / Orthographic | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Roll Left | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Roll Right | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Camera View | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Set Active Camera | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Extrude | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| New Face from Vertices | <kbd>F</kbd> | — | `3d.cara` |
| Recalculate Normals | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Loop Cut and Slide | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connect Vertex Path | <kbd>J</kbd> | — | `3d.conectar` |
| Rip | <kbd>V</kbd> | — | `3d.rip` |
| Vertex Slide | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separate | <kbd>P</kbd> | — | `3d.separar` |
| Merge | <kbd>M</kbd> | — | `3d.merge` |
| Context menu | <kbd>W</kbd> | — | `3d.menuContexto` |
| Triangulate | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| UV menu | <kbd>U</kbd> | — | `3d.menuUV` |

### During a transform (Move / Rotate / Scale)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Confirm | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Cancel | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| X axis | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Y axis | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Z axis | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Exclude X (plane) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Exclude Y (plane) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Exclude Z (plane) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Proportional radius + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Proportional radius - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### UV Editor

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Select Linked | <kbd>L</kbd> | — | `uv.vinculado` |
| Box Select | <kbd>B</kbd> | — | `uv.caja` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Insert Keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Insert Keyframe (all) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Extrude | <kbd>E</kbd> | — | `uv.extruir` |
| Duplicate | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Frame Selected | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Timeline

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Rotate handles | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Delete | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolation | <kbd>T</kbd> | — | `tl.interp` |
| Handle type | <kbd>V</kbd> | — | `tl.handle` |
| Frame Selected | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Previous frame | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Next frame | <kbd>Right</kbd> | — | `tl.frameSig` |
| Previous keyframe | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Next keyframe | <kbd>Up</kbd> | — | `tl.keySig` |
| Go to start | <kbd>Home</kbd> | — | `tl.inicio` |
| Go to end | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Move (reorder) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Rename | <kbd>F2</kbd> | — | `out.renombrar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Duplicate Linked | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Hide / Show | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Show active | <kbd>Num.</kbd> | — | `out.encuadrar` |

### 2D Editor

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Select All | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Deselect All | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Invert Selection | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Move | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Rotate | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Scale | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Duplicate | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Delete | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Frame Selected | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Game View | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Properties (étapes d'une routine)

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.agarrar` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| Fonction | Raccourci par défaut | Nokia N95 (clavier du téléphone) | Nom dans atajos.ini |
|---|---|---|---|
| Save script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Undo | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copy | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Cut | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Paste | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Select All | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Redo | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Find | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Replace | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Touches fixes (non réassignables)

Certaines touches ne figurent pas dans la liste, car elles dépendent de ce que l'on est en train de faire : les flèches pour naviguer dans les menus et les panneaux, <kbd>Enter</kbd> et <kbd>Esc</kbd> dans les menus et les fenêtres, les flèches de l'Outliner pendant le déplacement d'une ligne, le clavier de l'IDE pendant la saisie, et le clavier du téléphone sur le N95 (voir [Navigation](navegacion.md)).

# Scorciatoie da tastiera

Tutte le funzioni delle tabelle qui sotto si possono riassegnare, e ognuna può avere **una o più** combinazioni (o nessuna). Funziona su PC, Android (con tastiera fisica) e Web. Sull'N95 la finestra non può catturare i tasti: lì il file `atajos.ini` si modifica a mano.

## Come cambiare una scorciatoia

1. Aprire **Properties → scheda File/Render → riquadro Settings → Keyboard Shortcuts…**
2. Scegliere il **Context** (General, 3D Viewport, During a transform, UV Editor, Timeline, Outliner, 2D Editor, Properties, Script Editor). Anche le frecce sinistra/destra lo cambiano.
3. Scegliere la funzione nell'elenco e toccare **Add shortcut** (o toccare di nuovo la riga scelta). Appare "Press a key…".
4. Premere la nuova combinazione (per esempio <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> annulla senza cambiare nulla.

| Pulsante | Cosa fa |
|---|---|
| Add shortcut | Aggiunge una combinazione alla funzione. Se un'altra funzione dello stesso contesto la aveva, gliela toglie e avvisa. |
| Clear | Lascia la funzione senza scorciatoia. |
| Reset | Riporta la funzione alla sua scorciatoia predefinita. |
| Reset all | Tutte le funzioni tornano a quelle predefinite. |

I menu mostrano accanto a ogni opzione la scorciatoia **configurata**: se riassegni una funzione, il menu mostra la nuova combinazione. Sull'N95 i menu mostrano il tasto del telefono (per esempio <kbd>1</kbd> per Move o <kbd>C</kbd> per Delete); se è collegata una tastiera Bluetooth, mostrano il tasto di quella tastiera quando lo riconoscono.

Le combinazioni modificate appaiono in verde. Quando a una funzione si toglie una scorciatoia predefinita, quel tasto resta libero: non fa più nulla in quel contesto.

Le scorciatoie di ogni contesto valgono quando il puntatore si trova su quel pannello; quelle di **General** valgono ovunque. Quelle di **During a transform** valgono mentre si sposta, ruota o scala qualcosa, e hanno la precedenza. Nell'editor di script le lettere scrivono sempre: lì si possono riassegnare solo combinazioni con <kbd>Ctrl</kbd>, <kbd>Alt</kbd> o i tasti F.

## Il file atajos.ini

Le modifiche si salvano da sole in `res/atajos.ini`, accanto a `config.ini`. Compaiono solo le funzioni modificate, e si può modificare a mano:

```ini
# funzione = combinazione, altra combinazione   (vuoto = nessuna scorciatoia)
3d.mover = G, Ctrl+M
3d.ocultar = Ctrl+Q
xf.cancelar = Esc, Q
3d.overlays =
```

Nomi dei tasti: lettere e numeri così come sono, `Enter`, `Esc`, `Tab`, `Space`, `Backspace`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Left`, `Right`, `Up`, `Down`, `F1`…`F12`, `Num0`…`Num9`, `Num.`, `NumEnter`, `Comma`, `Plus`. Modificatori: `Ctrl+`, `Shift+`, `Alt+`.

## Scorciatoie predefinite

<!-- Le tabelle qui sotto, fino a ATAJOS-FIN, sono generate da tools/genmanual_atajos.py a partire dal codice: non modificarle a mano. -->

<!--ATAJOS-INICIO-->

### General (qualsiasi pannello)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

### During a transform (spostare / ruotare / scalare)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

### Properties (passi di una routine)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Grab step | <kbd>G</kbd> | — | `props.agarrar` |
| Delete step | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Insert Keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Script Editor

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
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

## Tasti fissi (non riassegnabili)

Alcuni tasti non sono nell'elenco perché dipendono da ciò che si sta facendo: le frecce per navigare nei menu e nei pannelli, <kbd>Enter</kbd> e <kbd>Esc</kbd> nei menu e nelle finestre, le frecce dell'outliner mentre si sposta una riga, la tastiera dell'IDE quando si scrive, e la tastiera del telefono sull'N95 (vedi [Navigazione](navegacion.md)).

# Scorciatoie da tastiera

Tutte le funzioni delle tabelle qui sotto si possono riassegnare, e ognuna può avere **una o più** combinazioni (o nessuna). Funziona su PC, Android (con tastiera fisica) e Web. Sull'N95 la finestra non può catturare i tasti: lì il file `atajos.ini` si modifica a mano.

## Come cambiare una scorciatoia

1. Aprire **Proprietà → scheda File/Render → riquadro Impostazioni → Scorciatoie da tastiera…**
2. Scegliere il **Contesto** (Generale, Vista 3D, Durante una trasformazione, Editor UV, Timeline, Outliner, Editor 2D, Proprietà, Editor di script). Anche le frecce sinistra/destra lo cambiano.
3. Scegliere la funzione nell'elenco e toccare **Aggiungi scorciatoia** (o toccare di nuovo la riga scelta). Appare "Premi un tasto…".
4. Premere la nuova combinazione (per esempio <kbd>Ctrl+M</kbd>). <kbd>Esc</kbd> annulla senza cambiare nulla.

| Pulsante | Cosa fa |
|---|---|
| Aggiungi scorciatoia | Aggiunge una combinazione alla funzione. Se un'altra funzione dello stesso contesto la aveva, gliela toglie e avvisa. |
| Pulisci | Lascia la funzione senza scorciatoia. |
| Ripristina | Riporta la funzione alla sua scorciatoia predefinita. |
| Ripristina tutto | Tutte le funzioni tornano a quelle predefinite. |

I menu mostrano accanto a ogni opzione la scorciatoia **configurata**: se riassegni una funzione, il menu mostra la nuova combinazione. Sull'N95 i menu mostrano il tasto del telefono (per esempio <kbd>1</kbd> per Sposta o <kbd>C</kbd> per Elimina); se è collegata una tastiera Bluetooth, mostrano il tasto di quella tastiera quando lo riconoscono.

Le combinazioni modificate appaiono in verde. Quando a una funzione si toglie una scorciatoia predefinita, quel tasto resta libero: non fa più nulla in quel contesto.

Le scorciatoie di ogni contesto valgono quando il puntatore si trova su quel pannello; quelle di **Generale** valgono ovunque. Quelle di **Durante una trasformazione** valgono mentre si sposta, ruota o scala qualcosa, e hanno la precedenza. Nell'editor di script le lettere scrivono sempre: lì si possono riassegnare solo combinazioni con <kbd>Ctrl</kbd>, <kbd>Alt</kbd> o i tasti F.

## Il file shortcuts.ini

Le modifiche si salvano da sole in `res/shortcuts.ini`, accanto a `config.ini`. Compaiono solo le funzioni modificate, e si può modificare a mano:

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

### Generale (qualsiasi pannello)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Salva progetto | <kbd>Ctrl+S</kbd> | — | `global.save` |
| Annulla | <kbd>Ctrl+Z</kbd> | <kbd>C+Sx</kbd> | `global.undo` |
| Rifai | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Dx</kbd> | `global.redo` |
| Modalità Oggetto / Modifica | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.mode` |
| Attiva/disattiva aggancio | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menu aggancio (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Riproduci / Pausa | <kbd>Space</kbd> | <kbd>Verde+Soft sx</kbd> | `global.play` |
| Barra dei menu | <kbd>M</kbd> | <kbd>Soft sx</kbd> | `global.menuBar` |
| Genitore | <kbd>Ctrl+P</kbd> | — | `global.parent` |
| Rimuovi genitore | <kbd>Alt+P</kbd> | — | `global.clearParent` |
| Schermo intero | <kbd>F11</kbd> | — | `global.screen` |
| Renderizza immagine | <kbd>F12</kbd> | — | `global.render` |
| Renderizza animazione | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Vista 3D

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | <kbd>Matita+Su</kbd> | `3d.selectAll` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | <kbd>Matita+Su</kbd> | `3d.deselect` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `3d.invert` |
| Selezione a riquadro | <kbd>B</kbd> | — | `3d.box` |
| Seleziona collegati | <kbd>L</kbd> | — | `3d.linked` |
| Aggiungi | <kbd>Shift+A</kbd> | — | `3d.add` |
| Applica | <kbd>Ctrl+A</kbd> | — | `3d.apply` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `3d.move` |
| Ruota | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotate` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `3d.scale` |
| Azzera posizione (Posa) | <kbd>Alt+G</kbd> | — | `3d.clearPos` |
| Azzera rotazione (Posa) | <kbd>Alt+R</kbd> | — | `3d.clearRot` |
| Ingrassa / Azzera scala | <kbd>Alt+S</kbd> | — | `3d.clearScale` |
| Duplica | <kbd>Shift+D</kbd> | — | `3d.duplicate` |
| Duplica collegato | <kbd>Alt+D</kbd> | — | `3d.instance` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.delete` |
| Nascondi / Mostra | <kbd>H</kbd> | — | `3d.hide` |
| Unisci | <kbd>Ctrl+J</kbd> | — | `3d.join` |
| Inserisci keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Inserisci keyframe (tutto) | <kbd>Shift+I</kbd> | — | `3d.keyframeAll` |
| Modifica proporzionale | <kbd>O</kbd> | — | `3d.proportional` |
| Sovrapposizioni | <kbd>K</kbd> | — | `3d.overlays` |
| Vista locale | <kbd>/</kbd> | — | `3d.viewLocal` |
| Inquadra selezionati | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.focus` |
| Vista frontale | <kbd>Num1</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.viewFront` |
| Vista posteriore | <kbd>Ctrl+Num1</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.viewBack` |
| Vista destra | <kbd>Num3</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.viewRight` |
| Vista sinistra | <kbd>Ctrl+Num3</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.viewLeft` |
| Vista superiore | <kbd>Num7</kbd> | <kbd>#+Su</kbd> | `3d.viewTop` |
| Vista inferiore | <kbd>Ctrl+Num7</kbd> | <kbd>#+Giù</kbd> | `3d.viewBottom` |
| Prospettiva / Ortografica | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspective` |
| Ruota vista a sinistra | <kbd>Num4</kbd> | — | `3d.rollLeft` |
| Ruota vista a destra | <kbd>Num6</kbd> | — | `3d.rollRight` |
| Vista camera | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.viewCamera` |
| Imposta camera attiva | <kbd>Ctrl+Num0</kbd> | — | `3d.activeCamera` |
| Estrudi | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extrude` |
| Nuova faccia da vertici | <kbd>F</kbd> | — | `3d.face` |
| Ricalcola normali | <kbd>Shift+N</kbd> | — | `3d.normals` |
| Taglio ad anello e scorri | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connetti percorso di vertici | <kbd>J</kbd> | — | `3d.connect` |
| Strappa | <kbd>V</kbd> | — | `3d.rip` |
| Scorri vertici | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separa | <kbd>P</kbd> | — | `3d.separate` |
| Fondi | <kbd>M</kbd> | — | `3d.merge` |
| Menu contestuale | <kbd>W</kbd> | — | `3d.contextMenu` |
| Triangola | <kbd>Ctrl+T</kbd> | — | `3d.triangulate` |
| Menu UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Durante una trasformazione (spostare / ruotare / scalare)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Conferma | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirm` |
| Annulla | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancel` |
| Asse X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.axisX` |
| Asse Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.axisY` |
| Asse Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.axisZ` |
| Escludi X (piano) | <kbd>Shift+X</kbd> | — | `xf.planeX` |
| Escludi Y (piano) | <kbd>Shift+Y</kbd> | — | `xf.planeY` |
| Escludi Z (piano) | <kbd>Shift+Z</kbd> | — | `xf.planeZ` |
| Raggio proporzionale + | <kbd>PageUp</kbd> | — | `xf.radiusPlus` |
| Raggio proporzionale - | <kbd>PageDown</kbd> | — | `xf.radiusMinus` |

### Editor UV

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `uv.selectAll` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `uv.deselect` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `uv.invert` |
| Seleziona collegati | <kbd>L</kbd> | — | `uv.linked` |
| Selezione a riquadro | <kbd>B</kbd> | — | `uv.box` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `uv.move` |
| Ruota | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotate` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `uv.scale` |
| Inserisci keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Inserisci keyframe (tutto) | <kbd>Shift+I</kbd> | — | `uv.keyframeAll` |
| Estrudi | <kbd>E</kbd> | — | `uv.extrude` |
| Duplica | <kbd>Shift+D</kbd> | — | `uv.duplicate` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.delete` |
| Inquadra selezionati | <kbd>Num.</kbd> | — | `uv.focus` |

### Timeline

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `tl.selectAll` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `tl.deselect` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `tl.invert` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `tl.move` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `tl.scale` |
| Ruota maniglie | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotate` |
| Duplica | <kbd>Shift+D</kbd> | — | `tl.duplicate` |
| Elimina | <kbd>X</kbd> | — | `tl.delete` |
| Interpolazione | <kbd>T</kbd> | — | `tl.interp` |
| Tipo maniglia | <kbd>V</kbd> | — | `tl.handle` |
| Inquadra selezionati | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.focus` |
| Frame precedente | <kbd>Left</kbd> | — | `tl.prevFrame` |
| Frame successivo | <kbd>Right</kbd> | — | `tl.nextFrame` |
| Keyframe precedente | <kbd>Down</kbd> | — | `tl.prevKey` |
| Keyframe successivo | <kbd>Up</kbd> | — | `tl.nextKey` |
| Vai all'inizio | <kbd>Home</kbd> | — | `tl.start` |
| Vai alla fine | <kbd>End</kbd> | — | `tl.end` |

### Outliner

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `out.selectAll` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `out.deselect` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `out.invert` |
| Sposta (riordina) | <kbd>G</kbd> | <kbd>1</kbd> | `out.move` |
| Rinomina | <kbd>F2</kbd> | — | `out.rename` |
| Duplica | <kbd>Shift+D</kbd> | — | `out.duplicate` |
| Duplica collegato | <kbd>Alt+D</kbd> | — | `out.instance` |
| Nascondi / Mostra | <kbd>H</kbd> | <kbd>2</kbd> | `out.hide` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.delete` |
| Mostra l'attivo | <kbd>Num.</kbd> | — | `out.focus` |

### Editor 2D

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `2d.selectAll` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `2d.deselect` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `2d.invert` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `2d.move` |
| Ruota | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotate` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `2d.scale` |
| Duplica | <kbd>Shift+D</kbd> | — | `2d.duplicate` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.delete` |
| Inquadra selezionati | <kbd>Num.</kbd> | — | `2d.focus` |
| Vista gioco | <kbd>Num0</kbd> | — | `2d.gameView` |

### Proprietà (passi di una routine)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Afferra passo | <kbd>G</kbd> | — | `props.grab` |
| Elimina passo | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.delete` |
| Inserisci keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Editor di script

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in shortcuts.ini |
|---|---|---|---|
| Salva script | <kbd>Ctrl+S</kbd> | — | `ide.save` |
| Annulla | <kbd>Ctrl+Z</kbd> | — | `ide.undo` |
| Copia | <kbd>Ctrl+C</kbd> | — | `ide.copy` |
| Taglia | <kbd>Ctrl+X</kbd> | — | `ide.cut` |
| Incolla | <kbd>Ctrl+V</kbd> | — | `ide.paste` |
| Seleziona tutto | <kbd>Ctrl+A</kbd> | — | `ide.all` |
| Rifai | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.redo` |
| Trova | <kbd>Ctrl+F</kbd> | — | `ide.find` |
| Sostituisci | <kbd>Ctrl+H</kbd> | — | `ide.replace` |

<!--ATAJOS-FIN-->

## Tasti fissi (non riassegnabili)

Alcuni tasti non sono nell'elenco perché dipendono da ciò che si sta facendo: le frecce per navigare nei menu e nei pannelli, <kbd>Enter</kbd> e <kbd>Esc</kbd> nei menu e nelle finestre, le frecce dell'outliner mentre si sposta una riga, la tastiera dell'IDE quando si scrive, e la tastiera del telefono sull'N95 (vedi [Navigazione](navegacion.md)).

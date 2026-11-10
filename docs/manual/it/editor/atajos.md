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

### Generale (qualsiasi pannello)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Salva progetto | <kbd>Ctrl+S</kbd> | — | `global.guardar` |
| Annulla | <kbd>Ctrl+Z</kbd> | <kbd>C+Sx</kbd> | `global.deshacer` |
| Rifai | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | <kbd>C+Dx</kbd> | `global.rehacer` |
| Modalità Oggetto / Modifica | <kbd>Tab</kbd> | <kbd>OK</kbd> | `global.modo` |
| Attiva/disattiva aggancio | <kbd>Shift+Tab</kbd> | — | `global.snap` |
| Menu aggancio (Snap) | <kbd>Shift+S</kbd> | — | `global.menuSnap` |
| Riproduci / Pausa | <kbd>Space</kbd> | <kbd>Verde+Soft sx</kbd> | `global.play` |
| Barra dei menu | <kbd>M</kbd> | <kbd>Soft sx</kbd> | `global.barraMenu` |
| Genitore | <kbd>Ctrl+P</kbd> | — | `global.emparentar` |
| Rimuovi genitore | <kbd>Alt+P</kbd> | — | `global.desemparentar` |
| Schermo intero | <kbd>F11</kbd> | — | `global.pantalla` |
| Renderizza immagine | <kbd>F12</kbd> | — | `global.render` |
| Renderizza animazione | <kbd>Ctrl+F12</kbd> | — | `global.renderAnim` |

### Vista 3D

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | <kbd>Matita+Su</kbd> | `3d.seleccionarTodo` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | <kbd>Matita+Su</kbd> | `3d.deseleccionar` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `3d.invertir` |
| Selezione a riquadro | <kbd>B</kbd> | — | `3d.caja` |
| Seleziona collegati | <kbd>L</kbd> | — | `3d.vinculado` |
| Aggiungi | <kbd>Shift+A</kbd> | — | `3d.agregar` |
| Applica | <kbd>Ctrl+A</kbd> | — | `3d.aplicar` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `3d.mover` |
| Ruota | <kbd>R</kbd> | <kbd>2</kbd> | `3d.rotar` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `3d.escalar` |
| Azzera posizione (Posa) | <kbd>Alt+G</kbd> | — | `3d.limpiarPos` |
| Azzera rotazione (Posa) | <kbd>Alt+R</kbd> | — | `3d.limpiarRot` |
| Ingrassa / Azzera scala | <kbd>Alt+S</kbd> | — | `3d.limpiarEsc` |
| Duplica | <kbd>Shift+D</kbd> | — | `3d.duplicar` |
| Duplica collegato | <kbd>Alt+D</kbd> | — | `3d.instancia` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `3d.borrar` |
| Nascondi / Mostra | <kbd>H</kbd> | — | `3d.ocultar` |
| Unisci | <kbd>Ctrl+J</kbd> | — | `3d.unir` |
| Inserisci keyframe | <kbd>I</kbd> | — | `3d.keyframe` |
| Inserisci keyframe (tutto) | <kbd>Shift+I</kbd> | — | `3d.keyframeTodo` |
| Modifica proporzionale | <kbd>O</kbd> | — | `3d.proporcional` |
| Sovrapposizioni | <kbd>K</kbd> | — | `3d.overlays` |
| Vista locale | <kbd>/</kbd> | — | `3d.vistaLocal` |
| Inquadra selezionati | <kbd>Num.</kbd> | <kbd>0</kbd> | `3d.encuadrar` |
| Vista frontale | <kbd>Num1</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.vistaFrente` |
| Vista posteriore | <kbd>Ctrl+Num1</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.vistaAtras` |
| Vista destra | <kbd>Num3</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.vistaDerecha` |
| Vista sinistra | <kbd>Ctrl+Num3</kbd> | <kbd>#+Sx/Dx</kbd> | `3d.vistaIzquierda` |
| Vista superiore | <kbd>Num7</kbd> | <kbd>#+Su</kbd> | `3d.vistaArriba` |
| Vista inferiore | <kbd>Ctrl+Num7</kbd> | <kbd>#+Giù</kbd> | `3d.vistaAbajo` |
| Prospettiva / Ortografica | <kbd>Num5</kbd> | <kbd>#</kbd> | `3d.perspectiva` |
| Ruota vista a sinistra | <kbd>Num4</kbd> | — | `3d.rodarIzq` |
| Ruota vista a destra | <kbd>Num6</kbd> | — | `3d.rodarDer` |
| Vista camera | <kbd>Num0</kbd> | <kbd>8</kbd> | `3d.vistaCamara` |
| Imposta camera attiva | <kbd>Ctrl+Num0</kbd> | — | `3d.camaraActiva` |
| Estrudi | <kbd>E</kbd> | <kbd>7</kbd> | `3d.extruir` |
| Nuova faccia da vertici | <kbd>F</kbd> | — | `3d.cara` |
| Ricalcola normali | <kbd>Shift+N</kbd> | — | `3d.normales` |
| Taglio ad anello e scorri | <kbd>Ctrl+R</kbd> | <kbd>8</kbd> | `3d.loopcut` |
| Connetti percorso di vertici | <kbd>J</kbd> | — | `3d.conectar` |
| Strappa | <kbd>V</kbd> | — | `3d.rip` |
| Scorri vertici | <kbd>Shift+V</kbd> | — | `3d.slide` |
| Separa | <kbd>P</kbd> | — | `3d.separar` |
| Fondi | <kbd>M</kbd> | — | `3d.merge` |
| Menu contestuale | <kbd>W</kbd> | — | `3d.menuContexto` |
| Triangola | <kbd>Ctrl+T</kbd> | — | `3d.triangular` |
| Menu UV | <kbd>U</kbd> | — | `3d.menuUV` |

### Durante una trasformazione (spostare / ruotare / scalare)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Conferma | <kbd>Enter</kbd>, <kbd>NumEnter</kbd> | <kbd>OK</kbd> | `xf.confirmar` |
| Annulla | <kbd>Esc</kbd> | <kbd>C</kbd> | `xf.cancelar` |
| Asse X | <kbd>X</kbd> | <kbd>1</kbd> | `xf.ejeX` |
| Asse Y | <kbd>Y</kbd> | <kbd>2</kbd> | `xf.ejeY` |
| Asse Z | <kbd>Z</kbd> | <kbd>3</kbd> | `xf.ejeZ` |
| Escludi X (piano) | <kbd>Shift+X</kbd> | — | `xf.planoX` |
| Escludi Y (piano) | <kbd>Shift+Y</kbd> | — | `xf.planoY` |
| Escludi Z (piano) | <kbd>Shift+Z</kbd> | — | `xf.planoZ` |
| Raggio proporzionale + | <kbd>PageUp</kbd> | — | `xf.radioMas` |
| Raggio proporzionale - | <kbd>PageDown</kbd> | — | `xf.radioMenos` |

### Editor UV

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `uv.seleccionarTodo` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `uv.deseleccionar` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `uv.invertir` |
| Seleziona collegati | <kbd>L</kbd> | — | `uv.vinculado` |
| Selezione a riquadro | <kbd>B</kbd> | — | `uv.caja` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `uv.mover` |
| Ruota | <kbd>R</kbd> | <kbd>2</kbd> | `uv.rotar` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `uv.escalar` |
| Inserisci keyframe | <kbd>I</kbd> | — | `uv.keyframe` |
| Inserisci keyframe (tutto) | <kbd>Shift+I</kbd> | — | `uv.keyframeTodo` |
| Estrudi | <kbd>E</kbd> | — | `uv.extruir` |
| Duplica | <kbd>Shift+D</kbd> | — | `uv.duplicar` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `uv.borrar` |
| Inquadra selezionati | <kbd>Num.</kbd> | — | `uv.encuadrar` |

### Timeline

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `tl.seleccionarTodo` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `tl.deseleccionar` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `tl.invertir` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `tl.mover` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `tl.escalar` |
| Ruota maniglie | <kbd>R</kbd> | <kbd>2</kbd> | `tl.rotar` |
| Duplica | <kbd>Shift+D</kbd> | — | `tl.duplicar` |
| Elimina | <kbd>X</kbd> | — | `tl.borrar` |
| Interpolazione | <kbd>T</kbd> | — | `tl.interp` |
| Tipo maniglia | <kbd>V</kbd> | — | `tl.handle` |
| Inquadra selezionati | <kbd>Num.</kbd> | <kbd>0</kbd> | `tl.encuadrar` |
| Frame precedente | <kbd>Left</kbd> | — | `tl.frameAnt` |
| Frame successivo | <kbd>Right</kbd> | — | `tl.frameSig` |
| Keyframe precedente | <kbd>Down</kbd> | — | `tl.keyAnt` |
| Keyframe successivo | <kbd>Up</kbd> | — | `tl.keySig` |
| Vai all'inizio | <kbd>Home</kbd> | — | `tl.inicio` |
| Vai alla fine | <kbd>End</kbd> | — | `tl.fin` |

### Outliner

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `out.seleccionarTodo` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `out.deseleccionar` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `out.invertir` |
| Sposta (riordina) | <kbd>G</kbd> | <kbd>1</kbd> | `out.mover` |
| Rinomina | <kbd>F2</kbd> | — | `out.renombrar` |
| Duplica | <kbd>Shift+D</kbd> | — | `out.duplicar` |
| Duplica collegato | <kbd>Alt+D</kbd> | — | `out.instancia` |
| Nascondi / Mostra | <kbd>H</kbd> | <kbd>2</kbd> | `out.ocultar` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | <kbd>C</kbd> | `out.borrar` |
| Mostra l'attivo | <kbd>Num.</kbd> | — | `out.encuadrar` |

### Editor 2D

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Seleziona tutto | <kbd>A</kbd> | — | `2d.seleccionarTodo` |
| Deseleziona tutto | <kbd>Alt+A</kbd> | — | `2d.deseleccionar` |
| Inverti selezione | <kbd>Ctrl+I</kbd> | — | `2d.invertir` |
| Sposta | <kbd>G</kbd> | <kbd>1</kbd> | `2d.mover` |
| Ruota | <kbd>R</kbd> | <kbd>2</kbd> | `2d.rotar` |
| Scala | <kbd>S</kbd> | <kbd>3</kbd> | `2d.escalar` |
| Duplica | <kbd>Shift+D</kbd> | — | `2d.duplicar` |
| Elimina | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `2d.borrar` |
| Inquadra selezionati | <kbd>Num.</kbd> | — | `2d.encuadrar` |
| Vista gioco | <kbd>Num0</kbd> | — | `2d.vistaJuego` |

### Proprietà (passi di una routine)

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Afferra passo | <kbd>G</kbd> | — | `props.agarrar` |
| Elimina passo | <kbd>X</kbd>, <kbd>Delete</kbd> | — | `props.borrar` |
| Inserisci keyframe | <kbd>I</kbd> | — | `props.keyframe` |

### Editor di script

| Funzione | Scorciatoia predefinita | Nokia N95 (tastiera del telefono) | Nome in atajos.ini |
|---|---|---|---|
| Salva script | <kbd>Ctrl+S</kbd> | — | `ide.guardar` |
| Annulla | <kbd>Ctrl+Z</kbd> | — | `ide.deshacer` |
| Copia | <kbd>Ctrl+C</kbd> | — | `ide.copiar` |
| Taglia | <kbd>Ctrl+X</kbd> | — | `ide.cortar` |
| Incolla | <kbd>Ctrl+V</kbd> | — | `ide.pegar` |
| Seleziona tutto | <kbd>Ctrl+A</kbd> | — | `ide.todo` |
| Rifai | <kbd>Ctrl+Y</kbd>, <kbd>Ctrl+Shift+Z</kbd> | — | `ide.rehacer` |
| Trova | <kbd>Ctrl+F</kbd> | — | `ide.buscar` |
| Sostituisci | <kbd>Ctrl+H</kbd> | — | `ide.reemplazar` |

<!--ATAJOS-FIN-->

## Tasti fissi (non riassegnabili)

Alcuni tasti non sono nell'elenco perché dipendono da ciò che si sta facendo: le frecce per navigare nei menu e nei pannelli, <kbd>Enter</kbd> e <kbd>Esc</kbd> nei menu e nelle finestre, le frecce dell'outliner mentre si sposta una riga, la tastiera dell'IDE quando si scrive, e la tastiera del telefono sull'N95 (vedi [Navigazione](navegacion.md)).

# 3D Viewport

Il 3D Viewport è dove si vede e si modifica la scena. Lavora in diverse **modalità**: ciò che fanno i clic, i tasti e i menu dipende dalla modalità attuale. La modalità si sceglie con il selettore della barra superiore (appare solo se l'oggetto attivo è una mesh o un'armatura) o con <kbd>Tab</kbd>.

| Modalità | Per quali oggetti | A cosa serve |
|---|---|---|
| [Object Mode](modo-objeto.md) | Tutti | Creare, spostare, ruotare, scalare, duplicare, imparentare ed eliminare oggetti interi. |
| [Edit Mode](modo-edicion.md) | Mesh e curve | Modificare vertici, spigoli e facce (modellare). |
| [Armature Edit Mode](esqueletos.md#armature-edit-mode) | Armature | Creare e sistemare le ossa. |
| [Pose Mode](esqueletos.md#pose-mode) | Armature | Mettere in posa e animare le ossa. |
| [Weight Paint](pintura.md#weight-paint) | Solo mesh 3D | Dipingere quanto ogni osso muove ogni vertice. |
| [Vertex Paint](pintura.md#vertex-paint) | Solo mesh 3D | Dipingere colori direttamente sui vertici. |
| [Texture Paint](pintura.md#texture-paint) | Solo mesh 3D | Dipingere sulla texture, direttamente sopra l'oggetto. |

## Barra superiore del 3D Viewport

| Pulsante | Descrizione |
|---|---|
| Selettore di modalità | Cambia modalità (Object, Edit, Pose, Paint…). |
| Vertex / Edge / Face | Solo in Edit Mode: cosa si seleziona. |
| Transform Pivot Point | Attorno a cosa si ruota e si scala: 3D Cursor, Individual Origins, Median Point o Active Element. Con la casella Lock Normals. |
| Orientation | Gli assi della trasformazione: Global, Local, Normal (la direzione della faccia) o View. |
| Snap | Opzioni di snap. Diventa verde se è attivo (<kbd>Shift+Tab</kbd>). |
| Proportional Editing | Quando si sposta qualcosa, ciò che è vicino lo accompagna dolcemente. Diventa verde se è attivo (<kbd>O</kbd>). |
| View | Camere, punti di vista, inquadrare, vista locale, prospettiva, bloccare l'orbita, bloccare la camera alla vista. |
| Select | Tutto, niente, invertire, riquadro, collegato, loop e percorsi. |
| Add (+) | Solo in Object Mode: creare nuovi oggetti. |
| Mesh | Solo in Edit Mode: le operazioni di modellazione. |
| Save Texture (dischetto) | Solo in Texture Paint. Verde se c'è pittura non salvata. |
| Animation (rombo) | Inserire, eliminare e ripulire keyframe; scia di movimento. |
| Context | Il menu della modalità: Object, Vertex/Edge/Face, Armature o Pose. |
| Overlays (occhio) | Quali aiuti vengono disegnati: pavimento, assi, gizmo, cursore 3D, normali, statistiche, raggi X. |
| Render (camera) | La modalità di shading. |
| Scene | Scegliere quale scena, gioco o prefab del progetto si modifica, e crearne di nuovi. |
| UV | Solo in Edit Mode: fare l'unwrap e proiettare le UV. |

## Menu comuni a più modalità

### Select

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| All | Seleziona tutto. | <kbd>A</kbd> |
| None | Deseleziona tutto. | <kbd>Alt+A</kbd> |
| Invert | Ciò che è selezionato viene deselezionato e viceversa. | <kbd>Ctrl+I</kbd> |
| Box Select | Trascinando da sinistra a destra (verde) prende solo ciò che resta interamente dentro; da destra a sinistra (blu) basta sfiorare. <kbd>Shift</kbd> aggiunge. | <kbd>B</kbd> |
| Show selection | Mostra o nasconde il contorno di ciò che è selezionato. | Menu |

### Render (modalità di shading)

| Funzione | Descrizione |
|---|---|
| Render Preview | Con le luci della scena. Se il ray tracing è attivo, mostra l'immagine tracciata poco a poco. |
| Material Preview | Quella predefinita: una luce neutra dalla vista, senza dipendere dalle luci della scena. |
| Solid / Wireframe / ZBuffer Preview | Grigio solido, solo gli spigoli, o la profondità. |
| Normal View / Alpha Preview | Le normali a colori, o la trasparenza in bianco e nero. |
| Motion vectors | Quanto si è mosso ogni pixel sullo schermo dal fotogramma precedente (rosso = X, verde = Y; lavanda = fermo, oliva = lo sfondo). La stessa immagine della passata `_vectores` del render. |
| Wireframe: Only Visible | La vista wireframe mostra solo ciò che viene disegnato davvero. |

### Overlays

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Show Overlays | Accende o spegne tutti gli aiuti visivi in una volta. | <kbd>K</kbd> |
| Floor, X Axis, Y Axis, Origins, 3D Cursor, Relationship Lines | Ogni aiuto separatamente. | Menu |
| Gizmo | Il gizmo per spostare/ruotare/scalare. | Menu |
| Objects | Mostrare o nascondere armature, luci, camere, empty, particelle, curve, hitbox. | Menu |
| Normals | Disegna le normali di vertice, personalizzate o di faccia, con la loro dimensione. | Menu |
| Statistics | FPS, vertici, facce, chiamate di disegno e tempi. | Menu |
| X-Ray | La mesh in modifica appare semitrasparente e si possono scegliere gli elementi che stanno dietro. | Menu |

### Snap

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Attivare lo snap | Ciò che si sposta si aggancia a vertici, spigoli, facce o ai loro centri. | <kbd>Shift+Tab</kbd> |
| Snap Base / Snap Target | Quale punto di ciò che si sposta si aggancia (il più vicino, il centro, la mediana, l'attivo) e a cosa. | Menu |
| Face Project / Face Nearest | Per la retopologia: i vertici si appoggiano su un'altra superficie. | Menu |
| Affect Move / Rotate / Scale | In quali operazioni agisce lo snap. | Menu |
| Snap menu del cursore | Selection to Grid, to Cursor o to Active; Cursor to Selected, to World Origin, to Grid o to Active. | <kbd>Shift+S</kbd> |

### Proportional Editing

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Attivare | Quando si sposta, ruota o scala, i vicini seguono in base alla distanza. | <kbd>O</kbd> |
| Connected Only | Misura la distanza lungo gli spigoli: non trascina pezzi separati. | Menu |
| Falloff | Forma dell'influenza: Smooth, Sphere, Root, Inverse Square, Sharp, Linear, Constant, Random. | Menu |
| Proportional Size | Il raggio d'influenza. Durante la trasformazione si cambia con la rotella o con <kbd>PageUp</kbd> / <kbd>PageDown</kbd>. | Menu / rotella |

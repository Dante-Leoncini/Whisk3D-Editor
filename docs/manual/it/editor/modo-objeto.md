# Object Mode

| Funzione | Descrizione | Come si usa |
|---|---|---|
| Select | Sceglie un oggetto. Un clic nel vuoto deseleziona. | Clic; <kbd>Shift</kbd>+clic aggiunge o toglie |
| Move / Rotate / Scale | Vedi [Spostare, ruotare e scalare](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Add | Crea un nuovo oggetto nel cursore 3D (vedi [menu Add](#menu-add)). | <kbd>Shift+A</kbd> o pulsante + |
| Apply | Incorpora nella mesh la posizione, la rotazione, la scala o tutto (l'oggetto resta in 0, 0, 0 senza rotazione e a scala 1). | <kbd>Ctrl+A</kbd> |
| Set Origin | Geometry to Origin, Origin to Geometry, Origin to 3D Cursor. | Menu Object |
| Duplicate Objects | Copia indipendente; resta agganciata al puntatore per spostarla. | <kbd>Shift+D</kbd> |
| Duplicate Linked | Copia che condivide la mesh con l'originale (modificarne una modifica entrambe). | <kbd>Alt+D</kbd> |
| Join | Unisce le mesh selezionate nell'oggetto attivo. | <kbd>Ctrl+J</kbd> |
| Set Parent | Ciò che è selezionato diventa figlio dell'oggetto attivo (mantenendo o no la trasformazione). | <kbd>Ctrl+P</kbd> |
| Clear Parent | Rimuove il genitore (mantenendo o no la posizione). | <kbd>Alt+P</kbd> |
| Delete | Elimina gli oggetti selezionati (chiede conferma). | <kbd>X</kbd> o <kbd>Delete</kbd> |
| Hide / Show | Alterna la visibilità dell'oggetto attivo. | <kbd>H</kbd> |
| Create Prefab / Unpack Prefab | Converte la selezione in un prefab riutilizzabile, o separa di nuovo un'istanza. | Menu Object |
| Insert Keyframe | Salva la posizione, la rotazione e/o la scala nel frame attuale (menu dei canali). <kbd>Shift+I</kbd> salva tutto senza chiedere. | <kbd>I</kbd> |
| Delete / Clear Keyframe | Elimina il keyframe del frame attuale o tutta l'animazione. | Menu Animation |
| Motion Trail | Disegna il percorso seguito dagli oggetti animati. | Menu Animation |
| Cambiare modalità | Mesh: Object ↔ Edit. Armatura: Object ↔ Pose. Curva: modifica dei nodi. | <kbd>Tab</kbd> |

## Menu Add

Solo in Object Mode. Il nuovo oggetto appare nel cursore 3D e resta selezionato. Le primitive aprono un pannello per regolarne le misure.

| Funzione | Descrizione |
|---|---|
| Mesh | Plane, Cube, Circle, UV Sphere, Cone, Cylinder (con dimensione, raggio, segmenti, shading smussato). Anche Vertex (un punto isolato per iniziare a modellare) e Reference (un piano con un'immagine, per ricalcare). |
| Empty | Un oggetto senza mesh, per raggruppare o come punto di riferimento. |
| Camera / Light / Fog | Camera, luce e nebbia della scena. La luce ha un **Tipo**: Punto, Sole (direzionale) o Spot (un cono, disegnato nella vista); Aggiungi offre anche Sole e Spot. **Intensity** la rende più forte (animabile); **Distance** (m) è fin dove arriva una luce puntiforme o uno spot, e la lunghezza del suo cono (0 = senza limite; il ray tracing la spegne dolcemente lì). Con la luce selezionata, uno spot mostra il suo cono e una luce puntiforme una sfera di quella distanza; un sole mostra sempre una linea fino al pavimento, verso dove illumina. |
| Suono | Un suono posizionato nella scena (icona dell'altoparlante). Suona con la timeline in **secondi**, non in frame; con **Positional (3D)** si sente da dove si trova (più forte quanto più è vicina la camera, dal lato in cui sta); senza, è come una musica. Rilasciare un .wav nella vista 3D ne crea uno lì. |
| Armature | Un'armatura con un osso, per animare personaggi. |
| Path | Una curva di due nodi (si modifica come una mesh: <kbd>E</kbd> aggiunge nodi). |
| Collection | Una cartella per organizzare gli oggetti. |
| UI | Un'interfaccia 2D per giochi (si modifica nel 2D Editor). |
| Hitbox | Un box di rilevamento delle collisioni; se c'è un oggetto attivo nasce come suo figlio, della sua dimensione. |
| Physics | Non crea un oggetto: aggiunge un corpo rigido all'oggetto attivo. |
| Particles | Un emettitore di particelle. |
| LOD / Culling / Grid Cull | Oggetti di ottimizzazione: livelli di dettaglio e occultamento di ciò che non si vede. |
| Clear Depth / Screen Rect | Strumenti di disegno: pulire la profondità o ritagliare una zona dello schermo. |
| Prefab / Scene / Proxy W3D | Inserire un prefab del progetto, un'altra scena, o un prefab di una libreria esterna. |
| Imports | OBJ, FBX, glTF, GLB o un'interfaccia Whisk3D UI. |
| Core (advanced) | Routine: liste di passi di disegno costruite a mano (trasformare, disegnare, texture, luci, nebbia, fusione, profondità, controllo, Call Lua). Include **Routine loop**: una routine che ripete i suoi passi più volte (per esempio "traslare, disegnare" 20 volte), come un array ma più veloce. |

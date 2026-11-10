# Modalità Oggetto

| Funzione | Descrizione | Come si usa |
|---|---|---|
| Seleziona | Sceglie un oggetto. Un clic nel vuoto deseleziona. | Clic; <kbd>Shift</kbd>+clic aggiunge o toglie |
| Sposta / Ruota / Scala | Vedi [Spostare, ruotare e scalare](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Aggiungi | Crea un nuovo oggetto nel cursore 3D (vedi [menu Aggiungi](#menu-aggiungi)). | <kbd>Shift+A</kbd> o pulsante + |
| Applica | Incorpora nella mesh la posizione, la rotazione, la scala o tutto (l'oggetto resta in 0, 0, 0 senza rotazione e a scala 1). | <kbd>Ctrl+A</kbd> |
| Imposta origine | Geometria all'origine, Origine alla geometria, Origine al cursore 3D. | Menu Oggetto |
| Aggancio | Porta il cursore 3D alla selezione, all'oggetto attivo, all'origine del mondo o alla griglia, e la selezione al cursore, all'oggetto attivo o alla griglia. | <kbd>Shift+S</kbd> o menu Oggetto → Aggancio |
| Duplica oggetti | Copia indipendente; resta agganciata al puntatore per spostarla. | <kbd>Shift+D</kbd> |
| Duplica collegato | Copia che condivide la mesh con l'originale (modificarne una modifica entrambe). | <kbd>Alt+D</kbd> |
| Unisci | Unisce le mesh selezionate nell'oggetto attivo. | <kbd>Ctrl+J</kbd> |
| Imposta genitore | Ciò che è selezionato diventa figlio dell'oggetto attivo (mantenendo o no la trasformazione). | <kbd>Ctrl+P</kbd> |
| Rimuovi genitore | Rimuove il genitore (mantenendo o no la posizione). | <kbd>Alt+P</kbd> |
| Elimina | Elimina gli oggetti selezionati (chiede conferma). | <kbd>X</kbd> o <kbd>Delete</kbd> |
| Nascondi / Mostra | Alterna la visibilità dell'oggetto attivo. | <kbd>H</kbd> |
| Crea prefab / Spacchetta prefab | Converte la selezione in un prefab riutilizzabile, o separa di nuovo un'istanza. | Menu Oggetto |
| Inserisci keyframe | Salva la posizione, la rotazione e/o la scala nel frame attuale (menu dei canali). <kbd>Shift+I</kbd> salva tutto senza chiedere. | <kbd>I</kbd> |
| Elimina / Rimuovi keyframe | Elimina il keyframe del frame attuale o tutta l'animazione. | Menu Animazione |
| Scia di movimento | Disegna il percorso seguito dagli oggetti animati. | Menu Animazione |
| Cambiare modalità | Mesh: Oggetto ↔ Modifica. Armatura: Oggetto ↔ Posa. Curva: modifica dei nodi. | <kbd>Tab</kbd> |

## Menu Aggiungi

Solo in Modalità Oggetto. Il nuovo oggetto appare nel cursore 3D e resta selezionato. Le primitive aprono un pannello per regolarne le misure.

| Funzione | Descrizione |
|---|---|
| Mesh | Piano, Cubo, Cerchio, Sfera UV, Cono, Cilindro (con dimensione, raggio, segmenti, ombreggiatura smussata). Anche Vertice (un punto isolato per iniziare a modellare) e Riferimento (un piano con un'immagine, per ricalcare). |
| Vuoto | Un oggetto senza mesh, per raggruppare o come punto di riferimento. |
| Camera / Luce / Nebbia | Camera, luce e nebbia della scena. La luce ha un **Tipo**: Punto, Sole (direzionale) o Faretto (un cono, disegnato nella vista); Aggiungi offre anche direttamente Sole e Faretto. **Intensità** la rende più forte (animabile); **Distanza** (m) è fin dove arriva una luce puntiforme o un faretto, e la lunghezza del suo cono (0 = senza limite; il ray tracing la spegne dolcemente lì). Con la luce selezionata, un faretto mostra il suo cono e una luce puntiforme una sfera di quella distanza; un sole mostra sempre una linea fino al pavimento, verso dove illumina. |
| Suono | Un suono posizionato nella scena (icona dell'altoparlante). Suona con la Timeline in **secondi**, non in frame; con **Posizionale (3D)** si sente da dove si trova (più forte quanto più è vicina la camera, dal lato in cui sta); senza, è come una musica. Rilasciare un .wav nella Vista 3D ne crea uno lì. |
| Armatura | Un'armatura con un osso, per animare personaggi. |
| Percorso | Una curva di due nodi (si modifica come una mesh: <kbd>E</kbd> aggiunge nodi). |
| Collezione | Una cartella per organizzare gli oggetti. |
| UI | Un'interfaccia 2D per giochi (si modifica nell'Editor 2D). |
| Hitbox | Un box di rilevamento delle collisioni; se c'è un oggetto attivo nasce come suo figlio, della sua dimensione. |
| Fisica | Non crea un oggetto: aggiunge un corpo rigido all'oggetto attivo. |
| Particelle | Un emettitore di particelle. |
| LOD / Culling / Grid Cull | Oggetti di ottimizzazione: livelli di dettaglio e occultamento di ciò che non si vede. |
| Pulisci Z / Ritaglio | Strumenti di disegno: pulire la profondità o ritagliare una zona dello schermo. |
| Prefab / Scena / Proxy W3D | Inserire un prefab del progetto, un'altra scena, o un prefab di una libreria esterna. |
| Importazioni | OBJ, FBX, glTF, GLB, STL o un'interfaccia Whisk3D UI. L'STL (stampa 3D, CAD) entra con la Z verso l'alto, ruotato in modo che stia in piedi, con i vertici coincidenti saldati. |
| Nucleo (avanzato) | Routine: liste di passi di disegno costruite a mano (trasformare, disegnare, texture, luci, nebbia, fusione, profondità, controllo, Chiama Lua). Include **Ciclo di routine**: una routine che ripete i suoi passi più volte (per esempio "traslare, disegnare" 20 volte), come un array ma più veloce. |

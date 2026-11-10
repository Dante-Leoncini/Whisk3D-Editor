# Spostare, ruotare e scalare

Funziona allo stesso modo in Modalità Oggetto, Modalità Modifica, Modalità Posa e nella modifica delle ossa.

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Sposta (traslare) | Ciò che è selezionato segue il puntatore. | <kbd>G</kbd> |
| Ruota | Ruota attorno al pivot. Premendo di nuovo <kbd>R</kbd> si alterna tra rotazione libera (trackball) e orbitale. | <kbd>R</kbd> |
| Scala | Ingrandisce o rimpicciolisce a partire dal pivot. | <kbd>S</kbd> |
| Vincolare a un asse | Durante la trasformazione. Premendo di nuovo si passa ciclicamente Globale → Locale → Vista → libero. | <kbd>X</kbd> / <kbd>Y</kbd> / <kbd>Z</kbd> |
| Vincolare a un piano | Esclude quell'asse (sposta sugli altri due). | <kbd>Shift+X</kbd> / <kbd>Shift+Y</kbd> / <kbd>Shift+Z</kbd> |
| Valore esatto | Digitare un numero o un calcolo (`2*3`, `(1+0.5)/2`). <kbd>-</kbd> inverte il segno, <kbd>Backspace</kbd> cancella. | Numeri |
| Conferma | Accetta l'operazione. | Clic sinistro, <kbd>Enter</kbd>, spunta verde |
| Annulla | Tutto torna com'era. | <kbd>Esc</kbd>, clic destro, croce rossa |
| Passi fini | Sposta, ruota o scala poco a poco. | Frecce |

Mentre dura, la barra superiore mostra cosa si sta facendo, per esempio "Sposta X 1.5 lungo X globale". Su touch, toccando quella barra si apre un tastierino numerico.

## Il gizmo

Il gizmo appare sopra il pivot in Modalità Oggetto e in Modalità Modifica (si accende e si spegne in Sovrapposizioni → Gizmo).

| Funzione | Descrizione |
|---|---|
| Gizmo di spostamento | Frecce = un asse; quadrati = un piano; cerchio centrale = libero. |
| Gizmo di rotazione | Un semicerchio colorato per asse, un anello bianco per ruotare rispetto alla vista, e l'interno per ruotare liberamente. |
| Gizmo di scala | Punte quadrate per asse; il cerchio centrale scala in modo uniforme. |
| Cambiare gizmo | Un clic con il mouse o un tocco con il dito sul gizmo, senza trascinare, passa da Sposta a Ruota, a Scala e di nuovo a Sposta. Funziona allo stesso modo su PC, nel browser e sugli schermi touch. |

## Regolare l'ultima operazione

Dopo aver spostato, ruotato o scalato (in Modalità Oggetto o Modifica) appare in basso a sinistra un pannello con ciò che è stato fatto: i valori X, Y, Z (o l'angolo), l'orientamento e la modifica proporzionale con le sue opzioni. Cambiando un valore l'operazione viene rifatta con il nuovo valore. **Annulla** la disfa e **Accetta** la mantiene. Anche un clic fuori dal pannello accetta. Mentre il pannello è aperto, la barra degli strumenti della Vista 3D si nasconde.

Il pannello ha il bordo verde perché è la finestra con il focus. Se non entra nella metà dell'altezza dello schermo, si riduce a quella metà e si scorre trascinando il dito (senza toccare le opzioni), con la rotella o con le frecce. Con la tastiera o il telefono: le frecce percorrono le righe, <kbd>OK</kbd> / <kbd>Enter</kbd> modifica il valore senza chiudere il pannello, **Accetta**, <kbd>Esc</kbd> o <kbd>Delete</kbd> (la <kbd>C</kbd> sull'N95) lo chiudono mantenendo la modifica, e solo **Annulla** la disfa.

## Barra degli strumenti

Contiene Annulla e Rifai, le ultime azioni usate e il pulsante della modifica proporzionale. Durante una trasformazione mostra la spunta (accettare), la croce (annullare), l'orientamento e gli assi X / Y / Z (due assi accesi = un piano). Su touch, dopo un'estrusione appare **Ripeti**, che accetta ed estrude di nuovo.

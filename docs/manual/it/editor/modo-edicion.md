# Modalità Modifica (mesh)

Si entra con <kbd>Tab</kbd> su una mesh. Si lavora su **vertici**, **spigoli** o **facce** a seconda del pulsante della barra superiore.

## Selezionare

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Seleziona | Sceglie un vertice, uno spigolo o una faccia. | Clic; <kbd>Shift</kbd>+clic alterna |
| Seleziona collegati | Tutta l'isola connessa sotto il puntatore. | <kbd>L</kbd> |
| Seleziona anello | Un anello di spigoli o di facce. | <kbd>Shift+Alt</kbd>+clic |
| Percorso più breve | Seleziona il percorso tra l'elemento attivo e quello cliccato. | <kbd>Ctrl</kbd>+clic |
| Percorso più breve (riempi regione) | Seleziona tutta la regione tra i due. | <kbd>Ctrl+Shift</kbd>+clic |
| Tutto / Nessuno / Inverti / Selezione a riquadro | Come in Modalità Oggetto, sugli elementi della mesh. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>B</kbd> |

## Modellare

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Sposta / Ruota / Scala | Trasforma ciò che è selezionato. Con uno spostamento in corso, premendo di nuovo <kbd>G</kbd> si passa a far scorrere. | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Restringi/Ingrassa | Sposta ogni vertice lungo la sua normale (gonfia o sgonfia). | <kbd>Alt+S</kbd> |
| Estrudi | Crea nuova geometria a partire da ciò che è selezionato (le facce escono lungo la loro normale). | <kbd>E</kbd> |
| Taglio ad anello e scorri | Taglia un anello di spigoli. La rotella cambia il numero di tagli; il clic applica e poi si fa scorrere. | <kbd>Ctrl+R</kbd> |
| Nuovo spigolo/faccia da vertici | Unisce i vertici selezionati con uno spigolo o una faccia. | <kbd>F</kbd> |
| Connetti percorso di vertici | Taglia in linea retta tra due vertici. | <kbd>J</kbd> |
| Scorri vertici | Fa scorrere i vertici lungo i loro spigoli. | <kbd>Shift+V</kbd> |
| Strappa | Separa la mesh lungo la selezione. | <kbd>V</kbd> |
| Duplica | Copia ciò che è selezionato e lo lascia agganciato al puntatore. | <kbd>Shift+D</kbd> |
| Separa | Sposta le facce selezionate in un nuovo oggetto. | <kbd>P</kbd> |
| Fondi | Salda i vertici: Al centro, Al cursore, Collassa o Per distanza. | <kbd>M</kbd> |
| Fusione automatica | Al termine dello spostamento, salda i vertici rimasti più vicini della soglia. | Menu Mesh |
| Elimina | Vertici, spigoli, facce o loop di spigoli. | <kbd>X</kbd> o <kbd>Delete</kbd> |
| Ricalcola normali / Capovolgi | Corregge la direzione verso cui guardano le facce, o le capovolge. | <kbd>Shift+N</kbd> |
| Triangola facce | Divide in triangoli le facce con più di 3 lati. | <kbd>Ctrl+T</kbd> |
| Ombreggiatura liscia / piatta | Come viene illuminata la superficie. | Menu Faccia |
| Segna / Rimuovi netto | Spigoli che appaiono netti anche se la mesh è smussata. | Menu Spigolo |
| Segna / Rimuovi cucitura | Dove si taglia la mesh quando si fa l'unwrap delle UV. | Menu Spigolo o UV |
| Menu contestuale | Le operazioni della sotto-modalità attuale (vertice, spigolo o faccia) sul puntatore. | <kbd>W</kbd> o clic destro |
| Inserisci keyframe | Salva la forma della mesh nel frame attuale (animazione dei vertici). | <kbd>I</kbd> |

## Menu UV

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Srotola | Basato su angoli, Conforme o Stiramento minimo: apre la mesh lungo le sue cuciture. | <kbd>U</kbd> |
| Proiezione UV intelligente | Taglia e fa l'unwrap da sola in base agli angoli tra le facce. | <kbd>U</kbd> |
| Impacchetta lightmap / Segui quad attivi | Isole ordinate senza sovrapporsi / UV ordinate seguendo la faccia attiva. | <kbd>U</kbd> |
| Proiezione cubo, cilindro, sfera, Proietta dalla vista | Proiezioni semplici. | <kbd>U</kbd> |
| Ripristina | Ogni faccia occupa tutta l'immagine. | <kbd>U</kbd> |

## Modifica delle curve (path)

Con <kbd>Tab</kbd> su una curva se ne modificano i nodi come se fosse una mesh di punti: <kbd>G</kbd> sposta, <kbd>E</kbd> aggiunge il nodo successivo, <kbd>X</kbd> elimina.

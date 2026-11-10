# Editor di script (IDE)

Editor di testo per gli script Lua e gli [shader scritti](shaders.md) (`.glsl`), con numeri di riga, colori della sintassi, guide di indentazione e supporto per la ñ e le lettere accentate. Quando ha il focus, le lettere scrivono (non attivano le scorciatoie dell'editor). Su Android, toccando il testo si apre la tastiera. Sull'N95 si scrive con il T9.

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Script | Scegliere quale .lua del progetto si modifica, o Carica .lua… (un file esterno). | Menu Script |
| Salva | Salva lo script. Se si trova dentro il .w3d, salva anche il progetto. | <kbd>Ctrl+S</kbd> o File → Salva |
| Salva come… | Un altro nome, dentro o fuori dal progetto, in una cartella a scelta. | File |
| Salva nel .w3d | Solo se lo script è esterno: lo inserisce nel progetto. | File |
| Salva fuori dal .w3d… | Solo se lo script è interno: lo porta fuori in un file; gli oggetti passano a usare quel file. | File |
| Aggiorna | Ricarica il file così com'è sul disco (ciò che è stato modificato con un altro editor). Se nell'editor ci sono modifiche non salvate, chiede prima di scartarle (e si possono recuperare con <kbd>Ctrl+Z</kbd>). Con il gioco in esecuzione, ricarica al volo gli oggetti che usano lo script. | Pulsante |
| Shader (.glsl) | Si aprono dal materiale (il pulsantino della riga **Shader**), dalla Libreria o dal selettore, e sono colorati come GLSL. Accanto al nome compare **Compila**: compila ciò che stai scrivendo (se un materiale lo usa, lo salva e compila quel materiale). Salvare uno shader lo compila in tutti i materiali che lo usano e avvisa se è stato compilato, oppure mostra l'errore con la sua riga. | <kbd>Ctrl+S</kbd> |
| Protezione al salvataggio | Se il file esterno è stato modificato fuori dall'editor da quando è stato aperto, Salva chiede prima di sovrascriverlo. Salva ricarica anche lo script al volo se il gioco è in esecuzione. | Automatico |
| Indentazione (Tab) | <kbd>Tab</kbd> completa fino al prossimo multiplo di 4; con più righe selezionate aggiunge loro un livello e <kbd>Shift+Tab</kbd> lo toglie. Le linee verticali segnano ogni livello; quella del blocco del cursore appare più chiara. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Menu Modifica | Annulla, Rifai, Taglia, Copia, Incolla, Trova… e Sostituisci… | Pulsante Modifica |
| Menu Seleziona | Seleziona tutto. | Pulsante Seleziona |
| Annulla / Rifai | Sul testo dello script (non sulla scena). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> o <kbd>Ctrl+Shift+Z</kbd> |
| Trova / Sostituisci | Apre un pannello in alto a destra: il testo da cercare, quante corrispondenze ci sono ("3 di 7"), Maiuscole/minuscole, Parola intera, Precedente / Successivo, e in Sostituisci il nuovo testo con Sostituisci e Sostituisci tutto (un solo passo di annullamento). Le corrispondenze vengono evidenziate nel codice. Il pannello non blocca l'editor: con il puntatore sullo script si continua a scrivere. Si chiude con Chiudi o <kbd>Esc</kbd>. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copia / Taglia / Incolla | Con gli appunti di sistema. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Seleziona tutto / Annulla | Annulla agisce sul testo, non sulla scena. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Selezionare con il dito | Il doppio tocco seleziona la parola, il triplo tocco l'intera riga. Con una selezione compaiono due maniglie (cerchi verdi) alle sue estremità: si trascinano per regolarla. | Tocchi |
| Lente (touch) | Tenendo fermo il dito per un momento sul codice, sopra il dito appare la riga ingrandita con il cursore; trascinando, il cursore segue il dito e resta dove lo rilasci. | Pressione prolungata |
| Selezionare sull'N95 | Le frecce muovono il cursore; tenendo premuto il tasto matita, le frecce selezionano a partire dalla posizione del cursore. Mentre tieni premuto il tasto matita appare in basso una barra: il tasto di selezione sinistro **copia** e quello destro **incolla**. | Matita + frecce / softkey |
| Muoversi nel testo | Frecce, Home, End, PageUp, PageDown (con <kbd>Shift</kbd> selezionano); <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> vanno all'inizio o alla fine. | Tastiera |

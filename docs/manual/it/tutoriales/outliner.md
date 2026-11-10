# L'Outliner

> [!NOTE]
> ![Nokia](img/icono-nokia.svg) Il Nokia N95 supporta mouse e tastiera (via Bluetooth). Le parti di questa pagina dedicate al Nokia spiegano come usarlo senza mouse né tastiera, solo con i tasti del telefono.

L'**Outliner** è l'elenco di tutto ciò che c'è nella scena: oggetti, camere, luci e collezioni. È uno dei viewport visti in [I viewport](viewports.md); sul PC si trova in alto a destra. Da qui gli oggetti si selezionano, si nascondono, si riordinano, si mettono uno dentro l'altro e si rinominano.

Ciò che selezioni nell'Outliner resta selezionato anche nella Vista 3D, e viceversa. L'ultimo che hai selezionato è l'**oggetto attivo**: quello che mostra Proprietà.

![Android](img/icono-android.svg) **Su Android** l'Outliner non è sullo schermo: lo spazio resta alla Vista 3D e a Proprietà. Per usarlo, trasforma un viewport in Outliner (o dividine uno) con il pulsante dell'angolo, come in [Cambiare il tipo](viewports.md#cambiare-il-tipo). Si può lavorare anche senza l'Outliner: gli oggetti si selezionano toccandoli nella Vista 3D, e Proprietà ha il nome e le caselle **Visibile** e **Renderizzabile**.

![Nokia](img/icono-nokia.svg) **Sul Nokia N95** l'Outliner non è sullo schermo: ci stanno solo due viewport. Mentre metti in ordine la scena, trasforma Proprietà in Outliner: con il **tasto di selezione sinistro** entri nelle sue schede e con la freccia sinistra arrivi al menu del pulsante dell'angolo; lì scegli Outliner. Per tornare indietro, il tasto di selezione sinistro nell'Outliner apre direttamente quel menu. Con il **tasto verde** ![](img/tecla-verde.svg) lo rendi il viewport attivo: tutto ciò che segue sull'N95 si fa con il bordo verde sull'Outliner.

## Cosa mostra ogni riga

Ogni riga è un oggetto. La **freccia** comprime o espande ciò che contiene; l'**icona** ne dice il tipo (mesh, camera, luce, collezione...); poi viene il **nome**, in verde se è l'attivo e in un verde più scuro se è selezionato; e a destra, l'**occhio** e la **camera**. Un oggetto che sta dentro un altro appare più a destra, appeso a lui, e una riga sbiadita è un oggetto nascosto.

In un progetto nuovo, in alto c'è il **Costruttore**, una routine che prepara lo schermo, e sotto la **Collezione** con la camera, la luce e il cubo. Una collezione è una cartella per mettere in ordine.

## Selezionare

| | Come si seleziona |
|---|---|
| ![PC](img/icono-pc.svg) PC | Un clic sulla riga seleziona solo quell'oggetto. <kbd>Shift</kbd> + clic seleziona tutte le righe dall'attivo fino a quella. <kbd>Ctrl</kbd> + clic aggiunge o toglie una riga. Con il mouse sull'Outliner, le frecce su e giù passano alla riga precedente o alla successiva. |
| ![Android](img/icono-android.svg) Schermo touch | Un tocco sulla riga. Con **Shift** o **Ctrl** attivi nella barra degli strumenti della Vista 3D, il tocco fa lo stesso che con la tastiera. Trascinare il dito fa scorrere l'elenco. |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Le frecce su e giù passano alla riga precedente o alla successiva, e l'elenco scorre da solo per seguirla. |
| Menu | Seleziona → Seleziona tutto, Deseleziona tutto o Inverti selezione |

![Un clic su Camera la seleziona; Shift + clic su Cubo seleziona da Camera a Cubo; un clic su Luce lascia solo la luce; Ctrl + clic su Cubo lo aggiunge e un altro Ctrl + clic lo toglie](img/outliner-seleccionar-raton.webp)

## Comprimere ed espandere

Quando un oggetto ne contiene altri, la sua freccia li mostra o li nasconde. Comprimere ciò che non stai usando mantiene l'elenco corto.

| | Come si comprime e si espande |
|---|---|
| ![PC](img/icono-pc.svg) PC | Un clic sulla freccia. Con il mouse sull'Outliner, la freccia destra della tastiera espande l'attivo e la sinistra lo comprime; se era già compresso, la sinistra sale a quello che lo contiene e lo comprime. |
| ![Android](img/icono-android.svg) Schermo touch | Un tocco sulla freccia. |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Destra espande l'attivo e sinistra lo comprime; se era già compresso, sinistra sale a quello che lo contiene e lo comprime. |

![Un clic sulla freccia di Collezione nasconde Camera, Luce e Cubo; un altro clic li mostra di nuovo](img/outliner-plegar-raton.webp)

## Nascondere: l'occhio e la camera

- L'**occhio** mostra o nasconde l'oggetto nella Vista 3D. Un oggetto nascosto non compare nemmeno nel render. Se ne contiene altri, si nascondono con lui e le loro righe appaiono sbiadite.
- La **camera** decide se compare nel **render**. Se è spenta, l'oggetto continua a vedersi mentre lavori, ma non appare nelle immagini né nei video che renderizzi.

Toccarli non cambia la selezione.

| | L'occhio | La camera |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | Clic sull'occhio, o <kbd>H</kbd> (con il mouse sull'Outliner) per l'attivo | Clic sulla camera |
| ![Android](img/icono-android.svg) Schermo touch | Un tocco sull'occhio | Un tocco sulla camera |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>2</kbd>, quello dell'attivo | <kbd>3</kbd>, quella dell'attivo |
| Menu | Elemento → Nascondi/Mostra (quello dell'attivo) | — |

![Un clic sull'occhio di Cubo lo chiude e il cubo scompare dalla Vista 3D; un altro clic lo mostra di nuovo. Un clic sulla camera di Cubo la spegne, e il cubo continua a vedersi](img/outliner-ocultar-raton.webp)

Entrambi si trovano anche in Proprietà, nel riquadro Trasforma: **Visibile** e **Renderizzabile**. Hanno il loro rombo, quindi si possono animare: per esempio, per far comparire qualcosa in un momento del video.

## Riordinare e mettere un oggetto dentro un altro

L'ordine dell'elenco si può cambiare, e un oggetto si può mettere **dentro** un altro: diventa suo **figlio**, e quando il genitore si sposta, ruota o scala, il figlio lo segue. È ciò che in [La Modalità Oggetto](modo-objeto.md#loutliner-gli-oggetti-della-scena) si chiama **imparentare**. L'oggetto non si sposta dal suo posto nella scena: cambia solo nell'elenco.

![PC](img/icono-pc.svg) **Con il mouse:** prendi la riga e trascinala. Sul bordo superiore o inferiore di un'altra riga compare una **linea verde**: rilasciando, l'oggetto resta prima o dopo quella riga, al suo stesso livello. Sul centro di un'altra riga, quella riga si evidenzia: rilasciando, l'oggetto finisce **dentro** di essa. Rilasciato sotto l'ultima riga, esce da tutto ciò che lo conteneva. Vicino al bordo superiore o inferiore, l'elenco scorre da solo, così si arriva a qualsiasi riga di un elenco lungo.

![La riga di Luce si trascina sopra Camera e la linea verde la mette per prima; poi Camera si rilascia al centro della riga di Cubo e finisce dentro, spostata a destra](img/outliner-mover-raton.webp)

![Android](img/icono-android.svg) **Con il dito:** tieni il dito fermo sulla riga per un secondo, finché il telefono non vibra: la riga resta afferrata. Poi trascinala e rilasciala come con il mouse. Se la rilasci senza muoverla, si apre il menu dell'oggetto. Senza questa attesa, trascinare il dito fa scorrere l'elenco.

![Nokia](img/icono-nokia.svg) **Sul Nokia N95:** seleziona la riga con le frecce e premi <kbd>1</kbd>. La riga diventa verde, e ora le frecce la spostano:

- ![Nokia](img/icono-nokia.svg) **Su** e **giù** la alzano o la abbassano tra quelle che stanno al suo stesso livello.
- ![Nokia](img/icono-nokia.svg) **Destra** la mette dentro quella che ha sopra, al suo stesso livello.
- ![Nokia](img/icono-nokia.svg) **Sinistra** la toglie da quella che la contiene: resta dopo di essa, al suo livello.
- ![Nokia](img/icono-nokia.svg) <kbd>1</kbd> di nuovo (o <kbd>OK</kbd>) la lascia lì. <kbd>C</kbd> annulla e la riporta dov'era.

![Nokia](img/icono-nokia.svg) Tutto lo spostamento si annulla in una volta, con <kbd>C</kbd> + sinistra. Serve, per esempio, a mettere una luce prima degli oggetti che illumina.

![Un Nokia N95 con l'Outliner attivo: con l'1 la riga di Luce diventa verde, la freccia su la porta sopra Camera e l'1 la lascia lì; con il 2 il cubo scompare dalla Vista 3D e ricompare; con il 3 si spegne la camera della sua riga](img/outliner-nokia.webp)

## Trovare l'attivo e inquadrarlo

In una scena grande l'elenco è lungo, e selezionare qualcosa nella Vista 3D non lo fa scorrere: la sua riga può restare fuori vista. Per trovarla nell'elenco, o per vedere l'oggetto nella Vista 3D (**inquadrarlo**, come in [Inquadrare la selezione](navegacion.md#inquadrare-la-selezione)):

| | Nell'Outliner | Nella Vista 3D |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num .</kbd> con il mouse sull'Outliner (**Mostra l'attivo**): l'elenco scorre finché ciò che è selezionato non resta al centro. Se è dentro qualcosa di compresso non ha una riga: espandilo prima. | <kbd>Num .</kbd> con il mouse sulla Vista 3D |
| ![Android](img/icono-android.svg) Schermo touch | Trascina l'elenco con il dito | Doppio tocco con due dita sulla Vista 3D |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | L'elenco segue da solo la riga che scegli con le frecce | Una pressione dello <kbd>0</kbd>, anche se il viewport attivo è l'Outliner |
| Menu | — | Vista → Inquadra selezionati, nella barra della Vista 3D |

![Una scena con decine di oggetti: se ne seleziona uno nella Vista 3D e la sua riga non si vede nell'Outliner; con il mouse sull'Outliner, Num . fa scorrere l'elenco e la riga verde resta al centro](img/outliner-activo-raton.webp)

## Rinominare

| | Come si rinomina |
|---|---|
| ![PC](img/icono-pc.svg) PC | Doppio clic sul nome, o <kbd>F2</kbd> per l'attivo: la riga diventa un campo di testo. Scrivi il nuovo nome e conferma con <kbd>Enter</kbd> o con un clic fuori; <kbd>Esc</kbd> lo lascia com'era. |
| ![Android](img/icono-android.svg) Schermo touch | Doppio tocco sul nome: si apre la tastiera sullo schermo. |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Con il tasto di selezione sinistro entri nella barra e con la freccia destra arrivi a Elemento → Rinomina. Scrivi con il T9 e conferma con <kbd>OK</kbd>. |
| Menu | Elemento → Rinomina |

![Doppio clic sul nome di Cubo: diventa un campo, si scrive Scatola e Enter conferma; poi si seleziona Luce, F2, si scrive Lampada e Enter](img/outliner-renombrar-raton.webp)

Due oggetti della stessa scena non possono avere lo stesso nome: se il nome è già usato, Whisk3D gli aggiunge un numero e ti avvisa. Se uno script si riferiva all'oggetto per nome, si aggiorna da solo.

## Duplicare ed eliminare

| | Duplicare | Eliminare |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Shift</kbd> + <kbd>D</kbd>; <kbd>Alt</kbd> + <kbd>D</kbd> per Duplica collegato | <kbd>X</kbd> o <kbd>Delete</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Elemento → Duplica | <kbd>C</kbd> |
| Menu | Elemento → Duplica o Duplica collegato | Elemento → Elimina |

Dall'Outliner, la copia resta esattamente sopra l'originale, senza seguire il mouse come nella Vista 3D: spostala dopo. Eliminare chiede conferma, ed entrambe le cose si possono annullare.

## La barra dei menu dell'Outliner

| Pulsante | Cosa contiene |
|---|---|
| **Opzioni vista** (l'occhio) | Cosa si mostra: il Costruttore, le altre routine, e le colonne dell'occhio e della camera (**Icone di visibilità** e **Icone di render**). |
| **Elemento** (il cubo) | Elimina, Rinomina, Sposta, Nascondi/Mostra, Duplica, Duplica collegato, Crea prefab e Spacchetta prefab. |
| **Seleziona** | Seleziona tutto, Deseleziona tutto e Inverti selezione. |
| La scena | Quale scena, gioco o prefab del progetto si sta modificando ([Scene, giochi e prefab](outliner.md#scene-giochi-e-prefab)). |

![PC](img/icono-pc.svg) Un clic destro su una riga la seleziona e apre lo stesso menu lì, sul puntatore.

![Android](img/icono-android.svg) Con il dito, tieni premuta la riga finché non vibra e rilasciala senza muoverla: si apre lo stesso menu.

## La Libreria

Le **risorse** del progetto (mesh, materiali, texture, animazioni, suoni, script, prefab...) non sono nell'Outliner: hanno il loro viewport, la **Libreria**, che si sceglie con il pulsante dell'angolo. Mostra miniature in cartelle, dice quali risorse sono usate e quali no, e una risorsa si può trascinare nella Vista 3D: una mesh crea un oggetto, e un materiale viene assegnato a ciò che resta sotto. È spiegata in [Vista Libreria](outliner.md#vista-libreria); il resto dell'Outliner, nel suo [manuale](outliner.md) e in [Scorciatoie da tastiera](../editor/atajos.md#outliner).

## Vista Libreria

| Funzione | Descrizione |
|---|---|
| Risorse | Mesh, materiali, texture, animazioni, prefab, scene, suoni, script, [shader](../editor/shaders.md), font e video, in cartelle. Un materiale o uno shader trascinato su una mesh della Vista 3D si vede sulla parte sotto il puntatore prima di rilasciarlo. Spunta verde = in uso; croce rossa = orfana; `*` = non salvata. |
| Filtro / Lista o Griglia | Mostrare un solo tipo; vedere le miniature. |
| Trascinare una risorsa | In una cartella (la sposta), nella Vista 3D (mesh = nuovo oggetto, materiale o texture = si assegna a ciò che si tocca, prefab = istanza) o in un menu a tendina di Proprietà. |
| Nuova cartella / Sposta in cartella | Organizzare (le cartelle servono solo a mettere in ordine). |
| Duplica risorsa / Seleziona utenti / Elimina orfani | Copiare, trovare chi la usa, eliminare ciò che nessuno usa. |
| Dentro il .w3d / Esterno | Da Proprietà (riquadro Risorsa): sposta la risorsa dentro o fuori dal progetto. |

## Scene, giochi e prefab

Il pulsante con il nome della scena (nell'Outliner e nella Vista 3D) elenca le scene, i giochi e i prefab del progetto. Da lì se ne creano di nuovi, si sceglie la scena iniziale e si converte una scena in gioco (o viceversa).

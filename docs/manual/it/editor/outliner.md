# Outliner

L'elenco di tutto ciò che c'è nella scena, e anche la libreria delle risorse del progetto. Su Android non viene mostrato (si lavora con il 3D Viewport e Properties).

## Menu View (occhio)

| Funzione | Descrizione |
|---|---|
| Scene / Library / librerie | Cosa si elenca: l'albero della scena, tutte le risorse del progetto, o quelle di un altro .w3d collegato (sola lettura). |
| Constructor / Routines | Mostrano o nascondono la routine Constructor (ciò che viene eseguito una sola volta all'avvio) e le altre routine. |
| Visibility icons / Render icons | Mostrano o nascondono le colonne dell'occhio e della camera. |
| Grid | In Library: miniature invece dell'elenco. |
| Link .w3d… / Unlink | Usa un altro progetto come libreria, o lo scollega. |

## Vista Scene

| Funzione | Descrizione | Come si usa |
|---|---|---|
| Select | Sceglie l'oggetto. <kbd>Shift</kbd>+clic prende un intervallo; <kbd>Ctrl</kbd>+clic aggiunge o toglie. | Clic |
| Espandere / comprimere | Mostra o nasconde i figli. | Freccia; <kbd>Left</kbd> / <kbd>Right</kbd> |
| Visible / Renderable | L'occhio nasconde l'oggetto (è animabile); la camera lo esclude dal render. | Clic sull'occhio / sulla camera |
| Riordinare e imparentare | Rilasciare prima, dopo o sopra un'altra riga (come figlio). | Trascinare la riga |
| Move (reorder) | Con la tastiera: su/giù riordina, sinistra toglie dal genitore, destra mette sotto il precedente, <kbd>Enter</kbd> conferma. | <kbd>G</kbd> |
| Rename | La riga diventa un campo di testo. | Doppio clic o <kbd>F2</kbd> |
| Duplicate / Duplicate Linked | Come nel 3D Viewport. | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Hide / Show | Alterna la visibilità. | <kbd>H</kbd> |
| Delete | Con conferma. | <kbd>X</kbd> o <kbd>Delete</kbd> |
| Create / Unpack Prefab | Dal menu Object. | Clic destro |
| Show active | Scorre l'elenco fino all'oggetto attivo. | <kbd>Num.</kbd> |
| Scorrere l'elenco | Rotella, o trascinare (anche con il dito). | Rotella / trascinare |

## Vista Library

| Funzione | Descrizione |
|---|---|
| Risorse | Mesh, materiali, texture, animazioni, prefab, scene, suoni, script, [shader](shaders.md), font e video, in cartelle. Un materiale o uno shader trascinato su una mesh del 3D Viewport si vede sulla parte sotto il puntatore prima di rilasciarlo. Spunta verde = in uso; croce rossa = orfana; `*` = non salvata. |
| Filter / List o Grid | Mostrare un solo tipo; vedere le miniature. |
| Trascinare una risorsa | In una cartella (la sposta), nel 3D Viewport (mesh = nuovo oggetto, materiale o texture = si assegna a ciò che si tocca, prefab = istanza) o in un menu a tendina di Properties. |
| New Folder / Move to Folder | Organizzare (le cartelle servono solo a mettere in ordine). |
| Duplicate Resource / Select Users / Purge Orphans | Copiare, trovare chi la usa, eliminare ciò che nessuno usa. |
| Inside the .w3d / External | Da Properties (riquadro Resource): sposta la risorsa dentro o fuori dal progetto. |

## Scene, giochi e prefab

Il pulsante con il nome della scena (nell'Outliner e nel 3D Viewport) elenca le scene, i giochi e i prefab del progetto. Da lì se ne creano di nuovi, si sceglie la scena iniziale e si converte una scena in gioco (o viceversa).

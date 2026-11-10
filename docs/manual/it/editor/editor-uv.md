# Editor UV

Mostra come la texture si appoggia sulla mesh. Per modificare, la mesh deve essere in **Modalità Modifica nella Vista 3D**; altrimenti l'editor mostra solo la texture. La modalità dell'Editor UV è indipendente: non cambia la modalità della Vista 3D. Gli strumenti per fare l'unwrap e proiettare le UV si trovano nella Vista 3D (Modalità Modifica, tasto <kbd>U</kbd>).

## Navigazione e barra superiore

| Funzione | Descrizione | Come si usa |
|---|---|---|
| Zoom / Spostare la vista | Avvicina verso il puntatore / sposta la vista. | Rotella / tasto centrale o frecce; touch: 1 dito sposta la vista, 2 dita zoom |
| Modalità | Modalità Oggetto, Modalità Modifica, Pittura pesi, Modifica ossa e Modalità Posa (questi ultimi due se la mesh ha un'armatura 2D). | Pulsante della modalità, <kbd>Tab</kbd> |
| Vertice / Spigolo / Faccia | Cosa si seleziona in Modalità Modifica. | Pulsante |
| Perno | Cursore, Origini individuali o Punto mediano. | Pulsante |
| Cursore | Cursore alla selezione, Selezione al cursore, Cursore al centro. | Menu |
| Vista | Sincronizza selezione (la selezione UV è la stessa della mesh), Ripeti texture (mosaico), Mostra Chrome UV, Inquadra selezionati, Inquadra tutto. | Menu; <kbd>Num.</kbd> |
| Texture | Quale texture si mostra: Nuova texture…, Auto (quella della parte attiva), o una qualsiasi del progetto. | Menu |
| Nuova texture | Crea un'immagine (nome, dimensione, colore, trasparenza; tinta unita, griglia UV o griglia di colori), la salva nel progetto e la assegna al materiale. | Texture → Nuova texture… |
| Mipmap | Mostra un livello di mipmap in particolare, per controllarlo. | Pulsante Mip |
| Modifica | Entra ed esce dalla pittura di texture sull'immagine. | Pulsante Modifica |
| Salva (dischetto, sempre visibile) | Salva, Salva come…, Salva fuori dal .w3d…, Salva nel .w3d, Esporta layout UV… | Menu |
| Esporta layout UV | Salva un PNG con sfondo trasparente: facce semitrasparenti (più opache dove si sovrappongono) e linee opache. Si scelgono nome, cartella e risoluzione (predefinita 512, 256 sull'N95, o la dimensione della texture mostrata). | Salva → Esporta layout UV… |
| Animazione | Inserire, eliminare o ripulire keyframe del livello UV, o della posa 2D. | Menu |

## Modalità Oggetto (UV)

Il clic sceglie cosa si modifica: un'armatura 2D (se si tocca un osso) o la geometria. <kbd>Tab</kbd> entra a modificare l'elemento scelto.

## Modalità Modifica (UV)

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Seleziona | Vertici, spigoli o facce UV. Senza sincronizzazione, la Vista 3D filtra: si vedono solo le facce selezionate in 3D. | Clic; <kbd>Shift</kbd>+clic |
| Tutto / Nessuno / Inverti | Su ciò che è visibile. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Seleziona collegati | L'isola UV sotto il puntatore (<kbd>Shift</kbd> aggiunge). | <kbd>L</kbd> |
| Selezione a riquadro | Rettangolo; <kbd>Shift</kbd> aggiunge. | <kbd>B</kbd> |
| Sposta / Ruota / Scala | Trasforma le UV. <kbd>X</kbd> / <kbd>Y</kbd> bloccano un asse; si può digitare il valore (1.0 = una piastrella). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Posizionare il cursore 2D | Mette il cursore dove si clicca. | Clic destro |
| Inserisci keyframe | Salva il livello UV nell'animazione dei vertici attiva. | <kbd>I</kbd> |

## Pittura pesi (UV)

Dipinge i pesi delle ossa 2D sulla mappa UV, con gli stessi controlli della Vista 3D (modalità +/−/=, gruppo, marcatori, falloff, solo la selezione). <kbd>Ctrl</kbd>+clic su un osso lo sceglie per dipingere.

## Modifica ossa e Modalità Posa (armatura 2D)

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Aggiungi Armatura 2D / Osso | Crea l'armatura 2D della mesh (con il suo gruppo UV) o aggiunge un osso. | Menu Aggiungi |
| Seleziona | Un'estremità o l'osso intero; trascinando lo si sposta. | Clic; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>L</kbd> / <kbd>B</kbd> |
| Sposta / Ruota / Scala | In Modifica ossa cambia la posizione di riposo; in Posa, mette in posa (le UV pesate seguono l'osso). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Estrudi / Duplica / Elimina | Solo in Modifica ossa. | <kbd>E</kbd> / <kbd>Shift+D</kbd> / <kbd>X</kbd> |
| Imposta genitore / Rimuovi genitore | Mantieni offset o Connesso / Disconnetti. | <kbd>Ctrl+P</kbd> / <kbd>Alt+P</kbd> |
| Inserisci keyframe | In Posa: menu dei canali, o tutto con <kbd>Shift+I</kbd>. | <kbd>I</kbd> |

## Pittura texture (UV)

Con il pulsante **Modifica** si dipinge direttamente sull'immagine, con colore, falloff, raggio e forza. Si salva dal dischetto.

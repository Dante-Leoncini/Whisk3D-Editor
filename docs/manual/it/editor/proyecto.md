# Il progetto (.w3d) e i file

| Funzione | Descrizione | Dove |
|---|---|---|
| Apri progetto | Apre un .w3d (anche con un doppio clic dal sistema). | Proprietà → File/Render → File |
| Salva / Salva come | Salva nella cartella e con il nome indicati nel riquadro; Salva come ne chiede altri. Se manca il nome, il campo diventa rosso. | Riquadro File, <kbd>Ctrl+S</kbd> |
| Salva versione | Salva e inoltre lascia una copia numerata (`proyecto_v03.w3d`). | Riquadro File |
| Estrai asset | Copia tutto il contenuto interno in una cartella (le copie non cambiano il progetto). | Riquadro File |
| Mesh come testo | Salva le mesh come testo leggibile invece che in binario. | Riquadro File |
| Importazioni | OBJ, FBX, glTF, GLB e interfacce .w3dui. Ciò che viene importato si copia dentro il progetto (senza duplicati). | Vista 3D → Aggiungi → Importazioni |
| Esporta | OBJ, glTF o GLB dell'oggetto selezionato (con o senza modificatori e trasformazioni). | Proprietà → Esporta |
| Modifiche non salvate | Ciò che è stato modificato e non salvato (script, texture dipinte) è segnato con `*`. Quando chiudi o apri un altro progetto, un avviso elenca ciò che andrebbe perso. | Automatico |
| Risorsa dentro o fuori | Una risorsa esterna non viene copiata: si salva il suo percorso e si avvisa se manca. | Riquadro Risorsa; "Salva nel / fuori dal .w3d" nell'IDE e nell'Editor UV |

Il salvataggio non lascia mai il file a metà: prima scrive una copia temporanea e poi la rinomina.

## Nel browser (versione web)

Il browser non permette a una pagina di leggere né di scrivere i file del PC per conto suo. Per questo, nella versione web:

- **Apri progetto**, **Importazioni** e la scelta di una texture aprono l'esplora file del browser: appare un avviso con il pulsante **Scegli file**. Per un OBJ si scelgono insieme il .obj, il suo .mtl e le sue texture; per un glTF, il .gltf e il suo .bin.
- **Salva** (<kbd>Ctrl+S</kbd>), **Salva come**, **Salva versione** ed **Estrai asset** scaricano il file nella cartella dei download del browser. Lo stesso succede con **Esporta** (l'OBJ si scarica con il suo .mtl e le sue texture accanto), il render, la mappa UV, le texture dipinte salvate fuori e gli script esterni.
- Quando l'editor chiede una cartella in cui salvare, non si apre nessun esplora file: il file si scarica direttamente.
- Se il browser chiede se la pagina può scaricare più file, bisogna accettare. Altrimenti si scarica solo il primo.
- Ciò che è aperto vive nella memoria della pagina: ricaricandola si perde tutto ciò che non è stato scaricato.

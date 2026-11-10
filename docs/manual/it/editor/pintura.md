# Modalità di pittura

> [!NOTE]
> Le tre modalità di pittura sono **solo per mesh 3D**: non appaiono con camere, luci, armature, curve né elementi 2D. Mentre si dipinge, il clic non cambia la selezione; per dipingere un altro oggetto lo si sceglie nell'Outliner.

## Cosa hanno in comune

| Funzione | Descrizione | Come si usa |
|---|---|---|
| Dipingere | Dipinge solo le facce frontali visibili (con Raggi X, anche quelle dietro). Ogni tratto è un passo di annullamento. | Clic e trascinare |
| Raggio | La dimensione del pennello in pixel. Quando la cambi appare un cerchio al centro della vista con la dimensione reale. | Barra trascinabile sopra la barra degli strumenti |
| Valore | L'intensità del pennello. | Barra trascinabile |
| Decadimento | Come si ammorbidisce il bordo del pennello; si può disegnare una curva personalizzata. | Pulsante del decadimento |
| Vista (touch) | Acceso: il dito naviga. Spento: il dito dipinge. | Pulsante a forma di monitor |

## Pittura pesi

Dipinge quanto un osso influisce su ogni vertice (blu = niente, rosso = tutto). È ciò che fa deformare bene la mesh con l'armatura.

| Funzione | Descrizione |
|---|---|
| Modalità del pennello | Somma (+), Sottrai (−) o Uguaglia (=, imposta il valore esatto). |
| Gruppo di vertici | Quale gruppo (osso) si dipinge; si possono aggiungere gruppi. Se non ce n'è nessuno, il primo tratto ne crea uno. |
| Marcatori | Mostra un quadratino per ogni vertice; il pennello agisce solo su di essi. |
| Solo la selezione | Protegge ciò che non è selezionato (appare scurito). |

## Pittura vertici

Dipinge colori sui vertici di una **mesh 3D** (non serve per immagini né elementi 2D: per quello c'è la pittura di texture). È molto leggera ed è il modo consigliato di dare colore sull'N95.

| Funzione | Descrizione |
|---|---|
| Colore | Apre il selettore di colore (RGB, HSV, esadecimale o un indice della tavolozza). |
| Per vertice / Per angolo | Per vertice, il colore è condiviso tra le facce; per angolo, ogni faccia può avere il proprio (bordi netti). |
| Livello colore | Scegliere o creare livelli di colore. |
| Marcatori / Solo la selezione | Come nella pittura dei pesi. |

## Pittura texture

Dipinge direttamente sull'oggetto 3D e il tratto finisce nella sua texture.

| Funzione | Descrizione |
|---|---|
| Texture automatica | Se la mesh non ha una texture, al primo tratto se ne crea una da sola: un materiale proprio, UV automatiche e un'immagine di 1024×1024 (256×256 sull'N95) del colore del materiale. |
| Colore, decadimento, raggio, valore | Come nelle altre modalità. |
| Salva texture | La pittura resta in memoria finché non la salvi. Il dischetto diventa verde se ci sono modifiche non salvate. Opzioni: Salva, Salva come…, Salva nel .w3d e Salva fuori dal .w3d… |

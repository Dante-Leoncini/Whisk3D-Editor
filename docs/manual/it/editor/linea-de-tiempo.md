# Timeline

Mostra e modifica l'animazione: foglio dei keyframe (dope sheet) o editor delle curve. Mostra le animazioni di ciò che è **selezionato**.

## Barra superiore

| Funzione | Descrizione |
|---|---|
| Trasporto | Vai all'inizio, keyframe precedente, frame precedente, riproduci all'indietro, riproduci / pausa, frame successivo, keyframe successivo, vai alla fine. |
| Start / End / FPS | Intervallo e velocità dell'animazione (clic per digitare). |
| Animation | Quale animazione si modifica (scena, clip, armature, oggetti, flipbook, Game, Mix) e crearne di nuove. |
| Auto Key | In rosso: ogni trasformazione registra keyframe da sola. |
| Curves / Dope Sheet | Passa dal foglio dei keyframe all'editor delle curve e viceversa. |
| Select, Key, View, Pivot | Menu di selezione, operazioni sui keyframe, inquadratura e pannello laterale, e da dove scala la <kbd>S</kbd> (centro o frame attuale). |

## Navigare nel tempo

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Cambiare frame | Clic o trascinamento sui numeri dei frame (scrub). Il frame attuale (verde) si può digitare. | Clic |
| Previous / Next frame | Un frame indietro o avanti. | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Previous / Next keyframe | Salta al keyframe più vicino. | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Go to start / end | Primo o ultimo frame. | <kbd>Home</kbd> / <kbd>End</kbd> |
| Play / Pause | Da qualsiasi pannello. | <kbd>Space</kbd> |
| Zoom e spostare la vista | Rotella = zoom (nelle curve, <kbd>Ctrl</kbd> = solo tempo, <kbd>Shift</kbd> = solo valore); tasto centrale = spostare la vista. | Rotella / tasto centrale |
| Frame Selected | Adatta la vista a ciò che è selezionato. | <kbd>Num.</kbd> |

## Modificare i keyframe (dope sheet e curve)

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Select | Clic su un rombo, o su una riga per prendere tutti i suoi keyframe; <kbd>Shift</kbd> aggiunge. | Clic; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Spostare i keyframe | Sposta i keyframe nel tempo (anche trascinando un rombo). | <kbd>G</kbd> |
| Scalare i keyframe | Allunga o comprime a partire dal pivot. | <kbd>S</kbd> |
| Rotate handles | Solo nelle curve. | <kbd>R</kbd> |
| Duplicate | Copia i keyframe e li lascia agganciati al puntatore. | <kbd>Shift+D</kbd> |
| Delete | Elimina i keyframe (o le curve delle righe) selezionati. | <kbd>X</kbd> |
| Interpolation | Constant, Linear o Bezier. | <kbd>T</kbd> |
| Handle Type | Free, Aligned, Vector, Automatic, Auto Clamped (solo curve). | <kbd>V</kbd> |
| Smart Euler | Elimina i giri di troppo nelle rotazioni selezionate. | Menu Key |
| Nascondere una curva | L'occhio di ogni riga la nasconde. | Clic sull'occhio |
| Valori esatti | Frame, valore e maniglie del keyframe scelto. | Properties → Animation → Keyframe |

## Suoni

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Striscia del suono | Ogni oggetto Sound selezionato ha la sua striscia: dove inizia e quanto dura, in **secondi** (cambiare gli FPS non la sposta). Con il loop si ripete fino alla fine. | — |
| Spostare | Trascinare la striscia, o <kbd>G</kbd> senza keyframe scelti (sposta tutti i suoni selezionati). <kbd>Ctrl</kbd> aggancia ai frame. | Trascinare / <kbd>G</kbd> |
| Tagliare | Afferrare un estremo della striscia. | Trascinare un estremo |
| Forma d'onda / Linea del volume | Menu View: la forma d'onda e la curva del volume dentro la striscia (la striscia occupa tre righe). Senza, è solo una striscia. | Menu View |

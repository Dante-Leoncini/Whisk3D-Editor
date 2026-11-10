# Cambiare l'origine: una porta

In questo tutorial farai in modo che una porta si apra dal cardine. Per farlo bisogna cambiare la sua **origine**: il punto attorno a cui gira l'oggetto (vedi [L'origine](modo-objeto.md#lorigine) nel tutorial sulla Modalità Oggetto).

Partiamo dalla porta del tutorial precedente, [Modellare una porta](modelar-puerta.md), con l'origine al centro. Se l'hai fatta spostando un lato, l'origine è rimasta spostata: **Oggetto → Imposta origine → Origine alla geometria** la riporta al centro.

## La porta gira male

Prova a ruotarla. La porta gira sul suo centro, come una porta girevole, e non dal cardine:

![PC](img/icono-pc.svg) Con il gizmo: un clic sul suo centro lo passa a **Ruota**, e l'anello blu la fa girare sull'asse verticale.

![Un clic sul centro del gizmo lo passa a Ruota; si trascina l'anello blu e la porta gira sul suo centro, come una porta girevole, e torna indietro](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) Con il dito: un tocco sul centro del gizmo lo passa a **Ruota**, e si trascina l'anello blu.

![Il dito tocca il centro del gizmo e lo passa a Ruota; poi trascina l'anello blu e la porta gira sul suo centro, e torna indietro](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) Sul Nokia N95 il gizmo è spento: <kbd>2</kbd> (ruotare), <kbd>3</kbd> (l'asse Z, quello verticale) e le frecce; <kbd>OK</kbd> conferma.

![Un Nokia N95: 2 e 3 iniziano a ruotare sull'asse verticale; la freccia destra fa girare la porta sul suo centro, la sinistra la riporta indietro e OK conferma](img/puerta-gira-mal-nokia.webp)

Gira male perché la sua origine è al centro. Portiamola sul cardine.

## Portare l'origine sul cardine

1. **Entra in Modalità Modifica.** Un clic sul pulsante della modalità e un altro su **Modalità Modifica** (sul PC, anche con <kbd>Tab</kbd>).

   ![Il pulsante della modalità aperto, con Modalità Modifica evidenziata](img/puerta-paso-1.webp)

2. **Scegli le facce.** In Modalità Modifica compare il pulsante **Vertice / Spigolo / Faccia**: un clic e scegli **Faccia**.

   ![Il pulsante Vertice, Spigolo, Faccia aperto, con Faccia evidenziata](img/puerta-paso-2.webp)

3. **Seleziona la faccia del cardine.** Un clic sulla faccia stretta sul fianco della porta, dal lato dove va il cardine: diventa verde.

   ![La faccia stretta sul fianco della porta, selezionata in verde e segnata con un cerchio](img/puerta-paso-3.webp)

4. **Porta lì il cursore 3D.** Nel menu **Mesh**, **Aggancio** → **Cursore al selezionato**: il cursore 3D si posiziona sul cardine.

   ![Il menu Mesh aperto, con il sottomenu Aggancio e Cursore al selezionato evidenziato](img/puerta-paso-4.webp)

5. **Torna in Modalità Oggetto.** Un clic sul pulsante della modalità e un altro su **Modalità Oggetto** (o <kbd>Tab</kbd>).

   ![Il pulsante della modalità aperto, con Modalità Oggetto evidenziata; il cursore 3D è già sul cardine](img/puerta-paso-5.webp)

6. **Porta l'origine sul cursore 3D.** Nel menu **Oggetto**, **Imposta origine** → **Origine al cursore 3D**. La porta non si muove: a muoversi è la sua origine, e con lei il gizmo, che resta sul cardine.

   ![Il menu Oggetto aperto, con il sottomenu Imposta origine e Origine al cursore 3D evidenziato](img/puerta-paso-6.webp)

## Ora gira bene

Ruotala di nuovo con il gizmo: ora si apre e si chiude dal cardine.

![Con l'origine sul cardine, l'anello blu del gizmo apre la porta finché non resta di lato e la richiude](img/puerta-gira-bien-raton.webp)

![Con il dito, l'anello blu apre la porta dal cardine e la richiude](img/puerta-gira-bien-dedo.webp)

![Un Nokia N95: 2, 3 e la freccia destra aprono la porta dal cardine; la sinistra la chiude e OK conferma](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) Su uno schermo touch, i passi qui sopra sono uguali, con i tocchi: i menu si aprono e le opzioni si scelgono con un tocco.

![Nokia](img/icono-nokia.svg) Sul Nokia N95, <kbd>OK</kbd> passa dalla Modalità Oggetto alla Modalità Modifica e i menu si aprono con il tasto di selezione sinistro.

## Le opzioni di Imposta origine

| Opzione | Cosa fa |
|---|---|
| **Origine alla geometria** | Porta l'origine al centro della mesh. |
| **Origine al cursore 3D** | Porta l'origine dove si trova il cursore 3D. |
| **Geometria all'origine** | Al contrario: lascia l'origine dov'è e sposta la mesh finché il suo centro non coincide con essa. |

Nel prossimo tutorial le facciamo la maniglia: [Modellare la maniglia](modelar-picaporte.md).

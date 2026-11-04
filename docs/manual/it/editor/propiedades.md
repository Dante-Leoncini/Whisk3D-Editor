# Properties

Il pannello con tutti i dati dell'oggetto attivo, organizzato in schede (icone). Le schede che appaiono dipendono dal tipo di oggetto. Un clic sul titolo di un riquadro lo comprime. Nei campi numerici: clic per digitare (accetta calcoli), trascinare per far scorrere il valore. Il rombo a destra inserisce o rimuove un keyframe.

| Scheda | Cosa contiene |
|---|---|
| File / Render (dischetto) | **File**: aprire, salvare, salvare con nome, salvare una versione, estrarre gli asset. **Render**: percorso, dimensione, passate extra, sfondo, **Ray Tracing** (raggi, campioni, passate), Render Image e Render Animation. **Game**: piattaforma (Windows, Symbian, Linux .deb, AppImage, WebGL, Android), finestra, orientamento, icona, fisica, suono e Build game. **Export**: OBJ, glTF, GLB. **Settings**: lingua, antialiasing, grafica, skin, log, scala, **Keyboard Shortcuts…** e Save Changes. |
| Object | Posizione, rotazione (Eulero, quaternione o asse-angolo), scala, nome, visibile, renderizzabile; animazioni e tavolozza dell'oggetto. |
| Contextual | Dipende dal tipo: parti della mesh e **materiale** (texture, illuminazione, colore dei vertici, trasparenza, riflessi, normal map, fusione, profondità, livelli di texture, **Advanced: custom shader** — vedi [Shader scritti](shaders.md); Roughness, Metallic, Transmission e IOR con il ray tracing), luce, camera, nebbia, particelle, fisica, hitbox, armatura e ossa, elementi 2D, prefab, routine. |
| 3D Mesh | La mesh usata (e quanti la condividono), mappe UV, livelli di colore, gruppi di vertici, gruppi UV. Con una risorsa scelta in Library: i suoi dati e la sua posizione (dentro o fuori dal .w3d). |
| Modifiers | Vedi [Modificatori](modificadores.md). |
| Transform | Solo in Edit Mode: centro della selezione e delle sue UV, modificabili. |
| Armature 2D | Armature e ossa 2D della mesh. |
| Constraints | Copy Location, Copy Rotation, Billboard (guardare verso la camera), Child Of. |
| Scripts | Gli script Lua dell'oggetto e le loro proprietà (vedi il [documento su Lua](../lua/README.md)). |
| Animation | L'animazione attiva, livelli di mix (Mix), valori esatti del keyframe scelto e tagli di camera. |

## Routine

Una routine è un elenco di passi di disegno (traslare, ruotare, impostare la texture, disegnare una mesh, chiamare un'altra routine, Call Lua…) che si costruisce in Properties. È il modo più veloce di disegnare sull'N95.

| Funzione | Descrizione | Scorciatoia |
|---|---|---|
| Add / Remove (passi) | Aggiunge passi raggruppati per tema, o rimuove quello scelto. | Pulsanti; <kbd>X</kbd> o <kbd>Delete</kbd> rimuove |
| Grab step | Riordina il passo con le frecce; <kbd>Enter</kbd> conferma. | <kbd>G</kbd> |
| Insert Keyframe | Anima i valori del passo nel frame attuale. | <kbd>I</kbd> |
| Repeat | Quante volte di seguito viene eseguito l'elenco (ciclo della routine). 1 = routine normale. | Campo Repeat |
| Render mode / Use | Un elenco diverso per ogni modalità di shading, o lo stesso per tutte. | Menu a tendina |

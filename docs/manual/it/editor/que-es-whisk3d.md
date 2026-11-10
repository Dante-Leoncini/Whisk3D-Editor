# Cos'è Whisk3D

Whisk3D è un **editor 3D e motore di gioco** libero, creato da zero. L'idea è poter creare quello che vuoi con quello che hai: funziona su un PC, su un telefono e anche su console e dispositivi vecchi, senza dipendere da hardware moderno né da software proprietario.

## Cosa si può fare

Con **Whisk3D Editor** si può:

- **Modellare** in 3D: mesh con vertici, spigoli e facce, e [modificatori](objeto-malla.md#la-scheda-modificatori) che lavorano senza distruggere la mesh.
- Fare il **rig**: costruire l'[armatura](esqueletos.md) di un personaggio e dipingere quanto muove ogni osso.
- **Animare**: keyframe, l'editor delle curve e animazioni con armatura ([Timeline](linea-de-tiempo.md)).
- **Illuminare** la scena con luci puntiformi, luci solari e faretti.
- **Renderizzare** immagini e video, in tempo reale o con il **ray tracing** (ombre morbide, riflessi e trasparenze).
- **Dipingere** i colori dei vertici e le texture, direttamente sul modello ([Modalità di pittura](pintura.md)).
- **Creare giochi**: la logica si scrive in [Lua](../lua/README.md), con l'[editor di script](editor-de-scripts.md) integrato; le interfacce (menu, HUD) si costruiscono nell'[Editor 2D](editor-2d.md), e il gioco si compila dallo stesso editor per ogni sistema.
- Importare ed esportare modelli in OBJ, FBX, glTF, GLB e STL.

Whisk3D ha inoltre un suo **formato libero** per salvare i progetti e i giochi: il **`.w3d`**. Un gioco in `.w3d` si può aprire direttamente su qualsiasi sistema su cui funziona Whisk3D, oppure **compilare** per un sistema specifico, per sfruttarne al massimo l'hardware. Vedi l'articolo [W3D: un formato libero perché i videogiochi non muoiano](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=it).

I giochi si possono fare anche **senza l'editor**, programmando direttamente su **Whisk3D Core** in C e C++. Questa strada è solo per utenti esperti, che sanno già programmare in C e C++: vedi il [Manuale di Whisk3D Core](../core/empezar.md).

## Dove funziona

| Sistema | |
|---|---|
| **PC** | Windows, Linux e macOS |
| **Telefoni** | Android (dalla 2.3) e i Nokia N95 e N8 (Symbian) |
| **Browser** | Qualsiasi browser moderno (WebGL) |
| **Console** | Sega Dreamcast, Sony PSP, PlayStation Vita e Nintendo 3DS |

## Com'è fatto

Whisk3D è composto da tre progetti, uno sopra l'altro, come tre ingranaggi che girano insieme:

- **Whisk3D Editor** (l'ingranaggio verde): l'editor, con tutto ciò che è descritto sopra.
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=it)** (quello giallo): si occupa dell'**interfaccia**: i pulsanti, i menu, i riquadri delle proprietà, il testo e le icone. L'editor è fatto con lei. Vedi il [Manuale di Whisk3D UI](../ui/README.md).
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=it)** (quello rosso): lo **strato di astrazione**. Whisk3D UI lo usa per tutto ciò che fa.

Un gioco o un progetto fatto nell'editor vive **dentro** l'editor, con la sua logica scritta in Lua. Ma l'Editor e Whisk3D UI sono **opzionali**: si possono usare strumenti propri. Un gioco in C o C++ può usare Whisk3D UI per la sua interfaccia, senza l'editor, oppure parlare **direttamente con il Core**, con la propria interfaccia e i propri strumenti. L'unica cosa che non può mancare è il Core.

![L'architettura di Whisk3D in tre strati: in alto l'Editor (verde), con dentro il tuo gioco o progetto in Lua; nel mezzo Whisk3D UI (giallo); in basso Whisk3D Core (rosso). L'Editor e la UI sono opzionali: dall'esterno, un gioco in C++ può usare la UI e un altro direttamente il Core. Il Core parla con ogni sistema tramite il proprio backend](img/whisk3d-capas.webp)

Il Core è l'**ingrediente segreto**. Ogni sistema disegna, suona, legge i file e riceve i tasti a modo suo: un PC usa OpenGL, il Dreamcast ha il suo chip grafico, la PSP e il 3DS le loro librerie, il browser WebGL. Né l'editor né i giochi parlano con ogni sistema: chiedono le cose al Core, sempre allo stesso modo ("disegna questa mesh", "riproduci questo suono", "apri questo file"), e il Core sa come farlo su ognuno. Per questo lo **stesso codice** funziona su macchine così diverse, e aggiungere un nuovo sistema significa insegnare al Core a parlarci, non riscrivere l'editor.

Come lo fa, in tutti i dettagli (la grafica, il suono, i file, l'input, la rete, i log): [Manuale di Whisk3D Core](../core/README.md).

## Il progetto

Tutto il lavoro viene salvato in un file di progetto `.w3d`. Al suo interno viaggiano la scena e anche le risorse: texture (`texturas/`), script (`scripts/`), suoni, scene, prefab. Una risorsa può stare **dentro** il .w3d o **fuori**, come file separato sul disco; i menu "Salva nel .w3d" e "Salva fuori dal .w3d" la spostano da una parte all'altra. Vedi [Il progetto](proyecto.md).

> [!NOTE]
> Questo manuale descrive ciò che Whisk3D sa fare oggi. I nomi delle funzioni sono quelli che vedi nell'editor in italiano. I tasti indicati sono quelli predefiniti: puoi cambiarli in [Scorciatoie da tastiera](atajos.md).

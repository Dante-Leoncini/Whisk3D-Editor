# Cos'è Whisk3D UI

**Whisk3D UI** è una libreria C++ per costruire interfacce: pulsanti, schede, menu a tendina, riquadri delle proprietà, testo e icone. Ha un proprio repository, [Whisk3D-UI](https://github.com/Dante-Leoncini/Whisk3D-UI), con licenza MIT. È costruita sopra [Whisk3D Core](../core/README.md), e l'editor di Whisk3D è fatto con essa.

![L'architettura di Whisk3D: l'Editor e Whisk3D UI, opzionali, sopra Whisk3D Core, che parla con ogni sistema tramite il proprio backend](img/whisk3d-capas.webp)

## Dove gira

Sugli stessi sistemi del Core: **Windows**, **Linux**, **macOS**, **Android**, il **browser**, i **Nokia N95** e **N8**, il **Sega Dreamcast**, la **Sony PSP**, la **PS Vita** e il **Nintendo 3DS**. L'editor la compila su tutti.

La UI non ha codice proprio per ciascun sistema. Non chiama OpenGL: disegna tutto con le funzioni di `w3dEngine` ([Grafica](../core/graficos.md)) e il backend di ogni sistema fa il resto. Il poco che cambia riguarda la compilazione:

- Gli header includono `GL/gl.h`, o `GLES/gl.h` su Symbian, per i tipi (`GLfloat`, `GLshort`, `GLubyte`).
- Su Android, `loadColorsW3d` aggiorna la tabella dei colori in byte ma non quella in `float`.
- Il codice è C++03, perché il compilatore di Symbian non conosce il C++11: gli enum stanno dentro uno `struct` (`IconType::camera`) e si usa `W3D_OVERRIDE` al posto di `override`.

## Perché atlas e non vettori

Quasi tutte le librerie di interfaccia disegnano con i vettori. Molti dispositivi vecchi non li accelerano: l'interfaccia si disegna via software e va lenta. Ciò che invece dispositivi come l'N95 accelerano è il 3D: triangoli con texture.

Per questo Whisk3D UI disegna tutto con triangoli che leggono da una texture, un **atlas**. Le lettere, i bordi arrotondati dei riquadri e le icone stanno nella stessa texture: un solo `BindTexture` basta per tutta l'interfaccia. Un testo si disegna con una chiamata per atlas, non con una per lettera.

Il prezzo: le lettere sono fatte di pixel e crescono per multipli interi ([Colori e scala](tema.md)), e ci sono solo le lettere disegnate nell'atlas ([Testo](texto.md)).

## Cosa c'è dentro

| Cartella | Cosa risolve |
|---|---|
| `core/` | Le misure dell'interfaccia e la loro scala (`UI.h`): [Colori e scala](tema.md) |
| `theme/` | La tavolozza dei colori (`colores.h`): [Colori e scala](tema.md) |
| `draw/` | Il disegno 2D (`glesdraw`), i rettangoli, le icone e il packer di atlas: [Disegno e atlas](dibujo.md), [Icone](iconos.md) |
| `text/` | Il font bitmap, l'atlas esteso e il testo con font TTF prerenderizzati: [Testo](texto.md) |
| `widgets/` | I riquadri con bordo, i pulsanti, le schede, i menu e i campi di testo: [Pulsanti, schede e caselle di controllo](botones.md), [Menu a tendina](menus.md), [Input e campi di testo](entrada.md) |
| `Propieties/` | I riquadri delle proprietà e le loro righe: [Riquadri e proprietà](propiedades.md) |

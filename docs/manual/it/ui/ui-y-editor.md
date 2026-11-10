# Cosa è della UI e cosa dell'editor

L'editor è fatto con Whisk3D UI, ma molto di ciò che vi si vede è suo. Ciò che è della UI sta in `libs/WhiskUI/`; ciò che è dell'editor, in `main/` del repository di Whisk3D.

## Della UI

- Il disegno 2D, i riquadri con bordo e i rettangoli.
- La tabella delle icone e il packer di atlas.
- Il font bitmap, l'atlas esteso e il testo con font TTF prerenderizzati.
- La tavolozza dei colori e le misure, con la loro scala.
- I pulsanti, le schede, le caselle di controllo e i menu a tendina.
- I riquadri delle proprietà e le loro righe, con il rombo dell'animazione.
- Lo stato dei campi di testo e il loro disegno.

## Dell'editor

| Cosa | Dove |
|---|---|
| I viewport: dividere lo schermo, i loro bordi, le loro barre dei menu e degli strumenti | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| Il pannello delle proprietà: quali riquadri ci sono, le sue schede, il clic e il trascinamento sulle righe | `main/ui/ViewPorts/Properties.cpp` |
| Le finestre modali: il selettore di colore, l'esploratore di file, le conferme, la barra di avanzamento, le tastiere su schermo | `main/ui/ViewPorts/PopUp/` |
| Le notifiche | `main/ui/ViewPorts/Notificaciones.cpp` |
| Le barre di scorrimento dei pannelli | `main/ui/ViewPorts/ScrollBar.cpp` |
| I tooltip. Il testo lo conserva `Button::ayuda` | `W3dAyudaRender`, in `main/ui/ViewPorts/LayoutInput.cpp` |
| Quale tasto, clic o tocco va a ogni widget, e i tasti dell'N95 | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| Le funzioni di focus dei campi di testo | `main/ui/ViewPorts/NumInput.cpp` |
| Le traduzioni, `T()`. La UI ha solo l'hook `W3dMenuTraducir` | `main/config/W3dLang.cpp` |
| L'avvio della UI | `main/app/W3dInitUI.cpp` |
| Costruire l'atlas della skin con le icone separate | `main/io/SkinAtlas.cpp` |
| Ricalcolare la scala | `SetGlobalScale`, in `main/ui/ViewPorts/ViewPorts.cpp` |
| Leggere `skin.ini` sul PC e su Android | `loadColors`, in `main/app/main.cpp` |
| I colori propri dell'editor: il pavimento, gli assi, le normali | `main/ui/W3dColors.cpp` |
| I file della skin: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| Il generatore dell'atlas esteso | `tools/fuente_ext.py` |
| L'[Editor 2D](../editor/editor-2d.md) e le interfacce `.w3dui` dei giochi: testi, immagini, rettangoli, contenitori, slice 9, pulsanti e video | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

L'Editor 2D disegna il testo con `W3dTextAtlas`, che è della UI. Prerenderizzare un TTF durante l'esecuzione, con stb_truetype, spetta all'editor (`main/io/Fuente2D.cpp`).

## Nella UI, ma dell'editor

Cose che stanno in `libs/WhiskUI/` e sono ancora dell'editor:

- `PropListMeshParts` legge mesh, armature, animazioni, vincoli e script, e chiama `RebindMaterialMeshPart()` del pannello delle proprietà.
- `PropColor` conserva la tavolozza e l'oggetto da cui viene il colore (`palRef`, `palObj`), per il selettore di colore.
- `GroupProperties` è l'elenco globale dei riquadri del pannello delle proprietà.
- `core/UI.h` contiene stato dell'editor: il modo di visualizzazione touch della Modalità Modifica (`g_viewEditMode`), il movimento del mouse (`dx`, `dy`), i tasti Alt, Shift e Ctrl (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) e le misure dei viewport (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- La maggior parte delle icone sono strumenti dell'editor.

Ciò che resta da separare, e lo stato della libreria: la fine di [Usare la UI direttamente](empezar.md).

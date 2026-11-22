# Input e campi di testo

## La UI non legge gli eventi

Whisk3D UI non apre finestre né ascolta il mouse, la tastiera o lo schermo touch. Gli eventi li riceve il programma, su quasi tutti i sistemi tramite SDL2 ([Input](../core/entrada.md) del Core), e passa a ogni widget ciò che gli spetta:

| Evento | A chi va |
|---|---|
| Si muove il mouse o il dito | `PopupMenu::MouseMove(x, y)`; l'`hover` di pulsanti e schede; `PropHoverGroup` e `PropHoverRow` nei riquadri |
| Clic o tocco | `PopupMenu::Click(x, y)`; `Button::Contains` e `Tab::Contains`; in un riquadro, la riga calcolata dal programma |
| Rotella | `PopupMenu::Wheel(dir)` |
| Frecce | `button_up`, `button_down`, `button_left` e `button_right` di menu, riquadri e righe |
| OK o Enter | `PopupMenu::Enter()`, `GroupPropertie::EnterPropertieSelect()` |
| Annulla o Esc | `PopupMenu::Close()`, `GroupPropertie::Cancel()` |
| Una lettera | Il campo di testo che ha il focus |

Le coordinate sono pixel dello schermo, con la Y verso il basso.

## Senza mouse

Tutto si può usare con quattro frecce, OK e Annulla: è così che si usa sull'N95. Ciò che la UI mette a disposizione per questo:

- I riquadri si percorrono riga per riga e saltano ciò che non si può scegliere.
- I menu si percorrono con le frecce, e l'opzione evidenziata non esce mai dalla vista.
- I numeri si cambiano con le frecce, senza digitarli.
- Il focus della tastiera si vede: `focusMenu` nei pulsanti e `focus` nelle schede, in `accent`, e il bordo bianco di `g_kfFocusProp` su un rombo.

Quale tasto del telefono corrisponda a ciascuno, i soft key o l'OK, non riguarda la UI: lo traducono l'editor e ogni sistema.

## Schermo touch

- Un tocco è un clic.
- Il tocco che apre un sottomenu non sceglie niente al suo interno ([Menu a tendina](menus.md)).
- `UITouch` ingrandisce i pulsanti e le barre per il dito ([Colori e scala](tema.md)). Oggi niente la attiva.

Trascinare per scorrere, tenere premuto o toccare due volte lo gestisce chi usa la UI.

## Campi di testo

`TextField` (`widgets/TextField.h`) è lo stato di un campo di testo: il testo (`text`), la posizione del cursore (`caret`) e se è tutto selezionato (`selectAll`). Non disegna né legge tasti: è solo il dato.

| Funzione | Cosa fa |
|---|---|
| `SetText(t)` | Imposta il testo, con il cursore alla fine |
| `SelectAll()` | Seleziona tutto: il primo tasto lo sostituisce |
| `InsertChar(c)` | Scrive un byte dove si trova il cursore |
| `Backspace()`, `DelForward()` | Cancellano all'indietro o in avanti |
| `CaretLeft()`, `CaretRight()` | Spostano il cursore. Con tutto selezionato, tolgono la selezione |

Cancellare e spostare il cursore trattano come una sola una lettera di più byte, come una `ñ` o una lettera cirillica. Per scriverne una, si passa ogni byte a `InsertChar`.

C'è un solo campo con il focus alla volta: `g_activeTextField`, o `NULL` se non ce n'è nessuno. I tasti vanno a quello.

```cpp
TextField name;
name.SetText("Jugador 1");
name.SelectAll();
g_activeTextField = &name;

// con cada letra
if (g_activeTextField) g_activeTextField->InsertChar(c);

// al dibujar, con el color del texto puesto
RenderTextField(name, g_activeTextField == &name, textAlign::left, 200);
```

`RenderTextField(box, focus, alineación, width)` lo disegna nell'origine attuale:

- Senza focus, è solo il testo.
- Con il focus e tutto selezionato, il testo è bianco su `accentDark`.
- Con il focus e senza selezione, appare il cursore: una lineetta che lampeggia ogni 530 ms.

`TextFieldBlinkTick()` restituisce `true` quando la lineetta cambia: è il momento di ridisegnare.

`TextField.h` dichiara anche `TextFieldFocus`, `TextFieldCancel` (Esc: torna al testo che c'era prima di dare il focus), `TextFieldInputChar` e `TextFieldFrameEnd`. Oggi sono definite nell'editor, in `main/ui/ViewPorts/NumInput.cpp`: la UI da sola non le include.

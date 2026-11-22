# Pulsanti, schede e caselle di controllo

## Pulsante

`Button` (`widgets/Button.h`) è un riquadro con un testo e un'icona opzionale a sinistra. Il bordo scuro è ciò che lo fa sembrare un pulsante. Sotto il mouse (`hover`) il bordo diventa bianco e lo sfondo si schiarisce un po'.

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // el ancho sigue al texto
guardar->Resize(200);                                         // nunca más ancho que esto

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // dónde quedó, para el mouse
guardar->hover = guardar->Contains(mx, my);
```

`Render` disegna nell'origine attuale e non sa dove è finito sullo schermo: `sx` e `sy` li imposta chi lo disegna, e `Contains` fa il confronto con questi. Il terzo argomento del costruttore è `adapt`: con `false`, il pulsante occupa tutta la larghezza che gli dà `Resize`.

| Campo | Cosa fa |
|---|---|
| `centered` | Icona e testo al centro, come in Accetta o Annulla |
| `square` | La larghezza uguale all'altezza: pulsanti con la sola icona |
| `rowHeight` | L'altezza di una riga di proprietà, invece di quella di una barra |
| `caretMenu` | Una piccola freccia a destra: indica che apre un menu |
| `tint` | Il colore dello sfondo, invece di `gray` |
| `textColor`, `borderColor` | Il colore del testo e dell'icona, e quello del bordo |
| `focusMenu` | Con il focus della tastiera: bordo e testo in `accent` |
| `alpha` | L'opacità dello sfondo e del bordo, per far vedere ciò che c'è dietro |
| `iconFlip` | L'icona specchiata: 1 orizzontale, 2 verticale |
| `bar` | Un numero da 0 a 1: il pulsante si riempie fino a quel punto, in `accentDark`, con il testo al centro |
| `fill` | Un quadrato di quel colore all'interno: per scegliere un colore |
| `editField` | Un `TextField`: il pulsante si disegna come campo di testo, per rinominare sul posto |
| `visible` | Con `false` non si disegna |

Ci sono campi che il pulsante conserva ma non disegna: li usa chi lo contiene. `dropdown` dice che apre un menu. `separator` chiede una linea prima, in una barra. `role` serve a trovarlo in una barra senza dipendere dall'ordine. `help` è il testo del tooltip che appare quando si lascia il mouse sopra; il tooltip non è della UI.

L'altezza è `UIButtonHeight()` ([Colori e scala](tema.md)), o `RowHeightGS + 2 * GlobalScale` con `rowHeight`. Non è mai più stretto di una riga di testo.

## Scheda

`Tab` (`widgets/Tab.h`) è un pulsante senza il bordo inferiore: si fonde con ciò che ha sotto. Quella attiva (`active`) è un po' più alta e del colore del contenuto; le altre, più scure. Il testo è in `accent` se ha il focus della tastiera (`focus`), in bianco se è quella attiva o è sotto il mouse, e in grigio altrimenti.

Si usa come il pulsante: `Resize(maxW)`, `Render()` e `Contains(mx, my)`, con `sx` e `sy` impostati da chi la disegna.

## Caselle di controllo e campioni di colore

L'aspetto di una casella di controllo lo decide un solo punto, `widgets/card.h`, così quella di un menu e quella di un riquadro non possono essere diverse.

| Funzione | Cosa fa |
|---|---|
| `DrawCheckbox(side, on)` | Disegna la casella e, se è attiva, il segno di spunta |
| `DrawSampleColor(side, rgba)` | Disegna un quadrato di quel colore, con gli angoli della casella |
| `CheckboxSide()` | Il lato: l'altezza di una riga di testo |
| `CheckboxBoxColor(on)`, `CheckboxCheckColor()` | I colori della casella e del segno di spunta |
| `CheckboxCheck()` | Il carattere del segno di spunta, `✔` |

Quando è attiva, la casella è `grayUI` e il segno di spunta ha il colore dello sfondo. Quando è disattivata, la casella ha il colore dello sfondo e non ha il segno di spunta.

```cpp
DrawCheckbox(CheckboxSide(), onSound);
```

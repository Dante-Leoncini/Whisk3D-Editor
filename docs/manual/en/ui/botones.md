# Buttons, tabs and checkboxes

## Button

`Button` (`widgets/Button.h`) is a card with text and an optional icon on the left. The dark border is what makes it look like a button. Under the mouse (`hover`) the border turns white and the background gets slightly lighter.

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // the width follows the text
guardar->Resize(200);                                         // never wider than this

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // where it ended up, for the mouse
guardar->hover = guardar->Contains(mx, my);
```

`Render` draws at the current origin and doesn't know where it ended up on screen: `sx` and `sy` are set by whoever draws it, and `Contains` compares against them. The constructor's third argument is `adaptar`: with `false`, the button takes up the whole width that `Resize` gives it.

| Field | What it does |
|---|---|
| `centrado` | Icon and text in the middle, as in Accept or Cancel |
| `cuadrado` | Width equal to height: icon-only buttons |
| `altoRenglon` | The height of a property row, instead of a bar's |
| `caretMenu` | A small arrow on the right: shows that it opens a menu |
| `tinte` | The background color, instead of `gris` |
| `colorTexto`, `colorBorde` | The color of the text and icon, and that of the border |
| `focoMenu` | Focused with the keyboard: border and text in `accent` |
| `alpha` | The opacity of the background and border, so what's behind shows through |
| `iconFlip` | The icon mirrored: 1 horizontal, 2 vertical |
| `barra` | A number from 0 to 1: the button fills up to there, in `accentDark`, with the text in the middle |
| `relleno` | A square of that color inside: for choosing a color |
| `editField` | A `TextField`: the button is drawn as a text box, for renaming in place |
| `visible` | With `false` it isn't drawn |

Some fields are stored by the button but not drawn: whoever contains it uses them. `desplegable` says it opens a menu. `separador` asks for a line before it, in a bar. `rol` is used to find it in a bar without depending on the order. `ayuda` is the text of the tooltip that appears when you leave the mouse over it; the tooltip isn't part of the UI.

The height is `UIBotonAltura()` ([Colors and scale](tema.md)), or `RenglonHeightGS + 2 * GlobalScale` with `altoRenglon`. It's never narrower than one line.

## Tab

`Tab` (`widgets/Tab.h`) is a button without the bottom border: it blends into what's below it. The active one (`activa`) is a bit taller and the color of the content; the others, darker. The text is `accent` if it has keyboard focus (`foco`), white if it's the active one or under the mouse, and gray otherwise.

It's used like the button: `Resize(maxW)`, `Render()` and `Contains(mx, my)`, with `sx` and `sy` set by whoever draws it.

## Checkboxes and color swatches

How a checkbox looks is decided in a single place, `widgets/card.h`, so the one in a menu and the one in a card can't be different.

| Function | What it does |
|---|---|
| `DibujarCheckbox(lado, prendida)` | Draws the box and, if it's checked, the check mark |
| `DibujarMuestraColor(lado, rgba)` | Draws a square of that color, with the checkbox's corners |
| `CheckboxLado()` | The side: the height of one line |
| `CheckboxColorCaja(prendida)`, `CheckboxColorTilde()` | The colors of the box and of the check mark |
| `CheckboxTilde()` | The check mark character, `✔` |

Checked, the box is `grisUI` and the check mark is the background color. Unchecked, the box is the background color and has no check mark.

```cpp
DibujarCheckbox(CheckboxLado(), sonidoPrendido);
```

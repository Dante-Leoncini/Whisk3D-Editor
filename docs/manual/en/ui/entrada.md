# Input and text boxes

## The UI doesn't read events

Whisk3D UI doesn't open windows or listen to the mouse, the keyboard or the touch screen. The program receives the events, on almost every system through SDL2 (the Core's [Input](../core/entrada.md)), and passes each widget what it needs:

| Event | Where it goes |
|---|---|
| The mouse or finger moves | `PopupMenu::MouseMove(x, y)`; the `hover` of buttons and tabs; `PropHoverGroup` and `PropHoverFila` on cards |
| Click or tap | `PopupMenu::Click(x, y)`; `Button::Contains` and `Tab::Contains`; on a card, the row the program works out |
| Wheel | `PopupMenu::Wheel(dir)` |
| Arrows | `button_up`, `button_down`, `button_left` and `button_right` of menus, cards and rows |
| OK or Enter | `PopupMenu::Enter()`, `GroupPropertie::EnterPropertieSelect()` |
| Cancel or Esc | `PopupMenu::Cerrar()`, `GroupPropertie::Cancel()` |
| A letter | The focused text box |

Coordinates are screen pixels, with Y pointing down.

## Without a mouse

Everything can be used with four arrows, OK and Cancel: that's how it's used on the N95. What the UI provides for that:

- Cards are traversed row by row and skip what can't be selected.
- Menus are traversed with the arrows, and the highlighted option never goes out of view.
- Numbers are changed with the arrows, without typing.
- Keyboard focus is visible: `focoMenu` on buttons and `foco` on tabs, in `accent`, and the white border of `g_kfFocoProp` on a diamond.

Which phone key is which, the soft keys or OK, isn't up to the UI: the editor and each system translate them.

## Touch screen

- A tap is a click.
- The tap that opens a submenu doesn't choose anything inside it ([Drop-down menus](menus.md)).
- `UITactil` makes buttons and bars bigger for a finger ([Colors and scale](tema.md)). Today nothing turns it on.

Dragging to scroll, long-pressing or double-tapping are handled by whoever uses the UI.

## Text boxes

`TextField` (`widgets/TextField.h`) is the state of a text box: the text (`text`), the cursor position (`caret`) and whether everything is selected (`selectAll`). It doesn't draw or read keys: it's just the data.

| Function | What it does |
|---|---|
| `SetText(t)` | Sets the text, with the cursor at the end |
| `SelectAll()` | Selects everything: the first key replaces it |
| `InsertChar(c)` | Writes one byte at the cursor |
| `Backspace()`, `DelForward()` | Delete backward or forward |
| `CaretIzq()`, `CaretDer()` | Move the cursor. With everything selected, they clear the selection |

Deleting and moving the cursor treat a multi-byte letter, like an `ñ` or a Cyrillic one, as a single one. To type one of those, you pass each byte to `InsertChar`.

Only one box is focused at a time: `g_textFieldActivo`, or `NULL` if there's none. Keys go to that one.

```cpp
TextField nombre;
nombre.SetText("Jugador 1");
nombre.SelectAll();
g_textFieldActivo = &nombre;

// with each letter
if (g_textFieldActivo) g_textFieldActivo->InsertChar(c);

// when drawing, with the text color set
RenderTextField(nombre, g_textFieldActivo == &nombre, textAlign::left, 200);
```

`RenderTextField(caja, foco, alineación, ancho)` draws it at the current origin:

- Without focus, it's the text.
- With focus and everything selected, the text is white on `accentDark`.
- With focus and no selection, the cursor appears: a small line that blinks every 530 ms.

`TextFieldParpadeoTick()` returns `true` when the line changes: that's the moment to redraw.

`TextField.h` also declares `TextFieldEnfocar`, `TextFieldCancelar` (Esc: goes back to the text from before focusing), `TextFieldInputChar` and `TextFieldFinDeCuadro`. Today they're defined in the editor, in `main/ui/ViewPorts/NumInput.cpp`: the UI alone doesn't provide them.

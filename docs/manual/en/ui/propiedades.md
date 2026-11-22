# Cards and properties

## The card

`GroupPropertie` (`Propieties/GroupPropertie.h`) is a card with a title and rows. Each row is a property, a class that inherits from `PropertieBase` and points to a variable of yours: the card shows it and changes it right there.

```cpp
GroupPropertie* player = new GroupPropertie("Jugador");
player->icon = IconType::gamepad;        // optional: between the arrow and the title

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
player->properties.push_back(vel);

player->Resize(width, 0);    // the card works out the height
player->Render();            // at the current origin
```

The card owns its rows: deleting it deletes them.

- The title has an arrow to fold it (`open`). With `arrow = false` it doesn't: for a floating window.
- Below there are two columns: the label on the left and the value on the right. `valueWidth` is the share of the width the value takes: 0.5 by default.
- `reserveKeyBtn = true` leaves a column at the far right for the animation diamond.
- Almost every row is one line plus a `gap` tall. A number, a checkbox or a color without a variable (`value` set to `NULL`) takes up no space and isn't drawn.
- `visible = false` hides the whole card.

The selected row has its label in `accent`, the value in white and a white border; while it's being edited, the value and border turn `accent`. The row under the mouse is white: whoever contains the card marks it, with `PropHoverGroup` and `PropHoverRow`.

## Selecting and editing

`selectIndex` is the selected row: `-2` none, `-1` the title, `0` and up a row.

| Function | What it does |
|---|---|
| `NextSelect()`, `PrevSelect()` | Move to the row below or above. They skip spaces and rows without a variable. They return `true` when leaving the card, so you can move on to the next one |
| `EnterPropertieSelect()` | On the title, folds or unfolds the card. On a row, edits it |
| `button_up()`, `button_down()`, `button_left()`, `button_right()` | While editing, they go to the row |
| `Cancel()` | Leaves the row as it was |

`editing` says whether a row is being edited. With that, the arrows change the value or move the selection:

```cpp
if (player->editing) player->button_up();   // changes the value
else player->PrevSelect();                    // the row above
```

Whoever contains the card handles the mouse: the card doesn't know where it ended up on screen. At the top are the border (`borderGS`) and the title (`RowHeightGS + gapGS`). Each row is as tall as its `Resize(width)` returns.

## The rows

| Class | What it is | How it's edited |
|---|---|---|
| `PropFloat` | A number | With the arrows, or typed |
| `PropBool` | A checkbox, flush right | OK checks or unchecks it |
| `PropColor` | A square of the color, flush right | OK enters editing and saves the color, in case it's canceled |
| `PropButton` | A button | OK calls its action |
| `PropButtonRow` | Several buttons in one row | Left and right choose one; OK presses it |
| `PropText` | A text box | OK focuses it for typing |
| `PropListMeshParts` | A scrolling list | Up and down choose |
| `PropLabel` | A text to read, on one or several lines | Can't be selected |
| `PropSeparator` | A line | Can't be selected |
| `PropGap` | A space one `gap` tall | Can't be selected |
| `PropSelectText` | A box with its label, without a value | Does nothing yet |

### Number

```cpp
PropFloat* life = new PropFloat("Vida");
life->value = &lifePlayer;
life->SetRange(0, 100);           // it doesn't go past that
life->integer = true;              // shown and stepped as integers
life->fineStep = 1;               // up and down
life->thickStep = 10;            // left and right
life->onChange = OnChangeLife;   // void AlCambiarVida()
```

| Field | What it does |
|---|---|
| `unit` | A text right after the number, like `°`. It's the constructor's second argument |
| `fineStep`, `thickStep` | What the arrows add: 0.01 and 0.1 by default |
| `integer` | Shows the number rounded, and the arrows start from the rounded value. Without it, it has four decimals |
| `centered` | The number in the middle of its box |
| `arrows` | Draws `<` and `>` on the sides. The container handles clicks on them |
| `options`, `nOptions` | With `integer`: the number is an index and the name of that option is shown, translated with `W3dMenuTranslate` |
| `accelerates` | Left and right start in steps of 1 and, if repeated, go up to 10 |
| `dragStep` | How much it changes per pixel when dragging. The container does the dragging |

`Set(v)` assigns the value within the range and calls `onChange`. The arrows go through it.

To type it:

```cpp
life->StartEditText();   // shows the value, all selected
// keys go to g_activeTextField: see Input and text boxes
NumEditCommit();               // Enter: applies it
NumEditCancel();               // Esc: stays as it was
```

`NumEditActive()` says whether a number is being edited. A text that isn't a number counts as 0.

### Checkbox

`value` points to a `bool`. `onChange` is called every time it changes. The label takes up all the space to its left and, if it doesn't fit, is cut with `..`.

### Color

`value` points to four `float`s (RGBA). The UI draws the square; the color picker isn't part of the UI. While it's being edited, the arrows move only the first component.

### Button

`action` is the function it calls. `gray = true` dims it and it does nothing. `hidden = true` takes up no row.

With `withLabel = true`, the row is a name and a button: the name on the left, as wide as its text, up to 55 % of the row, and the button in all the rest. `labelChars` fixes the name's width, to align several rows. With `button->dropdown` it has a menu's small arrow. Also:

- `AddPrefix(action, help)` puts a square before the button, to show a color.
- `AddExtra(icon, action, help)` puts up to two small icon-only buttons on the right. `slotsExtras` reserves room for them, so they stay aligned across rows. `ExtraInX(x)` says which one is at that x, for the click.

### Row of buttons

```cpp
PropButtonRow* row = new PropButtonRow();
row->Add("Aceptar", Accept);
row->Add("Cancelar", Cancel);
player->properties.push_back(row);
```

The buttons split the width into equal parts. Invisible ones are skipped and the rest share their space. `Fire(i)` presses button `i`: the container calls it when the click lands on that part.

### Text box

```cpp
PropText* name = new PropText("Nombre", "Jugador 1");
// name->field.text holds what was typed
```

`onClick`: instead of editing, it calls that function, for example to open a file browser. `error = true` paints the selected row's border red, until something is typed. `hidden = true` takes up no row. OK focuses it with `TextFieldFocus`, which today is defined in the editor. For typing, see [Input and text boxes](entrada.md).

### Text

`PropLabel("texto")` is one line. `PropLabel("texto", true)` wraps at spaces so it fits in the width; a word that doesn't fit by itself stays whole. `red = true`, for an error. `hidden = true` takes up no row.

### List

`PropListMeshParts` shows `rowsMax` rows, 3 by default, with a small bar if there are more. The selected one always stays in view. `Cancel` sets `selectIndex` back to what it was when editing started.

It isn't yet usable for an arbitrary list: depending on `mode`, it reads a list from the scene (a mesh's parts, its UV maps, its color layers, the modifiers, the vertex groups, the bones, the animations, the constraints, the scripts) or asks for it from hooks to editor functions (`MixRow*`, `RoutineRow*`, `PostRow*`).

## The animation diamond

An animatable row (`animProp` of 0 or more, in `PropFloat`, `PropBool`, `PropColor` and `PropButton`) shows a diamond on the right if the card has `reserveKeyBtn`. Its color shows the state:

| State | Color | What it means |
|---|---|---|
| 0 | Gray | No animation |
| 1 | `accent`, dimmed | Animated, with no keyframe here |
| 2 | `accent` | There's a keyframe here |
| 3 | Green | The value changed and wasn't saved |

The diamond belongs to the UI; its state doesn't. It asks a hook installed by the program, and the click calls the other one:

```cpp
W3dKeyframeState = MiState;   // int MiEstado(int prop, int comp): from 0 to 3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp): sets or removes the keyframe
```

Without hooks, the diamond stays gray. The card doesn't call `W3dKeyframeToggle`: whoever handles the click calls it. `g_kfFocusProp` points to the row whose diamond has keyboard focus, and marks it with a white border. `W3dRenderDiamondKey(state)` draws a standalone diamond.

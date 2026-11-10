# Colors and scale

## The palette

The UI's colors are roles, not fixed colors: the `ColorID` enum (`theme/colores.h`).

| Role | What it's used for |
|---|---|
| `background` | The background. The box of an unchecked checkbox and the border of a button at rest |
| `blanco` | The text and the border of whatever is under the mouse or selected |
| `accent` | Focus: the border of menus, what's being edited, the active option, the fill of a slider |
| `accentDark` | The background of selected text and the bar of a numeric button |
| `negro` | The `PropSeparator` line |
| `gris` | The body of cards, buttons and menus |
| `headerColor` | The highlighted row of a menu and the track of a slider |
| `negroTransparente` | Black at 25 %. The UI doesn't use it |
| `grisUI` | Normal text and the box of a checked checkbox |
| `grisLinea` | The lines that separate groups in menus |

`ListaColores` holds them as `float` and `ListaColoresUbyte` as bytes. `SetColorID` sets one as the drawing color, with an optional alpha:

```cpp
SetColorID(ColorID::accent);         // opaque
SetColorID(ColorID::grisUI, 0.5f);   // at half opacity
```

`loadColorsW3d(ruta)` reads them from a text file, one color per line: the name and the four components from 0 to 1, like `accent 0.0 1.0 0.0 1.0`. It ignores names it doesn't know, so the same `skin.ini` can carry another program's colors. It returns `false` if it couldn't open the file. Without the file, the colors in the code stay (`theme/colores.cpp`); the Whisk3D skin changes them, and for example `accent` becomes green.

Some colors are written in the code and don't come from the palette: the green of the diamond for an unsaved change, the red of an error `PropLabel`, the red border of an incomplete `PropText` and the gray of a disabled `PropButton`.

## The scale

The base measurements are in `core/UI.h`, in pixels:

| Measurement | Value |
|---|---|
| `margin`, `padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`, `CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` multiplies them, always by an integer: that way the pixel letters stay crisp. For example, 1 on the N95's 240-pixel screen and 3 on a PC.

Each measurement has its already-multiplied version, with `GS` at the end: `RenglonHeightGS`, `gapGS`, `borderGS`, `bordersGS` (twice the border), `CharacterWidthGS`, `LetterHeightGS`... The widgets use those.

The UI doesn't include the function that recalculates them: the editor's is `SetGlobalScale`, in `main/ui/ViewPorts/ViewPorts.cpp`. On its own, it's done like this:

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

Each widget's sizes are calculated in its `Resize`: after changing the scale, you have to call it again on every card, button and menu.

## Compact or touch

`UIBotonAltura()` gives the height of a button and `UIBarPadding()` the space above and below a button in a bar. Both depend on `UITactil`:

- `false`, compact mode, designed for the N95: the button is one line tall plus the borders, minus `2 * GlobalScale`, and the bar leaves `GlobalScale` above and below.
- `true`: the button and the bar add a `gap`, so a finger fits. The borders don't get thicker.

`UITactil` starts as `false` and today nothing turns it on.

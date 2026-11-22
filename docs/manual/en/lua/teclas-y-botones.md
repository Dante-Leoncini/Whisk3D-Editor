# Key and button names

## For `key` and `keyPressed`

| Where | Names |
|---|---|
| PC, Android and Web | `"a"` … `"z"` (lowercase), `"0"` … `"9"`, `"up"`, `"down"`, `"left"`, `"right"`, `"space"`, `"enter"`, `"shift"`, `"ctrl"` |
| Nokia N95 (phone keypad) | `"up"`, `"down"`, `"left"`, `"right"`, `"enter"` (the center of the arrow keys also sends `"space"`), `"softleft"`, `"softright"`, `"0"` … `"9"`, `"*"`, `"#"` |

> [!NOTE]
> There's no name for Esc: in the editor, Esc pauses the game and releases the mouse. There are no F keys or Tab either.

## For `button` and `buttonPressed` (gamepad)

`"a"`, `"b"`, `"x"`, `"y"`, `"up"`, `"down"`, `"left"`, `"right"` (the D-pad). The D-pad arrows also move `stick("left")` when the stick is at rest.

Also: `"l1"`, `"r1"` (the shoulder buttons), `"l2"`, `"r2"` (the triggers as buttons: pressed more than halfway; for the analog value there's `trigger`), `"l3"`, `"r3"` (the sticks pressed in), `"start"` and `"select"`. The names are the ones on a PlayStation controller; on an Xbox one they're LB / RB, LT / RT and Back. The letters follow the button's position, not the symbol on it: `"a"` is the bottom one (the cross ×), `"b"` the right one (the circle ○), `"x"` the left one (the square □) and `"y"` the top one (the triangle △).

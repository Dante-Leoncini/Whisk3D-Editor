# Move, rotate and scale

It works the same in Object Mode, Edit Mode, Pose and in bone editing.

| Function | Description | Shortcut |
|---|---|---|
| Move (translate) | The selection follows the pointer. | <kbd>G</kbd> |
| Rotate | Turns around the pivot. Pressing <kbd>R</kbd> again toggles between free rotation (trackball) and orbital. | <kbd>R</kbd> |
| Scale | Enlarges or shrinks from the pivot. | <kbd>S</kbd> |
| Constrain to an axis | During the transform. Pressing again cycles Global → Local → View → free. | <kbd>X</kbd> / <kbd>Y</kbd> / <kbd>Z</kbd> |
| Constrain to a plane | Excludes that axis (moves along the other two). | <kbd>Shift+X</kbd> / <kbd>Shift+Y</kbd> / <kbd>Shift+Z</kbd> |
| Exact value | Type a number or an expression (`2*3`, `(1+0.5)/2`). <kbd>-</kbd> flips the sign, <kbd>Backspace</kbd> deletes. | Numbers |
| Confirm | Accepts the operation. | Left click, <kbd>Enter</kbd>, green check mark |
| Cancel | Everything goes back to how it was. | <kbd>Esc</kbd>, right click, red cross |
| Fine steps | Moves, rotates or scales a little at a time. | Arrows |

While it lasts, the top bar shows what is being done, for example "Move X 1.5 along global X". On touch screens, tapping that bar opens a numeric keypad.

## The gizmo

The gizmo appears over the pivot in Object Mode and in Edit Mode (it's turned on and off in Overlays → Gizmo).

| Function | Description |
|---|---|
| Move gizmo | Arrows = one axis; squares = one plane; central circle = free. |
| Rotate gizmo | A colored semicircle per axis, a white ring to rotate facing the view, and the inside to rotate freely. |
| Scale gizmo | Square tips per axis; the central circle scales uniformly. |
| Switch gizmo | A quick click (or tap) on the gizmo, without dragging, goes from Move to Rotate, to Scale and back to Move. |

## Adjust the last operation

After moving, rotating or scaling (in Object or Edit Mode) a panel appears at the bottom left with what was done: the X, Y, Z values (or the angle), the orientation and proportional editing with its options. Changing a value redoes the operation with the new value. **Cancel** undoes it and **Accept** keeps it. Clicking outside the panel also accepts. While the panel is open, the 3D Viewport's toolbar is hidden.

The panel has a green border because it's the window with focus. If it doesn't fit in half the screen height, it shrinks to that half and scrolls by dragging your finger (without touching the options), with the wheel or with the arrows. With the keyboard or the phone: the arrows go through the rows, <kbd>OK</kbd> / <kbd>Enter</kbd> edits the value without closing the panel, **Accept**, <kbd>Esc</kbd> or <kbd>Delete</kbd> (<kbd>C</kbd> on the N95) close it keeping the change, and only **Cancel** undoes it.

## Toolbar

It has Undo and Redo, the last actions used and the proportional editing button. During a transform it shows the check mark (accept), the cross (cancel), the orientation and the X / Y / Z axes (two axes on = a plane). On touch screens, after extruding, **Repeat** appears, which accepts and extrudes again.

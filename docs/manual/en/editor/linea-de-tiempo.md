# Timeline

Shows and edits the animation: keyframe sheet (dope sheet) or curve editor. It shows the animated items of the **selection**.

## Top bar

| Function | Description |
|---|---|
| Transport | Go to start, previous keyframe, previous frame, play backwards, play / pause, next frame, next keyframe, go to end. |
| Start / End / FPS | Range and speed of the animation (click to type). |
| Animation | Which animation is being edited (scene, clips, armatures, objects, flipbooks, Game, Mix), and create new ones. |
| Auto Key | In red: every transform records keyframes by itself. |
| Curves / Dope Sheet | Switches between the keyframe sheet and the curve editor. |
| Select, Key, View, Pivot | Menus for selection, keyframe operations, framing and side panel, and where <kbd>S</kbd> scales from (center or current frame). |

## Navigating time

| Function | Description | Shortcut |
|---|---|---|
| Change frame | Click or drag over the frame numbers (scrub). The current frame (green) can be typed. | Click |
| Previous / Next frame | One frame back or forward. | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Previous / Next keyframe | Jumps to the nearest keyframe. | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Go to start / end | First or last frame. | <kbd>Home</kbd> / <kbd>End</kbd> |
| Play / Pause | From any panel. | <kbd>Space</kbd> |
| Zoom and pan | Wheel = zoom (in curves, <kbd>Ctrl</kbd> = time only, <kbd>Shift</kbd> = value only); middle button = pan. | Wheel / middle button |
| Frame Selected | Fits the view to the selection. | <kbd>Num.</kbd> |

## Editing keyframes (dope sheet and curves)

| Function | Description | Shortcut |
|---|---|---|
| Select | Click on a diamond, or on a row to take all its keyframes; <kbd>Shift</kbd> adds. | Click; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Move keyframes | Shifts the keyframes in time (also by dragging a diamond). | <kbd>G</kbd> |
| Scale keyframes | Stretches or compresses from the pivot. | <kbd>S</kbd> |
| Rotate handles | Curves only. | <kbd>R</kbd> |
| Duplicate | Copies the keyframes and leaves them grabbed. | <kbd>Shift+D</kbd> |
| Delete | Deletes the selected keyframes (or the curves of the selected rows). | <kbd>X</kbd> |
| Interpolation | Constant, Linear or Bezier. | <kbd>T</kbd> |
| Handle Type | Free, Aligned, Vector, Automatic, Auto Clamped (curves only). | <kbd>V</kbd> |
| Smart Euler | Removes extra turns from the selected rotations. | Key menu |
| Hide a curve | The eye on each row hides it. | Click on the eye |
| Exact values | Frame, value and handles of the chosen keyframe. | Properties → Animation → Keyframe |

## Sounds

| Function | Description | Shortcut |
|---|---|---|
| Sound strip | Each selected Sound object has a strip: where it starts and how long it lasts, in **seconds** (changing the FPS does not move it). With a loop it repeats until the end. | — |
| Move | Drag the strip, or <kbd>G</kbd> with no keyframes selected (moves all the selected sounds). <kbd>Ctrl</kbd> snaps to frames. | Drag / <kbd>G</kbd> |
| Trim | Grab an end of the strip. | Drag an end |
| Waveform / Volume line | View menu: the waveform and the volume curve inside the strip (the strip takes three rows). Without them it is just a strip. | View menu |

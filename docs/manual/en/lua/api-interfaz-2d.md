# 2D interface and screen

| Function | What it does |
|---|---|
| `screen() → width, height` | Size of the canvas. |
| `screenMargin() → mx, my` | How much screen is left on each side around a fixed-size canvas, in canvas units, up to the real edges (the camera framing bands are screen too). In portrait with a 4:3 canvas: moving a meter `my` up sticks it to the top edge. |
| `screenFrame() → x, y, width, height` | The frame of the 3D image (the camera framing, without the bands) in canvas units, from its center. |
| `captureScreen(name [, withHud]) → path` | Copies this frame's 3D image (when the scene is done; with `withHud`, after the HUD) to a texture. The path (`"pantalla:<name>"`) works with `setTexture`, a material or the "Bind texture" step of a routine. |
| `captureUV(name) → u0, v0, u1, v1` | The part of the captured texture with the upright image, for `setUVRect` (or `nil` if the copy hasn't happened yet: it happens when the frame is drawn). |
| `safeArea() → x, y, w, h` | The part of the screen without the notch (don't use it in `start`). |
| `getScale() → f` | With no arguments: factor to adapt sizes to the screen (shorter side / 480). |
| `getPosPx(obj) → x, y`, `setPosPx(obj, x, y)` | Position of a 2D element (a `nil` leaves that axis unchanged). |
| `getSizePx(obj) → w, h`, `setSizePx(obj, w, h)` | Size of a 2D element. |
| `uiBox(obj) → x, y, w, h` | The rectangle it actually takes up (x, y = center). |
| `setText(obj, value)`, `setSize(text, px)` | Changes the text of a text or button element, and the font size. |
| `setTexture(img, "ruta.png")`, `setUVRect`, `setFlipbook` | Changes the image, crops a part of it (with u1 < u0 or v1 < v0, mirrored), or animates a sprite sheet. |
| `setOpacity(obj, 0..1)`, `fade(obj, objetivo, paso)` | Transparency and fades. |
| `setRot2D(obj, degrees)` | Rotates a 2D element. |
| `collide(a, b) → bool` | Whether two 2D elements overlap on the screen. |
| `limit(obj, area [, axis])`, `inside(area, x [, y])` | Keep an element inside another one, or ask whether a point is inside. |
| `quit()` | Closes the game (in the editor it does nothing). |

# Lights and camera

| Function | What it does |
|---|---|
| `getColor(light) → r, g, b`, `setColor(light, r, g, b)` | The color of a light. |
| `getEnergy(light) → e`, `setEnergy(light, e)` | Its brightness, from 0 to 1 (to make it flicker or fade out). |
| `setActiveCamera(cam)` | Changes the camera the game is seen from. |
| `getLens(cam) → fov, near, far`, `setLens(cam [, fov, near, far])` | Field of view and clipping distances. |
| `cameraXZ() → fx, fy, rx, ry` | Where the camera is looking on the floor plane, in X and Y: forward (`fx, fy`) and to the right (`rx, ry`), to move the character relative to the camera. (The name is from when the floor was XZ.) |
| `screenOf(obj) → x, y, front` | Where a 3D object appears on the screen (to put a 2D label on top of it). |
| `setRail(cam, curve [, offset])`, `setRailNode`, `setRailLook`, `getRail` | Make the camera follow a curve (rail). |
| `resolution() → width, height` | Actual size of the 3D image in pixels. |

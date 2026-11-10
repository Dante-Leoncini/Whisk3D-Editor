# Paint modes

> [!NOTE]
> The three paint modes are **for 3D meshes only**: they don't appear with cameras, lights, armatures, curves or 2D elements. While painting, clicking doesn't change the selection; to paint another object, pick it in the Outliner.

## What the three have in common

| Function | Description | How to use it |
|---|---|---|
| Paint | Paints only the front-facing faces that are visible (with X-Ray, also the ones behind). Each stroke is one undo step. | Click and drag |
| Radius | The brush size in pixels. When you change it, a circle with the real size appears in the center of the view. | Draggable bar above the toolbar |
| Value | The brush intensity. | Draggable bar |
| Falloff | How the brush edge is softened; you can draw your own curve. | Falloff button |
| View (touch) | On: your finger navigates. Off: your finger paints. | Monitor-shaped button |

## Weight Paint

Paints how much a bone influences each vertex (blue = none, red = full). It's what makes the mesh deform properly with the armature.

| Function | Description |
|---|---|
| Brush mode | Add (+), Subtract (−) or Set (=, sets the exact value). |
| Vertex group | Which group (bone) is painted; you can add groups. If there are none, the first stroke creates one. |
| Markers | Shows a small square per vertex; the brush acts only on them. |
| Selected only | Protects what isn't selected (it looks darkened). |

## Vertex Paint

Paints colors on the vertices of a **3D mesh** (it doesn't work for images or 2D elements: that's what Texture Paint is for). It's very lightweight and it's the recommended way to add color on the N95.

| Function | Description |
|---|---|
| Color | Opens the color picker (RGB, HSV, hexadecimal or a palette index). |
| Per-Vertex / Per-Corner | Per vertex, the color is shared between faces; per corner, each face can have its own (hard edges). |
| Color layer | Choose or create color layers. |
| Markers / Selected only | Same as in Weight Paint. |

## Texture Paint

Paints directly on the 3D object, and the stroke ends up on its texture.

| Function | Description |
|---|---|
| Automatic texture | If the mesh has no texture, one is created automatically on the first stroke: its own material, automatic UVs and a 1024×1024 image (256×256 on the N95) in the material's color. |
| Color, falloff, radius, value | As in the other modes. |
| Save Texture | The paint lives in memory until it's saved. The floppy disk turns green if there are unsaved changes. Options: Save, Save As..., Save inside the .w3d and Save outside the .w3d... |

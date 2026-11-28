# Edit Mode (meshes)

You enter it with <kbd>Tab</kbd> on a mesh. You work on **vertices**, **edges** or **faces**: the long button in the top bar has the three icons, the active one in green, and a click (or a tap; on Symbian the arrows and OK) switches between them. The Vertex, Edge and Face menus live inside **Mesh**, on <kbd>W</kbd> and on right click.

## Select

| Function | Description | Shortcut |
|---|---|---|
| Select | Picks a vertex, edge or face. | Click; <kbd>Shift</kbd>+click toggles |
| Select Linked | The whole connected island under the pointer, **added** to what was already selected. | <kbd>L</kbd> |
| Loop Select | A ring of edges or faces. | <kbd>Shift+Alt</kbd>+click |
| Pick Shortest Path | Selects the path between the active element and the clicked one. With no active element, it starts from the nearest selected one. Whatever you **extrude** becomes the active element: extrude a vertex and Ctrl+click builds the path from the new one. | <kbd>Ctrl</kbd>+click |
| Shortest Path (Fill Region) | Selects the whole region between the two. | <kbd>Ctrl+Shift</kbd>+click |
| All / None / Invert / Box | As in Object Mode, on the mesh elements. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>B</kbd> |

## Modeling

| Function | Description | Shortcut |
|---|---|---|
| Move / Rotate / Scale | Transforms the selection. With a move in progress, pressing <kbd>G</kbd> again switches to sliding. | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Shrink/Fatten | Moves each vertex along its normal (inflates or deflates). | <kbd>Alt+S</kbd> |
| Extrude | Pulls new geometry out of the selection (faces come out along their normal). A face pulled from an edge follows the direction of the face next to it; from a loose edge, once released it faces the view. What you extrude becomes the active element. | <kbd>E</kbd> |
| Loop Cut and Slide | Cuts an edge ring. The wheel changes the number of cuts; a click applies it and then you slide. | <kbd>Ctrl+R</kbd> |
| Knife | Draws a cut on the mesh, click by click. Hovering snaps to a **vertex** (green square), an **edge** (it turns yellow: the cut lands right on it) or stays on the **face**, middle included. Red dots are the vertices about to be born. <kbd>Enter</kbd> or <kbd>Space</kbd> cuts, <kbd>Esc</kbd> or right click cancels, <kbd>Backspace</kbd> removes the last point and <kbd>Z</kbd> also cuts what is behind (what you can't see). A face the cut crosses from side to side is split; a cut that starts or ends in the middle of a face stays as a loose edge. One <kbd>Ctrl+Z</kbd> undoes the cut. | <kbd>K</kbd> |
| New Edge/Face from Vertices | Joins the selected vertices with an edge or a face. | <kbd>F</kbd> |
| Connect Vertex Path | Cuts in a straight line between two vertices. | <kbd>J</kbd> |
| Slide Vertices | Slides the vertices along their edges. | <kbd>Shift+V</kbd> |
| Rip | Splits the mesh apart along the selection. | <kbd>V</kbd> |
| Duplicate | Copies the selection and leaves it grabbed. | <kbd>Shift+D</kbd> |
| Separate | Moves the selected faces to a new object. | <kbd>P</kbd> |
| Merge | Welds vertices: At Center, At Cursor, Collapse or By Distance. Also in the <kbd>W</kbd> menu (vertex, edge or face). **By Distance** shows below how many vertices it merged (0 = nothing was overlapping). | <kbd>M</kbd> |
| Auto Merge | When you finish moving, welds the vertices that ended up closer than the threshold. | Mesh menu |
| Delete | Vertices, edges, faces or edge loops. **Edges** also deletes the selected loose edges. | <kbd>X</kbd> or <kbd>Delete</kbd> |
| Recalculate Normals / Flip | Fixes which way the faces point, or flips them. | <kbd>Shift+N</kbd> |
| Triangulate Faces | Splits faces with more than 3 sides into triangles. | <kbd>Ctrl+T</kbd> |
| Shade Smooth / Flat | How the surface is lit. | Face menu |
| Mark / Clear Sharp | Edges that look hard even if the mesh is smooth. | Edge menu |
| Mark / Clear Seam | Where the mesh is cut when unwrapping the UVs. | Edge or UV menu |
| Context menu | The operations of the current sub-mode (vertex, edge or face) at the pointer. | <kbd>W</kbd> or right click |
| Insert Keyframe | Stores the mesh shape at the current frame (vertex animation). | <kbd>I</kbd> |

## UV menu

| Function | Description | Shortcut |
|---|---|---|
| Unwrap | Angle Based, Conformal or Minimum Stretch: opens the mesh up along its seams. | <kbd>U</kbd> |
| Smart UV Project | Cuts and unwraps by itself based on the angles between faces. | <kbd>U</kbd> |
| Lightmap Pack / Follow Active Quads | Islands arranged without overlapping / UVs arranged following the active face. | <kbd>U</kbd> |
| Cube, Cylinder, Sphere Projection, Project from View | Simple projections. | <kbd>U</kbd> |
| Reset | Each face takes up the whole image. | <kbd>U</kbd> |

## Curve editing (paths)

With <kbd>Tab</kbd> on a curve you edit its nodes as if it were a mesh of points: <kbd>G</kbd> moves, <kbd>E</kbd> adds the next node, <kbd>X</kbd> deletes.

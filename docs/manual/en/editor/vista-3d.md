# 3D Viewport

The 3D Viewport is where you see and edit the scene. It works in different **modes**: what clicks, keys and menus do depends on the current mode. The mode is chosen with the selector in the top bar (it only appears if the active object is a mesh or an armature) or with <kbd>Tab</kbd>.

| Mode | For which objects | What it's for |
|---|---|---|
| [Object Mode](modo-objeto.md) | All | Create, move, rotate, scale, duplicate, parent and delete whole objects. |
| [Edit Mode](modo-edicion.md) | Meshes and curves | Edit vertices, edges and faces (modeling). |
| [Armature Edit Mode](esqueletos.md#armature-edit-mode) | Armatures | Create and arrange the bones. |
| [Pose Mode](esqueletos.md#pose-mode) | Armatures | Pose and animate the bones. |
| [Weight Paint](pintura.md#weight-paint) | 3D meshes only | Paint how much each bone moves each vertex. |
| [Vertex Paint](pintura.md#vertex-paint) | 3D meshes only | Paint colors directly on the vertices. |
| [Texture Paint](pintura.md#texture-paint) | 3D meshes only | Paint on the texture, directly on top of the object. |

## 3D Viewport top bar

| Button | Description |
|---|---|
| Mode selector | Switches mode (Object, Edit, Pose, Paint…). |
| Vertex / Edge / Face | Edit Mode only: what gets selected. |
| Transform Pivot Point | What rotating and scaling happen around: 3D Cursor, Individual Origins, Median Point or Active Element. With the Lock Normals checkbox. |
| Orientation | The transform axes: Global, Local, Normal (the face's direction) or View. |
| Snap | Snap options. It turns green when active (<kbd>Shift+Tab</kbd>). |
| Proportional Editing | When you move something, whatever is nearby follows smoothly. It turns green when active (<kbd>O</kbd>). |
| View | Cameras, viewpoints, framing, local view, perspective, lock orbit, lock the camera to the view. |
| Select | All, none, invert, box, linked, loops and paths. |
| Add (+) | Object Mode only: create new objects. |
| Mesh | Edit Mode only: the modeling operations. |
| Save Texture (floppy disk) | Texture Paint only. Green if there's unsaved paint. |
| Animation (diamond) | Insert, delete and clear keyframes; motion trail. |
| Context | The mode's menu: Object, Vertex/Edge/Face, Armature or Pose. |
| Overlays (eye) | Which helpers are drawn: floor, axes, gizmo, 3D cursor, normals, statistics, X-ray. |
| Render (camera) | The shading mode. |
| Scene | Choose which scene, game or prefab of the project is being edited, and create new ones. |
| UV | Edit Mode only: unwrap and project UVs. |

## Menus shared by several modes

### Select

| Function | Description | Shortcut |
|---|---|---|
| All | Selects everything. | <kbd>A</kbd> |
| None | Deselects everything. | <kbd>Alt+A</kbd> |
| Invert | What was selected gets deselected and vice versa. | <kbd>Ctrl+I</kbd> |
| Box Select | Dragging from left to right (green) only takes what ends up completely inside; from right to left (blue) just touching is enough. <kbd>Shift</kbd> adds. | <kbd>B</kbd> |
| Show selection | Shows or hides the outline of the selection. | Menu |

### Render (shading modes)

| Function | Description |
|---|---|
| Render Preview | With the scene's lights. If ray tracing is on, it shows the traced image little by little. |
| Material Preview | The default: a neutral light from the view, independent of the scene's lights. |
| Solid / Wireframe / ZBuffer Preview | Solid gray, only the edges, or the depth. |
| Normal View / Alpha Preview | The normals in color, or the transparency in black and white. |
| Motion vectors | How far each pixel moved on screen since the previous frame (red = X, green = Y; lavender = still, olive = background). The same image as the `_vectores` pass of the render. |
| Wireframe: Only Visible | The wireframe view shows only what is actually drawn. |

### Overlays

| Function | Description | Shortcut |
|---|---|---|
| Show Overlays | Turns all the visual helpers on or off at once. | <kbd>K</kbd> |
| Floor, X Axis, Y Axis, Origins, 3D Cursor, Relationship Lines | Each helper separately. | Menu |
| Gizmo | The move/rotate/scale gizmo. | Menu |
| Objects | Show or hide armatures, lights, cameras, empties, particles, curves, hitboxes. | Menu |
| Normals | Draws vertex, custom or face normals, with their size. | Menu |
| Statistics | FPS, vertices, faces, draw calls and timings. | Menu |
| X-Ray | The mesh being edited looks semi-transparent and you can pick the elements behind. | Menu |

### Snap

| Function | Description | Shortcut |
|---|---|---|
| Enable Snap | What you move sticks to vertices, edges, faces or their centers. | <kbd>Shift+Tab</kbd> |
| Snap Base and Snap Target | Which point of what's being moved sticks (closest, center, median, active) and to what. | Menu |
| Face Project / Face Nearest | For retopology: vertices rest on another surface. | Menu |
| Affect Move / Rotate / Scale | Which operations snapping acts on. | Menu |
| Cursor Snap menu | Selection to Grid, to Cursor or to Active; Cursor to Selected, to World Origin, to Grid or to Active. | <kbd>Shift+S</kbd> |

### Proportional Editing

| Function | Description | Shortcut |
|---|---|---|
| Enable | When moving, rotating or scaling, the neighbors follow according to their distance. | <kbd>O</kbd> |
| Connected Only | Measures distance along the edges: it doesn't drag loose pieces. | Menu |
| Falloff | Shape of the influence: smooth, sphere, root, inverse square, sharp, linear, constant, random. | Menu |
| Proportional Size | The influence radius. During the transform it's changed with the wheel or with <kbd>PageUp</kbd> / <kbd>PageDown</kbd>. | Menu / wheel |

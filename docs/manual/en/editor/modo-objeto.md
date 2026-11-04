# Object Mode

| Function | Description | How to use it |
|---|---|---|
| Select | Picks an object. Clicking on empty space deselects. | Click; <kbd>Shift</kbd>+click adds or removes |
| Move / Rotate / Scale | See [Move, rotate and scale](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Add | Creates a new object at the 3D cursor (see [Add menu](#add-menu)). | <kbd>Shift+A</kbd> or + button |
| Apply | Bakes the location, rotation, scale or all of them into the mesh (the object ends up at 0, 0, 0 with no rotation and at scale 1). | <kbd>Ctrl+A</kbd> |
| Set Origin | Geometry to Origin, Origin to Geometry, Origin to 3D Cursor. | Object menu |
| Duplicate Objects | Independent copy; it stays grabbed so you can move it. | <kbd>Shift+D</kbd> |
| Duplicate Linked | A copy that shares the mesh with the original (editing one edits both). | <kbd>Alt+D</kbd> |
| Join | Merges the selected meshes into the active object. | <kbd>Ctrl+J</kbd> |
| Set Parent | The selection becomes a child of the active object (keeping the transform or not). | <kbd>Ctrl+P</kbd> |
| Clear Parent | Removes the parent (keeping the position or not). | <kbd>Alt+P</kbd> |
| Delete | Deletes the selected objects (asks for confirmation). | <kbd>X</kbd> or <kbd>Delete</kbd> |
| Hide / Show | Toggles the visibility of the active object. | <kbd>H</kbd> |
| Create Prefab / Unpack Prefab | Turns the selection into a reusable prefab, or splits an instance apart again. | Object menu |
| Insert Keyframe | Stores the location, rotation and/or scale at the current frame (channel menu). <kbd>Shift+I</kbd> stores everything without asking. | <kbd>I</kbd> |
| Delete / Clear Keyframe | Deletes the keyframe at the current frame or the whole animation. | Animation menu |
| Motion Trail | Draws the path the animated objects follow. | Animation menu |
| Change mode | Mesh: Object ↔ Edit. Armature: Object ↔ Pose. Curve: node editing. | <kbd>Tab</kbd> |

## Add menu

Object Mode only. New things appear at the 3D cursor and stay selected. Primitives open a panel to adjust their dimensions.

| Function | Description |
|---|---|
| Mesh | Plane, Cube, Circle, UV Sphere, Cone, Cylinder (with size, radius, segments, smooth shading). Also Vertex (a single loose point to start modeling from) and Reference (a plane with an image, for tracing). |
| Empty | An object without a mesh, for grouping or as a reference point. |
| Camera / Light / Fog | The scene's camera, light and fog. Lights have a **Type**: Point, Sun (directional) or Spot (a cone, drawn in the viewport); Add also offers Sun and Spot directly. **Intensity** makes it stronger (animatable); **Distance** (m) is how far a point or spot light reaches and the length of its cone (0 = no limit; ray tracing fades it out there). While the light is selected, a spot shows its cone and a point light a sphere of that distance; a sun always shows a line down to the floor, toward where it shines. |
| Sound | A sound placed in the scene (speaker icon). Plays with the timeline in **seconds**, not frames; **Positional (3D)** makes it sound from where it is (louder when the camera is closer, from the side it is on); without it, it is like music. Dropping a .wav on the 3D view creates one there. |
| Armature | An armature with one bone, for animating characters. |
| Path | A two-node curve (edited like a mesh: <kbd>E</kbd> adds nodes). |
| Collection | A folder for organizing objects. |
| UI | A 2D interface for games (edited in the 2D Editor). |
| Hitbox | A collision detection box; if there's an active object, it's created as its child, at its size. |
| Physics | It doesn't create an object: it adds a rigid body to the active object. |
| Particles | A particle emitter. |
| LOD / Culling / Grid Cull | Optimization objects: levels of detail and hiding what can't be seen. |
| Clear Depth / Screen Rect | Drawing tools: clear the depth or clip an area of the screen. |
| Prefab / Scene / Proxy W3D | Insert a prefab from the project, another scene, or a prefab from an external library. |
| Imports | OBJ, FBX, glTF, GLB or a Whisk3D UI interface. |
| Core (advanced) | Routines: lists of drawing steps built by hand (transform, draw, textures, lights, fog, blending, depth, control, Call Lua). Includes **Routine loop**: a routine that repeats its steps several times (for example "translate, draw" 20 times), like an array but faster. |

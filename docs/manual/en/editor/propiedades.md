# Properties

The panel with all the data of the active object, organized in tabs (icons). The tabs that appear depend on the object type. Clicking a card's title collapses it. In number fields: click to type (it accepts expressions), drag to slide the value. The diamond on the right sets or removes a keyframe.

| Tab | What it has |
|---|---|
| File / Render (floppy disk) | **File**: open, save, save as, save version, extract assets. **Render**: path, size, extra passes, background, **Ray Tracing** (rays, samples, passes), Render Image and Render Animation. **Game**: platform (Windows, Symbian, Linux .deb, AppImage, WebGL, Android), window, orientation, icon, physics, sound and Build game. **Export**: OBJ, glTF, GLB. **Settings**: language, antialiasing, graphics, skin, logs, scale, **Keyboard Shortcuts...** and Save Changes. |
| Object | Location, rotation (Euler, quaternion or axis-angle), scale, name, visible, renderable; the object's animations and palette. |
| Contextual | Depends on the type: mesh parts and **material** (texture, lighting, vertex color, transparency, reflections, normal map, blending, depth, texture layers, **Advanced: custom shader** — see [Hand-written shaders](shaders.md); Roughness, Metallic, Transmission and IOR with ray tracing), light, camera, fog, particles, physics, hitbox, armature and bones, 2D elements, prefabs, routines. |
| 3D Mesh | The mesh in use (and how many objects share it), UV maps, color layers, vertex groups, UV groups. With a resource chosen in the Library: its data and its location (inside or outside the .w3d). |
| Modifiers | See [Modifiers](modificadores.md). |
| Transform | Edit Mode only: center of the selection and of its UVs, editable. |
| Armature 2D | The mesh's 2D armatures and bones. |
| Constraints | Copy Location, Copy Rotation, Billboard (look at the camera), Child Of. |
| Scripts | The object's Lua scripts and their properties (see the [Lua document](../lua/README.md)). |
| Animation | The active animation, blend layers (Mix), exact values of the chosen keyframe and camera cuts. |

## Routines

A routine is a list of drawing steps (translate, rotate, bind texture, draw a mesh, call another routine, Call Lua…) that you build in Properties. It's the fastest way to draw on the N95.

| Function | Description | Shortcut |
|---|---|---|
| Add / Remove step | Adds steps grouped by topic, or removes the chosen one. | Buttons; <kbd>X</kbd> or <kbd>Delete</kbd> removes |
| Grab step | Reorders the step with the arrows; <kbd>Enter</kbd> confirms. | <kbd>G</kbd> |
| Insert Keyframe | Animates the step's values at the current frame. | <kbd>I</kbd> |
| Repeat | How many times in a row the list runs (routine loop). 1 = normal routine. | Repeat field |
| Render mode / Use | A different list per shading mode, or the same one for all of them. | Drop-downs |

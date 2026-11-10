# The project (.w3d) and files

| Function | Description | Where |
|---|---|---|
| Open project | Opens a .w3d (also with a double click from the system). | Properties → File/Render → File |
| Save / Save As | Saves in the folder and with the name shown in the card; Save As asks for another one. If the name is missing, the field turns red. | File card, <kbd>Ctrl+S</kbd> |
| Save version | Saves and also leaves a numbered copy (`proyecto_v03.w3d`). | File card |
| Extract assets | Copies everything inside to a folder (the copies don't change the project). | File card |
| Meshes as text | Saves the meshes as readable text instead of binary. | File card |
| Import | OBJ, FBX, glTF, GLB and .w3dui interfaces. What you import is copied into the project (without duplicates). | 3D Viewport → Add → Imports |
| Export | OBJ, glTF or GLB of the selected object (with or without modifiers and transformations). | Properties → Export |
| Unsaved changes | Whatever was edited and not saved (scripts, painted textures) is marked with `*`. When closing or opening another project, a warning lists what would be lost. | Automatic |
| Resource inside or outside | An external resource isn't copied: its path is saved and you get a warning if it's missing. | Resource card; "Save inside / outside the .w3d" in the IDE and the UV Editor |

Saving never leaves the file half-written: it first writes a temporary copy and then renames it.

## In the browser (web version)

The browser doesn't let a page read or write the PC's files on its own. That's why, in the web version:

- **Open project**, **Imports** and choosing a texture open the browser's file picker: a box appears with the **Choose file** button. For an OBJ you choose the .obj, its .mtl and its textures together; for a glTF, the .gltf and its .bin.
- **Save** (<kbd>Ctrl+S</kbd>), **Save As**, **Save version** and **Extract assets** download the file to the browser's downloads folder. The same happens with **Export** (the OBJ comes down with its .mtl and its textures next to it), the render, the UV map, painted textures saved outside and external scripts.
- When the editor asks for a folder to save to, no file browser opens: the file is downloaded directly.
- If the browser asks whether the page may download several files, accept. Otherwise only the first one is downloaded.
- What you open lives in the page's memory: when you reload it, everything that wasn't downloaded is lost.

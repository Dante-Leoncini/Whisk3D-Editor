# Files

A single API for reading and writing: `w3dFileSystem::ReadFileBytes`, `ReadTextFile`, `WriteTextFile`, `ReadFileRange` (a chunk, for loading meshes in parts), `ListDir`, `FileExists`. Nobody calls `fopen` on their own.

`ReadFileBytes` searches in this order:

1. **A linked library** (the `lib:<nombre>/...` paths).
2. **The embedded pak:** a game built with "Assets: Packed" carries all its files inside the executable.
3. **The mounted container:** the open .w3d. The container **is mounted, not extracted**: textures, fonts, scripts and interfaces load from inside the zip without anyone changing their code.
4. **The APK assets** (Android): a relative path is read from the APK with the NDK's `AAssetManager`.
5. **The disk.**

Paths are normalized before searching (no `.` or `..`). An .mtl that named `modelos/crash/../../texturas/sombra_blob.png` wasn't in any index, and inside the APK the texture came out white.

| System | The resources (`res/`) | Where it writes |
|---|---|---|
| Windows, Linux | Next to the executable (on Linux also `<prefijo>/share/Whisk3d/res`) | The user's folder; settings, in SDL's (`SDL_GetPrefPath`) |
| Android | Inside the APK | `/storage/emulated/0`; exports, in Downloads |
| Browser | Packed in the `.data`, mounted at `/res` | Settings, in `localStorage`; saving downloads the file ([how](../editor/proyecto.md)) |
| Symbian | The phone's own paths | `E:/` (the memory card) or `C:/` |
| Dreamcast | A romdisk inside the executable (`/rd`) | `/ram`, which is wiped when it's turned off |
| PSP | Next to the `EBOOT.PBP` | `ms0:/` (or `ef0:/` on the PSP Go) |
| PS Vita | Inside the `.vpk` (`app0:res`) | `ux0:` |
| 3DS | The `.3dsx`'s romfs (`romfs:`) | The SD card (`sdmc:/`) |

Two more pieces of the module:

- **W3dRecursos** keeps track of each heavy piece of data (textures, meshes, animations): who uses it and when to free it. What isn't needed right away is loaded in slices, a little each frame.
- **W3dConfig** saves settings as `clave=valor`. In the browser they go to `localStorage`; everywhere else, to a text file. It doesn't allocate memory dynamically, so it works just as well on Symbian. From Lua: [Saved settings](../lua/api-configuracion.md).

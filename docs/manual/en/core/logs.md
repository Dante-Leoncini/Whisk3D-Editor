# Logs

Three printf-style levels: `w3dLogf` (information), `w3dLogfW` (warning) and `w3dLogfE` (error). `g_w3dLogNivel` filters at runtime (3 = everything, 0 = nothing), and compiling with `W3D_DEV_LOG=0` removes the calls from the executable. The last lines are also kept in memory: that's what the editor's [Console](../editor/consola.md) shows.

| System | Where it goes |
|---|---|
| PC | `whisk3d.log`, each line with a millisecond timestamp and `[INFO]`, `[WARN]` or `[ERROR]` |
| Android | logcat, with the `Whisk3D` tag (`adb logcat -s Whisk3D`) |
| Browser | The console: `console.log`, `console.warn` and `console.error` depending on the level |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | The serial port (dc-tool or the Flycast console) |
| PSP | Standard output (PPSSPP, psplink) and `whisk3d.log` next to the `EBOOT.PBP` |
| PS Vita | `sceClibPrintf` and `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` and `sdmc:/3ds/Whisk3D/whisk3d.log` |

Why it matters: on a real console there's no terminal, and the log on the card is the only thing left after a crash. That's why on the 3DS, the Vita and the PSP each line is written right away. On the N95 it was the other way around: writing each line right away took tens of milliseconds and made the game stutter. There, information is gathered and flushed every 3 KB, warnings are flushed immediately and errors are forced to disk. From Lua: [Messages and debugging](../lua/api-mensajes-y-depuracion.md).

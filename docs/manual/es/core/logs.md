# Logs

Tres niveles con estilo printf: `w3dLogf` (información), `w3dLogfW` (aviso) y `w3dLogfE` (error). `g_w3dLogNivel` filtra en tiempo de ejecución (3 = todo, 0 = nada) y compilar con `W3D_DEV_LOG=0` borra las llamadas del ejecutable. Las últimas líneas quedan además en memoria: es lo que muestra la [Consola](../editor/consola.md) del editor.

| Sistema | A dónde va |
|---|---|
| PC | `whisk3d.log`, cada línea con una marca en milisegundos y `[INFO]`, `[WARN]` o `[ERROR]` |
| Android | logcat, con la etiqueta `Whisk3D` (`adb logcat -s Whisk3D`) |
| Navegador | La consola: `console.log`, `console.warn` y `console.error` según el nivel |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | El puerto serie (dc-tool o la consola de Flycast) |
| PSP | La salida estándar (PPSSPP, psplink) y `whisk3d.log` al lado del `EBOOT.PBP` |
| PS Vita | `sceClibPrintf` y `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` y `sdmc:/3ds/Whisk3D/whisk3d.log` |

Por qué importa: en una consola de verdad no hay terminal, y el log de la tarjeta es lo único que queda después de un cuelgue. Por eso en la 3DS, la Vita y la PSP cada línea se escribe en el momento. En el N95 pasaba lo contrario: escribir cada línea al momento tardaba decenas de milisegundos y trababa el juego. Ahí la información se junta y baja cada 3 KB, los avisos bajan enseguida y los errores se fuerzan al disco. Desde Lua: [Mensajes y depuración](../lua/api-mensajes-y-depuracion.md).

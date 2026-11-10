# Log

Tre livelli in stile printf: `w3dLogf` (informazione), `w3dLogfW` (avviso) e `w3dLogfE` (errore). `g_w3dLogNivel` filtra durante l'esecuzione (3 = tutto, 0 = niente) e compilare con `W3D_DEV_LOG=0` elimina le chiamate dall'eseguibile. Le ultime righe restano anche in memoria: è ciò che mostra la [Console](../editor/consola.md) dell'editor.

| Sistema | Dove va |
|---|---|
| PC | `whisk3d.log`, ogni riga con un timestamp in millisecondi e `[INFO]`, `[WARN]` o `[ERROR]` |
| Android | logcat, con l'etichetta `Whisk3D` (`adb logcat -s Whisk3D`) |
| Browser | La console: `console.log`, `console.warn` e `console.error` a seconda del livello |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | La porta seriale (dc-tool o la console di Flycast) |
| PSP | L'output standard (PPSSPP, psplink) e `whisk3d.log` accanto all'`EBOOT.PBP` |
| PS Vita | `sceClibPrintf` e `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` e `sdmc:/3ds/Whisk3D/whisk3d.log` |

Perché è importante: su una console vera non c'è un terminale, e il log sulla scheda è l'unica cosa che resta dopo un blocco. Per questo sul 3DS, sulla Vita e sulla PSP ogni riga viene scritta subito. Sull'N95 succedeva il contrario: scrivere ogni riga subito richiedeva decine di millisecondi e faceva scattare il gioco. Lì le informazioni si accumulano e vengono scritte ogni 3 KB, gli avvisi vengono scritti subito e gli errori vengono forzati su disco. Da Lua: [Messaggi e debug](../lua/api-mensajes-y-depuracion.md).

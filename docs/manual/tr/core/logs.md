# Log'lar

printf tarzında üç seviye: `w3dLogf` (bilgi), `w3dLogfW` (uyarı) ve `w3dLogfE` (hata). `g_w3dLogNivel` çalışma zamanında filtreler (3 = hepsi, 0 = hiçbiri) ve `W3D_DEV_LOG=0` ile derlemek çağrıları çalıştırılabilir dosyadan siler. Son satırlar ayrıca bellekte kalır: editörün [Konsol](../editor/consola.md) panelinde gösterilen budur.

| Sistem | Nereye gider |
|---|---|
| PC | `whisk3d.log`, her satır milisaniye cinsinden bir zaman damgası ve `[INFO]`, `[WARN]` ya da `[ERROR]` ile |
| Android | logcat, `Whisk3D` etiketiyle (`adb logcat -s Whisk3D`) |
| Tarayıcı | Konsol: seviyeye göre `console.log`, `console.warn` ve `console.error` |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | Seri port (dc-tool ya da Flycast konsolu) |
| PSP | Standart çıkış (PPSSPP, psplink) ve `EBOOT.PBP` dosyasının yanında `whisk3d.log` |
| PS Vita | `sceClibPrintf` ve `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` ve `sdmc:/3ds/Whisk3D/whisk3d.log` |

Neden önemli: gerçek bir konsolda terminal yoktur ve bir çökmeden sonra geriye kalan tek şey karttaki log'dur. Bu yüzden 3DS'te, Vita'da ve PSP'de her satır anında yazılır. N95'te tersi oluyordu: her satırı anında yazmak onlarca milisaniye sürüyor ve oyunu takıltıya sokuyordu. Orada bilgiler biriktirilir ve her 3 KB'ta bir diske iner, uyarılar hemen iner ve hatalar diske zorla yazılır. Lua'dan: [Mesajlar ve hata ayıklama](../lua/api-mensajes-y-depuracion.md).

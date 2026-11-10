# Logs

Trois niveaux de style printf : `w3dLogf` (information), `w3dLogfW` (avertissement) et `w3dLogfE` (erreur). `g_w3dLogNivel` filtre à l'exécution (3 = tout, 0 = rien) et compiler avec `W3D_DEV_LOG=0` retire les appels de l'exécutable. Les dernières lignes restent aussi en mémoire : c'est ce qu'affiche la [Console](../editor/consola.md) de l'éditeur.

| Système | Où ça va |
|---|---|
| PC | `whisk3d.log`, chaque ligne avec un horodatage en millisecondes et `[INFO]`, `[WARN]` ou `[ERROR]` |
| Android | logcat, avec l'étiquette `Whisk3D` (`adb logcat -s Whisk3D`) |
| Navigateur | La console : `console.log`, `console.warn` et `console.error` selon le niveau |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | Le port série (dc-tool ou la console de Flycast) |
| PSP | La sortie standard (PPSSPP, psplink) et `whisk3d.log` à côté de l'`EBOOT.PBP` |
| PS Vita | `sceClibPrintf` et `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` et `sdmc:/3ds/Whisk3D/whisk3d.log` |

Pourquoi c'est important : sur une vraie console, il n'y a pas de terminal, et le log sur la carte est la seule chose qui reste après un plantage. C'est pourquoi, sur la 3DS, la Vita et la PSP, chaque ligne est écrite sur le moment. Sur le N95, c'était l'inverse : écrire chaque ligne sur le moment prenait des dizaines de millisecondes et faisait ramer le jeu. Là, les informations sont regroupées et écrites tous les 3 Ko, les avertissements sont écrits tout de suite et les erreurs sont forcées sur le disque. Depuis Lua : [Messages et débogage](../lua/api-mensajes-y-depuracion.md).

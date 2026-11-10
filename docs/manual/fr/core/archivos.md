# Fichiers

Une seule API pour lire et écrire : `w3dFileSystem::ReadFileBytes`, `ReadTextFile`, `WriteTextFile`, `ReadFileRange` (un morceau, pour charger les maillages par parties), `ListDir`, `FileExists`. Personne n'appelle `fopen` directement.

`ReadFileBytes` cherche dans cet ordre :

1. **Une bibliothèque liée** (les chemins `lib:<nombre>/...`).
2. **Le pak intégré :** un jeu compilé avec « Assets : Empaquetés » emporte tous ses fichiers à l'intérieur de l'exécutable.
3. **Le conteneur monté :** le .w3d ouvert. Le conteneur **est monté, pas extrait** : textures, polices, scripts et interfaces se chargent depuis l'intérieur du zip sans que personne ne change son code.
4. **Les assets de l'APK** (Android) : un chemin relatif se lit dans l'APK avec l'`AAssetManager` du NDK.
5. **Le disque.**

Les chemins sont normalisés avant la recherche (sans `.` ni `..`). Un .mtl qui désignait `modelos/crash/../../texturas/sombra_blob.png` ne figurait dans aucun index, et dans l'APK la texture sortait blanche.

| Système | Les ressources (`res/`) | Où l'on écrit |
|---|---|---|
| Windows, Linux | À côté de l'exécutable (sous Linux, aussi `<prefijo>/share/Whisk3d/res`) | Le dossier de l'utilisateur ; les réglages, dans celui de SDL (`SDL_GetPrefPath`) |
| Android | Dans l'APK | `/storage/emulated/0` ; ce qui est exporté, dans Téléchargements |
| Navigateur | Empaquetées dans le `.data`, montées dans `/res` | La configuration, dans `localStorage` ; enregistrer télécharge le fichier ([comment](../editor/proyecto.md)) |
| Symbian | Chemins propres au téléphone | `E:/` (la carte mémoire) ou `C:/` |
| Dreamcast | Un romdisk dans l'exécutable (`/rd`) | `/ram`, qui s'efface à l'extinction |
| PSP | À côté de l'`EBOOT.PBP` | `ms0:/` (ou `ef0:/` sur la PSP Go) |
| PS Vita | Dans le `.vpk` (`app0:res`) | `ux0:` |
| 3DS | Le romfs du `.3dsx` (`romfs:`) | La carte SD (`sdmc:/`) |

Deux autres pièces du module :

- **W3dRecursos** tient le compte de chaque donnée lourde (textures, maillages, animations) : qui l'utilise et quand la libérer. Ce dont on n'a pas besoin tout de suite se charge par tranches, un peu à chaque image.
- **W3dConfig** enregistre des réglages sous la forme `clave=valor`. Dans le navigateur, ils vont dans `localStorage` ; ailleurs, dans un fichier texte. Il ne demande pas de mémoire dynamique, ce qui lui permet de servir aussi sous Symbian. Depuis Lua : [Configuration enregistrée](../lua/api-configuracion.md).

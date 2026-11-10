# File

Una sola API per leggere e scrivere: `w3dFileSystem::ReadFileBytes`, `ReadTextFile`, `WriteTextFile`, `ReadFileRange` (un pezzo, per caricare le mesh a parti), `ListDir`, `FileExists`. Nessuno chiama `fopen` direttamente.

`ReadFileBytes` cerca in quest'ordine:

1. **Una libreria collegata** (i percorsi `lib:<nombre>/...`).
2. **Il pak incorporato:** un gioco compilato con "Asset: Impacchettati" porta tutti i suoi file dentro l'eseguibile.
3. **Il contenitore montato:** il .w3d aperto. Il contenitore **si monta, non si estrae**: texture, font, script e interfacce si caricano da dentro lo zip senza che nessuno debba cambiare il proprio codice.
4. **Gli asset dell'APK** (Android): un percorso relativo si legge dall'APK con l'`AAssetManager` dell'NDK.
5. **Il disco.**

I percorsi vengono normalizzati prima della ricerca (senza `.` né `..`). Un .mtl che indicava `modelos/crash/../../texturas/sombra_blob.png` non compariva in nessun indice, e dentro l'APK la texture risultava bianca.

| Sistema | Le risorse (`res/`) | Dove si scrive |
|---|---|---|
| Windows, Linux | Accanto all'eseguibile (su Linux anche `<prefijo>/share/Whisk3d/res`) | La cartella dell'utente; le impostazioni, in quella di SDL (`SDL_GetPrefPath`) |
| Android | Dentro l'APK | `/storage/emulated/0`; ciò che si esporta, in Download |
| Browser | Impacchettate nel `.data`, montate in `/res` | La configurazione, in `localStorage`; salvare scarica il file ([come](../editor/proyecto.md)) |
| Symbian | Percorsi propri del telefono | `E:/` (la scheda di memoria) o `C:/` |
| Dreamcast | Un romdisk dentro l'eseguibile (`/rd`) | `/ram`, che si cancella allo spegnimento |
| PSP | Accanto all'`EBOOT.PBP` | `ms0:/` (o `ef0:/` sulla PSP Go) |
| PS Vita | Dentro il `.vpk` (`app0:res`) | `ux0:` |
| 3DS | Il romfs del `.3dsx` (`romfs:`) | La scheda SD (`sdmc:/`) |

Altri due pezzi del modulo:

- **W3dRecursos** tiene il conto di ogni dato pesante (texture, mesh, animazioni): chi lo usa e quando liberarlo. Ciò che non serve subito si carica a fette, un po' a ogni fotogramma.
- **W3dConfig** salva impostazioni come `clave=valor`. Nel browser vanno in `localStorage`; altrove, in un file di testo. Non chiede memoria dinamica, quindi funziona anche su Symbian. Da Lua: [Configurazione salvata](../lua/api-configuracion.md).

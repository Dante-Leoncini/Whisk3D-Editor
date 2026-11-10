# Archivos

Una sola API para leer y escribir: `w3dFileSystem::ReadFileBytes`, `ReadTextFile`, `WriteTextFile`, `ReadFileRange` (un pedazo, para cargar mallas por partes), `ListDir`, `FileExists`. Nadie llama a `fopen` suelto.

`ReadFileBytes` busca en este orden:

1. **Una librería vinculada** (las rutas `lib:<nombre>/...`).
2. **El pak embebido:** un juego compilado con "Assets: Empaquetados" lleva todos sus archivos adentro del ejecutable.
3. **El contenedor montado:** el .w3d abierto. El contenedor **se monta, no se extrae**: texturas, fuentes, scripts e interfaces cargan de adentro del zip sin que nadie cambie su código.
4. **Los assets del APK** (Android): una ruta relativa se lee del APK con el `AAssetManager` del NDK.
5. **El disco.**

Las rutas se normalizan antes de buscar (sin `.` ni `..`). Un .mtl que nombra `modelos/crash/../../texturas/sombra_blob.png` no estaba en ningún índice, y adentro del APK la textura salía blanca.

| Sistema | Los recursos (`res/`) | Donde se escribe |
|---|---|---|
| Windows, Linux | Al lado del ejecutable (en Linux también `<prefijo>/share/Whisk3d/res`) | La carpeta del usuario; los ajustes, en la de SDL (`SDL_GetPrefPath`) |
| Android | Adentro del APK | `/storage/emulated/0`; lo exportado, en Descargas |
| Navegador | Empaquetados en el `.data`, montados en `/res` | La configuración, en `localStorage`; guardar descarga el archivo ([cómo](../editor/proyecto.md)) |
| Symbian | Rutas propias del teléfono | `E:/` (la tarjeta de memoria) o `C:/` |
| Dreamcast | Un romdisk adentro del ejecutable (`/rd`) | `/ram`, que se borra al apagar |
| PSP | Al lado del `EBOOT.PBP` | `ms0:/` (o `ef0:/` en la PSP Go) |
| PS Vita | Adentro del `.vpk` (`app0:res`) | `ux0:` |
| 3DS | El romfs del `.3dsx` (`romfs:`) | La tarjeta SD (`sdmc:/`) |

Dos piezas más del módulo:

- **W3dRecursos** lleva la cuenta de cada dato pesado (texturas, mallas, animaciones): quién lo usa y cuándo liberarlo. Lo que no se necesita enseguida se carga de a tajadas, un poco en cada cuadro.
- **W3dConfig** guarda ajustes como `clave=valor`. En el navegador van a `localStorage`; en el resto, a un archivo de texto. No pide memoria dinámica, así sirve igual en Symbian. Desde Lua: [Configuración guardada](../lua/api-configuracion.md).

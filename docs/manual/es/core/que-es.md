# Qué es Whisk3D Core

**Whisk3D Core** es la capa que separa a Whisk3D de cada máquina. Es el ingrediente secreto: gracias a él, el mismo código corre en **Windows**, **Linux**, **macOS**, **Android**, el **navegador** (WebGL), los **Nokia N95** y **N8** (Symbian), la **Sega Dreamcast**, la **Sony PSP**, la **PS Vita** y la **Nintendo 3DS**.

El Core es una biblioteca de C++ con su propio repositorio, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), bajo licencia MIT. Su diseño se inspira en motores clásicos como **RenderWare**: una API chica y estable arriba y una implementación distinta para cada sistema abajo.

![La arquitectura de Whisk3D en tres capas: arriba el Editor (verde), con tu juego o proyecto en Lua adentro; en el medio Whisk3D UI (amarillo); abajo Whisk3D Core (rojo). El Editor y la UI son opcionales: desde afuera, un juego en C++ puede usar la UI y otro el Core directamente. El Core habla con cada sistema por su propio backend](img/whisk3d-capas.webp)

## La idea: pedir, no hacer

Una PC con una placa de video moderna, un teléfono de 2007 y una Dreamcast dibujan de formas que no se parecen en nada. La PC habla OpenGL o Vulkan; la PSP tiene su propio chip (el GE) y una biblioteca (sceGu); la 3DS usa citro3d; el N95, OpenGL ES 1.1. Si el juego llamara a cada una, habría que escribirlo once veces.

Una **capa de abstracción** resuelve eso. El juego no dibuja: **pide**. Dice "dibujá estos triángulos con esta textura y esta luz", y la pieza que conoce ese sistema, el **backend**, sabe cómo hacerlo. Pasa lo mismo con el sonido, los archivos, el log y la red: una sola forma de pedir y una implementación por sistema.

Las reglas de la casa:

- **El código de arriba nunca llama al sistema.** Ni a OpenGL, ni a sceGu, ni a Symbian. Todo pasa por funciones del Core.
- **Los headers no muestran tipos del sistema.** Una textura es un `unsigned int`, y las capacidades son enums propios (`w3dEngine::DepthTest`, `w3dEngine::Blend`), no los de OpenGL. Así quien usa el Core no depende de ningún header de gráficos.
- **Cada sistema aporta lo mínimo.** Su arranque (la ventana, el contexto de la GPU) y lo que no tiene lugar en el código compartido. En el repositorio del editor eso vive en `platform/<sistema>/` y en bloques `#ifdef W3D_PSP`, `#ifdef W3D_3DS`, etc.

Un ejemplo chico de un juego escrito para usar Whisk3D Core, que funciona en todos los sistemas:

```cpp
#include "graphics/w3dGraphics.h"   // dibujar
#include "graphics/w3dTexture.h"    // texturas
#include "io/w3dFilesystem.h"       // archivos
#include "base/w3dlog.h"            // log
#include "audio/W3dAudio.h"         // sonido

namespace gfx = w3dEngine;

// un archivo: puede estar en el disco, adentro del .w3d, adentro del APK o empaquetado en el juego
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// una textura: la decodifica stb (o el ICL de Symbian) y la sube el backend
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// un sonido: lo mezcla el Core; la salida es de cada sistema
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// dibujar: ni una llamada a OpenGL, a sceGu ni a citro3d
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## Qué hay adentro

Cada carpeta del Core es un módulo:

| Carpeta | Qué resuelve |
|---|---|
| `graphics/` | La API de dibujo (`w3dEngine`), las texturas, la pantalla y los backends: [Gráficos](graficos.md) |
| `audio/` | Efectos, música y volumen: [Sonido y música](sonido.md) |
| `io/` | Archivos, contenedores (.w3d, zip, pak), mallas y recursos: [Archivos](archivos.md) |
| `base/` | El log, la configuración guardada y lo común a todos: [Logs](logs.md) |
| `input/` | Táctil, mandos, inclinación, vibración y portapapeles: [Entrada](entrada.md) |
| `net/` | Jugar en red: [Red](red.md) |
| `physics/` | Física, colisiones, hitboxes, ragdolls y navegación: [Física](fisica.md) |
| `script/` | Lua: [Scripts Lua](lua.md) |
| `math/` | Vectores, matrices y cuaterniones: [Matemática, escena y formatos](formatos.md) |
| `objects/`, `animation/` | La escena: objetos, mallas, materiales, luces, cámaras y animaciones: [Matemática, escena y formatos](formatos.md) |
| `tools/` | El generador del despacho, la prueba de independencia y el empaquetador |

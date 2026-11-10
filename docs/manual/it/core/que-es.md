# Cos'è Whisk3D Core

**Whisk3D Core** è lo strato che separa Whisk3D da ogni macchina. È l'ingrediente segreto: grazie a lui, lo stesso codice gira su **Windows**, **Linux**, **macOS**, **Android**, nel **browser** (WebGL), sui **Nokia N95** e **N8** (Symbian), sul **Sega Dreamcast**, sulla **Sony PSP**, sulla **PS Vita** e sul **Nintendo 3DS**.

Il Core è una libreria C++ con un proprio repository, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), con licenza MIT. Il suo design si ispira a motori classici come **RenderWare**: un'API piccola e stabile in alto e un'implementazione diversa per ogni sistema in basso.

![L'architettura di Whisk3D in tre strati: in alto l'Editor (verde), con dentro il tuo gioco o progetto in Lua; nel mezzo Whisk3D UI (giallo); in basso Whisk3D Core (rosso). L'Editor e la UI sono opzionali: dall'esterno, un gioco in C++ può usare la UI e un altro direttamente il Core. Il Core parla con ogni sistema tramite il proprio backend](img/whisk3d-capas.webp)

## L'idea: chiedere, non fare

Un PC con una scheda video moderna, un telefono del 2007 e un Dreamcast disegnano in modi che non si somigliano per niente. Il PC parla OpenGL o Vulkan; la PSP ha un suo chip (il GE) e una libreria (sceGu); il 3DS usa citro3d; l'N95, OpenGL ES 1.1. Se il gioco chiamasse ognuno di loro, bisognerebbe scriverlo undici volte.

Uno **strato di astrazione** risolve il problema. Il gioco non disegna: **chiede**. Dice "disegna questi triangoli con questa texture e questa luce", e il pezzo che conosce quel sistema, il **backend**, sa come farlo. Lo stesso vale per il suono, i file, il log e la rete: un solo modo di chiedere e un'implementazione per sistema.

Le regole della casa:

- **Il codice in alto non chiama mai il sistema.** Né OpenGL, né sceGu, né Symbian. Tutto passa per le funzioni del Core.
- **Gli header non mostrano tipi del sistema.** Una texture è un `unsigned int`, e le capacità sono enum propri (`w3dEngine::DepthTest`, `w3dEngine::Blend`), non quelli di OpenGL. Così chi usa il Core non dipende da nessun header grafico.
- **Ogni sistema fornisce il minimo.** Il proprio avvio (la finestra, il contesto della GPU) e ciò che non trova posto nel codice condiviso. Nel repository dell'editor tutto questo vive in `platform/<sistema>/` e nei blocchi `#ifdef W3D_PSP`, `#ifdef W3D_3DS`, ecc.

Un piccolo esempio di un gioco scritto per usare Whisk3D Core, che funziona su tutti i sistemi:

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

## Cosa c'è dentro

Ogni cartella del Core è un modulo:

| Cartella | Cosa risolve |
|---|---|
| `graphics/` | L'API di disegno (`w3dEngine`), le texture, lo schermo e i backend: [Grafica](graficos.md) |
| `audio/` | Effetti, musica e volume: [Suono e musica](sonido.md) |
| `io/` | File, contenitori (.w3d, zip, pak), mesh e risorse: [File](archivos.md) |
| `base/` | Il log, la configurazione salvata e ciò che è comune a tutti: [Log](logs.md) |
| `input/` | Touch, controller, inclinazione, vibrazione e appunti: [Input](entrada.md) |
| `net/` | Giocare in rete: [Rete](red.md) |
| `physics/` | Fisica, collisioni, hitbox, ragdoll e navigazione: [Fisica](fisica.md) |
| `script/` | Lua: [Script Lua](lua.md) |
| `math/` | Vettori, matrici e quaternioni: [Matematica, scena e formati](formatos.md) |
| `objects/`, `animation/` | La scena: oggetti, mesh, materiali, luci, camere e animazioni: [Matematica, scena e formati](formatos.md) |
| `tools/` | Il generatore della tabella di dispatch, il test di indipendenza e il packer |

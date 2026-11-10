# O que é o Whisk3D Core

O **Whisk3D Core** é a camada que separa o Whisk3D de cada máquina. É o ingrediente secreto: graças a ele, o mesmo código roda no **Windows**, no **Linux**, no **macOS**, no **Android**, no **navegador** (WebGL), nos **Nokia N95** e **N8** (Symbian), no **Sega Dreamcast**, no **Sony PSP**, no **PS Vita** e no **Nintendo 3DS**.

O Core é uma biblioteca C++ com seu próprio repositório, [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), sob licença MIT. Seu design se inspira em motores clássicos como o **RenderWare**: uma API pequena e estável em cima e uma implementação diferente para cada sistema embaixo.

![A arquitetura do Whisk3D em três camadas: em cima o Editor (verde), com o seu jogo ou projeto em Lua dentro; no meio o Whisk3D UI (amarelo); embaixo o Whisk3D Core (vermelho). O Editor e a UI são opcionais: por fora, um jogo em C++ pode usar a UI e outro, o Core diretamente. O Core fala com cada sistema pelo seu próprio backend](img/whisk3d-capas.webp)

## A ideia: pedir, não fazer

Um PC com uma placa de vídeo moderna, um celular de 2007 e um Dreamcast desenham de formas que não se parecem em nada. O PC fala OpenGL ou Vulkan; o PSP tem seu próprio chip (o GE) e uma biblioteca (sceGu); o 3DS usa citro3d; o N95, OpenGL ES 1.1. Se o jogo chamasse cada um deles, seria preciso escrevê-lo onze vezes.

Uma **camada de abstração** resolve isso. O jogo não desenha: **pede**. Ele diz "desenhe estes triângulos com esta textura e esta luz", e a peça que conhece esse sistema, o **backend**, sabe como fazer. O mesmo vale para o som, os arquivos, o log e a rede: uma única forma de pedir e uma implementação por sistema.

As regras da casa:

- **O código de cima nunca chama o sistema.** Nem o OpenGL, nem o sceGu, nem o Symbian. Tudo passa por funções do Core.
- **Os headers não mostram tipos do sistema.** Uma textura é um `unsigned int`, e as capacidades são enums próprios (`w3dEngine::DepthTest`, `w3dEngine::Blend`), não os do OpenGL. Assim, quem usa o Core não depende de nenhum header de gráficos.
- **Cada sistema contribui com o mínimo.** Sua inicialização (a janela, o contexto da GPU) e o que não tem lugar no código compartilhado. No repositório do editor, isso fica em `platform/<sistema>/` e em blocos `#ifdef W3D_PSP`, `#ifdef W3D_3DS` etc.

Um pequeno exemplo de um jogo escrito para usar o Whisk3D Core, que funciona em todos os sistemas:

```cpp
#include "graphics/w3dGraphics.h"   // desenhar
#include "graphics/w3dTexture.h"    // texturas
#include "io/w3dFilesystem.h"       // arquivos
#include "base/w3dlog.h"            // log
#include "audio/W3dAudio.h"         // som

namespace gfx = w3dEngine;

// um arquivo: pode estar no disco, dentro do .w3d, dentro do APK ou empacotado no jogo
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// uma textura: o stb a decodifica (ou o ICL do Symbian) e o backend a envia para a GPU
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// um som: o Core o mixa; a saída é de cada sistema
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// desenhar: nenhuma chamada ao OpenGL, ao sceGu nem ao citro3d
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## O que há dentro

Cada pasta do Core é um módulo:

| Pasta | O que resolve |
|---|---|
| `graphics/` | A API de desenho (`w3dEngine`), as texturas, a tela e os backends: [Gráficos](graficos.md) |
| `audio/` | Efeitos, música e volume: [Som e música](sonido.md) |
| `io/` | Arquivos, contêineres (.w3d, zip, pak), malhas e recursos: [Arquivos](archivos.md) |
| `base/` | O log, a configuração salva e o que é comum a todos: [Logs](logs.md) |
| `input/` | Toque, controles, inclinação, vibração e área de transferência: [Entrada](entrada.md) |
| `net/` | Jogar em rede: [Rede](red.md) |
| `physics/` | Física, colisões, hitboxes, ragdolls e navegação: [Física](fisica.md) |
| `script/` | Lua: [Scripts Lua](lua.md) |
| `math/` | Vetores, matrizes e quatérnios: [Matemática, cena e formatos](formatos.md) |
| `objects/`, `animation/` | A cena: objetos, malhas, materiais, luzes, câmeras e animações: [Matemática, cena e formatos](formatos.md) |
| `tools/` | O gerador do despacho, o teste de independência e o empacotador |

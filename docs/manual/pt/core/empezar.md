# Usar o Core diretamente

Para quem programa em C ou C++ e quer um jogo sem o editor. É preciso:

- **O código-fonte do Core.** Os diretórios de includes são a raiz do Core mais `base/`, `graphics/`, `io/` e `thirdparty/`.
- **Um backend de `graphics/`**, o do sistema (por exemplo `graphics/opengl/w3dGraphics.cpp`), e `base/w3dlog.cpp`. Depois, os módulos que forem usados (áudio, arquivos, física...).
- **Uma janela com seu contexto.** Os exemplos usam SDL2. Com o backend de shaders, é preciso chamar `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` uma vez, depois de criar o contexto.

O mínimo, do exemplo `simple_cube`:

```cpp
gfx::Viewport(0, 0, w, h);
gfx::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
gfx::MatrixMode(gfx::Projection);  gfx::LoadIdentity();
gfx::Perspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
gfx::MatrixMode(gfx::ModelView);   gfx::LoadIdentity();
gfx::Translatef(0.0f, 0.0f, -5.0f);
gfx::Rotatef(angulo, 0.0f, 1.0f, 0.0f);
gfx::EnableArray(gfx::VertexArray); gfx::VertexPointer3f(0, cubeVerts);
gfx::EnableArray(gfx::NormalArray); gfx::NormalPointer3b(cubeNormals);
gfx::DrawTriangles(36, cubeIdx);
```

Os exemplos vão sendo publicados em [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples): `simple_cube` (um cubo com uma luz), `textured_cube` (com uma textura carregada por `LoadTexture`) e `normal_map_cube` (relevo falso com DOT3). Lá também está o `game/w3drun`, o runtime que roda um jogo feito no editor (cena, interface e Lua) sem o editor.

O estado atual, sem rodeios:

- **O Core ainda não está pronto para produção.** Continua sendo reescrito e muda com frequência.
- **Tem lógica do editor dentro** (modo edição, modificadores, modos de render), que está sendo retirada. O `tools/prueba_independencia.sh` mede quanto falta para ele poder ser usado sozinho.
- **Os exemplos foram escritos para uma versão anterior** da organização em pastas: o CMake deles cita `w3dGraphics.cpp` na raiz do Core, que hoje está em `graphics/opengl/`. Pode ser preciso ajustar os caminhos.
- **O que vem por aí:** esqueletos, um sistema estilo BSP para mapas, física opcional, mais backends retrô, mais exemplos e documentação.

As dúvidas, no grupo do [Telegram](https://t.me/Whisk3D).

Se não for preciso descer ao C++, o caminho curto é o editor: o jogo é programado em [Lua](../lua/README.md) e o cartão **Jogo** de [Propriedades](../editor/propiedades.md) o compila para cada sistema. Por baixo, ele usa o mesmo Core.

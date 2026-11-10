# Usare il Core direttamente

Per chi programma in C o C++ e vuole un gioco senza l'editor. Serve:

- **I sorgenti del Core.** Le directory di include sono la radice del Core più `base/`, `graphics/`, `io/` e `thirdparty/`.
- **Un backend di `graphics/`**, quello del sistema (per esempio `graphics/opengl/w3dGraphics.cpp`), e `base/w3dlog.cpp`. Poi, i moduli che si usano (audio, file, fisica...).
- **Una finestra con il suo contesto.** Gli esempi usano SDL2. Con il backend a shader bisogna chiamare `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` una volta, dopo aver creato il contesto.

Il minimo, dall'esempio `simple_cube`:

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

Gli esempi vengono pubblicati man mano su [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples): `simple_cube` (un cubo con una luce), `textured_cube` (con una texture caricata da `LoadTexture`) e `normal_map_cube` (rilievo finto con DOT3). Lì c'è anche `game/w3drun`, il runtime che esegue un gioco fatto nell'editor (scena, interfaccia e Lua) senza l'editor.

Lo stato attuale, senza giri di parole:

- **Il Core non è ancora pronto per la produzione.** Lo si sta ancora riscrivendo e cambia spesso.
- **Contiene logica dell'editor** (modalità modifica, modificatori, modalità di render) che si sta togliendo. `tools/prueba_independencia.sh` misura quanto manca perché si possa usare da solo.
- **Gli esempi sono stati scritti per una versione precedente** dell'organizzazione in cartelle: il loro CMake indica `w3dGraphics.cpp` nella radice del Core, mentre oggi è in `graphics/opengl/`. Potrebbe essere necessario sistemare i percorsi.
- **Cosa arriverà:** armature, un sistema in stile BSP per le mappe, fisica opzionale, più backend retro, più esempi e documentazione.

Per i dubbi, c'è il gruppo [Telegram](https://t.me/Whisk3D).

Se non serve scendere fino al C++, la strada più breve è l'editor: il gioco si programma in [Lua](../lua/README.md) e il riquadro **Gioco** di [Proprietà](../editor/propiedades.md) lo compila per ogni sistema. Sotto usa lo stesso Core.

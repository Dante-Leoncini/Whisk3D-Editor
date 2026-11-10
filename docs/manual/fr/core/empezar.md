# Utiliser le Core directement

Pour ceux qui programment en C ou en C++ et veulent un jeu sans l'éditeur. Il faut :

- **Les sources du Core.** Les répertoires d'includes sont la racine du Core, plus `base/`, `graphics/`, `io/` et `thirdparty/`.
- **Un backend de `graphics/`**, celui du système (par exemple `graphics/opengl/w3dGraphics.cpp`), et `base/w3dlog.cpp`. Ensuite, les modules utilisés (audio, fichiers, physique...).
- **Une fenêtre avec son contexte.** Les exemples utilisent SDL2. Avec le backend à shaders, il faut appeler `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` une fois, après avoir créé le contexte.

Le minimum, tiré de l'exemple `simple_cube` :

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

Les exemples sont publiés au fur et à mesure dans [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) : `simple_cube` (un cube avec une lumière), `textured_cube` (avec une texture chargée par `LoadTexture`) et `normal_map_cube` (faux relief avec DOT3). On y trouve aussi `game/w3drun`, le runtime qui fait tourner un jeu créé dans l'éditeur (scène, interface et Lua) sans l'éditeur.

L'état actuel, sans détour :

- **Le Core n'est pas encore prêt pour la production.** Il est toujours en cours de réécriture et change souvent.
- **Il contient de la logique de l'éditeur** (mode édition, modificateurs, modes de rendu) qui est en train d'en être retirée. `tools/prueba_independencia.sh` mesure ce qui lui manque pour être utilisé seul.
- **Les exemples ont été écrits pour une version antérieure** de l'organisation en dossiers : leur CMake désigne `w3dGraphics.cpp` à la racine du Core, alors qu'il se trouve aujourd'hui dans `graphics/opengl/`. Il faudra peut-être ajuster les chemins.
- **À venir :** squelettes, un système de type BSP pour les cartes, une physique facultative, d'autres backends rétro, d'autres exemples et de la documentation.

Pour les questions, rendez-vous sur le groupe [Telegram](https://t.me/Whisk3D).

S'il n'est pas nécessaire de descendre au C++, le plus court est de passer par l'éditeur : le jeu se programme en [Lua](../lua/README.md) et la carte **Jeu** de [Propriétés](../editor/propiedades.md) le compile pour chaque système. En dessous, il utilise le même Core.

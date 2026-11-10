# Использовать Core напрямую

Для тех, кто программирует на C или C++ и хочет сделать игру без редактора. Понадобится:

- **Исходники Core.** Каталоги заголовков — корень Core, а также `base/`, `graphics/`, `io/` и `thirdparty/`.
- **Бэкенд из `graphics/`** для твоей системы (например, `graphics/opengl/w3dGraphics.cpp`) и `base/w3dlog.cpp`. Дальше — модули, которые ты используешь (звук, файлы, физика...).
- **Окно с контекстом.** В примерах используется SDL2. С бэкендом шейдеров нужно один раз вызвать `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` после создания контекста.

Минимум, из примера `simple_cube`:

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

Примеры постепенно выкладываются в [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples): `simple_cube` (куб с одним источником света), `textured_cube` (с текстурой, которую загружает `LoadTexture`) и `normal_map_cube` (ложный рельеф с DOT3). Там же лежит `game/w3drun` — среда выполнения, которая запускает игру, сделанную в редакторе (сцену, интерфейс и Lua), без редактора.

Текущее состояние, без прикрас:

- **Core пока не готов для продакшена.** Его продолжают переписывать, и он часто меняется.
- **Внутри у него есть логика редактора** (режим редактирования, модификаторы, режимы рендера), которую сейчас выносят. `tools/prueba_independencia.sh` измеряет, сколько ещё осталось, чтобы он мог использоваться сам по себе.
- **Примеры написаны для прежней версии** структуры папок: их CMake указывает `w3dGraphics.cpp` в корне Core, а сейчас он лежит в `graphics/opengl/`. Возможно, пути придётся поправить.
- **Что дальше:** скелеты, система карт в стиле BSP, необязательная физика, больше ретро-бэкендов, больше примеров и документации.

Вопросы — в группе в [Telegram](https://t.me/Whisk3D).

Если спускаться до C++ не нужно, короткий путь — редактор: игра программируется на [Lua](../lua/README.md), а карточка **Игра** панели [Свойства](../editor/propiedades.md) собирает её для каждой системы. Под капотом он использует тот же Core.

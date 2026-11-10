# Core'u doğrudan kullanma

C ya da C++ ile programlayan ve editör olmadan bir oyun isteyenler için. Gerekenler:

- **Core'un kaynak kodu.** Include dizinleri, Core'un kökü ile `base/`, `graphics/`, `io/` ve `thirdparty/` klasörleridir.
- **`graphics/` içinden bir arka uç**, sistemin arka ucu (örneğin `graphics/opengl/w3dGraphics.cpp`), ve `base/w3dlog.cpp`. Ardından kullanılan modüller (ses, dosyalar, fizik...).
- **Bağlamıyla birlikte bir pencere.** Örnekler SDL2 kullanır. Shader arka ucuyla, bağlamı oluşturduktan sonra bir kez `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` çağrılmalıdır.

En azı, `simple_cube` örneğinden:

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

Örnekler [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) deposuna yavaş yavaş yükleniyor: `simple_cube` (ışıklı bir küp), `textured_cube` (`LoadTexture` ile yüklenen bir dokuyla) ve `normal_map_cube` (DOT3 ile sahte kabartma). Orada ayrıca editörde yapılmış bir oyunu (sahne, arayüz ve Lua) editör olmadan çalıştıran çalışma zamanı `game/w3drun` de bulunur.

Şu anki durum, lafı dolandırmadan:

- **Core henüz üretime hazır değil.** Yeniden yazılmaya devam ediyor ve sık sık değişiyor.
- **İçinde editör mantığı var** (düzenleme modu, değiştiriciler, render modları) ve bu mantık dışarı çıkarılıyor. `tools/prueba_independencia.sh`, tek başına kullanılabilmesi için ne kadar yol kaldığını ölçer.
- **Örnekler, klasör düzeninin önceki bir sürümü için yazıldı:** CMake dosyaları `w3dGraphics.cpp` dosyasını Core'un kökünde gösterir, ama bu dosya bugün `graphics/opengl/` içindedir. Yolları düzeltmek gerekebilir.
- **Sırada olanlar:** iskeletler, haritalar için BSP tarzı bir sistem, isteğe bağlı fizik, daha fazla retro arka uç, daha fazla örnek ve belge.

Sorular için [Telegram](https://t.me/Whisk3D) grubu var.

C++'a inmek gerekmiyorsa kısa yol editördür: oyun [Lua](../lua/README.md) ile programlanır ve [Özellikler](../editor/propiedades.md) panelinin **Oyun** kartı onu her sistem için derler. Altta aynı Core'u kullanır.

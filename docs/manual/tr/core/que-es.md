# Whisk3D Core nedir

**Whisk3D Core**, Whisk3D'yi her makineden ayıran katmandır. Gizli malzeme odur: onun sayesinde aynı kod **Windows**, **Linux**, **macOS**, **Android**, **tarayıcı** (WebGL), **Nokia N95** ve **N8** (Symbian), **Sega Dreamcast**, **Sony PSP**, **PS Vita** ve **Nintendo 3DS** üzerinde çalışır.

Core, kendi deposu olan bir C++ kütüphanesidir: [Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core), MIT lisanslı. Tasarımı **RenderWare** gibi klasik motorlardan esinlenir: üstte küçük ve kararlı bir API, altta her sistem için ayrı bir uygulama.

![Üç katmanda Whisk3D mimarisi: en üstte, içinde senin oyunun ya da Lua projen olan Editor (yeşil); ortada Whisk3D UI (sarı); en altta Whisk3D Core (kırmızı). Editor ve UI isteğe bağlıdır: dışarıdan bir C++ oyunu UI'ı, bir başkası doğrudan Core'u kullanabilir. Core her sistemle kendi arka ucu üzerinden konuşur](img/whisk3d-capas.webp)

## Fikir: yapmak değil, istemek

Modern ekran kartlı bir PC, 2007'den bir telefon ve bir Dreamcast birbirine hiç benzemeyen biçimlerde çizer. PC OpenGL ya da Vulkan konuşur; PSP'nin kendi çipi (GE) ve bir kütüphanesi (sceGu) vardır; 3DS citro3d kullanır; N95 ise OpenGL ES 1.1. Oyun her birini ayrı ayrı çağırsaydı, onu on bir kez yazmak gerekirdi.

Bunu bir **soyutlama katmanı** çözer. Oyun çizmez: **ister**. "Bu üçgenleri bu doku ve bu ışıkla çiz" der ve o sistemi tanıyan parça, yani **arka uç** (backend), bunu nasıl yapacağını bilir. Ses, dosyalar, log ve ağ için de aynısı geçerlidir: istemenin tek bir yolu ve her sistem için bir uygulama.

Evin kuralları:

- **Üstteki kod sistemi asla çağırmaz.** Ne OpenGL'i, ne sceGu'yu, ne de Symbian'ı. Her şey Core fonksiyonlarından geçer.
- **Header'lar sistem türlerini göstermez.** Bir doku bir `unsigned int`'tir ve yetenekler OpenGL'inkiler değil, kendi enum'larıdır (`w3dEngine::DepthTest`, `w3dEngine::Blend`). Böylece Core'u kullanan, hiçbir grafik header'ına bağımlı olmaz.
- **Her sistem en azını katar.** Kendi başlatmasını (pencere, GPU bağlamı) ve ortak kodda yeri olmayanları. Editörün deposunda bunlar `platform/<sistema>/` içinde ve `#ifdef W3D_PSP`, `#ifdef W3D_3DS` gibi bloklarda durur.

Whisk3D Core'u kullanmak için yazılmış ve tüm sistemlerde çalışan bir oyundan küçük bir örnek:

```cpp
#include "graphics/w3dGraphics.h"   // çizim
#include "graphics/w3dTexture.h"    // dokular
#include "io/w3dFilesystem.h"       // dosyalar
#include "base/w3dlog.h"            // log
#include "audio/W3dAudio.h"         // ses

namespace gfx = w3dEngine;

// bir dosya: diskte, .w3d'nin içinde, APK'nın içinde ya da oyuna paketlenmiş olabilir
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// bir doku: onu stb (ya da Symbian'ın ICL'i) çözer ve arka uç yükler
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// bir ses: onu Core karıştırır; çıkış her sistemin kendisinindir
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// çizim: OpenGL'e, sceGu'ya ya da citro3d'ye tek bir çağrı bile yok
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## İçinde neler var

Core'un her klasörü bir modüldür:

| Klasör | Neyi çözer |
|---|---|
| `graphics/` | Çizim API'si (`w3dEngine`), dokular, ekran ve arka uçlar: [Grafik](graficos.md) |
| `audio/` | Efektler, müzik ve ses düzeyi: [Ses ve müzik](sonido.md) |
| `io/` | Dosyalar, kapsayıcılar (.w3d, zip, pak), mesh'ler ve kaynaklar: [Dosyalar](archivos.md) |
| `base/` | Log, kaydedilen ayarlar ve herkese ortak olanlar: [Log'lar](logs.md) |
| `input/` | Dokunmatik ekran, oyun kumandaları, eğim, titreşim ve pano: [Girdi](entrada.md) |
| `net/` | Ağda oynama: [Ağ](red.md) |
| `physics/` | Fizik, çarpışmalar, hitbox'lar, ragdoll'lar ve yol bulma: [Fizik](fisica.md) |
| `script/` | Lua: [Lua script'leri](lua.md) |
| `math/` | Vektörler, matrisler ve kuaterniyonlar: [Matematik, sahne ve formatlar](formatos.md) |
| `objects/`, `animation/` | Sahne: nesneler, mesh'ler, materyaller, ışıklar, kameralar ve animasyonlar: [Matematik, sahne ve formatlar](formatos.md) |
| `tools/` | Dağıtım (dispatch) üreteci, bağımsızlık testi ve paketleyici |

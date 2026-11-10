# UI'ı doğrudan kullanma

C ya da C++ ile programlayan ve arayüzü editör olmadan isteyenler için. Gerekenler:

- **Sahne modeliyle birlikte Core.** UI `w3dEngine` ile çizer ve bazı satırlar sahnenin nesnelerini okur (`objects/`, `animation/`, `script/`). Bkz. [Core'u doğrudan kullanma](../core/empezar.md).
- **UI'ın kaynak kodu.** `libs/WhiskUI/` içindeki tüm `.cpp` dosyaları. Include dizinleri, `"WhiskUI/draw/icons.h"` için `libs/` ile `widgets/`, `text/`, `draw/`, `theme/` ve `core/` alt klasörleridir, çünkü içeride birbirlerini klasör belirtmeden içerirler (`"icons.h"`, `"card.h"`).
- **Skin dosyaları.** `font.png` (harfler ve kartların kenarları), `font-ext.png` (genişletilmiş atlas), `skin.ini` (renkler) ve yeni simgeler için `atlas/iconos/` klasörü. Kütüphaneyle birlikte gelmezler: editörün deposunda, `res/Skins/Whisk3D/` içindedirler.

## Başlatma

Editörün açılışta yaptığının aynısı, ona ait olanlar hariç:

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // dosya olmazsa koddaki renkler kalır

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // yuvarlak kenarlar
CrearIconos(w, h);         // font.png'nin içindeki simgeler
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // ancak gerektiğinde yüklenir
```

`GlobalScale` 1 ile başlar. Başka bir ölçek için bkz. [Renkler ve ölçek](tema.md). `atlas/iconos/` içindeki ayrı simgeler için bkz. [Çizim ve atlas](dibujo.md).

Pencerenin boyutu her değiştiğinde:

```cpp
W3dPantallaAlto = alto;    // arayüzün Y'sini Scissor ve Viewport'un Y'sine çevirmek için
MenuPantallaW = ancho;     // menüler ekranın dışına taşmasın diye
MenuPantallaH = alto;
```

## 2D durum

UI piksel cinsinden çizer; Y aşağı doğrudur ve başlangıç noktası sol üsttedir. Her widget, o anki matrisin başlangıç noktasına çizilir: `Translatef` ile yerleştirilir. Çizmeden önce:

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y aşağı doğru
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // keskin pikseller
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## Bir kart

Bir sayı, bir onay kutusu ve bir düğme içeren bir kart. Her satır senin bir değişkenine işaret eder:

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // yüksekliği kart hesaplar

// her karede, 2D durum ayarlıyken
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

UI klavyeyi ya da fareyi okumaz: onları ona sen verirsin. Klavyeyle:

```cpp
// aşağı ok
if (opciones->editando) opciones->button_down();   // değeri değiştirir
else opciones->NextSelect();                        // alttaki satır
// OK ya da Enter: başlıkta kartı katlar; bir satırda onu düzenler ya da ona basar
opciones->EnterPropertieSelect();
// İptal: satırı olduğu gibi bırakır
opciones->Cancel();
```

Fareyle, işaretçinin altındaki satırı sen hesaplarsın: bkz. [Kartlar ve özellikler](propiedades.md). Menüler ise koordinat alır: bkz. [Açılır menüler](menus.md).

## Şu anki durum, lafı dolandırmadan

- **Üretime hazır değil.** Bunu deponun `README.md` dosyası söylüyor ve gerçekten öyle: içinde, ayrılmakta olan editör kodu var.
- **Onu tek başına kullanan bir örnek yok.** [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) içindeki `ui/` örnekleri, oyunun çalışma zamanının çalıştırdığı Lua ile `.w3dui` arayüzleridir: bu editöre aittir. Bu sayfadaki kod, editörün kullandığı çağrıların aynısıyla kurulmuştur; test edilmiş bir örnekten gelmez.
- **Bugün tek başına bağlanmıyor (link).** `PropList.cpp`, editörün özellikler panelinde bulunan `RebindMaterialMeshPart()` fonksiyonunu çağırır. `TextField.h` içindeki odak fonksiyonları (`TextFieldEnfocar`, `TextFieldCancelar`, `TextFieldInputChar`, `TextFieldFinDeCuadro`) editörde, `main/ui/ViewPorts/NumInput.cpp` içinde tanımlıdır.
- **Core'un sahne modeline bağımlı.** `PropertieBase.h`, `objects/Objects.h` dosyasını içerir ve özellik listesi mesh'leri, iskeletleri, animasyonları ve script'leri okur.
- **Ölçeği editör yeniden hesaplar.** `SetGlobalScale`, `main/ui/ViewPorts/ViewPorts.cpp` içindedir. Tek başına kullanıldığında ölçüleri elle yeniden hesaplamak gerekir ([Renkler ve ölçek](tema.md)).
- **Her şey global.** Program başına bir yazı tipi (`WhiskFont`), bir simge tablosu (`IconsUV`), bir açık menü (`MenuAbierto`) ve bir odaklanmış metin kutusu (`g_textFieldActivo`) vardır.
- **Header'lar OpenGL türlerini gösterir** (`GLfloat`, `GLshort`, `GLuint`), artık OpenGL'i çağırmasalar da.
- **Sırada olanlar,** `README.md`'ye göre: editöre ait kodu temizlemek ve kullanım örnekleri yapmak.

Neyin UI'a, neyin editöre ait olduğu, tek tek: [Neler UI'a, neler editöre ait](ui-y-editor.md). Sorular için [Telegram](https://t.me/Whisk3D) grubu var.

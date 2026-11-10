# Neler UI'a, neler editöre ait

Editör Whisk3D UI ile yapılmıştır, ama onda görülenlerin çoğu editörün kendisine aittir. UI'a ait olanlar `libs/WhiskUI/` içindedir; editöre ait olanlar Whisk3D deposunun `main/` klasöründe.

## UI'a ait olanlar

- 2D çizim, kenarlıklı kartlar ve dikdörtgenler.
- Simge tablosu ve atlas paketleyici.
- Bitmap yazı tipi, genişletilmiş atlas ve önceden pişirilmiş TTF yazı tipleriyle metin.
- Renk paleti ve ölçüler, ölçekleriyle birlikte.
- Düğmeler, sekmeler, onay kutuları ve açılır menüler.
- Özellik kartları ve satırları, animasyon eşkenar dörtgeniyle birlikte.
- Metin kutularının durumu ve çizimi.

## Editöre ait olanlar

| Ne | Nerede |
|---|---|
| Viewport'lar: ekranı bölmek, kenarlıkları, menü çubukları ve araç çubukları | `main/ui/ViewPorts/` (`ViewPorts.cpp`, `WithBorder.cpp`, `ToolbarBase.cpp`) |
| Özellikler paneli: hangi kartların olduğu, sekmeleri, satırlar üzerinde tıklama ve sürükleme | `main/ui/ViewPorts/Properties.cpp` |
| Kalıcı (modal) pencereler: renk seçici, dosya gezgini, onaylar, ilerleme çubuğu, ekran klavyeleri | `main/ui/ViewPorts/PopUp/` |
| Bildirimler | `main/ui/ViewPorts/Notificaciones.cpp` |
| Panellerin kaydırma çubukları | `main/ui/ViewPorts/ScrollBar.cpp` |
| Yardım balonları. Metni `Button::ayuda` saklar | `W3dAyudaRender`, `main/ui/ViewPorts/LayoutInput.cpp` içinde |
| Hangi tuşun, tıklamanın ya da dokunuşun hangi widget'a gittiği ve N95'in tuşları | `main/ui/ViewPorts/LayoutInput.cpp`, `main/app/controles.cpp`, `platform/symbian/src/` |
| Metin kutularının odak fonksiyonları | `main/ui/ViewPorts/NumInput.cpp` |
| Çeviriler, `T()`. UI'da yalnızca `W3dMenuTraducir` kancası vardır | `main/config/W3dLang.cpp` |
| UI'ın başlatılması | `main/app/W3dInitUI.cpp` |
| Ayrı simgelerle skin atlasını kurmak | `main/io/SkinAtlas.cpp` |
| Ölçeği yeniden hesaplamak | `SetGlobalScale`, `main/ui/ViewPorts/ViewPorts.cpp` içinde |
| PC'de ve Android'de `skin.ini` dosyasını okumak | `loadColors`, `main/app/main.cpp` içinde |
| Editörün kendi renkleri: zemin, eksenler, normaller | `main/ui/W3dColors.cpp` |
| Skin dosyaları: `font.png`, `font-ext.png`, `atlas/iconos/`, `skin.ini` | `res/Skins/Whisk3D/` |
| Genişletilmiş atlasın üreteci | `tools/fuente_ext.py` |
| [2D Düzenleyici](../editor/editor-2d.md) ve oyunların `.w3dui` arayüzleri: metinler, görüntüler, dikdörtgenler, kapsayıcılar, slice 9, düğmeler ve videolar | `main/objects/` (`UI.h`, `Texto2D.h`, `Boton2D.h`...), `main/render/UIOverlay.cpp`, `main/io/UI2DFormato.cpp`, `main/io/Fuente2D.cpp`, `main/ui/ViewPorts/Editor2D.cpp` |

2D Düzenleyici metni, UI'a ait olan `W3dTextAtlas` ile çizer. Bir TTF'yi çalışırken stb_truetype ile pişirmek ise editöre aittir (`main/io/Fuente2D.cpp`).

## UI'ın içinde, ama editöre ait

`libs/WhiskUI/` içinde duran ve hâlâ editöre ait olan şeyler:

- `PropListMeshParts` mesh'leri, iskeletleri, animasyonları, kısıtlamaları ve script'leri okur ve özellikler panelinin `RebindMaterialMeshPart()` fonksiyonunu çağırır.
- `PropColor`, renk seçici için paleti ve rengin geldiği nesneyi (`palRef`, `palObj`) saklar.
- `GroupProperties`, özellikler panelinin genel kart listesidir.
- `core/UI.h` editör durumu içerir: Düzenleme Modu'nun dokunmatik görünüm kipi (`g_viewEditMode`), farenin hareketi (`dx`, `dy`), Alt, Shift ve Ctrl tuşları (`LAltPressed`, `LShiftPressed`, `LCtrlPressed`) ve viewport'ların ölçüleri (`paddingViewport`, `MinViewportHeight`, `MinViewportWidth`).
- Simgelerin çoğu editörün araçlarıdır.

Ayrılması gerekenler ve kütüphanenin durumu: [UI'ı doğrudan kullanma](empezar.md) sayfasının sonu.

# Çizim ve atlas

## Nasıl çizer

Her şey `w3dEngine`'den geçer ([Grafik](../core/graficos.md)). UI, köşeleri `float` olan üçgenler (`DrawTrianglesArray`, `DrawTrianglesByte`) ve çizgiler çizer. `TRIANGLE_STRIP` ile ya da `short` köşelerle çizmez: N95'in sürücüsü bunları karenin 2D aşamasında çizmez.

Koordinatlar ekran pikselleridir; Y aşağı doğrudur ve başlangıç noktası sol üsttedir. Her widget, o anki matrisin başlangıç noktasına çizilir. Onu yerleştirmek için `PushMatrix` ile `PopMatrix` arasında `Translatef` kullanılır.

## `glesdraw` fonksiyonları

`draw/glesdraw.h` en alttaki çizimdir, diğerlerinin kullandığı:

| Fonksiyon | Ne çizer |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | Bir kare: bir şeridin sırasıyla 4 `GLshort` köşe ve bunların 8 UV'si. Onu iki üçgene çevirir |
| `W3dDrawElemsB(verts, uvs, idx, n)` | `GLubyte` indeksli küçük bir mesh, en fazla 64 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | 3 bileşenli `GLshort` köşelerle en fazla 21 çizgi |
| `W3dDrawLinesF(verts, idx, n)` | 3 bileşenli `float` köşelerle çizgiler, çizim başına 40'ar köşe |

`float`'a çevrilen köşeler hiçbir zaman yığına (stack) değil, döngüsel olarak kullanılan 256 yerlik sabit bir depoya gider: N95'in sürücüsü işaretçileri geç okur.

`W3dPantallaAlto` ekranın yüksekliğidir. Onu, boyut değiştiğinde UI'ı kullanan taraf ayarlar; arayüzün Y'sini, aşağıdan sayan `Scissor` ve `Viewport`'un Y'sine çevirmeye yarar.

## Düğümler ve dikdörtgenler

`Object2D` (`draw/object2D.h`) çocukları (`Childrens`) olan bir düğümdür: boyutu, rengi, opaklığı ve `visible` değeri vardır. `Render()` önce onu, ardından çocuklarını çizer. Silindiğinde çocuklarını da siler.

`Rec2D` (`draw/rectangle.h`) tek renkli bir dikdörtgendir. `SetSize(x, y, w, h)` onu yerleştirir ve `SetColor(r, g, b)` rengini verir. Dokuyu okumaz: `Texture2D` kapalıyken çizilir.

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false: kendi rengiyle değil, o anki renkle
gfx::Enable(gfx::Texture2D);
```

## Kartlar

`Card` (`widgets/card.h`), düğmelerin, menülerin ve özellik kartlarının yuvarlak köşeli dikdörtgenidir. Bir **9-patch**'tir: ortayı esneten ve köşeleri `2 * GlobalScale` boyutunda bırakan 16 köşe noktası. Köşeler ve orta, `font.png`'nin bir köşesinden gelir. `CalcCardUV(anchoAtlas, altoAtlas)` bu koordinatları hesaplar: dokunun boyutuyla bir kez çağrılır.

| Fonksiyon | Ne yapar |
|---|---|
| `Resize(w, h)` | Boyutu değiştirir |
| `RenderObject(propio)` | Dolguyu çizer |
| `RenderBorder(propio)` | Yalnızca kenarlığı çizer |
| `RenderObjectTab()`, `RenderBorderTab()` | Aynısı, alt kenarı düz: sekmeler için |

`propio` `true` olduğunda kendi rengini (`color` ve `opacity`) kullanır; `false` olduğunda o anki rengi.

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## Skin atlası

Tüm arayüz için tek bir doku. 128x128 boyutundaki `font.png`, harfleri, kartların kenarlarını ve eski simgeleri içerir. Yalnızca bununla UI çalışır: `CrearIconos`, simgeleri `font.png`'nin içinden alır.

Yeni simgeler `atlas/iconos/` içinde ayrı dosyalardır. Editör açılışta daha büyük bir doku kurar (`main/io/SkinAtlas.cpp`): `font.png`'nin tamamını (0,0) noktasına koyar, böylece koordinatları değişmez, ve her simgeyi artan boş yere paketler. Ardından her birinin nereye yerleştiğini UI'a `CrearIconosDesde` ile bildirir. Bu atlası kurmak editöre aittir; paketleyici ve simge tablosu UI'a aittir. Bkz. [Simgeler](iconos.md).

## Paketleyici

`W3dAtlasPack` (`draw/W3dAtlasPacker.h`), birkaç RGBA görüntüyü tek bir görüntüde birleştirir. GPU'ya dokunmaz: pikseller girer ve bir tuval çıkar; onu çağıran taraf yükler. Bir oyun için de aynı şekilde işe yarar: bir bölümün dokularını tek bir dokuda toplamak doku değişikliklerini azaltır.

```cpp
std::vector<W3dAtlasItem> items(n);       // her görüntüden: rgba, w, h
items[0].fijo = true;                     // bu, ne olursa olsun istenen yere gider
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // görüntüler arasında 2 piksel boşluk
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x, items[i].y: her birinin nereye yerleştiği
}
```

Giderek büyüyen tuvalleri dener: 128x128, 256x128, 256x256, 512x256, 512x512 ve 1024x512. Sonuncusuna bile sığmazlarsa `false` döndürür. Serbest görüntüler, sabit olanları atlayarak soldan sağa satırlar hâlinde dizilir. Görüntüler arasındaki boşluk, doğrusal filtrelemenin bir görüntüyü komşusuyla karıştırmasını önler.

## Eskiden kalanlar

`draw/sprites.h` (`Sprite`) ve `draw/image.h` (`Image`) dosyalarını hiçbir dosya içermez. Symbian'da derlenmeyen C++11 başlatıcılarını kullanırlar.

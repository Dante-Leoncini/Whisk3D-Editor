# Grafik

## API

Çizilen her şey `w3dEngine` fonksiyonlarından (`graphics/w3dGraphics.h`) geçer: yetenekleri açıp kapatma (`Enable`, `Disable`), matrisler (`MatrixMode`, `PushMatrix`, `Translatef`, `Perspective`), köşe dizileri (`VertexPointer3f`, `NormalPointer3b`, `TexCoordPointer2f`), GPU'daki buffer'lar (`GenBuffer`, `ArrayBufferData`), çizim (`DrawTriangles`, `DrawLines`, `DrawPoints`), materyaller (`Material`), ışıklar (`LightNfv`, `LightNSpot`), sis, karıştırma, dikdörtgenle kırpma, stencil ve kırpma düzlemleri.

Bilerek klasik OpenGL'e benzer: N95'ten Dreamcast'e kadar en çok sistemin anladığı çizim biçimi budur.

## Sabit boru hattı ve shader'lar

İki GPU ailesi vardır:

- **Sabit boru hattı.** GPU, adımları fabrikadan hazır getirir: köşeleri dönüştürmek, ışığı köşe başına hesaplamak, dokuyu birleştirmek, sis eklemek. Program yalnızca anahtarları açar ve değerleri verir. OpenGL ES 1.1 (N95), PC'nin klasik OpenGL'i, PSP'nin GE'si ve Dreamcast'in PVR'ı böyle çalışır.
- **Shader'lar.** GPU hiçbir şeyi hazır getirmez: her adım, onun üzerinde çalışan küçük bir programdır. OpenGL ES 2.0, WebGL ve Vulkan böyle çalışır.

Core sabit boru hattının dilini konuşur. Shader arka ucu bunu, OpenGL ES 1.1 ile aynı hesapları yapan bir "uber-shader" ile **taklit eder** (8 ışığa kadar köşe başına ışık, zayıflama, koni, sis, iki dokunun karıştırılması). Bu yüzden tarayıcı ve Android, PC ve N95 ile aynı görünür.

## Arka uçlar

| Arka uç | Nedir | Nerede kullanılır |
|---|---|---|
| `opengl/` | Sabit boru hattı: masaüstü OpenGL, OpenGL ES 1.1, GLdc, vitaGL | Linux, Windows, macOS, N95 ve S60, Dreamcast, PS Vita |
| `gles2/` | Shader'lar: OpenGL ES 2.0 ve WebGL | Android, tarayıcı, Nokia N8 (Symbian^3), PC (isteğe bağlı) |
| `vulkan/` | Aynı shader arka ucu, Vulkan ile yapılmış bir OpenGL ES 2.0 üzerine derlenmiş | PC (isteğe bağlı) |
| `gu/` | PSP'nin GE'si | PSP |
| `citro3d/` | 3DS'in PICA200 GPU'su | Nintendo 3DS |
| `pvr/` | Dreamcast'in PVR'ına doğrudan çizim | Yerel oyunlar için isteğe bağlı modül |
| `video/` | Dokuya video | İsteğe bağlı: masaüstünde FFmpeg, tarayıcıda `<video>` |

## Nasıl seçilir

Neredeyse her zaman **derlerken**: her sistem tek bir arka uç derler ve çağrılar hiçbir ek maliyet olmadan doğrudan gider.

İstisna PC'dir (Windows ve Linux). Orada çalıştırılabilir dosya OpenGL, OpenGL ES 2.0 ve Vulkan'ı birlikte taşır ve seçim **açılışta** yapılır, **Özellikler → Ayarlar → Grafik** içinden (yeniden başlatınca geçerli olur; bkz. [Diğer işlevler](../editor/otras-funciones.md)). Her arka uç kendi namespace'inde derlenir (`w3dEngine::gl`, `w3dEngine::es2`, `w3dEngine::vk`) ve kendi fonksiyonlarıyla bir **dağıtım tablosunu** (`w3dBackendTabla.h`) doldurur. Her `w3dEngine` fonksiyonu, etkin arka ucun fonksiyonunu çağırır:

```cpp
// graphics/w3dDespacho.inc, tools/generar_despacho.py tarafından üretilir
void Enable(Cap c) { W3D_TABLA()->Enable(c); }
void DrawTriangles(int count, const MeshIndex* indices) { W3D_TABLA()->DrawTriangles(count, indices); }
```

Seçilen arka uç o makinede çalışmazsa OpenGL'de kalınır. Vulkan; sürücüyü, GPU'yu ve çalıştırılabilir dosyanın yanında `shaderc_shared` shader derleyicisini gerektirir ve 32 bitlik derlemelerde kapalı gelir. macOS'ta çalıştırılabilir dosya yalnızca OpenGL taşır.

## Bir `DrawTriangles`'ın yolculuğu

Aynı istek çok farklı yerlerde son bulur:

![Aynı gfx::DrawTriangles Whisk3D Core'dan geçer ve her arka uca iner: opengl (PC, macOS, N95, Dreamcast, PS Vita), gles2 (Android, tarayıcı, N8, PC), vulkan (PC), gu (PSP) ve citro3d (3DS)](img/whisk3d-core-despacho.webp)

- **Sabit OpenGL (PC, N95):** neredeyse doğrudan `glDrawElements`'a. Matris ancak çizimden hemen önce, tek bir `glLoadMatrixf` ile gönderilir.
- **OpenGL ES 2.0 ve WebGL:** matris yığını CPU'da yaşar ve uber-shader'a uniform olarak gider. WebGL RAM'deki dizileri kabul etmez, bu yüzden arka uç onları buffer'lara aktarır. Android 2.3'ün VideoCore IV GPU'sunda her durum birleşimi için küçük bir program kurar ve arayüzün çizimlerini birleştirir: karede 179 çizimden 16'sı kalır.
- **PSP (GE):** CPU her köşeyi kırpma uzayına taşır ve kırpmayı orada yapar, çünkü GE yanlara karşı kırpmaz. Ardından onları, GE'nin okuduğu bir bellek halkasına iç içe yazar. Işık (en fazla dört) ve sis donanımla yapılır.
- **3DS (PICA200):** köşeler doğrusal belleğe kopyalanır ve onları kendi vertex shader'ı (`w3d.v.pica`) dönüştürür. Aynı durumdaki art arda küçük çizimler birlikte gider: karede 191 çizimden 18'i kalır. Çizgiler ve noktalar CPU'da üçgen olarak kurulur.
- **Dreamcast:** GLdc, PVR üzerinde OpenGL 1.x görevi görür ve kendi yamalarıyla PC ve N95 ile aynı sabit arka ucu kullanır. Her döngüye ihtiyacı olan bir oyun için `pvr/` vardır: GLdc köşe başına SH-4'ün yaklaşık 400 döngüsünü harcar, doğrudan çizim ise her köşeyi tek bir komutla (`FTRV`) dönüştürür. İkisi aynı programda bir arada bulunmaz.
- **PS Vita:** vitaGL, konsolun GPU'su üzerinde sabit boru hattını taklit eder ve shader'larını çalışırken derler (bu yüzden `libshacccg.suprx` gerekir).

## Arka uca neler yapabildiğini sormak

Her GPU aynı şeyleri yapamaz ve Core bunu gizlemez: her arka uç neye sahip olduğunu yanıtlar ve çizen taraf, bir şey eksikse başka bir yol seçer.

| Soru | Hayır derse ne olur |
|---|---|
| `VBOSoportado()` | Mesh'ler RAM'de dizi olarak gider (Dreamcast, Vita) |
| `EstencilBits()` | Su yansıması düzlemle kırpmaya düşer |
| `PlanosRecorteMax()` | Yansıma düzlemler olmadan halledilir (PVR'da yoktur) |
| `MezclaTexturasSoportada()` | Bir materyalin katmanları birkaç geçişte çizilir |
| `TieneTexGen()` | Küresel yansıma yazılımla ya da doku matrisiyle hesaplanır (`TexMatrixMatcap`, N95'te çalışır) |
| `SombreadoPorPixelSoportado()` | Işık köşe başına kalır ve editör seçeneği sunmaz bile |
| `ShadersEscritos()` | Kendi shader'ı olan materyal her zamanki gibi çizilir (bkz. [Yazılan shader'lar](../editor/shaders.md)) |

## Durum önbelleği

Her durum değişikliği sürücüye bir maliyet getirir. Core neyin ayarlı olduğunu hatırlar: zaten açık olan bir şeyi açmak ya da zaten bağlı olan dokuyu bağlamak hiçbir çağrı üretmez. `IsEnabled` ve `GetMatrix` bu bellekten yanıt verir ve GPU'ya sormaz, çünkü N95'te böyle bir soru çizimin bitmesini beklemeye zorlar.

## Dokular

`LoadTexture` iki adımdan oluşur. **Decode** (dosyadan RGBA piksellere) sisteme bağlıdır: neredeyse hepsinde stb, Symbian'da ICL. **Upload** (piksellerden GPU'ya) ortaktır ve her arka uç, kendi çipinin formatına dönüştürür:

- **PSP:** 512'ye kadar ikinin kuvveti, "swizzled", 565 ya da 8888.
- **3DS:** Morton sırasında 8x8 bloklar, kenar başına 8'den 1024'e.
- **Dreamcast:** 8'den 1024'e ikinin kuvveti, 16 bit (565 ya da 4444), mipmap'siz.
- **N95:** yalnızca ikinin kuvveti; diğerleri yüklenmeden önce yeniden örneklenir.

Bir proje **16 bitlik** dokular (belleğin yarısı) isteyebilir ve, yalnızca N95'te, belirli bir boyuttan itibaren onları **yarı boyutta** yükleyebilir (`texturasMitadDesde`). `.w3dt` formatı GPU'ya hazır bir dokuyu mipmap'leriyle birlikte saklar: onu yüklemek, okuyup GPU'ya göndermekten ibarettir.

Dürüst bir ayrıntı: doku yükleme ve ışıklar hâlâ iki üç OpenGL fonksiyonunu doğrudan çağırıyor. OpenGL'i olmayan konsollarda arka uç, bunları uygulayan küçük bir katman getirir (`gu/w3dGLgu.cpp`, `citro3d/w3dGLc3d.cpp`).

## Ekran ve güvenli alan

`PantallaSetTam` ve `PantallaEsVertical`, ekranın dik mi yatık mı olduğunu (yatay = genişliği yüksekliğinden fazla) tek bir kez kaydeder; böylece motorun herhangi bir parçası bunu sorabilir. **Güvenli alan** (`SafeAreaGet`), her kenardan kaç pikselin arayüz için kullanılamayacağını söyler: çentik, hareket çubuğu, cep telefonu tarayıcısının çubukları. Arka plan tüm pencereye, arayüz ise güvenli alanın içine çizilir. PC'de, Symbian'da ve konsollarda kenarlar 0'dır; tarayıcıda ölçülür.

## En küçüğün sınırları

Core için yazılan şey, en sınırlı makineye sığmalıdır. Hissedilen sınırlar şunlardır:

| Sınır | Nerede | Core nasıl ele alır |
|---|---|---|
| 16 bitlik indeksler (mesh başına en fazla 65.535 köşe) | Symbian | `MeshIndex` orada 16, diğerlerinde 32 bittir; içe aktarıcı sığmayanı reddeder |
| Aynı anda 8 ışık | Hepsi (`LucesMax`) | Shader arka ucu onları aynı hesapla taklit eder; PSP donanımla 4 yapar, Android 2.3 en fazla 4 kullanır |
| Shader yok | Sabit arka uçlar | Shader fonksiyonları hayır der ve materyal her zamanki gibi çizilir |
| 16 bitlik Z | PSP | Yakın düzlem, N95'teki gibi yörünge mesafesine göre ayarlanır |
| Her şey için 16 MB | Dreamcast | `-Os` ile derlenir ve bağlayıcı kullanılmayanı atar; dokular 16 bit gider |
| C++03 | Symbian | Header'larda `nullptr`, `auto` ya da `override` yok (`W3D_OVERRIDE`); `tools/prueba_independencia.sh` her header'ı tek başına, C++03 ile derler |

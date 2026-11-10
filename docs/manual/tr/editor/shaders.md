# Yazılan shader'lar

**Yazılan shader**, bir malzemenin piksel piksel nasıl çizileceğine karar veren bir metin dosyasıdır (`.glsl`). Sıradan malzemede olmayan efektler içindir: arkasındakini bulanık gösteren buzlu cam, hareket eden su, sıcaklık dalgalanması, çizgi film görünümü, nabız gibi atan bir parıltı…

Sen yalnızca kendi kısmını yazarsın: bir `Fragmento()` fonksiyonu (her pikselin rengi) ve istersen bir `Vertice()` (köşeleri hareket ettirmek için). Geri kalan her şeyi Whisk3D ekler.

## Nerede çalışır

Aynı dosya tüm grafik arka uçlarında aynı şekilde çalışır:

| Arka uç | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0: Android, tarayıcı (WebGL), Nokia N8, **Grafik → OpenGL ES 2.0** seçili PC | GPU'da | GPU'da |
| Vulkan (PC) | GPU'da | GPU'da |
| PC'nin OpenGL'i (varsayılan olan), OpenGL ES 1.1 (Nokia N95), Nintendo 3DS, Dreamcast | **CPU'da** | Hayır: malzeme her zamanki gibi çizilir |

Shader'ı olmayan bir arka uçta hiçbir şey bozulmaz: **`@vertice` CPU'da çalışır** (dalgalı su N95'te de aynı şekilde hareket eder) ve renk normal malzemeden gelir. Malzeme kartı bunu bildirir: *"Bu grafik arka ucunda shader yok: @vertice bölümü CPU'da çalışır, malzemenin geri kalanı her zamanki gibi çizilir"*.

### CPU'da `@vertice`

Shader olmayan yerlerde Whisk3D, `@vertice`'in **aynı metnini** okuyup çizmeden önce köşe köşe çalıştıran bir yorumlayıcı içerir. Farklı bir şey yazman gerekmez. Köşeleri hareket ettirmek için kullanılanları anlar:

- `float`, `int`, `bool`, `vec2`, `vec3`, `vec4`; sabitler (`const float VELOCIDAD = 2.0;`) ve kendi fonksiyonların (`in`, `out`, `inout` ile);
- `if` / `else`, `for` (`break` ve `continue` ile), `return`, üçlü `? :` operatörü;
- operatörler, bileşenler (`pos.y`, `n.xz`, yazmak için de) ve kurucular (`vec3(...)`);
- GLSL'in sayı fonksiyonları: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`;
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

Matrisleri, dizileri, `while`'ı ve `#define`'ı anlamaz: kullanırsan hata bunu satırıyla birlikte söyler (GPU'da ise çalışırdı). Her karede örgünün tüm köşeleri üzerinde bir kez çalışır: N95'te küçük örgülerde kullanmak iyi olur (örnekteki su gibi birkaç yüz köşe). Örgünün **tüm** parçaları aynı shader'ı kullanıyorsa çalışır; yoksa örgü hareket etmeden çizilir.

> **Aynı dosya neden her yerde çalışır:** dosyada bir arka uca özgü hiçbir şey yoktur. Her arka uç önüne kendi kısmını ekler (`w3d_*` girdileri, hassasiyet, `main`). OpenGL ES 2.0 onu doğrudan derler; Vulkan onu kendi içinde SPIR-V'ye çevirir. Yarın Whisk3D'ye başka bir arka uç eklenirse shader'ın yine işe yarar.

## Bir malzemede shader kullanmak

1. Nesneyi seç ve Özellikler'de **Malzeme** sekmesini aç.
2. En altta **Gelişmiş: özel shader** kutusunu işaretle. Malzemenin henüz dosyası yoksa bir şablonla yeni bir dosya (`shader.glsl`) oluşturulur, hemen kullanılmaya başlar ve IDE'de açılır.
3. **Shader** içinde dosyayı seçersin:
   - projede zaten bulunan `.glsl` dosyaları;
   - **Yeni shader**: şablonlu başka bir dosya;
   - **Diskten yükle...**: bilgisayarındaki bir `.glsl`. Dokular ve script'ler gibi projenin içine kopyalanır.
4. **Shader**'ın sağındaki küçük düğme dosyayı **IDE**'de açar (yerleşimde bir IDE olması gerekir).
5. Yazarsın ve **Ctrl+S** kaydedip derler. Malzemedeki ya da IDE çubuğundaki **Derle** düğmesine de basabilirsin (dosya bir `.glsl` olduğunda dosya adının yanında görünür).
6. Sonuç **Derle**'nin altında görünür: **Derlendi** ya da satırıyla birlikte kırmızı hata.

Başlamak için projeyi kaydetmen gerekmez. Henüz kaydetmediysen shader editörün bir çalışma klasörüne yazılır; projeyi kaydettiğinde `.w3d`'nin içine, `shaders/` klasörüne girer ve malzeme ile IDE o kopyayı kullanmaya geçer. O andan itibaren projeyle birlikte taşınır (telefona, derlenmiş oyuna, başka bir bilgisayara). İşareti kaldırırsan malzeme yine her zamanki gibi çizilir; dosya seçili kalır.

Bir shader derlenmezse, sen düzeltene kadar nesne normal malzemesiyle çizilir. Editör çökmez.

### Kütüphane'den: bir shader'ı örgüye sürüklemek

Projenin shader'ları **Kütüphane**'de (Kütüphane modundaki Outliner), **Shader'lar** altında, kaç nesnenin kullandığıyla birlikte görünür. Oradan:

- 3D Görünüm'de **onu bir örgünün üzerine sürükle**: sen hareket ettirirken işaretçinin altındaki **örgü parçası** o shader ile çizilir, böylece hangi parçaya uygulayacağını görürsün. Başka bir parçaya geçersen ya da örgüden çıkarsan, parça kendi malzemesine döner. Bıraktığında o parçanın malzemesi shader'ı kullanmaya başlar (varsayılan malzemede parça yeni bir malzeme alır). **Ctrl+Z** ile geri alınır.
- Bir **malzeme** ile de aynısı: sürüklerken parça onunla görünür.
- Bir script gibi **IDE'de aç**, yeniden adlandırma, başka klasöre taşıma ya da `.w3d`'nin dışına çıkarma.
- **Bilgisayarının dosya gezgininden** pencereye sürüklenen bir `.glsl` projeye girer; bir örgünün üzerine bırakırsan ayrıca o parçaya uygulanır.

## Dosya nasıl görünür

```glsl
// yorumlar // ya da /* */ ile yazılır

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // isteğe bağlı: köşeleri hareket ettirmek
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** piksel kodunun nerede başladığını, **`@vertice`** ise köşe kodunun nerede başladığını işaretler. İkisi de isteğe bağlıdır: işaretsiz bir dosyanın tamamı fragment'tır.
- **`vec4 Fragmento()`** zorunludur: pikselin rengini döndürür (kırmızı, yeşil, mavi, alfa; 0'dan 1'e).
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** isteğe bağlıdır. Köşenin konumunu ve normalini **nesne** uzayında (Y yukarı, metre cinsinden) alır ve bunları değiştirebilirsin.
- Kendi fonksiyonlarını ve sabitlerini (`const float VELOCIDAD = 2.0;`) kullandığın yerin üstüne yazabilirsin.
- Dil **GLSL ES 1.00**'dır, WebGL 1'inki. `float`, `vec2/3/4`, `mat3/4`, `sin`, `cos`, `mix`, `clamp`, `pow`, `dot`, `normalize`, `reflect`, `refract`, `fract`, `smoothstep` ve GLSL'in geri kalanı elinde.
- `main` **yazma**; `uniform`, `attribute`, `varying` ya da `precision` da tanımlama: bunları Whisk3D ekler.
- Hatalar **senin dosyandaki** satırı söyler (örneğin `0(4)` ya da `w3d.frag:4`, 4. satırdır).

## Kullanabileceklerin

### `Fragmento()` içinde

| Ad | Tür | Nedir |
|---|---|---|
| `w3d_uv` | `vec2` | Noktanın UV koordinatları (ilk UV katmanından). |
| `w3d_Normal()` | `vec3` | Noktanın kamera uzayındaki, zaten normalize edilmiş normali. Arka yüz görünüyorsa öbür yöne bakar. |
| `w3d_normal` | `vec3` | Normalize edilmemiş, çevrilmemiş normal. |
| `w3d_posVista` | `vec3` | Noktanın kamera uzayındaki konumu (kamera 0,0,0'da, −Z'ye bakar). |
| `w3d_HaciaCamara()` | `vec3` | Noktadan kameraya doğru yön. |
| `w3d_HaciaLuz()` | `vec3` | Noktadan ilk ışığa doğru yön. |
| `w3d_Luz(n)` | `vec3` | `n` normaliyle gelen ışık: ortam ışığı artı ilk ışık (dağınık). |
| `w3d_luzColor` | `vec4` | İlk ışığın rengi (şiddetiyle birlikte). |
| `w3d_luzPos` | `vec4` | İlk ışığın kamera uzayındaki konumu (`w` = 0: bir yöndür, güneş). |
| `w3d_ambiente` | `vec4` | Ortam ışığı. |
| `w3d_color` | `vec4` | Malzemenin **Renk** değeri (alfasıyla). |
| `w3d_emision` | `vec4` | Malzemenin **Işıma** değeri. |
| `w3d_Textura(uv)` | `vec4` | Malzemenin o UV'lerdeki dokusu (dokusu yoksa beyaz). `v` görüntünün **üstünden** başlar (0 = en üst satır, 1 = en alttaki). |
| `w3d_colorVertice` | `vec4` | Köşe rengi (örgüde varsa ve malzeme **Köşe rengi** kullanıyorsa). |
| `w3d_rugosidad`, `w3d_metalico` | `float` | Malzemenin **Pürüzlülük** ve **Metalik** değerleri. |
| `w3d_ior`, `w3d_transmision` | `float` | Malzemenin **IOR** ve **Geçirgenlik** değerleri (ışın izlemeninkiler). |
| `w3d_tiempo` | `float` | Saniyeler, animasyon için. Editörde **zaman çizelgesinin** zamanıdır (geçerli kare ÷ FPS): duraklatıldığında sabit kalır, **Oynat** ile ilerler ve zaman çizelgesini sürükleyerek herhangi bir anı görebilirsin. Dışa aktarılan oyunda, oyunun başlamasından beri geçen zamandır. |
| `w3d_PuntoPantalla()` | `vec2` | Pikselin görünümdeki yeri: (0,0) sol alt, (1,1) sağ üst. |
| `w3d_Pixel()` | `vec2` | Bir pikselin aynı birimlerdeki boyutu. |
| `w3d_Pantalla(p)` | `vec4` | Görünümün `p` noktasında **zaten çizilmiş olan**. Aşağıya bak. |
| `w3d_AMundo(v)` | `vec3` | Bir yönü kamera uzayından **dünya** uzayına (Y yukarı) çevirir. Kamerayla birlikte dönmeyen bir ortam haritasını okumak için kullanılır (bkz. *Ortam yansıması*). |

### `Vertice()` içinde

`pos` ve `normal` (nesnenin) ve `w3d_tiempo`.

## Arkadakini görmek: `w3d_Pantalla`

Ekranı `w3d_Pantalla` ile okuyan bir malzeme nesnenin arkasındakini gösterebilir; onu kaydırabilir, bulanıklaştırabilir ya da renklendirebilir. Cam, su, sıcak hava ya da bir kuvvet alanı böyle yapılır.

- Ekranı okuyan bir shader'a sahip örgüler, arkadaki şey zaten çizilmiş olsun diye sahnenin geri kalanından sonra, **en son** çizilir. Outliner'da nerede durdukları önemli değildir.
- Her birinden önce görünüm bir dokuya kopyalanır (her örgü ve her örgü parçası için bir kopya). Bir PC'de ya da modern bir telefonda ucuzdur. Eski bir telefonda bu tür nesneleri az tutmak iyi olur.
- **Parçacıklar** sonra çizilir, bu yüzden camın arkasından görünmezler.
- Malzemenin **Saydam** **olmaması** iyi olur: shader arkadakini zaten karıştırır ve alfa 1 döndürür.

## Örnekler

Bunların hepsi deponun **`examples/shaders/`** klasöründe dosya olarak, dokuları `examples/shaders/texturas/` içinde, ve **`examples/shaders/Ejemplos_Shaders.w3d`** projesinde nesnelere uygulanmış olarak bulunur: onu aç ve dene. `.glsl` dosyalarını kendi projene de sürükleyebilirsin.

| Dosya | Ne yapar |
|---|---|
| `vidrio_esmerilado.glsl` | Her şekil için cam: arkadaki bulanık ve bükülmüş. |
| `vidrio_plano.glsl` | Bir **düzlem** için buzlu cam (bir pencere, bir cam bölme); doku nerenin buzlu olduğunu söyler. |
| `reflejo_entorno.glsl` | Ortamın panoramik bir fotoğrafını (doku) yansıtan metal ya da plastik. |
| `reflejo_matcap.glsl` | En ucuz yansıma: boyanmış bir küre içeren bir doku. |
| `latido.glsl` | Nabız gibi atan bir parıltı (`w3d_tiempo`). |
| `dibujo_animado.glsl` | Üç tonda ışık. |
| `agua.glsl` | `@vertice` ile dalgalar ve alttakinin kaydırılması. |
| `bandera.glsl` | Dalgalanan bir bayrak (yalnızca `@vertice`: N95'te de çalışır). |
| `respirar.glsl` | Normalleri boyunca şişip söner (yalnızca `@vertice`). |
| `torcer.glsl` | Dikey eksen etrafında burulur (yalnızca `@vertice`). |
| `saltar.glsl` | Zemine değince zıplar ve yassılaşır (yalnızca `@vertice`). |

Köşe örnekleri, N95 için hazırlanmış **`examples/shaders/Ejemplos_Vertex_N95.w3d`** içinde küçük örgülere uygulanmıştır (orada `@vertice` CPU'da çalışır). Zaman çizelgesinin **Oynat** düğmesiyle hareket ederler.

### Nabız gibi atan bir renk

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

`w3d_tiempo` kullanan bir shader, zaman çizelgesinin **Oynat** düğmesiyle hareket eder (bir animasyon gibi); duraklatıldığında sabit kalır ve editör gereksiz yere yeniden çizmez. Dışa aktarılan oyunda kendi kendine hareket eder.

### Çizgi film (üç ton)

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### Hareket eden su (`@vertice`)

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // alttaki, biraz kaydırılmış
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

Dalganın fark edilmesi için düzlemin köşelere ihtiyacı var: onu Düzenleme Modunda alt bölümlere ayır.

### Buzlu cam

Bu, `examples/shaders/vidrio_esmerilado.glsl` dosyasıdır (örnek projesindeki cam küre). Arkadaki bulanık ve biraz bükülmüş görünür:

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // kırılma: ekran normale göre kaydırılarak okunur
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // bulanıklık: çevredeki 13 okumanın ortalaması; pürüzlülük daireyi genişletir
    vec2 r = w3d_Pixel() * (1.0 + 18.0 * w3d_rugosidad);
    vec3 c = w3d_Pantalla(p).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 2.5,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  2.5)).rgb;
    c += w3d_Pantalla(p + r * vec2(-2.5, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0, -2.5)).rgb;
    c /= 13.0;

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // camın rengiyle renklenmiş
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // ışığın parlaması ve daha açık kenar
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

Malzemeden yönetilir: **Geçirgenlik** (0 = malzeme renginde opak cam, 1 = arkadaki her şey görünür), **IOR** (arkadakinin ne kadar büküldüğü) ve **Pürüzlülük** (ne kadar bulanıklaştığı). Bunlar **ışın izlemenin** kullandığı değerlerle aynıdır, bu yüzden Render Önizleme'de aynı nesne gerçek camdır.

### Düz buzlu cam

Bir **düzlem** için: bir pencere, bir cam bölme, bir cam ekran. Dosya `examples/shaders/vidrio_plano.glsl`. Arkadakinin 17 okumasının ortalamasını iki halka üzerinde alır; **Pürüzlülük** halkaları genişletir. Malzemenin **dokusu** *nerenin* buzlu olduğunu söyler: beyaz = bulanık, siyah = pürüzsüz. `texturas/escarcha.png` ile lekeler, küçük temiz bir pencere ve aşağı süzülen damlalar oluşur; dokusuz bütün cam buzludur. Düzlemi iki taraftan da görmek için **Culling**'i kapatmak iyi olur.

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // BU noktanın ne kadar bulanıklaştığı
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // okumaların yarıçapı
```

### Ortam yansıması (yansıtmak için doku nasıl kullanılır)

Dosya `examples/shaders/reflejo_entorno.glsl`. **Malzemenin dokusu**, nesneyi çevreleyenlerin **panoramik** bir fotoğrafıdır: genişliği yüksekliğinin iki katı olan, çevrede 360° ve yukarıdan aşağıya 180° kapsayan *eşdikdörtgen* (equirectangular) bir görüntü (`texturas/entorno.png` gibi: gökyüzü, güneş, binalar ve damalı bir zemin). Her piksel için:

1. `reflect(-ojo, n)`, kamera uzayında **bakışın nereye sektiğini** söyler.
2. `w3d_AMundo(...)` bu yönü **dünyaya** çevirir; böylece kamera dönse bile yansıma yerinde kalır.
3. `Panorama(...)` yönü fotoğrafta bir noktaya çevirir: çevredeki açı (`atan`) `u`, yukarıdan aşağıya açı (`acos`) ise `v` olur.
4. Yüzeyle **Fresnel**'e (yandan bakınca daha çok yansır) ve **Metalik**'e (bir metal her şeyi, kendi **Renk**'i ile renklenmiş olarak yansıtır) göre karıştırılır.

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // çevre etrafında tur (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // yukarıdan (0) aşağıya (1)
    return vec2(u, v);
}

vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();
    vec3 r = reflect(-ojo, n);
    vec3 entorno = w3d_Textura(Panorama(normalize(w3d_AMundo(r)))).rgb;
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(n, ojo), 0.0), 5.0);
    float cuanto = mix(fresnel, 1.0, w3d_metalico);
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 reflejo = entorno * mix(vec3(1.0), w3d_color.rgb, w3d_metalico);
    return vec4(mix(superficie, reflejo, cuanto), 1.0);
}
```

Herhangi bir 2:1 panoramik fotoğraf işe yarar (bir telefonun 360° fotoğrafları ya da PNG'ye çevrilmiş HDRI'lar).

### Matcap yansıması

Dosya `examples/shaders/reflejo_matcap.glsl`: tek bir doku okuması, her GPU'da çalışır. Doku **boyanmış bir küredir** (`texturas/matcap.png`); her piksel, kürede normaliyle aynı yöne bakan noktanın rengini alır. Yansıma kamerayı izler (dünyayı değil).

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // (dokunun v değeri üstten başlar: y ters)
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## Bir rutinde: **Shader** adımı

Yazılan bir shader bir **GL durumu** gibidir: ayarlanır ve sonra çizilen her şey onu kullanır. Bir **rutinde** (Ekle → Draw → **Shader**) adımın bir `.glsl` **dosyası**, bir **On** kutusu ve dört sayısı vardır: **IOR**, **Transmission**, **Roughness** ve **Metallic** (`w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico` olarak gelirler; canlandırılabilir ya da bir bellekten okunabilirler).

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← artık shader yok
```

**On** ile sonraki "Draw elements" adımları shader'ı kullanır; kutu kapalıyken shader kaldırılır. Rutin bitince kendiliğinden kaldırılır. **OpenGL ES 2.0 ve Vulkan**'da çalışır; OpenGL ES 1.1'de ve sabit GL'de adım hiçbir şey yapmaz (bir rutinin çizimleri doğrudan örgü verisini gösterir: CPU'daki `@vertice` malzemeler içindir). Ekranı okuyan bir shader (`w3d_Pantalla`), rutinin ve sahnenin o adımdan **önce** çizdiklerini görür: sırayı sen belirlersin.

## Işın izleme ile

Işın izleme senin shader'ını çalıştırmaz. Malzemenin değerlerini kullanır: **Renk**, **Pürüzlülük**, **Metalik**, **Geçirgenlik**, **IOR** ve doku. Shader'ında bu aynı değerleri kullanırsan (buzlu cam gibi), Malzeme Önizleme ile Render Önizleme aynı şeyi gösterir.

**Geçirgenlik** (0'dan 1'e, ışın izleme açıkken Malzeme kartında) yüzeyin ne kadarının cam olduğunu söyler: ışık **IOR** ile bükülerek geçer ve **Pürüzlülük** ile buzlu çıkar. **Alfa** başka bir şeydir: alfa (ya da haritası) yüzünden saydam olan kısım yoktur ve oradan arkadaki düz, bükülmeden görünür. Alfa %50 ve geçirgenlik 1 iken yarısı düz, öbür yarısı kırılarak geçer. Geçirgenlik 0 iken IOR, önceki sürümlerde olduğu gibi alfanın arasından görüneni büker.

## (Henüz) yapmadıkları

- Tek bir ışık kullanır: sahnedeki ilk ışık (ya da önizlemeninki).
- **Sisi** ve aynaların **kırpma düzlemlerini** almaz.
- **Katı**'da ve hata ayıklama görünümlerinde (normaller, alfa, vektörler) normal malzeme görünür.
- Kendine ait parametreleri yoktur: malzemeninkileri kullanır (renk, pürüzlülük, metaliklik, geçirgenlik, IOR, ışıma).

## Sık görülen hatalar

| Mesaj | Ne oluyor |
|---|---|
| `undefined variable` / `undeclared identifier` | Yanlış yazılmış ya da var olmayan bir ad (`w3d_Normal()` parantezleriyle mi?). |
| `cannot convert` / `wrong operand types` | Türler karışmış: `1` bir `int`'tir, `1.0` yaz. Bir `vec3` bir `vec4` ile toplanamaz. |
| `Fragmento: no matching overloaded function` ya da `Fragmento` olmadan derlenmiyor | `vec4 Fragmento()` fonksiyonu eksik ya da başka bir tür döndürüyor. |
| `redefinition of main` | `main` yazma: onu Whisk3D ekler. |
| Arkadaki görünmüyor | Shader `w3d_Pantalla` kullanıyor ama malzeme düşük alfayla **Saydam** durumda, ya da arkadaki daha sonra çiziliyor (parçacıklar). |

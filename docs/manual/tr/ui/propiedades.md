# Kartlar ve özellikler

## Kart

`GroupPropertie` (`Propieties/GroupPropertie.h`), bir başlığı ve satırları olan bir karttır. Her satır bir özelliktir: `PropertieBase`'den türeyen ve senin bir değişkenine işaret eden bir sınıf. Kart onu gösterir ve orada, yerinde değiştirir.

```cpp
GroupPropertie* jugador = new GroupPropertie("Jugador");
jugador->icono = IconType::gamepad;        // isteğe bağlı: ok ile başlık arasında

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
jugador->properties.push_back(vel);

jugador->Resize(ancho, 0);    // yüksekliği kart hesaplar
jugador->Render();            // o anki başlangıç noktasında
```

Kart satırlarının sahibidir: kart silindiğinde satırları da siler.

- Başlıkta onu katlamak için bir ok bulunur (`open`). `flecha = false` ile ok olmaz: yüzen bir pencere için.
- Altında iki sütun vardır: solda etiket, sağda değer. `anchoValores`, genişliğin değere ayrılan payıdır: varsayılan 0.5.
- `reservaKeyBtn = true`, en sağda animasyon eşkenar dörtgeni için bir sütun ayırır.
- Satırların neredeyse hepsi bir satır yüksekliği artı bir `gap` ölçer. Değişkeni olmayan (`value` değeri `NULL`) bir sayı, onay kutusu ya da renk yer kaplamaz ve çizilmez.
- `visible = false` kartın tamamını gizler.

Seçilen satırın etiketi `accent`, değeri beyaz ve kenarlığı beyazdır; düzenlenirken değer ve kenarlık `accent` olur. Farenin altındaki satır beyazdır: onu kartı içeren taraf `PropHoverGroup` ve `PropHoverFila` ile işaretler.

## Seçmek ve düzenlemek

`selectIndex` seçilen satırdır: `-2` hiçbiri, `-1` başlık, `0` ve sonrası bir satır.

| Fonksiyon | Ne yapar |
|---|---|
| `NextSelect()`, `PrevSelect()` | Alttaki ya da üstteki satıra geçer. Boşlukları ve değişkeni olmayan satırları atlar. Karttan çıkarken `true` döndürür; bir sonraki karta geçmek için |
| `EnterPropertieSelect()` | Başlıkta kartı katlar ya da açar. Bir satırda onu düzenler |
| `button_up()`, `button_down()`, `button_left()`, `button_right()` | Düzenleme sırasında satıra gider |
| `Cancel()` | Satırı olduğu gibi bırakır |

`editando`, düzenlenmekte olan bir satır olup olmadığını söyler. Bununla oklar ya değeri değiştirir ya da seçimi hareket ettirir:

```cpp
if (jugador->editando) jugador->button_up();   // değeri değiştirir
else jugador->PrevSelect();                    // üstteki satır
```

Fareyi kartı içeren taraf çözer: kart ekranda nereye yerleştiğini bilmez. Üstte kenarlık (`borderGS`) ve başlık (`RenglonHeightGS + gapGS`) bulunur. Her satır, kendi `Resize(ancho)`'sunun döndürdüğü kadar ölçer.

## Satırlar

| Sınıf | Nedir | Nasıl düzenlenir |
|---|---|---|
| `PropFloat` | Bir sayı | Oklarla ya da yazarak |
| `PropBool` | Sağa yaslanmış bir onay kutusu | OK onu açar ya da kapatır |
| `PropColor` | Sağa yaslanmış, o renkte bir kare | OK düzenlemeye girer ve iptal edilirse diye rengi saklar |
| `PropButton` | Bir düğme | OK eylemini çağırır |
| `PropButtonRow` | Bir satırda birkaç düğme | Sol ve sağ birini seçer; OK ona basar |
| `PropText` | Bir metin kutusu | OK yazmak için ona odaklanır |
| `PropListMeshParts` | Kaydırmalı bir liste | Yukarı ve aşağı seçer |
| `PropLabel` | Okumak için bir ya da birkaç satırlık bir metin | Seçilmez |
| `PropSeparator` | Bir çizgi | Seçilmez |
| `PropGap` | Bir `gap` boyutunda boşluk | Seçilmez |
| `PropSelectText` | Etiketli, değersiz bir kutu | Henüz hiçbir şey yapmaz |

### Sayı

```cpp
PropFloat* vida = new PropFloat("Vida");
vida->value = &vidaJugador;
vida->SetRango(0, 100);           // bunun dışına çıkmaz
vida->entero = true;              // tam sayılarla gösterilir ve ilerler
vida->stepFino = 1;               // yukarı ve aşağı
vida->stepGrueso = 10;            // sol ve sağ
vida->onChange = AlCambiarVida;   // void AlCambiarVida()
```

| Alan | Ne yapar |
|---|---|
| `unit` | Sayıya bitişik bir metin, örneğin `°`. Yapıcının ikinci argümanıdır |
| `stepFino`, `stepGrueso` | Okların eklediği miktar: varsayılan 0.01 ve 0.1 |
| `entero` | Sayıyı yuvarlanmış gösterir ve oklar yuvarlanmış değerden başlar. Bu olmadan dört ondalık basamak gösterir |
| `centrado` | Sayı kutusunun ortasında |
| `flechas` | Yanlara `<` ve `>` çizer. Bunlara tıklamayı kapsayıcı çözer |
| `opciones`, `nOpciones` | `entero` ile: sayı bir indekstir ve o seçeneğin adı, `W3dMenuTraducir` ile çevrilmiş olarak gösterilir |
| `acelera` | Sol ve sağ 1'er 1'er başlar ve tekrarlanırsa 10'a kadar çıkar |
| `dragStep` | Sürüklerken piksel başına ne kadar değiştiği. Sürüklemeyi kapsayıcı yapar |

`Set(v)` değeri aralığın içinde atar ve `onChange`'i çağırır. Oklar da bundan geçer.

Yazarak girmek için:

```cpp
vida->IniciarEdicionTexto();   // değeri tamamı seçili olarak gösterir
// tuşlar g_textFieldActivo'ya gider: bkz. Girdi ve metin kutuları
NumEditCommit();               // Enter: uygular
NumEditCancel();               // Esc: olduğu gibi kalır
```

`NumEditActivo()`, düzenlenmekte olan bir sayı olup olmadığını söyler. Sayı olmayan bir metin 0 değerini alır.

### Onay kutusu

`value` bir `bool`'a işaret eder. `onChange` her değiştiğinde çağrılır. Etiket solundaki tüm alanı kaplar ve sığmazsa `..` ile kesilir.

### Renk

`value` dört `float`'a (RGBA) işaret eder. Kareyi UI çizer; renk seçici UI'a ait değildir. Düzenleme sırasında oklar yalnızca ilk bileşeni hareket ettirir.

### Düğme

`action` çağırdığı fonksiyondur. `gris = true` onu soluklaştırır ve hiçbir şey yapmaz. `oculto = true` satır kaplamaz.

`conLabel = true` ile satır bir ad ve bir düğmedir: ad solda, metninin genişliğinde ve satırın en fazla %55'i kadar, düğme ise geri kalanın tamamında. `labelChars`, birkaç satırı hizalamak için adın genişliğini sabitler. `button->desplegable` ile bir menünün küçük okunu taşır. Ayrıca:

- `AgregarPrefijo(accion, ayuda)` bir rengi göstermek için düğmenin önüne bir kare koyar.
- `AgregarExtra(icono, accion, ayuda)` sağa, yalnızca simgeli en fazla iki küçük düğme koyar. `slotsExtras` onlar için yer ayırır; böylece satırlar arasında hizalı kalırlar. `ExtraEnX(x)`, tıklama için o x'te hangisinin bulunduğunu söyler.

### Düğme satırı

```cpp
PropButtonRow* fila = new PropButtonRow();
fila->Agregar("Aceptar", Aceptar);
fila->Agregar("Cancelar", Cancelar);
jugador->properties.push_back(fila);
```

Düğmeler genişliği eşit parçalara bölüşür. Görünmez olanlar atlanır ve diğerleri onların yerini paylaşır. `Disparar(i)`, `i` düğmesine basar: tıklama o parçaya düştüğünde onu kapsayıcı çağırır.

### Metin kutusu

```cpp
PropText* nombre = new PropText("Nombre", "Jugador 1");
// nombre->field.text yazılanı tutar
```

`onClick`: düzenlemek yerine o fonksiyonu çağırır, örneğin bir dosya gezgini açmak için. `error = true`, bir şey yazılana kadar seçilen satırın kenarlığını kırmızıya boyar. `oculto = true` satır kaplamaz. OK onu, bugün editörde tanımlı olan `TextFieldEnfocar` ile odaklar. Yazmak için bkz. [Girdi ve metin kutuları](entrada.md).

### Metin

`PropLabel("texto")` tek bir satırdır. `PropLabel("texto", true)` genişliğe sığması için boşluklardan böler; tek başına sığmayan bir sözcük bütün kalır. `rojo = true`, bir hata için. `oculto = true` satır kaplamaz.

### Liste

`PropListMeshParts` varsayılan olarak 3 olan `filasMax` kadar satır gösterir; daha fazlası varsa küçük bir çubukla. Seçilen her zaman görünür kalır. `Cancel`, `selectIndex`'i düzenlemeye girerken sahip olduğu değere döndürür.

Henüz herhangi bir liste için kullanılamaz: `modo`'ya göre sahneden bir liste okur (bir mesh'in parçaları, UV haritaları, renk katmanları, değiştiriciler, köşe grupları, kemikler, animasyonlar, kısıtlamalar, script'ler) ya da onu editörün fonksiyon kancalarından ister (`MixFila*`, `RutinaFila*`, `PostFila*`).

## Animasyon eşkenar dörtgeni

Canlandırılabilir bir satır (`PropFloat`, `PropBool`, `PropColor` ve `PropButton` içinde `animProp` değeri 0 ve üzeri), kartta `reservaKeyBtn` varsa sağda bir eşkenar dörtgen gösterir. Rengi durumu söyler:

| Durum | Renk | Anlamı |
|---|---|---|
| 0 | Gri | Animasyon yok |
| 1 | `accent`, loş | Canlandırılmış, burada anahtar kare yok |
| 2 | `accent` | Burada bir anahtar kare var |
| 3 | Yeşil | Değer değişti ve kaydedilmedi |

Eşkenar dörtgen UI'a aittir; hangi durumda olduğu değil. Durumu, programın kurduğu bir kancaya sorar ve tıklama diğerini çağırır:

```cpp
W3dKeyframeEstado = MiEstado;   // int MiEstado(int prop, int comp): 0'dan 3'e
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp): anahtar kareyi koyar ya da kaldırır
```

Kancalar olmadan eşkenar dörtgen gri kalır. Kart `W3dKeyframeToggle`'ı çağırmaz: onu tıklamayı çözen taraf çağırır. `g_kfFocoProp`, eşkenar dörtgeni klavye odağına sahip olan satıra işaret eder ve onu beyaz bir kenarlıkla belirtir. `W3dRenderRomboKey(estado)` tek başına bir eşkenar dörtgen çizer.

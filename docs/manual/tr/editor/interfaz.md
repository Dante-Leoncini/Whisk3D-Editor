# Arayüz

> [!NOTE]
> ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 fare ve klavyeyi (Bluetooth ile) destekler. Bu sayfanın Nokia ile ilgili kısımları, onu fare ve klavye olmadan, yalnızca telefonun tuşlarıyla nasıl kullanacağını anlatır.

Whisk3D penceresi **viewport**'lara bölünür: her birinin kendi editörü olan paneller (3D Görünüm, Outliner, Özellikler...). Onları bölebilir, boyutlarını değiştirebilir ve her birinin ne göstereceğini seçebilirsin. Fareyle, dokunmatik ekranda parmakla ve bir Nokia'nın tuş takımıyla aynı şekilde çalışır.

Program ilk kez açıldığında görünen ekran bu:

![Yeni açılmış Whisk3D: küp, kamera ve ışıkla 3D Görünüm; sağda Outliner ve Özellikler; altta Zaman Çizelgesi](../tutoriales/img/inicio.webp)

## Etkin viewport

Farenin altındaki viewport **etkin viewport**'tur ve yeşil bir kenarlıkla işaretlenir. Klavye komutlarını, oyun kumandalarını ve diğer tüm çevre birimlerini o alır. Fareyi bir viewport'tan diğerine götürdükçe yeşil kenarlık onu takip eder:

![Fare imleci 3D Görünüm, Outliner, Özellikler ve Zaman Çizelgesi üzerinden geçiyor ve yeşil kenarlık her seferinde altındaki viewport'u işaretliyor](../tutoriales/img/viewports-activo.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **Nokia N95'te:** yeşil **arama tuşuna** ![](../tutoriales/img/tecla-verde.svg) her basışta bir sonraki viewport'a geçilir. N95'in ekranına iki viewport sığar: solda 3D Görünüm, sağda Özellikler.

![Solda 3D Görünüm, sağda Özellikler olan bir Nokia N95: yeşil tuşa her basıldığında yeşil kenarlık diğer viewport'a geçiyor](../tutoriales/img/viewports-activo-nokia.webp)

## Menü çubuğu ve köşe düğmesi

Her viewport'un üstünde, o editörün menülerini ve düğmelerini içeren bir **menü çubuğu** (görselde yeşil) vardır. Sol üstte her zaman **köşe düğmesi** (turuncu) bulunur: viewport'u değiştiren ya da düzenleyen düğme budur.

![Büyütülmüş köşe düğmesi: 3D Görünüm'ün menü çubuğunun sol üstündeki aşağı bakan küçük ok](../tutoriales/img/viewports-esquina.webp)

Tüm viewport'larda aynıdır ve hepsinde bulunan tek düğme odur.

![Whisk3D'nin dört viewport'u; her birinin menü çubuğu yeşille, köşe düğmesi turuncuyla vurgulanmış](../tutoriales/img/viewports-barras.webp)

## Boyutu değiştirme

![PC](../tutoriales/img/icono-pc.svg) ![Android](../tutoriales/img/icono-android.svg) Boyutu değiştirmek için köşe düğmesini (parmağınla ya da fareyle) **basılı tut** ve sürükle: bu, o viewport'un sol ve üst kenarını hareket ettirir; yanındakiler de buna göre ayarlanır. Sürüklemeden **tek bir dokunuş** ise viewport'un seçeneklerini içeren açılır menüyü açar. Burada sağ alttaki viewport'un düğmesi daireler çizerek sürükleniyor ve üç viewport da boyut değiştiriyor:

![Bir parmak sağ alttaki viewport'un köşe düğmesini daireler çizerek sürüklüyor ve üç viewport da boyut değiştiriyor](../tutoriales/img/viewports-arrastrar.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **Nokia'da:** yeşil **arama tuşunu** ![](../tutoriales/img/tecla-verde.svg) basılı tut ve bir ok tuşuna bas: etkin viewport'un (yeşil çerçeveli olanın) kenarı o yöne kayar, böylece viewport büyür ya da küçülür. Yeşil tuş tek başına, ok tuşu olmadan, bir sonraki viewport'a geçer.

![Yeşil tuşu basılı tutulan bir Nokia N95: sağ ok tuşuyla 3D Görünüm büyüyor, sol ok tuşuyla küçülüyor ve sonra eski haline dönüyor](../tutoriales/img/viewports-tamano-nokia.webp)

## Viewport seçenekleri

Köşe düğmesine bir kez dokununca şu menü açılır:

![Bir parmak 3D Görünüm'ün köşe düğmesine gidiyor, ona dokunuyor ve menü açılıyor: üstte viewport türleri, çizginin altında Genişlet, Sütunlara böl, Satırlara böl ve Büyüt](../tutoriales/img/viewports-menu.webp)

Üstte **viewport türleri** vardır: birini seçmek viewport'u o editöre dönüştürür. Çizginin altında viewport'la yapılabilecekler bulunur:

| Seçenek | Ne yapar |
|---|---|
| Genişlet | Viewport yanındakinin yerini alır, yanındaki kapanır (yanındaki bölünmüşse o grubun tamamı kapanır). |
| Sütunlara böl | Onu yan yana iki parçaya böler. |
| Satırlara böl | Onu üst üste iki parçaya böler. |
| Büyüt | Onu tüm pencereyi kaplayacak şekilde büyütür. Büyütülmüşken menüde yalnızca **Küçült** vardır; o da viewport'u yerine geri döndürür. |

### Türü değiştirme

UV Düzenleyici'ye dönüştürülen 3D Görünüm:

![3D Görünüm'ün köşe düğmesine dokunuluyor, UV Düzenleyici seçiliyor ve viewport bir UV Düzenleyici'ye dönüşüyor](../tutoriales/img/viewports-cambiar-tipo.webp)

### Sütunlara ve satırlara bölme

![Sütunlara böl: 3D Görünüm yan yana iki parçaya bölünmüş](../tutoriales/img/viewports-dividir-columnas.webp)

![Satırlara böl: 3D Görünüm üst üste iki parçaya bölünmüş](../tutoriales/img/viewports-dividir-filas.webp)

### Genişlet

Outliner, Özellikler'in yerini alır:

![Outliner'da Genişlet seçiliyor ve Outliner, Özellikler'in yerini de kaplıyor](../tutoriales/img/viewports-expandir.webp)

### Büyüt ve Küçült

![3D Görünüm tüm pencereyi kaplayacak şekilde büyüyor, ardından Küçült ile yerine dönüyor](../tutoriales/img/viewports-maximizar.webp)

## Menü çubuğunun tüm seçenekleri görünmediğinde

Viewport darsa menü çubuğu tamamen sığmaz. Aynı şey 3D Görünüm'ün alttaki araç çubuğunda da olur. Geri kalanını görmek için:

- ![Android](../tutoriales/img/icono-android.svg) **Parmakla:** çubuğu yana doğru sürükle.
- ![PC](../tutoriales/img/icono-pc.svg) **Fareyle:** fare düğmesini çubuğun üzerinde basılı tutup sürükle ya da çubuğun üzerinde **tekerleği** çevir.
- ![PC](../tutoriales/img/icono-pc.svg) ![Nokia](../tutoriales/img/icono-nokia.svg) **Klavyeyle:** PC'de <kbd>M</kbd> ya da Nokia'da **sol seçim tuşu**, etkin viewport'un çubuğuna girmeni sağlar. Sol ve sağ ok tuşlarıyla çubukta gezersin; her menü üzerine gelince açılır. Yukarı ve aşağı ile menüde hareket eder, OK ile (PC'de <kbd>Enter</kbd>) seçersin. Çubuk, nerede olduğunu göstermek için kendiliğinden kayar.

![Bir parmak Outliner'ın çubuğunu yana doğru sürüklüyor ve sığmayan düğmeler beliriyor](../tutoriales/img/viewports-barra-dedo.webp)

![Fare işaretçisi Outliner'ın çubuğunun üzerinde: tekerlekle çubuk kayıyor](../tutoriales/img/viewports-barra-rueda.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) Bir Nokia N95'te: sol seçim tuşu çubuğa girer, ok tuşları da çubukta gezer.

![Nokia N95 arayüzü: sol seçim tuşu Mod menüsünü açıyor, sağ ok tuşu da çubuğun menülerinden sırayla geçiyor; çubuk kendiliğinden kayıyor](../tutoriales/img/viewports-barra-nokia.webp)

## Viewport türleri

| Viewport | Ne işe yarar |
|---|---|
| [3D Görünüm](vista-3d.md) | Sahnenin görüntülendiği ve düzenlendiği yer: modelleme, nesneleri taşıma, boyama, poz verme ve animasyon. |
| [Outliner](outliner.md) | Sahnedeki her şeyin listesi. |
| [Kütüphane](outliner.md#kütüphane-görünümü) | Projenin kaynakları: örgüler, malzemeler, dokular, animasyonlar, sesler, script'ler ve daha fazlası. |
| [Özellikler](propiedades.md) | Etkin nesnenin verileri, sekmeler hâlinde. |
| [UV Düzenleyici](editor-uv.md) | Dokunun örgünün üzerine nasıl oturduğu. |
| [Zaman Çizelgesi](linea-de-tiempo.md) | Animasyon: anahtar kareler ve eğriler. |
| [2D Düzenleyici](editor-2d.md) | Oyun arayüzleri, HUD'lar ve 2D oyunlar. |
| [Konsol](consola.md) | Motorun log kayıtları: hatalar, uyarılar ve bilgiler. |
| [IDE](editor-de-scripts.md) | Lua script'leri ve shader'lar için editör. |


Whisk3D büyümeye devam ediyor: zamanla yeni viewport'lar eklenecek, mevcut olanlar da değişebilir. Hepsi aynı şekilde çalışacak: üstte menü çubuğu, köşede düğme.

## Arayüzün diğer işlevleri

| İşlev | Açıklama | Nasıl kullanılır |
|---|---|---|
| Klavyeyle menü çubuğu | Etkin viewport'un menü çubuğunu açar, ok tuşlarıyla gezebilirsin. | <kbd>M</kbd> (Düzenleme Modu'nda <kbd>M</kbd>, Kaynaştır işlevidir). |
| Geri Al / Yinele | Son işlemi geri alır ya da yineler. | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> veya <kbd>Ctrl+Shift+Z</kbd>; araç çubuğundaki oklar. |
| Projeyi kaydet | Açık .w3d'yi kaydeder (yeniyse ad ister). | <kbd>Ctrl+S</kbd> ya da Özellikler → Dosya/Render sekmesi → Dosya kartı ([Proje](proyecto.md) bölümüne bak). |
| Tam ekran | Tam ekrana girer ve çıkar. | <kbd>F11</kbd>. Android'de telefonda çentik yoksa otomatiktir. |
| Oynat / Duraklat | Animasyonu (ya da oyunu) herhangi bir panelden oynatır. | <kbd>Space</kbd> ya da Zaman Çizelgesi'ndeki Oynat düğmesi. |

![Android](../tutoriales/img/icono-android.svg) **Android'de**: dikey konumda düzen, üstte 3D Görünüm ve altta Özellikler şeklindedir; yatay konumda buna Zaman Çizelgesi de eklenir. Ekran klavyesi açıldığında (örneğin Betik Düzenleyici'de), etkin viewport görünen alanda büyütülür ve klavye kapanınca yerine döner.

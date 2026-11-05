# Viewport'lar

Whisk3D penceresi **viewport**'lara bölünür: her birinin kendi editörü olan paneller (3D Viewport, Outliner, Properties...). Onları bölebilir, boyutlarını değiştirebilir ve her birinin ne göstereceğini seçebilirsin. Fareyle, dokunmatik ekranda parmakla ve bir Nokia'nın tuş takımıyla aynı şekilde çalışır.

## Menü çubuğu ve köşe düğmesi

Her viewport'un üstünde, o editörün menülerini ve düğmelerini içeren bir **menü çubuğu** (görselde yeşil) vardır. Sol üstte her zaman **köşe düğmesi** (turuncu) bulunur: viewport'u değiştiren ya da düzenleyen düğme budur.

![Whisk3D'nin dört viewport'u; her birinin menü çubuğu yeşille, köşe düğmesi turuncuyla vurgulanmış](img/viewports-barras.webp)

## Boyutu değiştirme

Parmağını (ya da fareyi) köşe düğmesine koy ve **sürükle**. Bu, o viewport'un sol ve üst kenarını hareket ettirir; yanındakiler de buna göre ayarlanır. Burada sağ alttaki viewport'un düğmesi daireler çizerek sürükleniyor ve üç viewport da boyut değiştiriyor:

![Bir parmak sağ alttaki viewport'un köşe düğmesini daireler çizerek sürüklüyor ve üç viewport da boyut değiştiriyor](img/viewports-arrastrar.webp)

> [!TIP]
> Düğmeye dokunup sürüklemeden bırakırsan menüsü açılır. PC'de iki viewport'u ayıran kenarı da sürükleyebilirsin.

## Köşe menüsü

Köşe düğmesine dokununca şu menü açılır:

![Köşe düğmesinin menüsü: üstte viewport türleri, çizginin altında Expand, Split in Columns, Split in Rows ve Maximize](img/viewports-menu.webp)

Üstte **viewport türleri** vardır: birini seçmek viewport'u o editöre dönüştürür. Çizginin altında viewport'la yapılabilecekler bulunur:

| Seçenek | Ne yapar |
|---|---|
| Expand | Viewport yanındakinin yerini alır, yanındaki kapanır (yanındaki bölünmüşse o grubun tamamı kapanır). |
| Split in Columns | Onu yan yana iki parçaya böler. |
| Split in Rows | Onu üst üste iki parçaya böler. |
| Maximize | Onu tüm pencereyi kaplayacak şekilde büyütür. Büyütülmüşken menüde yalnızca **Minimize** vardır; o da viewport'u yerine geri döndürür. |

### Türü değiştirme

UV Editor'e dönüştürülen 3D Viewport:

![3D Viewport'un köşe düğmesine dokunuluyor, UV Editor seçiliyor ve viewport bir UV Editor'e dönüşüyor](img/viewports-cambiar-tipo.webp)

### Split in Columns ve Split in Rows

![Split in Columns: 3D Viewport yan yana iki parçaya bölünmüş](img/viewports-dividir-columnas.webp)

![Split in Rows: 3D Viewport üst üste iki parçaya bölünmüş](img/viewports-dividir-filas.webp)

### Expand

Outliner, Properties'in yerini alır:

![Outliner'da Expand seçiliyor ve Outliner, Properties'in yerini de kaplıyor](img/viewports-expandir.webp)

### Maximize ve Minimize

![3D Viewport tüm pencereyi kaplayacak şekilde büyüyor, ardından Minimize ile yerine dönüyor](img/viewports-maximizar.webp)

## Çubuk sığmadığında

Viewport darsa çubuğu tamamen sığmaz. Aynı şey 3D Viewport'un alt çubuğunda da olur. Geri kalanını görmek için:

- **Parmakla:** çubuğu yana doğru sürükle.
- **Fareyle:** fare düğmesini çubuğun üzerinde basılı tutup sürükle ya da çubuğun üzerinde **tekerleği** çevir. 3D Viewport'ta tekerlek yakınlaştırma yapar, bu yüzden onun çubuğu sürüklenir.
- **Klavyeyle:** PC'de <kbd>M</kbd> ya da Nokia'da **sol seçim tuşu**, etkin viewport'un çubuğuna girmeni sağlar. Sol ve sağ ok tuşlarıyla çubukta gezersin; her menü üzerine gelince açılır. Yukarı ve aşağı ile menüde hareket eder, OK ile (PC'de <kbd>Enter</kbd>) seçersin. Çubuk, nerede olduğunu göstermek için kendiliğinden kayar.

![Bir parmak Outliner'ın çubuğunu yana doğru sürüklüyor ve sığmayan düğmeler beliriyor](img/viewports-barra-dedo.webp)

![Fare işaretçisi Outliner'ın çubuğunun üzerinde: tekerlekle çubuk kayıyor](img/viewports-barra-rueda.webp)

Bir Nokia N95'te: sol seçim tuşu çubuğa girer, ok tuşları da çubukta gezer.

![Nokia N95 arayüzü: sol seçim tuşu Mode menüsünü açıyor, sağ ok tuşu da çubuğun menülerinden sırayla geçiyor; çubuk kendiliğinden kayıyor](img/viewports-barra-nokia.webp)

## Viewport türleri

| Viewport | Ne işe yarar |
|---|---|
| [3D Viewport](../editor/vista-3d.md) | Sahnenin görüntülendiği ve düzenlendiği yer: modelleme, nesneleri taşıma, boyama, poz verme ve animasyon. |
| [Outliner](../editor/outliner.md) | Sahnedeki her şeyin listesi. |
| [Library](../editor/outliner.md#library-görünümü) | Projenin kaynakları: mesh'ler, materyaller, dokular, animasyonlar, sesler, script'ler ve daha fazlası. |
| [Properties](../editor/propiedades.md) | Etkin nesnenin verileri, sekmeler hâlinde. |
| [UV Editor](../editor/editor-uv.md) | Dokunun mesh'in üzerine nasıl oturduğu. |
| [Timeline](../editor/linea-de-tiempo.md) | Animasyon: keyframe'ler ve eğriler. |
| [2D Editor](../editor/editor-2d.md) | Oyun arayüzleri, HUD'lar ve 2D oyunlar. |
| [Console](../editor/consola.md) | Motorun log kayıtları: hatalar, uyarılar ve bilgiler. |
| [IDE](../editor/editor-de-scripts.md) | Lua script'leri ve shader'lar için editör. |

Her viewport'un daha ayrıntılı kendi eğitimi olacak. Şimdilik her birini [Whisk3D kılavuzu](../editor/README.md) anlatıyor.

Whisk3D büyümeye devam ediyor: zamanla yeni viewport'lar eklenecek, mevcut olanlar da değişebilir. Hepsi aynı şekilde çalışacak: üstte çubuk, köşede düğme.

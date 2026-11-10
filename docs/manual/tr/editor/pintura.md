# Boyama modları

> [!NOTE]
> Üç boyama modu da **yalnızca 3D örgüler içindir**: kameralar, ışıklar, iskeletler, eğriler ya da 2D öğelerle görünmezler. Boyama sırasında tık seçimi değiştirmez; başka bir nesneyi boyamak için onu Outliner'dan seç.

## Üçünde ortak olanlar

| İşlev | Açıklama | Nasıl kullanılır |
|---|---|---|
| Boyama | Yalnızca görünen, öne bakan yüzleri boyar (X-Işını ile arkadakileri de). Her fırça darbesi bir geri alma adımıdır. | Tıkla ve sürükle |
| Yarıçap | Fırçanın piksel cinsinden boyutu. Değiştirdiğinde görünümün ortasında gerçek boyutta bir daire belirir. | Araç çubuğunun üstündeki sürüklenebilir çubuk |
| Değer | Fırçanın yoğunluğu. | Sürüklenebilir çubuk |
| Azalma | Fırça kenarının nasıl yumuşatılacağı; kendi eğrini de çizebilirsin. | Azalma düğmesi |
| Görünüm (dokunmatik) | Açık: parmak gezinir. Kapalı: parmak boyar. | Monitör şeklindeki düğme |

## Ağırlık Boyama

Bir kemiğin her köşeyi ne kadar etkilediğini boyar (mavi = hiç, kırmızı = tamamen). Örgünün iskeletle düzgün deforme olmasını sağlayan budur.

| İşlev | Açıklama |
|---|---|
| Fırça modu | Ekleme (+), çıkarma (−) ya da eşitleme (=, tam değeri koyar). |
| Grup (köşe grubu) | Hangi grubun (kemiğin) boyanacağı; grup ekleyebilirsin. Hiç grup yoksa ilk fırça darbesi bir tane oluşturur. |
| İşaretler | Her köşe için küçük bir kare gösterir; fırça yalnızca onlara etki eder. |
| Yalnızca seçili olan | Seçili olmayanı korur (karartılmış görünür). |

## Köşe Boyama

Bir **3D örgünün** köşelerine renk boyar (resimler ya da 2D öğeler için işe yaramaz: onun için Doku Boyama var). Çok hafiftir ve N95'te renk vermenin önerilen yoludur.

| İşlev | Açıklama |
|---|---|
| Renk | Renk seçiciyi açar (RGB, HSV, onaltılık ya da paletten bir dizin). |
| Köşe Başına / Yüz Köşesi Başına | Köşe Başına'da renk yüzler arasında paylaşılır; Yüz Köşesi Başına'da her yüzün kendi rengi olabilir (sert kenarlar). |
| Renk katmanı | Renk katmanlarını seçme ya da oluşturma. |
| İşaretler / Yalnızca seçili olan | Ağırlık Boyama'daki gibi. |

## Doku Boyama

Doğrudan 3D nesnenin üzerine boyar ve fırça darbesi nesnenin dokusuna işlenir.

| İşlev | Açıklama |
|---|---|
| Otomatik doku | Örgünün dokusu yoksa ilk fırça darbesinde kendiliğinden oluşturulur: kendine ait bir malzeme, otomatik UV'ler ve malzemenin renginde 1024×1024'lük (N95'te 256×256) bir görüntü. |
| Renk, Azalma, Yarıçap, Değer | Diğer modlardaki gibi. |
| Dokuyu Kaydet | Boya, kaydedilene kadar bellekte durur. Kaydedilmemiş değişiklik varsa disket yeşil olur. Seçenekler: Kaydet, Farklı Kaydet…, .w3d içine kaydet ve .w3d dışına kaydet… |

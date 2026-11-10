# Kapı kolunu modelleme

Önceki eğitimdeki ([Orijini değiştirme: bir kapı](puerta.md)) kapıdan başlıyoruz: 0,90 × 0,10 × 2 metre ölçülerinde ve orijini menteşede. Ona iki parçalı bir kapı kolu yapacağız: kapıya yapışık bir **plaka** ve dönen bir **kol**.

Bu sırada şunları öğreneceksin:

- bir yüzü çoğaltıp ondan yeni bir parça oluşturmak;
- bir örgünün bir parçasını başka bir nesneye **ayırmak** (<kbd>P</kbd>) ve yeniden **birleştirmek** (<kbd>Ctrl</kbd> + <kbd>J</kbd>);
- bir örgünün her parçasına farklı bir **malzeme** vermek;
- kapı kolunun kapının iki tarafında da olması için **Ayna** değiştiricisini kullanmak;
- hiç görünmeyen yüzleri **X-Işını** ile görmek ve silmek;
- 3D imleçle **orijini** kolun döneceği yere koymak;
- birlikte hareket etmeleri için kolu kapıya **ebeveyn atama** ile bağlamak.

## 1. Plaka

1. Kapı seçiliyken **Düzenleme Modu**'na geç (<kbd>Tab</kbd>) ve **Köşe / Kenar / Yüz** düğmesinden **Yüz**'ü seç.
2. Kapının öndeki büyük yüzüne tıkla.
3. **Onu çoğalt:** <kbd>Shift</kbd> + <kbd>D</kbd>. Kopya fareye yapışık çıkar; sağ tık (ya da <kbd>Esc</kbd>) onu olduğu yerde, tam orijinalin üstünde bırakır.
4. Küçük bir kare olana kadar **küçült**. Göz kararı: <kbd>S</kbd> ve fareyi hareket ettir; ardından yalnızca yüksekliği ayarlamak için <kbd>S</kbd> <kbd>Z</kbd>. Sayılarla: <kbd>S</kbd> `0.15` <kbd>Enter</kbd> ve <kbd>S</kbd> <kbd>Z</kbd> `0.45` <kbd>Enter</kbd> 13,5 cm'lik bir kare bırakır.
5. Onu menteşenin karşı tarafına, kapı kolunun geleceği yere **taşı**: <kbd>G</kbd> <kbd>X</kbd> `-0.31` <kbd>Enter</kbd>.
6. Kalınlığı olsun diye onu biraz **ekstrüde et**: <kbd>E</kbd> `0.03` <kbd>Enter</kbd> (3 cm).

![Kapının ön yüzüne bir tık; Shift + D ve Esc kopyayı üstte bırakıyor; S 0.15 ve S Z 0.45 onu bir kareye küçültüyor; G X -0.31 onu menteşenin karşı tarafına taşıyor ve E 0.03 ona kalınlık veriyor](img/picaporte-placa-raton.webp)

![Android](img/icono-android.svg) Dokunmatik ekranda: yüze bir dokunuş ve **Örgü → Çoğalt** (kopya üstte kalır). Ardından araç çubuğunda **Ölçekle**, **Taşı** ve **Ekstrüde**; sayılar üstteki çubuğun klavyesiyle girilir, sonra onay işaretine dokunulur.

![Parmak ön yüze dokunuyor; Örgü, Çoğalt; üstteki çubuğun klavyesiyle Ölçekle 0.15, Ölçekle Z 0.45, Taşı X -0.31 ve Ekstrüde 0.03](img/picaporte-placa-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95'te: sol seçim tuşuyla **Örgü → Çoğalt**; ardından <kbd>3</kbd> (ölçekleme), <kbd>1</kbd> (taşıma) ve <kbd>7</kbd> (ekstrüzyon); sayılar için kalem tuşu basılı tutulur, sonra <kbd>OK</kbd>.

![Bir Nokia N95: 3 ve kalem + 0.15; 3, 3 (Z) ve 0.45; 1, 1 (X) ve -0.31; 7 ve 0.03; her birinde OK](img/picaporte-placa-nokia.webp)

## 2. Plakayı ayırma

Plaka hâlâ kapının örgüsünün bir parçası. Onu kendi nesnesine taşıyoruz:

1. Plaka seçiliyken <kbd>L</kbd> (**Bağlantılıyı Seç**) parçanın tamamını seçer: kapı hariç, plakanın 6 yüzünü.
2. <kbd>P</kbd> (**Ayır**): plaka yeni bir nesne olur, **Kapı.001**. Onu Outliner'da görürsün.

![Fare plakanın üzerindeyken L parçanın tamamını seçiyor, P de onu ayırıyor: Outliner'da Kapı.001 beliriyor](img/picaporte-separar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) Dokunmatik ekranda ve N95'te: **Seç → Bağlantılıyı Seç** ve plakaya bir dokunuş; ardından **Örgü → Ayır**.

![Seç, Bağlantılıyı Seç ve plakaya bir dokunuş; Örgü, Ayır](img/picaporte-separar-dedo.webp)

Ayırınca kapıda plakadan hiçbir şey kalmaz: ne yüzleri ne de çizgileri.

## 3. Malzemeler: ahşap ve metal

Artık her parçanın kendi malzemesi olabilir. **Özellikler**'de, **Malzeme** sekmesinde:

1. Kapıyı seç. **Malzemeyi Yeniden Adlandır** alanına `Ahşap` yaz ve **Renk**'ten bir kahverengi seç.
2. Plakayı seç (Kapı.001). Kapıdan çıktığı için onda da Ahşap var: **Yeni Malzeme**'ye dokun, adını `Metal` koy ve ona bir gri ver.

![Metal plakalı ahşap kapı ve plakanın Malzeme kartında Metal malzemesi](img/picaporte-materiales.webp)

## 4. Kapının öbür tarafı: Ayna

Bir kapı kolu iki tarafta da olur. Başka bir plaka yapmak yerine **Ayna** değiştiricisini kullanıyoruz:

1. Plaka seçiliyken **Özellikler → Değiştiriciler**'de **Ayna** ekle.
2. **Ayna X**'i kapat ve **Ayna Y**'yi aç: kapının öbür tarafında diğer plaka belirir.

![Yandan: Değiştiriciler'de Ayna ekleniyor, Ayna X kapanıyor, Ayna Y açılıyor ve kapının öbür tarafında plaka beliriyor](img/picaporte-espejo-raton.webp)

Aynalama nesnenin **orijinine** göre yapılır. Plaka kapıdan çıktı, bu yüzden orijini aynı: menteşe, tam kapının kalınlığının ortasında. Bu yüzden kopya tam olarak öbür tarafa gelir. [Ayna](../editor/modificador-espejo.md) sayfasına bak.

## 5. Kol

Kol, aynı numarayla plakadan çıkar:

1. Plaka seçiliyken sayısal tuş takımında <kbd>/</kbd> tuşuna bas (**Yerel görünüm**): yalnızca plaka görünür, üzerinde çalışmak daha kolay olur. **Düzenleme Modu**'na geç.
2. Plakanın ön yüzünü seç, onu **çoğalt** (<kbd>Shift</kbd> + <kbd>D</kbd> ve sağ tık) ve küçült: <kbd>S</kbd> `0.6` <kbd>Enter</kbd>.
3. Onu dışarı doğru **ekstrüde et**: <kbd>E</kbd> `0.08` <kbd>Enter</kbd>. Bu, kolun milidir.

![Plakanın yerel görünümünde: ön yüzüne bir tık, Shift + D ve Esc, S 0.6 onu küçültüyor ve E 0.08 dışarı çıkarıyor: kolun mili](img/picaporte-manija-raton.webp)
4. **Kenar**'a geç ve dıştaki, kapının ucuna bakan kenarı seç. Onu dışarı doğru taşı: <kbd>G</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>. Ön yüz eğik kalır.
5. **Yüz**'e geç ve o eğik yüzü seç. Onu normali yönünde değil, **X'te** ekstrüde et: <kbd>E</kbd>, <kbd>X</kbd>, `0.34` ve <kbd>Enter</kbd>. Kolun sapı çıkar.
6. Uç eğik kaldı. Düzeltmek için onu X'te sıfıra ölçekle: <kbd>S</kbd> <kbd>X</kbd> `0` <kbd>Enter</kbd>. Tüm köşeleri aynı X'te kalır: uç düz olur.

![Kenar'da dıştaki kenar G Y 0.05 ile kalkıyor; Yüz'de eğik yüz E X 0.34 ile X'te dışarı çıkıyor ve S X 0 ucu düzeltiyor](img/picaporte-brazo-raton.webp)
7. <kbd>L</kbd> kolun tamamını seçer, <kbd>P</kbd> de onu ayırır: **Kapı.002** olur. Ona bir ad ver: Outliner'da çift tıkla ve `Kol` yaz.

![L ve P kolu ayırıyor; Outliner'da bir çift tıkla adı Kol oluyor](img/picaporte-manija-separar-raton.webp)

![Android](img/icono-android.svg) Dokunmatik ekranda: yüze bir dokunuş, **Örgü → Çoğalt**, **Ölçekle** `0.6` ve **Ekstrüde** `0.08`; **Kenar**, dıştakine bir dokunuş ve **Taşı** **Y** `0.05`; **Yüz**, eğik olana bir dokunuş, **Ekstrüde** **X** `0.34` ve **Ölçekle** **X** `0`. Ayırmak için **Seç → Bağlantılıyı Seç** ve **Örgü → Ayır**.

![Parmakla: plakanın yüzü, Çoğalt, Ölçekle 0.6, Ekstrüde 0.08; dıştaki kenar Taşı Y 0.05 ile kalkıyor; eğik yüz Ekstrüde X 0.34 ile çıkıyor ve Ölçekle X 0 onu düzeltiyor](img/picaporte-manija-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95'te: <kbd>3</kbd> ve `0.6`, <kbd>7</kbd> ve `0.08`; kenar (sanal fareyle), <kbd>1</kbd>, <kbd>2</kbd> (Y ekseni) ve `0.05`; eğik yüz, <kbd>7</kbd>, <kbd>1</kbd> (X ekseni) ve `0.34`; <kbd>3</kbd>, <kbd>1</kbd> ve `0`.

![Bir Nokia N95: 3 ve 0.6; 7 ve 0.08; 1, 2 ve 0.05 kenarı kaldırıyor; 7, 1 ve 0.34 sapı X'te çıkarıyor; 3, 1 ve 0 ucu düzeltiyor](img/picaporte-manija-nokia.webp)

Kol plakadan çıktığı için onun malzemesini (Metal) ve Ayna değiştiricisini de taşır.

## 6. Plakayı kapıyla birleştirme

Plaka tek başına hareket etmez: kapının bir parçasıdır. Onu yeniden birleştiriyoruz:

1. Plakayı seç ve **Değiştiriciler**'de **Değiştiriciyi Uygula**'ya dokun: ayna gerçek geometriye dönüşür.
2. Plakayı seç, ardından <kbd>Shift</kbd> + tık ile kapıyı (kapı etkin olur) ve <kbd>Ctrl</kbd> + <kbd>J</kbd> (**Birleştir**). Artık iki malzemesiyle tek bir örgüdürler.

![Değiştiriciyi Uygula plakanın aynasını geometriye dönüştürüyor; plakaya tık, kapıya Shift + tık ve Ctrl + J: Outliner'da Kapı.001 kayboluyor, artık kapının parçası](img/picaporte-unir-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) Dokunmatik ekranda ve N95'te: **Nesne → Birleştir**.

![Plaka ve kapı seçiliyken Nesne, Birleştir: Kapı.001 Outliner'dan kayboluyor](img/picaporte-unir-dedo.webp)

Bir örgünün parçaları böyle ayrılır ve birleştirilir: ayırmak için <kbd>P</kbd>, birleştirmek için <kbd>Ctrl</kbd> + <kbd>J</kbd>.

## 7. Hiç görünmeyen yüzler

Plaka kapıya dayalı. Her plakanın arka yüzü örtülü kalır: hiçbir zaman görünmez ama program onu yine de çizer. Onu görmek için:

1. Kapıyı seç ve **Düzenleme Modu**'na geç.
2. **Bindirmeler**'de **X-Işını**'nı aç: örgü saydamlaşır ve arka yüzler görünür.
3. Her plakanın arka yüzünü (kapıya değeni) seç ve sil: <kbd>X</kbd> → **Yüzler**.

![Kapı Düzenleme Modu'nda; Bindirmeler, X-Işını; iki plakanın arka yüzleri seçiliyor ve X, Yüzler onları siliyor](img/picaporte-rayosx-raton.webp)

Görünmeyen geometriyi silmek iyi olur: boş yere çizilen yüzlerdir. Küçük bir modelde fark edilmez ama büyük bir sahnede ya da bir telefonda kazanılan performanstır.

## 8. Kolun döndüğü yer: orijin

Kol, kapının menteşesi etrafında değil, kendi ekseni etrafında dönmeli.

1. Kolu seç ve Ayna'sını uygula (**Değiştiriciler → Değiştiriciyi Uygula**).
2. **X-Işını** açıkken **Düzenleme Modu**'na geç. Kolun plakalara değen iki arka yüzünü seç (her taraftan bir tane).
3. <kbd>Shift</kbd> + <kbd>S</kbd> → **İmleç Seçilene**: 3D imleç ikisinin ortasına, tam kolun eksenine gelir.
4. Bu iki yüz de görünmez: onları sil (<kbd>X</kbd> → **Yüzler**).
5. **Nesne Modu**'na dön ve orijini imlece taşı: <kbd>W</kbd> (ya da sağ tık) → **Orijini Ayarla → Orijini 3D İmlece**.

![Kol: Değiştiriciyi Uygula; Düzenleme Modu'nda iki arka yüzü seçiliyor, Shift + S, İmleç Seçilene, X, Yüzler; Tab ve Orijini Ayarla, Orijini 3D İmlece](img/picaporte-origen-raton.webp)

Bu, [Orijini değiştirme: bir kapı](puerta.md#orijini-menteşeye-taşıma) eğitiminde menteşeyle yaptığımızın aynısı.

## 9. Kolun kapıyla birlikte hareket etmesi

Kapıyı döndürmeyi dene: kol havada asılı kalır. İkisi birbirinden bağımsız iki nesne. Döndürmeyi geri al (<kbd>Ctrl</kbd> + <kbd>Z</kbd>).

![R Z -70 kapıyı döndürüyor ve kol havada asılı kalıyor; Ctrl + Z kapıyı geri getiriyor](img/picaporte-flota-raton.webp)

Kolun kapıyla birlikte hareket etmesi için ona **ebeveyn atamak** gerekir: kol, kapının **çocuğu** olur.

1. Kolu, ardından <kbd>Shift</kbd> + tık ile kapıyı seç (ebeveyn olan kapı etkin olur).
2. <kbd>Ctrl</kbd> + <kbd>P</kbd> → **Nesne (Dönüşümü Koru)**. Outliner'da kol kapının içine girer.

Yalnızca kapıyı seç (ona bir tık) ve yeniden döndür: artık kol onunla birlikte hareket ediyor. Kol da gerçek bir kapı kolu gibi kendi ekseni etrafında dönmeye devam eder. İkisi de seçiliyse yalnızca kapı döner, kol da onun çocuğu olduğu için onu takip eder.

![Kola tık, kapıya Shift + tık, Ctrl + P ve Nesne (Dönüşümü Koru); yalnızca kapıya bir tık ve R Z -70: artık kol kapıyla birlikte hareket ediyor](img/picaporte-emparentar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) Dokunmatik ekranda ve N95'te: **Nesne → Ebeveyn Ata → Nesne (Dönüşümü Koru)**.

![Kol ve kapı seçiliyken Nesne, Ebeveyn Ata, Nesne (Dönüşümü Koru): Outliner'da kol kapının içinde](img/picaporte-emparentar-dedo.webp)

Sonraki eğitimde kapıyı canlandıracağız: [Kapıyı canlandırma](animar-puerta.md).

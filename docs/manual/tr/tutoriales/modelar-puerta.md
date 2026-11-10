# Bir kapı modelleme

İlk basit modelimizi yapacağız: bir kapı! Başlamak için sahnede hazır gelen küp işimizi görür; küp 2 × 2 × 2 metredir.

Ben (Dante) mekân oluştururken kapıları çok kullanırım: bunu inşaat okurken öğrendim. Kapılar neredeyse her zaman 2 metre yüksekliğindedir ve bir insan (normalde) bir kapıdan kısadır. Sahnede bir kapı olunca geri kalan her şeyin ölçeğini hemen görürsün.

## Zemine oturtma

Küpün merkezi zeminde, bu yüzden yarısı zeminin altında kalıyor. Zemine oturması için onu Z ekseninde 1 metre yukarı kaldırıyoruz. Küp seçiliyken:

![PC](img/icono-pc.svg) <kbd>G</kbd> tuşuna bas: küp fareyi takip eder. Ardından <kbd>Z</kbd>: Z eksenine bağlanır (mavi çizgi belirir). `1` yaz ve <kbd>Enter</kbd> tuşuna bas.

![G ile küp fareyi takip ediyor; Z ile dikey eksenin mavi çizgisine bağlanıyor; 1 ile tam 1 metre yükseliyor ve Enter onu orada, zemine oturmuş hâlde bırakıyor](img/modelar-subir-raton.webp)

![Android](img/icono-android.svg) Araç çubuğunda **Taşı**'ya, ardından **Z**'ye dokun; sayısal klavye için üstteki çubuğa bir dokunuş, `1`, **OK** ve yeşil onay işareti.

![Araç çubuğunda Taşı ve Z'ye dokunuluyor; üstteki çubuğa bir dokunuş sayısal klavyeyi açıyor, 1 ve OK küpü yükseltiyor, onay işareti de işlemi tamamlıyor; altta Taşı paneli beliriyor, bir dokunuşla açılıyor ve Kabul et ile kapanıyor](img/modelar-subir-dedo.webp)

![Nokia](img/icono-nokia.svg) <kbd>1</kbd> (taşıma), <kbd>3</kbd> (Z ekseni), **kalem** tuşu basılıyken <kbd>1</kbd>, ardından <kbd>OK</kbd>.

![Bir Nokia N95: 1 taşımayı başlatıyor, 3 onu Z eksenine bağlıyor, kalem ve 1 değeri yazıyor, OK da onaylıyor](img/modelar-subir-nokia.webp)

Her cihazda sayıların nasıl yazıldığı [Kesin sayılarla](modo-objeto.md#kesin-sayılarla) bölümünde anlatılıyor.

### Son işlemi düzeltme

Taşıma, döndürme ya da ölçeklemeden sonra sol altta, araç çubuğunun yerine küçük bir panel belirir: **Son işlemi ayarla**. Kapalıyken yalnızca ne yaptığını gösterir (örneğin **Taşı**); başlığına bir dokunuş ya da tık onu açar:

![Açılmış Taşı paneli: X 0, Y 0 ve Z 1 metre, Global yönelim, orantılı düzenleme onay kutusu, İptal ve Kabul et düğmeleri](img/modelar-ficha.webp)

Her değer düzeltilebilir: 1 metre yerine 1,2 istediysen Z'ye dokunur, `1.2` yazarsın ve işlem yeni değerle yeniden yapılır. **Kabul et** paneli kapatır ve araç çubuğu geri gelir; **İptal** işlemi geri alır.

## Şekil verme

Kapıyı modellemenin iki yolu var.

### En kolayı, ama önermediğim: nesneyi ölçeklemek

Nesne Modu'nda küpü, kapı şeklini alana kadar X ve Y eksenlerinde ölçekle. Örneğin <kbd>S</kbd> <kbd>X</kbd> `0.45` <kbd>Enter</kbd> (90 cm genişlik) ve <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd> (10 cm kalınlık).

Önermiyorum, çünkü nesnenin her ekseninde farklı bir ölçek kalır (0,45, 0,05 ve 1); idealde üç ekseninde de ölçeği **1** olmalıdır ([Ölçeği 1'de tutmak daha iyi](modo-objeto.md#ölçeği-1de-tutmak-daha-iyi) bölümüne bak). Bu yolla yaparsan **dönüşümü her zaman uygula**: <kbd>Ctrl</kbd> + <kbd>A</kbd> → **Ölçekle** (ya da Nesne → Uygula → Ölçekle). Şekil değişmez ve ölçek 1'e döner.

![Nesne Modu'nda S X 0.45 ve S Y 0.05 küpe kapı şekli veriyor, Özellikler de her eksende farklı ölçeği gösteriyor; Ctrl+A ve Ölçekle ölçeği uyguluyor ve ölçek 1'e dönüyor](img/modelar-escalar-raton.webp)

![Android](img/icono-android.svg) Dokunmatik ekranda: **Ölçekle**, **X**, üstteki çubuk, `0.45` ve OK; aynısını **Y** ve `0.05` ile yap. Ardından **Nesne → Uygula → Ölçekle**.

![Araç çubuğu ve sayısal klavyeyle X'te 0.45, Y'de 0.05 ölçekleme; ardından Nesne, Uygula, Ölçekle ve ölçek 1'e dönüyor](img/modelar-escalar-dedo.webp)

### Önerdiğim: Düzenleme Modu

Küp seçiliyken Düzenleme Modu'na geç: <kbd>Tab</kbd> tuşuna bas ya da 3D Görünüm'ün mod düğmesinden **Düzenleme Modu**'nu seç. Girdiğinde tüm köşeler seçili olur.

Düzenleme Modu'nda **köşeler**, **kenarlar** ve **yüzler** seçilir: temel olan budur. Nesne Modu'nda olduğu gibi bunlar da taşınabilir, döndürülebilir ve ölçeklenebilir ([Düzenleme Modu](../editor/modo-edicion.md) sayfasına bak).

Her şey seçiliyken kapı istediğin gibi olana kadar X ve Y'de küçült. Z'de gerek yok, çünkü zaten 2 metre! Değişen nesne değil örgü olduğu için ölçek 1'de kalır.

**Göz kararı, gizmoyla:** gizmonun ortasına bir tık (ya da dokunuş) onu Döndür'e, bir tane daha **Ölçekle**'ye geçirir. Kırmızı ucu (X) tut ve istediğin genişliğe gelene kadar ortaya doğru sürükle; ardından yeşil ucu (Y), kapı gibi incelene kadar.

![Düzenleme Modu'nda gizmonun ortasına iki tık onu ölçeklemeye geçiriyor; kırmızı uç ortaya doğru sürükleniyor ve küp daralıyor; ardından yeşil uç, ve küp bir kapı gibi inceliyor](img/modelar-gizmo-raton.webp)

![Aynısı parmakla: gizmonun ortasına iki dokunuş, kırmızı ve yeşil uçlar ortaya doğru sürükleniyor](img/modelar-gizmo-dedo.webp)

**Sayılarla:** <kbd>S</kbd>, <kbd>X</kbd>, `0.45`, <kbd>Enter</kbd> ve <kbd>S</kbd>, <kbd>Y</kbd>, `0.05`, <kbd>Enter</kbd>.

![Tab ile küp Düzenleme Modu'na geçiyor, her şey yeşil; S, X, 0.45 ve Enter onu daraltıyor, S, Y, 0.05 ve Enter inceltiyor; nesnenin ölçeği 1'de kalıyor](img/modelar-edicion-raton.webp)

![Android](img/icono-android.svg) Dokunmatik ekranda: mod düğmesi → **Düzenleme Modu**; ardından **Ölçekle**, **X**, üstteki çubuğa bir dokunuş, `0.45`, **OK** ve onay işareti; aynısını **Y** ve `0.05` ile yap.

![Mod düğmesiyle Düzenleme Modu'na geçiliyor; Ölçekle, X, üstteki çubuk, 0.45 ve OK küpü daraltıyor; aynısı Y ve 0.05 ile](img/modelar-edicion-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95'te: <kbd>OK</kbd> Düzenleme Modu'na geçirir. <kbd>3</kbd> (ölçekleme), <kbd>1</kbd> (X ekseni), kalem tuşu basılıyken `0` <kbd>*</kbd> `45` (<kbd>*</kbd> ondalık noktadır) ve <kbd>OK</kbd>. Aynısını <kbd>2</kbd> (Y ekseni) ve `0.05` ile yap.

![Bir Nokia N95: OK Düzenleme Modu'na geçiriyor; 3 ve 1 X'te ölçekliyor, kalem ve 0.45 değeri yazıyor, OK onaylıyor; aynısı 3, 2 ve 0.05 ile](img/modelar-edicion-nokia.webp)

## Kesin ölçülerle

Göz kararı yerine, örneğin tam **90 cm genişliğinde** bir kapı istiyorsan: **Köşe / Kenar / Yüz** düğmesinden **Yüz**'ü seç ve yan yüzlerden birine tıkla. Küpün 2 metre olduğunu biliyoruz, yani 0,9'a inmesi için fazlasını çıkarmamız gerekiyor: o yan yüzü **1,1 metre** kaydırmak gerekir. <kbd>G</kbd> <kbd>X</kbd> `-1.1` <kbd>Enter</kbd>.

![Düzenleme Modu'nda Yüz seçiliyor, sağ yan yüze bir tık yalnızca onu seçiyor, G, X, -1.1 ve Enter onu kaydırıyor: küp 0,9 metre genişliğinde kalıyor](img/modelar-exacto-raton.webp)

![Parmakla: seçim düğmesinde Yüz, yan yüze bir dokunuş, Taşı, X, üstteki çubuk, -1.1 ve OK; küp 0,9 metre genişliğinde kalıyor](img/modelar-exacto-dedo.webp)

Taşırken **X** ekseni hareketi tek bir eksene kilitler, böylece küp bozulmaz:

- ![PC](img/icono-pc.svg) <kbd>X</kbd> tuşuyla.
- ![Android](img/icono-android.svg) Araç çubuğunda **X**'e bir dokunuşla. `-1.1`, üstteki çubuğa dokununca açılan sayısal klavyeyle yazılır.
- ![Nokia](img/icono-nokia.svg) Taşırken <kbd>1</kbd> ile. Sayı kalem tuşu basılıyken yazılır: <kbd>#</kbd> işaretini değiştirir, <kbd>*</kbd> ondalık noktadır. Yüz sanal fareyle seçilir (sağ seçim tuşu, ok tuşları ve <kbd>OK</kbd>).

Kalınlık için, her şey seçiliyken, önceki gibi <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>.

İşte ilk 3D modelimiz hazır! Tebrikler.

Sonraki eğitimde kapının menteşesinden açılmasını sağlayacağız: [Orijini değiştirme: bir kapı](puerta.md).

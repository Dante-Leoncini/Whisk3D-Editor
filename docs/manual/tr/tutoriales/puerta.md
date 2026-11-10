# Orijini değiştirme: bir kapı

Bu eğitimde bir kapının menteşesinden açılmasını sağlayacaksın. Bunun için kapının **orijinini** değiştirmen gerekir: nesnenin etrafında döndüğü nokta (Nesne Modu eğitimindeki [Orijin](modo-objeto.md#orijin) bölümüne bak).

Önceki eğitimdeki ([Bir kapı modelleme](modelar-puerta.md)) orijini ortada olan kapıdan başlıyoruz. Kapıyı bir yan yüzünü kaydırarak yaptıysan orijin de kaymış kaldı: **Nesne → Orijini Ayarla → Orijini Geometriye** onu yeniden ortaya getirir.

## Kapı yanlış dönüyor

Kapıyı döndürmeyi dene. Kapı menteşesinden değil, döner kapı gibi kendi merkezi etrafında döner:

![PC](img/icono-pc.svg) Gizmoyla: ortasına bir tık onu **Döndür** moduna geçirir, mavi halka da kapıyı dikey eksen etrafında döndürür.

![Gizmonun ortasına bir tık onu Döndür'e geçiriyor; mavi halka sürükleniyor ve kapı döner kapı gibi kendi merkezi etrafında dönüp geri geliyor](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) Parmakla: gizmonun ortasına bir dokunuş onu **Döndür** moduna geçirir, sonra mavi halka sürüklenir.

![Parmak gizmonun ortasına dokunup onu Döndür'e geçiriyor; sonra mavi halkayı sürüklüyor ve kapı kendi merkezi etrafında dönüp geri geliyor](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95'te gizmo kapalı gelir: <kbd>2</kbd> (döndürme), <kbd>3</kbd> (Z ekseni, yani dikey eksen) ve ok tuşları; <kbd>OK</kbd> onaylar.

![Bir Nokia N95: 2 ve 3 dikey eksen etrafında döndürmeyi başlatıyor; sağ ok tuşu kapıyı merkezi etrafında döndürüyor, sol ok tuşu geri getiriyor ve OK onaylıyor](img/puerta-gira-mal-nokia.webp)

Yanlış dönüyor, çünkü orijini ortada. Onu menteşeye taşıyalım.

## Orijini menteşeye taşıma

1. **Düzenleme Modu'na geç.** Mod düğmesine, ardından **Düzenleme Modu**'na tıkla (PC'de <kbd>Tab</kbd> ile de olur).

   ![Mod düğmesi açık, Düzenleme Modu işaretli](img/puerta-paso-1.webp)

2. **Yüzleri seç.** Düzenleme Modu'nda **Köşe / Kenar / Yüz** düğmesi belirir: ona tıkla ve **Yüz**'ü seç.

   ![Köşe, Kenar, Yüz düğmesi açık, Yüz işaretli](img/puerta-paso-2.webp)

3. **Menteşe tarafındaki yüzü seç.** Kapının, menteşenin geleceği taraftaki dar yan yüzüne tıkla: yeşil olur.

   ![Kapının dar yan yüzü yeşille seçili ve bir daireyle işaretli](img/puerta-paso-3.webp)

4. **3D imleci oraya taşı.** **Örgü** menüsünde **Mıknatıs** → **İmleç Seçilene**: 3D imleç menteşenin üzerine gelir.

   ![Örgü menüsü açık; Mıknatıs alt menüsü ve İmleç Seçilene işaretli](img/puerta-paso-4.webp)

5. **Nesne Modu'na dön.** Mod düğmesine, ardından **Nesne Modu**'na tıkla (ya da <kbd>Tab</kbd>).

   ![Mod düğmesi açık, Nesne Modu işaretli; 3D imleç artık menteşede](img/puerta-paso-5.webp)

6. **Orijini 3D imlece taşı.** **Nesne** menüsünde **Orijini Ayarla** → **Orijini 3D İmlece**. Kapı yerinden oynamaz: hareket eden onun orijinidir, onunla birlikte de gizmo; gizmo artık menteşededir.

   ![Nesne menüsü açık; Orijini Ayarla alt menüsü ve Orijini 3D İmlece işaretli](img/puerta-paso-6.webp)

## Artık doğru dönüyor

Kapıyı gizmoyla yeniden döndür: artık menteşesinden açılıp kapanıyor.

![Orijin menteşedeyken gizmonun mavi halkası kapıyı yana dönene kadar açıyor ve yeniden kapatıyor](img/puerta-gira-bien-raton.webp)

![Parmakla, mavi halka kapıyı menteşesinden açıyor ve yeniden kapatıyor](img/puerta-gira-bien-dedo.webp)

![Bir Nokia N95: 2, 3 ve sağ ok tuşu kapıyı menteşesinden açıyor; sol ok tuşu kapatıyor ve OK onaylıyor](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) Dokunmatik ekranda yukarıdaki adımlar aynıdır, tıklar yerine dokunuşlarla: menüler bir dokunuşla açılır ve seçilir.

![Nokia](img/icono-nokia.svg) Nokia N95'te <kbd>OK</kbd> Nesne Modu'ndan Düzenleme Modu'na geçirir, menüler de sol seçim tuşuyla açılır.

## Orijini Ayarla seçenekleri

| Seçenek | Ne yapar |
|---|---|
| **Orijini Geometriye** | Orijini örgünün merkezine taşır. |
| **Orijini 3D İmlece** | Orijini 3D imlecin bulunduğu yere taşır. |
| **Geometriyi Orijine** | Tersini yapar: orijini yerinde bırakır ve örgüyü, merkezi orijine gelene kadar taşır. |

Sonraki eğitimde kapının kolunu yapıyoruz: [Kapı kolunu modelleme](modelar-picaporte.md).

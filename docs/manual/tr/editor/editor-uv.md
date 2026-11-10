# UV Düzenleyici

Dokunun örgünün üzerine nasıl oturduğunu gösterir. Düzenleme yapabilmek için örgünün **3D Görünüm'de Düzenleme Modunda** olması gerekir; değilse editör yalnızca dokuyu gösterir. UV Düzenleyici'nin modu kendine aittir: 3D Görünüm'ün modunu değiştirmez. UV açma ve yansıtma araçları 3D Görünüm'dedir (Düzenleme Modu, <kbd>U</kbd> tuşu).

## Gezinme ve üst çubuk

| İşlev | Açıklama | Nasıl kullanılır |
|---|---|---|
| Yakınlaştırma / Kaydırma | İşaretçiye doğru yakınlaştırır / görünümü kaydırır. | Tekerlek / orta tuş ya da ok tuşları; dokunmatik: 1 parmak kaydırır, 2 parmak yakınlaştırır |
| Mod | Nesne Modu, Düzenleme Modu, Ağırlık Boyama, Kemikleri Düzenle ve Poz Modu (son ikisi, örgünün 2D iskeleti varsa). | Mod düğmesi, <kbd>Tab</kbd> |
| Köşe / Kenar / Yüz | Düzenleme Modunda neyin seçileceği. | Düğme |
| Pivot | 3D İmleç, Ayrı Orijinler ya da Orta Nokta. | Düğme |
| İmleç | İmleç Seçime, Seçimi İmlece, İmleç Merkeze. | Menü |
| Görünüm | Seçimi Eşitle (UV seçimi örgününkiyle aynıdır), Dokuyu Tekrarla (döşeme), Krom UV'yi göster, Seçileni Çerçevele, Tümünü Çerçevele. | Menü; <kbd>Num.</kbd> |
| Doku | Hangi dokunun gösterileceği: Yeni Doku…, Oto (etkin parça) ya da projedeki herhangi bir doku. | Menü |
| Yeni Doku | Bir görüntü oluşturur (ad, boyut, renk, saydamlık; düz, UV Izgarası ya da Renk Izgarası), onu projeye kaydeder ve malzemeye atar. | Doku → Yeni Doku… |
| Mipmap | İncelemek için belirli bir mipmap seviyesini gösterir. | Mip düğmesi |
| Düzenle | Görüntü üzerinde doku boyamaya girer ve çıkar. | Düzenle düğmesi |
| Kaydet (disket, her zaman görünür) | Kaydet, Farklı Kaydet…, .w3d dışına kaydet…, .w3d içine kaydet, UV Düzenini Dışa Aktar… | Menü |
| UV Düzenini Dışa Aktar | Saydam arka planlı bir PNG kaydeder: yarı saydam yüzler (üst üste bindikleri yerde daha opak) ve opak çizgiler. Ad, klasör ve çözünürlük seçilir (varsayılan 512, N95'te 256 ya da gösterilen dokunun boyutu). | Kaydet → UV Düzenini Dışa Aktar… |
| Animasyon | UV katmanının ya da 2D pozun keyframe'lerini ekleme, silme ya da temizleme. | Menü |

## Nesne Modu (UV)

Tık neyin düzenleneceğini seçer: bir 2D iskelet (bir kemiğe dokunulursa) ya da geometri. <kbd>Tab</kbd> seçileni düzenlemeye girer.

## Düzenleme Modu (UV)

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Seç | UV köşeleri, kenarları ya da yüzleri. Senkronizasyon kapalıyken 3D Görünüm filtre görevi görür: yalnızca 3D'de seçili yüzler görünür. | Tık; <kbd>Shift</kbd>+tık |
| Tümü / Hiçbiri / Tersine çevir | Görünenler üzerinde. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Bağlantılıyı Seç | İşaretçinin altındaki UV adası (<kbd>Shift</kbd> ekler). | <kbd>L</kbd> |
| Kutu Seçimi | Dikdörtgen; <kbd>Shift</kbd> ekler. | <kbd>B</kbd> |
| Taşı / Döndür / Ölçekle | UV'leri dönüştürür. <kbd>X</kbd> / <kbd>Y</kbd> bir ekseni kilitler; değeri yazabilirsin (1.0 = bir döşeme). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| 2D imleci konumlandırma | İmleci tıklanan yere koyar. | Sağ tık |
| Anahtar Kare Ekle | UV katmanını etkin köşe animasyonuna kaydeder. | <kbd>I</kbd> |

## Ağırlık Boyama (UV)

2D kemiklerin ağırlıklarını UV haritası üzerinde boyar; 3D Görünüm'dekiyle aynı kontroller kullanılır (+/−/= modu, grup, işaretler, azalma, yalnızca seçili olan). Bir kemiğe <kbd>Ctrl</kbd>+tık, onu boyamak için seçer.

## Kemikleri Düzenle ve Poz Modu (2D iskelet)

| İşlev | Açıklama | Kısayol |
|---|---|---|
| 2D İskelet / Kemik | Örgünün 2D iskeletini (UV grubuyla birlikte) oluşturur ya da bir kemik ekler. | Ekle menüsü |
| Seç | Bir uç ya da kemiğin tamamı; sürüklemek onu taşır. | Tık; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>L</kbd> / <kbd>B</kbd> |
| Taşı / Döndür / Ölçekle | Kemikleri Düzenle'de dinlenme konumunu değiştirir; Poz'da poz verir (ağırlık verilmiş UV'ler kemiği takip eder). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Ekstrüde / Çoğalt / Sil | Yalnızca Kemikleri Düzenle'de. | <kbd>E</kbd> / <kbd>Shift+D</kbd> / <kbd>X</kbd> |
| Ebeveyn Ata / Ebeveyni Kaldır | Ofseti Koru ya da Bağlı / bağlantıyı kesme. | <kbd>Ctrl+P</kbd> / <kbd>Alt+P</kbd> |
| Anahtar Kare Ekle | Poz'da: kanal menüsü ya da <kbd>Shift+I</kbd> ile hepsi. | <kbd>I</kbd> |

## Doku Boyama (UV)

**Düzenle** düğmesiyle doğrudan görüntünün üzerine; renk, azalma, yarıçap ve kuvvetle boyanır. Disketten kaydedilir.

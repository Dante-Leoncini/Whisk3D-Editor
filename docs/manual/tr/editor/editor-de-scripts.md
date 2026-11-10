# Betik Düzenleyici (IDE)

Lua script'leri ve [yazılan shader'lar](shaders.md) (`.glsl`) için satır numaraları, sözdizimi renklendirme, girinti kılavuzları ve ñ ile aksanlı harf desteği olan bir metin editörü. Odaktayken harfler yazı yazar (editörün kısayollarını tetiklemez). Android'de metne dokunmak klavyeyi açar. N95'te T9 ile yazılır.

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Betik | Projedeki hangi .lua dosyasının düzenleneceğini seçme ya da .lua yükle… (harici bir dosya). | Betik menüsü |
| Kaydet | Script'i kaydeder. Script .w3d'nin içindeyse projeyi de kaydeder. | <kbd>Ctrl+S</kbd> ya da Dosya → Kaydet |
| Farklı Kaydet… | Başka bir adla, projenin içine ya da dışına, istediğin bir klasöre. | Dosya |
| .w3d içine kaydet | Yalnızca script haricîyse: onu projenin içine alır. | Dosya |
| .w3d dışına kaydet… | Yalnızca script dahilîyse: onu bir dosyaya çıkarır; nesneler artık o dosyayı kullanır. | Dosya |
| Yenile | Dosyayı diskteki hâliyle getirir (başka bir editörle düzenlenenler). Editörde kaydedilmemiş değişiklikler varsa, onları atmadan önce sorar (<kbd>Ctrl+Z</kbd> ile geri getirilebilirler). Oyun çalışırken, script'i kullanan nesneleri canlı olarak yeniden yükler. | Düğme |
| Shader'lar (.glsl) | Malzemeden (**Shader** satırındaki küçük düğme), Kütüphane'den ya da seçiciden açılır ve GLSL olarak renklendirilir. Adın yanında **Derle** görünür: yazmakta olduğunu derler (bir malzeme onu kullanıyorsa kaydeder ve o malzemeyi derler). Bir shader'ı kaydetmek onu kullanan tüm malzemelerde derler ve derlenip derlenmediğini ya da satırıyla birlikte hatayı bildirir. | <kbd>Ctrl+S</kbd> |
| Kaydetme koruması | Harici dosya açıldığından beri dışarıda değiştiyse, Kaydet üzerine yazmadan önce sorar. Oyun çalışıyorsa Kaydet script'i canlı olarak da yeniden yükler. | Otomatik |
| Girinti (Tab) | <kbd>Tab</kbd> bir sonraki 4'ün katına kadar tamamlar; birden fazla satır seçiliyken onlara bir seviye ekler, <kbd>Shift+Tab</kbd> ise bir seviye çıkarır. Dikey çizgiler her seviyeyi işaretler; imlecin bulunduğu bloğunki daha açık görünür. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Düzenle menüsü | Geri Al, Yinele, Kes, Kopyala, Yapıştır, Bul… ve Değiştir… | Düzenle düğmesi |
| Seç menüsü | Tümünü Seç. | Seç düğmesi |
| Geri Al / Yinele | Script'in metni üzerinde (sahne üzerinde değil). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> veya <kbd>Ctrl+Shift+Z</kbd> |
| Bul / Değiştir | Sağ üstte bir panel açar: aranacak metin, kaç eşleşme olduğu ("3 / 7"), Büyük/küçük harf duyarlı, Tam sözcük, Önceki / Sonraki; Değiştir'de ise yeni metin, Değiştir ve Tümünü değiştir ile birlikte (tek bir geri alma adımı). Eşleşmeler kodda vurgulanır. Panel editörü engellemez: işaretçi script'in üzerindeyken yazmaya devam edebilirsin. Kapat ya da <kbd>Esc</kbd> ile kapanır. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Kopyala / Kes / Yapıştır | Sistem panosuyla. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Tümünü Seç / Geri Al | Geri Al sahneyi değil metni etkiler. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Parmakla seçme | Çift dokunuş kelimeyi, üçlü dokunuş satırın tamamını seçer. Bir seçim varken uçlarında iki tutamaç (yeşil daireler) belirir: seçimi ayarlamak için onları sürükle. | Dokunuşlar |
| Büyüteç (dokunmatik) | Parmağını kodun üzerinde bir an hareketsiz tutmak, parmağın üstünde imleçle birlikte büyütülmüş satırı gösterir; sürüklediğinde imleç parmağı takip eder ve bıraktığın yerde kalır. | Uzun basma |
| N95'te seçme | Ok tuşları imleci hareket ettirir; Kalem tuşu basılıyken ok tuşları imlecin bulunduğu yerden itibaren seçer. Kalem basılı tutulurken altta bir çubuk belirir: sol seçim tuşu **kopyalar**, sağdaki **yapıştırır**. | Kalem + ok tuşları / softkey'ler |
| Metinde gezinme | Ok tuşları, Home, End, PageUp, PageDown (<kbd>Shift</kbd> ile seçerler); <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> başa ya da sona gider. | Klavye |

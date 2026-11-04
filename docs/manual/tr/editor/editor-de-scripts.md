# Script Editor (IDE)

Lua script'leri ve [yazılan shader'lar](shaders.md) (`.glsl`) için satır numaraları, sözdizimi renklendirme, girinti kılavuzları ve ñ ile aksanlı harf desteği olan bir metin editörü. Odaktayken harfler yazı yazar (editörün kısayollarını tetiklemez). Android'de metne dokunmak klavyeyi açar. N95'te T9 ile yazılır.

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Script | Projedeki hangi .lua dosyasının düzenleneceğini seçme ya da Load .lua… (harici bir dosya). | Script menüsü |
| Save | Script'i kaydeder. Script .w3d'nin içindeyse projeyi de kaydeder. | <kbd>Ctrl+S</kbd> ya da File → Save |
| Save As… | Başka bir adla, projenin içine ya da dışına, istediğin bir klasöre. | File |
| Save inside the .w3d | Yalnızca script haricîyse: onu projenin içine alır. | File |
| Save outside the .w3d… | Yalnızca script dahilîyse: onu bir dosyaya çıkarır; nesneler artık o dosyayı kullanır. | File |
| Refresh | Dosyayı diskteki hâliyle getirir (başka bir editörle düzenlenenler). Editörde kaydedilmemiş değişiklikler varsa, onları atmadan önce sorar (<kbd>Ctrl+Z</kbd> ile geri getirilebilirler). Oyun çalışırken, script'i kullanan nesneleri canlı olarak yeniden yükler. | Düğme |
| Shader'lar (.glsl) | Materyalden (**Shader** satırındaki küçük düğme), Library'den ya da seçiciden açılır ve GLSL olarak renklendirilir. Adın yanında **Compile** görünür: yazmakta olduğunu derler (bir materyal onu kullanıyorsa kaydeder ve o materyali derler). Bir shader'ı kaydetmek onu kullanan tüm materyallerde derler ve derlenip derlenmediğini ya da satırıyla birlikte hatayı bildirir. | <kbd>Ctrl+S</kbd> |
| Kaydetme koruması | Harici dosya açıldığından beri dışarıda değiştiyse, Save üzerine yazmadan önce sorar. Oyun çalışıyorsa Save script'i canlı olarak da yeniden yükler. | Otomatik |
| Girinti (Tab) | <kbd>Tab</kbd> bir sonraki 4'ün katına kadar tamamlar; birden fazla satır seçiliyken onlara bir seviye ekler, <kbd>Shift+Tab</kbd> ise bir seviye çıkarır. Dikey çizgiler her seviyeyi işaretler; imlecin bulunduğu bloğunki daha açık görünür. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Edit menüsü | Undo, Redo, Cut, Copy, Paste, Find… ve Replace… | Edit düğmesi |
| Select menüsü | Select All. | Select düğmesi |
| Undo / Redo | Script'in metni üzerinde (sahne üzerinde değil). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> veya <kbd>Ctrl+Shift+Z</kbd> |
| Find / Replace | Sağ üstte bir panel açar: aranacak metin, kaç eşleşme olduğu ("3 of 7"), Match case, Whole word, Previous / Next; Replace'te ise yeni metin, Replace ve Replace all ile birlikte (tek bir geri alma adımı). Eşleşmeler kodda vurgulanır. Panel editörü engellemez: işaretçi script'in üzerindeyken yazmaya devam edebilirsin. Close ya da <kbd>Esc</kbd> ile kapanır. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copy / Cut / Paste | Sistem panosuyla. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Select All / Undo | Undo sahneyi değil metni etkiler. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Parmakla seçme | Çift dokunuş kelimeyi, üçlü dokunuş satırın tamamını seçer. Bir seçim varken uçlarında iki tutamaç (yeşil daireler) belirir: seçimi ayarlamak için onları sürükle. | Dokunuşlar |
| Büyüteç (dokunmatik) | Parmağını kodun üzerinde bir an hareketsiz tutmak, parmağın üstünde imleçle birlikte büyütülmüş satırı gösterir; sürüklediğinde imleç parmağı takip eder ve bıraktığın yerde kalır. | Uzun basma |
| N95'te seçme | Ok tuşları imleci hareket ettirir; Pencil tuşu basılıyken ok tuşları imlecin bulunduğu yerden itibaren seçer. Pencil basılı tutulurken altta bir çubuk belirir: sol seçim tuşu **kopyalar**, sağdaki **yapıştırır**. | Pencil + ok tuşları / softkey'ler |
| Metinde gezinme | Ok tuşları, Home, End, PageUp, PageDown (<kbd>Shift</kbd> ile seçerler); <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> başa ya da sona gider. | Klavye |

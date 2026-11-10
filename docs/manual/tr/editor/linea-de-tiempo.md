# Zaman Çizelgesi

Animasyonu gösterir ve düzenler: keyframe sayfası (dope sheet) ya da eğri editörü. **Seçili** olanların canlandırılmış özelliklerini gösterir.

## Üst çubuk

| İşlev | Açıklama |
|---|---|
| Oynatma kontrolleri | Başa git, önceki keyframe, önceki kare, geriye oynat, oynat / duraklat, sonraki kare, sonraki keyframe, sona git. |
| Başlangıç / Son / FPS | Animasyonun aralığı ve hızı (yazmak için tıkla). |
| Animasyon | Hangi animasyonun düzenleneceği (sahne, klipler, iskeletler, nesneler, flipbook'lar, Oyun, Karıştır) ve yenilerini oluşturma. |
| Oto Anahtar | Kırmızıyken: her dönüştürme kendiliğinden keyframe kaydeder. |
| Eğriler / Anahtar Tablosu | Keyframe sayfası ile eğri editörü arasında geçiş yapar. |
| Seç, Anahtar, Görünüm, Pivot | Seçim, keyframe işlemleri, kadraja alma ve yan panel menüleri; ayrıca <kbd>S</kbd>'nin nereden ölçekleyeceği (Merkez ya da Geçerli Kare). |

## Zamanda gezinme

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Kare değiştirme | Kare numaraları üzerinde tıklama ya da sürükleme (scrub). Geçerli kare (yeşil) yazılarak da girilebilir. | Tık |
| Önceki kare / Sonraki kare | Bir kare geri ya da ileri. | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Önceki anahtar kare / Sonraki anahtar kare | En yakın keyframe'e atlar. | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Başa git / Sona git | İlk ya da son kare. | <kbd>Home</kbd> / <kbd>End</kbd> |
| Oynat / Duraklat | Herhangi bir panelden. | <kbd>Space</kbd> |
| Yakınlaştırma ve kaydırma | Tekerlek = yakınlaştırma (eğrilerde <kbd>Ctrl</kbd> = yalnızca zaman, <kbd>Shift</kbd> = yalnızca değer); orta tuş = kaydırma. | Tekerlek / orta tuş |
| Seçileni Çerçevele | Görünümü seçilene göre ayarlar. | <kbd>Num.</kbd> |

## Keyframe düzenleme (dope sheet ve eğriler)

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Seç | Bir eşkenar dörtgene ya da tüm keyframe'lerini almak için bir satıra tıkla; <kbd>Shift</kbd> ekler. | Tık; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Taşı (keyframe'ler) | Keyframe'leri zamanda kaydırır (bir eşkenar dörtgeni sürükleyerek de olur). | <kbd>G</kbd> |
| Ölçekle (keyframe'ler) | Pivottan itibaren uzatır ya da sıkıştırır. | <kbd>S</kbd> |
| Tutamaçları döndür | Yalnızca eğrilerde. | <kbd>R</kbd> |
| Çoğalt | Keyframe'leri kopyalar ve tutulmuş hâlde bırakır. | <kbd>Shift+D</kbd> |
| Sil | Seçili keyframe'leri (ya da satırların eğrilerini) siler. | <kbd>X</kbd> |
| Enterpolasyon | Sabit, Doğrusal ya da Bezier. | <kbd>T</kbd> |
| Tutamaç Türü | Serbest, Hizalı, Vektör, Otomatik, Oto Sınırlı (yalnızca eğriler). | <kbd>V</kbd> |
| Akıllı Euler | Seçili dönüşlerdeki fazladan turları kaldırır. | Anahtar menüsü |
| Bir eğriyi gizleme | Her satırdaki göz onu gizler. | Göze tıkla |
| Tam değerler | Seçilen keyframe'in karesi, değeri ve tutamaçları. | Özellikler → Animasyon → Anahtar kare |

## Sesler

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Ses şeridi | Seçili her Ses nesnesinin bir şeridi vardır: nerede başladığı ve ne kadar sürdüğü, **saniye** cinsinden (FPS değiştirmek onu kaydırmaz). Döngüde sona kadar tekrarlar. | — |
| Taşı | Şeridi sürükleyin ya da anahtar kare seçmeden <kbd>G</kbd> (seçili tüm sesleri taşır). <kbd>Ctrl</kbd> karelere yapıştırır. | Sürükle / <kbd>G</kbd> |
| Kırp | Şeridin bir ucunu tutun. | Ucu sürükle |
| Dalga biçimi / Ses düzeyi çizgisi | Görünüm menüsü: şeridin içinde dalga biçimi ve ses düzeyi eğrisi (şerit üç satır kaplar). Bunlar olmadan yalnızca şerittir. | Görünüm menüsü |

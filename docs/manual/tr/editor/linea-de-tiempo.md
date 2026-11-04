# Timeline

Animasyonu gösterir ve düzenler: keyframe sayfası (dope sheet) ya da eğri editörü. **Seçili** olanların canlandırılmış özelliklerini gösterir.

## Üst çubuk

| İşlev | Açıklama |
|---|---|
| Oynatma kontrolleri | Başa git, önceki keyframe, önceki kare, geriye oynat, oynat / duraklat, sonraki kare, sonraki keyframe, sona git. |
| Start / End / FPS | Animasyonun aralığı ve hızı (yazmak için tıkla). |
| Animation | Hangi animasyonun düzenleneceği (sahne, klipler, iskeletler, nesneler, flipbook'lar, Game, Mix) ve yenilerini oluşturma. |
| Auto Key | Kırmızıyken: her dönüştürme kendiliğinden keyframe kaydeder. |
| Curves / Dope Sheet | Keyframe sayfası ile eğri editörü arasında geçiş yapar. |
| Select, Key, View, Pivot | Seçim, keyframe işlemleri, kadraja alma ve yan panel menüleri; ayrıca <kbd>S</kbd>'nin nereden ölçekleyeceği (Center ya da Current Frame). |

## Zamanda gezinme

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Kare değiştirme | Kare numaraları üzerinde tıklama ya da sürükleme (scrub). Geçerli kare (yeşil) yazılarak da girilebilir. | Tık |
| Previous frame / Next frame | Bir kare geri ya da ileri. | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Previous keyframe / Next keyframe | En yakın keyframe'e atlar. | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Go to start / Go to end | İlk ya da son kare. | <kbd>Home</kbd> / <kbd>End</kbd> |
| Play / Pause | Herhangi bir panelden. | <kbd>Space</kbd> |
| Yakınlaştırma ve kaydırma | Tekerlek = yakınlaştırma (eğrilerde <kbd>Ctrl</kbd> = yalnızca zaman, <kbd>Shift</kbd> = yalnızca değer); orta tuş = kaydırma. | Tekerlek / orta tuş |
| Frame Selected | Görünümü seçilene göre ayarlar. | <kbd>Num.</kbd> |

## Keyframe düzenleme (dope sheet ve eğriler)

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Select | Bir eşkenar dörtgene ya da tüm keyframe'lerini almak için bir satıra tıkla; <kbd>Shift</kbd> ekler. | Tık; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Move (keyframe'ler) | Keyframe'leri zamanda kaydırır (bir eşkenar dörtgeni sürükleyerek de olur). | <kbd>G</kbd> |
| Scale (keyframe'ler) | Pivottan itibaren uzatır ya da sıkıştırır. | <kbd>S</kbd> |
| Rotate handles | Yalnızca eğrilerde. | <kbd>R</kbd> |
| Duplicate | Keyframe'leri kopyalar ve tutulmuş hâlde bırakır. | <kbd>Shift+D</kbd> |
| Delete | Seçili keyframe'leri (ya da satırların eğrilerini) siler. | <kbd>X</kbd> |
| Interpolation | Constant, Linear ya da Bezier. | <kbd>T</kbd> |
| Handle Type | Free, Aligned, Vector, Automatic, Auto Clamped (yalnızca eğriler). | <kbd>V</kbd> |
| Smart Euler | Seçili dönüşlerdeki fazladan turları kaldırır. | Key menüsü |
| Bir eğriyi gizleme | Her satırdaki göz onu gizler. | Göze tıkla |
| Tam değerler | Seçilen keyframe'in karesi, değeri ve tutamaçları. | Properties → Animation → Keyframe |

## Sesler

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Ses şeridi | Seçili her Sound nesnesinin bir şeridi vardır: nerede başladığı ve ne kadar sürdüğü, **saniye** cinsinden (FPS değiştirmek onu kaydırmaz). Döngüde sona kadar tekrarlar. | — |
| Taşı | Şeridi sürükleyin ya da anahtar kare seçmeden <kbd>G</kbd> (seçili tüm sesleri taşır). <kbd>Ctrl</kbd> karelere yapıştırır. | Sürükle / <kbd>G</kbd> |
| Kırp | Şeridin bir ucunu tutun. | Ucu sürükle |
| Waveform / Volume line | View menüsü: şeridin içinde dalga biçimi ve ses düzeyi eğrisi (şerit üç satır kaplar). Bunlar olmadan yalnızca şerittir. | View menüsü |

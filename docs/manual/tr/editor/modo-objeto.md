# Nesne Modu

| İşlev | Açıklama | Nasıl kullanılır |
|---|---|---|
| Seç | Bir nesne seçer. Boş alana tıklamak seçimi kaldırır. | Tık; <kbd>Shift</kbd>+tık ekler ya da çıkarır |
| Taşı / Döndür / Ölçekle | [Taşıma, döndürme ve ölçekleme](transformar.md) bölümüne bak. | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Ekle | 3D imleçte yeni bir nesne oluşturur ([Ekle menüsü](#ekle-menüsü) bölümüne bak). | <kbd>Shift+A</kbd> ya da + düğmesi |
| Uygula | Konumu, dönüşü, ölçeği ya da hepsini örgüye işler (nesne 0, 0, 0'da, dönüşsüz ve 1 ölçeğinde kalır). | <kbd>Ctrl+A</kbd> |
| Orijini Ayarla | Geometriyi Orijine, Orijini Geometriye, Orijini 3D İmlece. | Nesne menüsü |
| Mıknatıs | 3D imleci seçilene, etkin nesneye, dünyanın orijinine ya da ızgaraya; seçileni de imlece, etkin nesneye ya da ızgaraya götürür. | <kbd>Shift+S</kbd> ya da Nesne menüsü → Mıknatıs |
| Nesneleri Çoğalt | Bağımsız kopya; taşıyabilmen için tutulmuş hâlde kalır. | <kbd>Shift+D</kbd> |
| Bağlantılı Çoğalt | Örgüyü orijinalle paylaşan kopya (birini düzenlemek ikisini de düzenler). | <kbd>Alt+D</kbd> |
| Birleştir | Seçili örgüleri etkin nesnede birleştirir. | <kbd>Ctrl+J</kbd> |
| Ebeveyn Ata | Seçilenler etkin nesnenin çocuğu olur (dönüşümü koruyarak ya da korumadan). | <kbd>Ctrl+P</kbd> |
| Ebeveyni Kaldır | Ebeveyni kaldırır (konumu koruyarak ya da korumadan). | <kbd>Alt+P</kbd> |
| Sil | Seçili nesneleri siler (onay ister). | <kbd>X</kbd> ya da <kbd>Delete</kbd> |
| Gizle / Göster | Etkin nesnenin görünürlüğünü açıp kapatır. | <kbd>H</kbd> |
| Prefab oluştur / Prefabı paketten çıkar | Seçimi yeniden kullanılabilir bir prefab'a dönüştürür ya da bir örneği yeniden ayırır. | Nesne menüsü |
| Anahtar Kare Ekle | Konumu, dönüşü ve/veya ölçeği geçerli karede kaydeder (kanal menüsü). <kbd>Shift+I</kbd> sormadan hepsini kaydeder. | <kbd>I</kbd> |
| Anahtar Kareyi Sil / Anahtar Kareleri Temizle | Geçerli karedeki keyframe'i ya da tüm animasyonu siler. | Animasyon menüsü |
| Hareket İzi | Canlandırılmış nesnelerin izlediği yolu çizer. | Animasyon menüsü |
| Mod değiştirme | Örgü: Nesne ↔ Düzenleme. İskelet: Nesne ↔ Poz. Eğri: düğüm düzenleme. | <kbd>Tab</kbd> |

## Ekle menüsü

Yalnızca Nesne Modunda. Yeni nesne 3D imleçte belirir ve seçili kalır. İlkel şekiller, ölçülerini ayarlamak için bir panel açar.

| İşlev | Açıklama |
|---|---|
| Örgü | Düzlem, Küp, Daire, UV Küre, Koni, Silindir (boyut, yarıçap, segment sayısı ve yumuşak gölgelendirme ile). Ayrıca Köşe (modellemeye başlamak için tek başına bir nokta) ve Referans (üzerinden kopya çizmek için resimli bir düzlem). |
| Boş | Gruplamak ya da referans noktası olarak kullanmak için örgüsü olmayan bir nesne. |
| Kamera / Işık / Sis | Sahnenin kamerası, ışığı ve sisi. Işığın bir **Tür** değeri var: Nokta, Güneş (yönlü) veya Spot (görünümde çizilen bir koni); Ekle menüsü Güneş ve Spot da sunar. **Şiddet** ışığı güçlendirir (canlandırılabilir); **Mesafe** (m), nokta ışığın ya da spotun ne kadar uzağa ulaştığıdır ve konisinin uzunluğudur (0 = sınırsız; ışın izleme ışığı orada yumuşakça söndürür). Işık seçiliyken spot konisini, nokta ışık da o mesafede bir küre gösterir; güneş her zaman aydınlattığı yöne doğru zemine kadar bir çizgi gösterir. |
| Ses | Sahneye yerleştirilmiş bir ses (hoparlör simgesi). Zaman çizelgesiyle **saniye** cinsinden çalar, kare değil; **Konumsal (3D)** ile bulunduğu yerden duyulur (kamera yaklaştıkça daha yüksek, bulunduğu taraftan); onsuz müzik gibidir. 3D görünüme bir .wav bırakmak orada bir tane oluşturur. |
| İskelet | Karakterleri canlandırmak için tek kemikli bir iskelet. |
| Yol | İki düğümlü bir eğri (örgü gibi düzenlenir: <kbd>E</kbd> düğüm ekler). |
| Koleksiyon | Nesneleri düzenlemek için bir klasör. |
| UI | Oyunlar için bir 2D arayüz (2D Düzenleyici'de düzenlenir). |
| Hitbox | Bir çarpışma algılama kutusu; etkin bir nesne varsa onun çocuğu olarak ve onun boyutunda oluşur. |
| Fizik | Nesne oluşturmaz: etkin nesneye bir katı cisim ekler. |
| Parçacıklar | Bir parçacık yayıcı. |
| LOD / Culling / Grid Cull | Optimizasyon nesneleri: ayrıntı seviyeleri ve görünmeyenlerin gizlenmesi. |
| Derinliği Temizle / Ekran Kırpma | Çizim araçları: derinliği temizleme ya da ekranın bir bölgesini kırpma. |
| Prefab / Sahne / W3D Proxy | Projeden bir prefab'ı, başka bir sahneyi ya da harici bir kütüphaneden bir prefab'ı ekleme. |
| İçe aktar | OBJ, FBX, glTF, GLB, STL ya da bir Whisk3D UI arayüzü. STL (3D baskı, CAD), Z yukarı olduğu için dik duracak şekilde döndürülerek ve aynı konumdaki köşeler kaynaştırılarak içe aktarılır. |
| Çekirdek (gelişmiş) | Rutinler: elle kurulan çizim adımı listeleri (dönüştürme, çizme, dokular, ışıklar, sis, karıştırma, derinlik, kontrol, Lua çağır). **Rutin döngüsü** de buna dahildir: adımlarını birkaç kez tekrarlayan bir rutin (örneğin "taşı, çiz" 20 kez); bir array gibi ama daha hızlı. |

# Object Mode

| İşlev | Açıklama | Nasıl kullanılır |
|---|---|---|
| Select | Bir nesne seçer. Boş alana tıklamak seçimi kaldırır. | Tık; <kbd>Shift</kbd>+tık ekler ya da çıkarır |
| Move / Rotate / Scale | [Taşıma, döndürme ve ölçekleme](transformar.md) bölümüne bak. | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Add | 3D imleçte yeni bir nesne oluşturur ([Add menüsü](#add-menüsü) bölümüne bak). | <kbd>Shift+A</kbd> ya da + düğmesi |
| Apply | Konumu, dönüşü, ölçeği ya da hepsini mesh'e işler (nesne 0, 0, 0'da, dönüşsüz ve 1 ölçeğinde kalır). | <kbd>Ctrl+A</kbd> |
| Set Origin | Geometry to Origin, Origin to Geometry, Origin to 3D Cursor. | Object menüsü |
| Duplicate Objects | Bağımsız kopya; taşıyabilmen için tutulmuş hâlde kalır. | <kbd>Shift+D</kbd> |
| Duplicate Linked | Mesh'i orijinalle paylaşan kopya (birini düzenlemek ikisini de düzenler). | <kbd>Alt+D</kbd> |
| Join | Seçili mesh'leri etkin nesnede birleştirir. | <kbd>Ctrl+J</kbd> |
| Set Parent | Seçilenler etkin nesnenin çocuğu olur (dönüşümü koruyarak ya da korumadan). | <kbd>Ctrl+P</kbd> |
| Clear Parent | Ebeveyni kaldırır (konumu koruyarak ya da korumadan). | <kbd>Alt+P</kbd> |
| Delete | Seçili nesneleri siler (onay ister). | <kbd>X</kbd> ya da <kbd>Delete</kbd> |
| Hide / Show | Etkin nesnenin görünürlüğünü açıp kapatır. | <kbd>H</kbd> |
| Create Prefab / Unpack Prefab | Seçimi yeniden kullanılabilir bir prefab'a dönüştürür ya da bir örneği yeniden ayırır. | Object menüsü |
| Insert Keyframe | Konumu, dönüşü ve/veya ölçeği geçerli karede kaydeder (kanal menüsü). <kbd>Shift+I</kbd> sormadan hepsini kaydeder. | <kbd>I</kbd> |
| Delete Keyframe / Clear Keyframe | Geçerli karedeki keyframe'i ya da tüm animasyonu siler. | Animation menüsü |
| Motion Trail | Canlandırılmış nesnelerin izlediği yolu çizer. | Animation menüsü |
| Mod değiştirme | Mesh: Object ↔ Edit. İskelet: Object ↔ Pose. Eğri: düğüm düzenleme. | <kbd>Tab</kbd> |

## Add menüsü

Yalnızca Object Mode'da. Yeni nesne 3D imleçte belirir ve seçili kalır. İlkel şekiller, ölçülerini ayarlamak için bir panel açar.

| İşlev | Açıklama |
|---|---|
| Mesh | Plane, Cube, Circle, UV Sphere, Cone, Cylinder (boyut, yarıçap, segment sayısı ve yumuşak gölgelendirme ile). Ayrıca Vertex (modellemeye başlamak için tek başına bir nokta) ve Reference (üzerinden kopya çizmek için resimli bir düzlem). |
| Empty | Gruplamak ya da referans noktası olarak kullanmak için mesh'i olmayan bir nesne. |
| Camera / Light / Fog | Sahnenin kamerası, ışığı ve sisi. Işığın bir **Type** değeri var: Point, Sun (yönlü) veya Spot (görünümde çizilen bir koni); Add menüsü Sun ve Spot da sunar. **Intensity** ışığı güçlendirir (canlandırılabilir); **Distance** (m), nokta ışığın ya da spotun ne kadar uzağa ulaştığıdır ve konisinin uzunluğudur (0 = sınırsız; ışın izleme ışığı orada yumuşakça söndürür). Işık seçiliyken spot konisini, nokta ışık da o mesafede bir küre gösterir; güneş her zaman aydınlattığı yöne doğru zemine kadar bir çizgi gösterir. |
| Sound | Sahneye yerleştirilmiş bir ses (hoparlör simgesi). Zaman çizelgesiyle **saniye** cinsinden çalar, kare değil; **Positional (3D)** ile bulunduğu yerden duyulur (kamera yaklaştıkça daha yüksek, bulunduğu taraftan); onsuz müzik gibidir. 3D görünüme bir .wav bırakmak orada bir tane oluşturur. |
| Armature | Karakterleri canlandırmak için tek kemikli bir iskelet. |
| Path | İki düğümlü bir eğri (mesh gibi düzenlenir: <kbd>E</kbd> düğüm ekler). |
| Collection | Nesneleri düzenlemek için bir klasör. |
| UI | Oyunlar için bir 2D arayüz (2D Editor'de düzenlenir). |
| Hitbox | Bir çarpışma algılama kutusu; etkin bir nesne varsa onun çocuğu olarak ve onun boyutunda oluşur. |
| Physics | Nesne oluşturmaz: etkin nesneye bir katı cisim ekler. |
| Particles | Bir parçacık yayıcı. |
| LOD / Culling / Grid Cull | Optimizasyon nesneleri: ayrıntı seviyeleri ve görünmeyenlerin gizlenmesi. |
| Clear Depth / Screen Rect | Çizim araçları: derinliği temizleme ya da ekranın bir bölgesini kırpma. |
| Prefab / Scene / Proxy W3D | Projeden bir prefab'ı, başka bir sahneyi ya da harici bir kütüphaneden bir prefab'ı ekleme. |
| Imports | OBJ, FBX, glTF, GLB ya da bir Whisk3D UI arayüzü. |
| Core (advanced) | Rutinler: elle kurulan çizim adımı listeleri (dönüştürme, çizme, dokular, ışıklar, sis, karıştırma, derinlik, kontrol, Call Lua). **Routine loop** da buna dahildir: adımlarını birkaç kez tekrarlayan bir rutin (örneğin "taşı, çiz" 20 kez); bir array gibi ama daha hızlı. |

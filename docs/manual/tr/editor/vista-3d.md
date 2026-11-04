# 3D Viewport

3D Viewport, sahnenin görüntülendiği ve düzenlendiği yerdir. Farklı **modlarda** çalışır: tıklamaların, tuşların ve menülerin ne yaptığı o anki moda bağlıdır. Mod, üst çubuktaki seçiciyle (yalnızca etkin nesne bir mesh ya da iskeletse görünür) veya <kbd>Tab</kbd> ile seçilir.

| Mod | Hangi nesneler için | Ne işe yarar |
|---|---|---|
| [Object Mode](modo-objeto.md) | Hepsi | Nesnelerin bütününü oluşturma, taşıma, döndürme, ölçekleme, çoğaltma, ebeveyne bağlama ve silme. |
| [Edit Mode](modo-edicion.md) | Mesh'ler ve eğriler | Köşeleri, kenarları ve yüzleri düzenleme (modelleme). |
| [Edit Mode (iskelet)](esqueletos.md#edit-mode-iskelet) | İskeletler | Kemikleri oluşturma ve yerleştirme. |
| [Pose Mode](esqueletos.md#pose-mode) | İskeletler | Kemiklere poz verme ve onları canlandırma. |
| [Weight Paint](pintura.md#weight-paint) | Yalnızca 3D mesh'ler | Her kemiğin her köşeyi ne kadar hareket ettirdiğini boyama. |
| [Vertex Paint](pintura.md#vertex-paint) | Yalnızca 3D mesh'ler | Renkleri doğrudan köşelerin üzerine boyama. |
| [Texture Paint](pintura.md#texture-paint) | Yalnızca 3D mesh'ler | Doğrudan nesnenin üzerinden dokuya boyama. |

## 3D Viewport'un üst çubuğu

| Düğme | Açıklama |
|---|---|
| Mod seçici | Modu değiştirir (Object, Edit, Pose, Paint…). |
| Vertex / Edge / Face | Yalnızca Edit Mode'da: neyin seçileceği. |
| Transform Pivot Point | Döndürme ve ölçeklemenin neyin etrafında yapılacağı: 3D Cursor, Individual Origins, Median Point veya Active Element. Lock Normals onay kutusuyla birlikte. |
| Orientation | Dönüştürmenin eksenleri: Global, Local, Normal (yüzün yönü) veya View. |
| Snap | Snap (yapıştırma) seçenekleri. Etkinse yeşil olur (<kbd>Shift+Tab</kbd>). |
| Proportional Editing | Bir şeyi taşırken yakınındakiler de yumuşakça ona eşlik eder. Etkinse yeşil olur (<kbd>O</kbd>). |
| View | Cameras, Viewpoint, Frame Selected, Local View, Perspective/Ortho, Lock Orbit. |
| Select | All, None, Invert, Box Select, Select Linked, loop'lar ve yollar. |
| Add (+) | Yalnızca Object Mode'da: yeni nesneler oluşturma. |
| Mesh | Yalnızca Edit Mode'da: modelleme işlemleri. |
| Save Texture (disket) | Yalnızca Texture Paint'te. Kaydedilmemiş boya varsa yeşildir. |
| Animation (eşkenar dörtgen) | Keyframe ekleme, silme ve temizleme; Motion Trail. |
| Context | Modun menüsü: Object, Vertex/Edge/Face, Armature veya Pose. |
| Overlays (göz) | Hangi yardımcıların çizileceği: Floor, eksenler, Gizmo, 3D Cursor, Normals, Statistics, X-Ray. |
| Render (kamera) | Gölgelendirme modu. |
| Scene | Projenin hangi sahnesinin, oyununun ya da prefab'ının düzenleneceğini seçme ve yenilerini oluşturma. |
| UV | Yalnızca Edit Mode'da: UV açma ve yansıtma. |

## Birden fazla modda ortak menüler

### Select

| İşlev | Açıklama | Kısayol |
|---|---|---|
| All | Her şeyi seçer. | <kbd>A</kbd> |
| None | Tüm seçimi kaldırır. | <kbd>Alt+A</kbd> |
| Invert | Seçili olanların seçimi kalkar, seçili olmayanlar seçilir. | <kbd>Ctrl+I</kbd> |
| Box Select | Soldan sağa sürüklemek (yeşil) yalnızca tamamen içinde kalanları alır; sağdan sola sürüklemekte (mavi) değmesi yeterlidir. <kbd>Shift</kbd> ekler. | <kbd>B</kbd> |
| Show selection | Seçilenin dış çizgisini gösterir ya da gizler. | Menü |

### Render (gölgelendirme modları)

| İşlev | Açıklama |
|---|---|
| Render Preview | Sahnenin ışıklarıyla. Ray Tracing etkinse, ışın izlenen görüntüyü yavaş yavaş gösterir. |
| Material Preview | Varsayılan mod: sahnenin ışıklarına bağlı olmadan, görünümden gelen nötr bir ışık. |
| Solid Preview / Wireframe Preview / ZBuffer Preview | Düz gri, yalnızca kenarlar ya da derinlik. |
| Normal View / Alpha Preview | Renkli normaller ya da siyah beyaz saydamlık. |
| Motion vectors | Her pikselin önceki kareden beri ekranda ne kadar hareket ettiği (kırmızı = X, yeşil = Y; lavanta = sabit, zeytin yeşili = arka plan). Render’ın `_vectores` geçişiyle aynı görüntü. |
| Wireframe: Only Visible | Tel kafes görünüm yalnızca gerçekten çizilenleri gösterir. |

### Overlays

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Show Overlays | Tüm görsel yardımcıları tek seferde açar ya da kapatır. | <kbd>K</kbd> |
| Floor, X Axis, Y Axis, Origins, 3D Cursor, Relationship Lines | Her yardımcı ayrı ayrı. | Menü |
| Gizmo | Taşıma/döndürme/ölçekleme gizmosu. | Menü |
| Objects | İskeletleri, ışıkları, kameraları, boş nesneleri, parçacıkları, eğrileri, hitbox'ları gösterme ya da gizleme. | Menü |
| Normals | Köşe normallerini, özel normalleri ya da yüz normallerini boyutlarıyla birlikte çizer. | Menü |
| Statistics | FPS, köşeler, yüzler, çizim çağrıları ve süreler. | Menü |
| X-Ray | Düzenlenen mesh yarı saydam görünür ve arkadaki öğeler de seçilebilir. | Menü |

### Snap

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Enable | Taşınan şey köşelere, kenarlara, yüzlere ya da bunların merkezlerine yapışır. | <kbd>Shift+Tab</kbd> |
| Snap Base / Snap Target | Taşınanın hangi noktasının yapışacağı (Closest, Center, Median, Active) ve neye yapışacağı. | Menü |
| Face Project / Face Nearest | Retopoloji için: köşeler başka bir yüzeyin üzerine oturur. | Menü |
| Affect Move / Rotate / Scale | Snap'in hangi işlemlerde etkili olacağı. | Menü |
| İmlecin Snap menüsü | Selection to Grid, Selection to Cursor ya da Selection to Active; Cursor to Selected, Cursor to World Origin, Cursor to Grid ya da Cursor to Active. | <kbd>Shift+S</kbd> |

### Proportional Editing

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Enable | Taşırken, döndürürken ya da ölçeklerken komşu öğeler uzaklığa göre eşlik eder. | <kbd>O</kbd> |
| Connected Only | Uzaklığı kenarlar boyunca ölçer: ayrı parçaları sürüklemez. | Menü |
| Falloff | Etkinin şekli: Smooth, Sphere, Root, Inverse Square, Sharp, Linear, Constant, Random. | Menü |
| Proportional Size | Etki yarıçapı. Dönüştürme sırasında tekerlekle ya da <kbd>PageUp</kbd> / <kbd>PageDown</kbd> ile değiştirilir. | Menü / tekerlek |

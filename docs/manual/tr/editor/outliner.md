# Outliner

Sahnedeki her şeyin listesi ve aynı zamanda projenin kaynak kütüphanesi. Android'de gösterilmez (orada 3D Viewport ve Properties ile çalışılır).

## View menüsü (göz)

| İşlev | Açıklama |
|---|---|
| Scene / Library / kütüphaneler | Neyin listeleneceği: sahne ağacı, projenin tüm kaynakları ya da bağlı başka bir .w3d'nin kaynakları (salt okunur). |
| Constructor / Routines | Constructor rutinini (başlangıçta yalnızca bir kez çalışan kısım) ve diğer rutinleri gösterir ya da gizler. |
| Visibility icons / Render icons | Göz ve kamera sütunlarını gösterir ya da gizler. |
| Grid | Library'de: liste yerine küçük resimler. |
| Link .w3d… / Unlink | Başka bir projeyi kütüphane olarak kullanır ya da bağlantısını kaldırır. |

## Scene görünümü

| İşlev | Açıklama | Nasıl kullanılır |
|---|---|---|
| Select | Nesneyi seçer. <kbd>Shift</kbd>+tık bir aralık seçer; <kbd>Ctrl</kbd>+tık ekler ya da çıkarır. | Tık |
| Açma / kapama | Çocukları gösterir ya da gizler. | Ok simgesi; <kbd>Left</kbd> / <kbd>Right</kbd> |
| Visible / Renderable | Göz nesneyi gizler (canlandırılabilir); kamera onu render dışında bırakır. | Göze / kameraya tıkla |
| Yeniden sıralama ve ebeveyne bağlama | Başka bir satırın önüne, arkasına ya da üstüne (çocuk olarak) bırak. | Satırı sürükle |
| Move (reorder) | Klavyeyle: yukarı/aşağı yeniden sıralar, sol ebeveynden çıkarır, sağ bir öncekinin altına yerleştirir, <kbd>Enter</kbd> onaylar. | <kbd>G</kbd> |
| Rename | Satır bir metin alanına dönüşür. | Çift tık ya da <kbd>F2</kbd> |
| Duplicate / Duplicate Linked | 3D Viewport'taki gibi. | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Hide / Show | Görünürlüğü açıp kapatır. | <kbd>H</kbd> |
| Delete | Onay ister. | <kbd>X</kbd> ya da <kbd>Delete</kbd> |
| Create Prefab / Unpack Prefab | Object menüsünden. | Sağ tık |
| Show active | Listeyi etkin nesneye kadar kaydırır. | <kbd>Num.</kbd> |
| Listeyi kaydırma | Tekerlek ya da sürükleme (parmakla da olur). | Tekerlek / sürükleme |

## Library görünümü

| İşlev | Açıklama |
|---|---|
| Kaynaklar | Klasörler içinde mesh'ler, materyaller, dokular, animasyonlar, prefab'lar, sahneler, sesler, script'ler, [shader'lar](shaders.md), yazı tipleri ve videolar. 3D Viewport'ta bir mesh'in üzerine sürüklenen bir materyal ya da shader, bırakılmadan önce işaretçinin altındaki parçada görünür. Yeşil onay işareti = kullanımda; kırmızı çarpı = sahipsiz; `*` = kaydedilmemiş. |
| Filter / Liste ya da Grid | Yalnızca tek bir türü gösterme; küçük resimleri görme. |
| Bir kaynağı sürükleme | Bir klasöre (onu taşır), 3D Viewport'a (mesh = yeni nesne, materyal ya da doku = dokunulan şeye atanır, prefab = örnek) ya da Properties'teki bir açılır listeye. |
| New Folder / Move to Folder | Düzenleme (klasörler yalnızca düzen içindir). |
| Duplicate Resource / Select Users / Purge Orphans | Kopyalama, onu kimin kullandığını bulma, kimsenin kullanmadığını silme. |
| Inside the .w3d / External | Properties'ten (Resource kartı): kaynağı projenin içine ya da dışına taşır. |

## Sahneler, oyunlar ve prefab'lar

Sahnenin adını taşıyan düğme (Outliner'da ve 3D Viewport'ta) projenin sahnelerini, oyunlarını ve prefab'larını listeler. Oradan yenileri oluşturulur, başlangıç sahnesi seçilir ve bir sahne oyuna dönüştürülür (ya da tersi).

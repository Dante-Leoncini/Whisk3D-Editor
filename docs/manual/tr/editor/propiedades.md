# Properties

Etkin nesnenin tüm verilerini içeren, sekmelere (simgelere) ayrılmış panel. Görünen sekmeler nesnenin türüne bağlıdır. Bir kartın başlığına tıklamak onu katlar. Sayısal alanlarda: yazmak için tıkla (işlem de kabul eder), değeri kaydırmak için sürükle. Sağdaki eşkenar dörtgen bir keyframe ekler ya da kaldırır.

| Sekme | İçeriği |
|---|---|
| File / Render (disket) | **File**: açma, kaydetme, farklı kaydetme, sürüm kaydetme, asset'leri çıkarma. **Render**: yol, boyut, ek geçişler, arka plan, **Ray Tracing** (ışınlar, örnekler, geçişler), Render Image ve Render Animation. **Game**: platform (Windows, Symbian, Linux .deb, AppImage, WebGL, Android), pencere, yönelim, simge, fizik, ses ve Build game. **Export**: OBJ, glTF, GLB. **Settings**: dil, antialiasing, grafikler, skin, log'lar, ölçek, **Keyboard Shortcuts…** ve Save Changes. |
| Object | Konum, dönüş (Euler, kuaterniyon ya da eksen-açı), ölçek, ad, görünürlük, render edilebilirlik; nesnenin animasyonları ve paleti. |
| Contextual | Türe bağlıdır: mesh parçaları ve **materyal** (doku, aydınlatma, köşe rengi, saydamlık, yansımalar, normal map, karıştırma, derinlik, doku katmanları, **Advanced: custom shader** — bkz. [Yazılan shader'lar](shaders.md); ray tracing ile Roughness, Metallic, Transmission ve IOR), ışık, kamera, sis, parçacıklar, fizik, hitbox, iskelet ve kemikler, 2D öğeler, prefab'lar, rutinler. |
| 3D Mesh | Kullanılan mesh (ve onu kaç nesnenin paylaştığı), UV haritaları, renk katmanları, köşe grupları, UV grupları. Library'de bir kaynak seçiliyken: onun verileri ve konumu (.w3d'nin içinde ya da dışında). |
| Modifiers | [Modifier'lar](modificadores.md) bölümüne bak. |
| Transform | Yalnızca Edit Mode'da: seçimin ve UV'lerinin merkezi, düzenlenebilir. |
| Armature 2D | Mesh'in 2D iskeletleri ve kemikleri. |
| Constraints | Copy Location, Copy Rotation, Billboard (kameraya bakma), Child Of. |
| Scripts | Nesnenin Lua script'leri ve özellikleri ([Lua belgesine](../lua/README.md) bak). |
| Animation | Etkin animasyon, karışım katmanları (Mix), seçilen keyframe'in tam değerleri ve kamera geçişleri. |

## Rutinler

Rutin, Properties'te kurulan bir çizim adımları listesidir (taşıma, döndürme, doku belirleme, bir mesh çizme, başka bir rutini çağırma, Call Lua…). N95'te çizim yapmanın en hızlı yoludur.

| İşlev | Açıklama | Kısayol |
|---|---|---|
| Add / Remove (adım) | Konuya göre gruplanmış adımlar ekler ya da seçili olanı kaldırır. | Düğmeler; <kbd>X</kbd> ya da <kbd>Delete</kbd> kaldırır |
| Grab step | Adımı ok tuşlarıyla yeniden sıralar; <kbd>Enter</kbd> onaylar. | <kbd>G</kbd> |
| Insert Keyframe | Adımın değerlerini geçerli karede canlandırır. | <kbd>I</kbd> |
| Repeat | Listenin art arda kaç kez çalışacağı (rutin döngüsü). 1 = normal rutin. | Repeat alanı |
| Render mode / Use | Her gölgelendirme modu için ayrı bir liste ya da hepsi için aynı liste. | Açılır listeler |

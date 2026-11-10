# Proje (.w3d) ve dosyalar

| İşlev | Açıklama | Nerede |
|---|---|---|
| Proje aç | Bir .w3d açar (sistemden çift tıklayarak da olur). | Özellikler → Dosya/Render → Dosya |
| Kaydet / Farklı Kaydet | Karttaki klasöre ve karttaki adla kaydeder; Farklı Kaydet başka bir ad ister. Ad eksikse alan kırmızı olur. | Dosya kartı, <kbd>Ctrl+S</kbd> |
| Sürüm kaydet | Kaydeder ve ayrıca numaralı bir kopya bırakır (`proyecto_v03.w3d`). | Dosya kartı |
| Varlıkları çıkar | İçerideki her şeyi bir klasöre kopyalar (kopyalar projeyi değiştirmez). | Dosya kartı |
| Metin olarak örgüler | Örgüleri ikili biçim yerine okunabilir metin olarak kaydeder. | Dosya kartı |
| İçe aktar | OBJ, FBX, glTF, GLB ve .w3dui arayüzleri. İçe aktarılanlar projenin içine kopyalanır (yinelenmeden). | 3D Görünüm → Ekle → İçe aktar |
| Dışa Aktar | Seçili nesnenin OBJ, glTF ya da GLB'si (değiştiriciler ve dönüşümlerle ya da onlarsız). | Özellikler → Dışa Aktar |
| Kaydedilmemiş değişiklikler | Düzenlenmiş ama kaydedilmemiş olanlar (script'ler, boyanmış dokular) `*` ile işaretlenir. Kapatırken ya da başka bir proje açarken, bir uyarı kaybolacak olanları listeler. | Otomatik |
| Kaynak içeride ya da dışarıda | Harici bir kaynak kopyalanmaz: yolu kaydedilir ve eksikse uyarı verilir. | Kaynak kartı; IDE'de ve UV Düzenleyici'de ".w3d içine / dışına kaydet" |

Kaydetme işlemi dosyayı asla yarım bırakmaz: önce geçici bir kopya yazar, sonra onu yeniden adlandırır.

## Tarayıcıda (web sürümü)

Tarayıcı, bir sayfanın bilgisayardaki dosyaları kendi başına okumasına ya da yazmasına izin vermez. Bu yüzden web sürümünde:

- **Proje aç**, **İçe aktar** ve bir doku seçmek tarayıcının dosya gezginini açar: **Dosya seç** düğmeli bir pencere belirir. Bir OBJ için .obj, onun .mtl dosyası ve dokuları birlikte seçilir; bir glTF için .gltf ve onun .bin dosyası.
- **Kaydet** (<kbd>Ctrl+S</kbd>), **Farklı Kaydet**, **Sürüm kaydet** ve **Varlıkları çıkar** dosyayı tarayıcının indirilenler klasörüne indirir. Aynısı **Dışa Aktar** (OBJ, yanında .mtl dosyası ve dokularıyla iner), render, UV haritası, dışarıya kaydedilen boyanmış dokular ve harici script'ler için de geçerlidir.
- Editör kaydetmek için bir klasör istediğinde hiçbir gezgin açılmaz: dosya doğrudan indirilir.
- Tarayıcı, sayfanın birden fazla dosya indirip indiremeyeceğini sorarsa kabul etmek gerekir. Yoksa yalnızca ilki iner.
- Açılan her şey sayfanın belleğinde yaşar: sayfa yeniden yüklendiğinde indirilmemiş olan her şey kaybolur.

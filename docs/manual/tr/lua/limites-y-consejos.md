# Sınırlar ve ipuçları

## Lua'nın hangi kısımları yok

- `string`, `table` ve `math` ile temel fonksiyonlar (`print`, `pairs`, `ipairs`, `type`, `tostring`, `tonumber`, `pcall`, `assert`…) mevcuttur.
- `io` ve `os` **yoktur** (bir oyun diskteki dosyalara dokunmaz; bu yüzden `os.time` yoktur); `require`, `coroutine`, `utf8` ve `debug` de yoktur. Veri kaydetmek için `getConfig` / `saveConfig` kullan.
- Her script bağımsızdır: başka bir script'in değişkenlerini göremez. İletişim kurmak için `setShared` / `getShared` kullanılır.

## Hatalar

- Bir fonksiyonda hata varsa, hata günlüğe yazılır (`archivo.lua:LÍNEA: mensaje`) ve o çağrı kesilir, ama fonksiyon bir sonraki karede yeniden çağrılır: sen düzeltene kadar aynı hata her karede tekrarlanır.
- Dosya okunamıyorsa ya da bir sözdizimi hatası içeriyorsa, o script bir sonraki Oynat'a kadar kapalı kalır.
- Whisk3D'de `error("...")` yalnızca mesajı günlüğe yazar ve script devam eder. Durdurmak istiyorsan `assert(condicion, "mensaje")` kullan.

## Dikkat edilmesi gerekenler

- `update` yalnızca etkin sahnedeki **görünür** nesnelerde çalışır: bir nesneyi gizlemek onun mantığını kapatır.
- `destroy`, `changeScene` ve `playScene` o anda değil, karenin sonunda uygulanır.
- Oyun metinlerinin yazı tipi aksanlı harfleri ve ñ'yi çizmez: `"MENÚ"` yerine `"MENU"` yaz.
- `safeArea`, `uiBox` ve `resolution` ancak ilk kare çizildikten sonra gerçek değerlere sahip olur: onları `start` içinde kullanma.
- Web'de ve Android'de yalnızca **ilk** ışık aydınlatır.
- Nokia N95'te oyun kumandası ve fare yoktur: telefon tuş takımının adlarıyla `key` kullan. Tam sayılar 32 bittir.

## Hızlı çalışması için (özellikle telefonda)

- Nesneleri yalnızca bir kez, `start` içinde ara ve `local` değişkenlerde sakla.
- Tekli sürümü bin kez çağırmak yerine "toplu" sürümleri (`setVertices`, `setVerticesColor`) kullan.
- Hesapları Lua'da yapmak yerine motorun fonksiyonlarını tercih et (`scrollUV`, `linePoint`, dönüş sırası belirtilmiş `setRotation`).
- Körü körüne optimize etmeden önce `luaProfile` ve `clockMs` ile ölç.

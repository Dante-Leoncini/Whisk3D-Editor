# Script, bir nesnenin bileşenidir

Whisk3D'de bir script her zaman sahnedeki bir **nesneye** aittir (bir örgü, bir boş nesne, bir 2D düğme…). Bir nesnenin birden fazla script'i olabilir; her biri kendi değişkenleriyle, ayrı ayrı ve göründükleri sırayla çalışır.

Script'in içinden `yo()` sahibi olan nesneyi döndürür. *Başka* nesneleri kullanmak için script, `propiedades` adında global bir tablo tanımlar. Editör onu okur ve Özellikler panelinde, script'in kartında her girdi için bir alan gösterir:

```lua
propiedades = {
  pelota     = "objeto",                          -- sahneden bir nesne seçmek için bir alan
  dificultad = { "facil", "normal", "dificil" },  -- bir açılır liste (ilki varsayılandır)
  velocidad  = 200,                               -- script'i kullanan her nesneye özel bir sayı
  activo     = true,                              -- bir evet/hayır
  etiqueta   = "caja",                            -- bir metin
}

function inicio()
  local p   = objeto("pelota")         -- "pelota" alanında seçilen nesne
  local dif = opcion("dificultad")     -- "facil", "normal" ya da "dificil"
  local v   = propiedad("velocidad")   -- 200 ya da o nesnede ne ayarlandıysa
end
```

Böylece aynı script, farklı değerlere sahip birçok nesne için kullanılabilir: bir düşmanın `velocidad` alanı 200, bir diğerininki 350 olabilir.

> [!NOTE]
> `propiedades`'i okumak için editör, oyun çalışmıyor olsa bile **dosyayı çalıştırır**. Bu yüzden fonksiyonların dışına yalnızca tanımlar koy (tablolar, sabitler, `local`), asla bir şey yapan kod koyma.

Script'ler birbirleriyle değişken paylaşmaz. Birbirlerine veri aktarmak için (puanlar, oyuncunun kazanıp kazanmadığı…) ortak bir alan kullanırlar: `setCompartido("puntos", 10)` yazar, `compartido("puntos")` okur. Sayıları, evet/hayır değerlerini ve metinleri saklar; oyun durdurulduğunda boşaltılır.

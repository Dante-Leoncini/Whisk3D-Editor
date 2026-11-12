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

> [!NOTE]
> `propiedades`'i okumak için editör, oyun çalışmıyor olsa bile **dosyayı çalıştırır**. Bu yüzden fonksiyonların dışına yalnızca tanımlar koy (tablolar, sabitler, `local`), asla bir şey yapan kod koyma.

Script'in kartında, Özellikler'de böyle görünür: her özellik için bir alan, koddakiyle aynı sırada.

![Özellikler'de script'in kartı: pelota (seçilecek bir nesne), dificultad (bir liste), velocidad (200), activo (işaretli bir onay kutusu) ve etiqueta (caja)](../tutoriales/img/lua-propiedades.webp)

Böylece aynı script, farklı değerlere sahip birçok nesne için kullanılabilir: bir düşmanın `velocidad` alanı 200, bir diğerininki 350 olabilir.

## Özellikleri gerçek zamanlı değiştirme

`propiedad("velocidad")` değeri o anda okur. Oyun çalışırken onu Özellikler'de değiştirirsen, script değişikliği bir sonraki karede görür, ama yalnızca onu `propiedad()` ile yeniden okursa. Değeri bir değişkene kaydettiysen (`inicio()` içinde `local v = propiedad("velocidad")`), değişken baştaki değerde kalır.

```lua
function actualizar(dt)
  -- her karede okunur: oyun çalışırken Özellikler'de değiştirdiğin şeyi görür
  mover(yo(), propiedad("velocidad") * dt, 0, 0)
end
```

`propiedad()`'i her karede okumak, bir değişken kullanmaktan daha yavaştır. Bütün oyunda böyle çalışmak iyi bir fikir değil: değerleri ayarlarken kullan, değerler yerine oturunca da onları `inicio()` içinde değişkenlere kaydet.

## Düğmeler

Bir özellik bir **fonksiyon** ise kartta bir **düğme** belirir: ona dokunmak o fonksiyonu çağırır. Düğmede adından başka bir şey yazması için metni ve fonksiyonu bir tabloya koy:

```lua
propiedades = {
  velocidad = 90,
  cero = { "Sıfıra dön", function()
    setRotacion(yo(), 0, 0, 0)
  end },
}
```

![velocidad alanı ve Sıfıra dön düğmesiyle script'in kartı](../tutoriales/img/lua-boton.webp)

Oyun çalışırken fonksiyon, oyundaki script'te çalışır. Oyun yokken script yalnızca o çağrı için yüklenir: böylece bir düğme, sahneye bir şey yapan bir **editör eklentisi** işi görür. Örneğin `seleccionados()` ile, yani editörde seçili nesnelerle:

```lua
propiedades = {
  subir = { "Seçileni yükselt", function()
    for _, o in ipairs(seleccionados()) do
      mover(o, 0, 0, 1)   -- bir metre yukarı
    end
  end },
}
```

> [!NOTE]
> Bir düğmenin oyun dururken yaptığı şey, elle yapmışsın gibi sahnede kalır ve aynı şekilde geri alınır: <kbd>Ctrl</kbd> + <kbd>Z</kbd> düğmenin yaptığı her şeyi tek seferde geri alır. Oyun çalışırken geçmişe hiçbir şey kaydedilmez: **Durdur**'a dokunduğunda her şey eski hâline döner.

Script'ler birbirleriyle değişken paylaşmaz. Birbirlerine veri aktarmak için (puanlar, oyuncunun kazanıp kazanmadığı…) ortak bir alan kullanırlar: `setCompartido("puntos", 10)` yazar, `compartido("puntos")` okur. Sayıları, evet/hayır değerlerini ve metinleri saklar; oyun durdurulduğunda boşaltılır.

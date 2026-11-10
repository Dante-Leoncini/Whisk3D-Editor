# Ağda oynama (LAN, Wi-Fi ya da internet)

Ağdaki bir oturumun bir **ev sahibi** (onu oluşturan: 0 numaralı oyuncu) ve ona **katılanlar** (1, 2…) vardır. Her şey ev
sahibinden geçer: bir oyuncu ev sahibine gönderir, ev sahibi de bunu dağıtır. Aynı ağdakiler (aynı LAN ya da aynı
Wi-Fi) birbirini kendiliğinden bulur; internet üzerinden ev sahibinin adresiyle katılmak gerekir (ve ev sahibinin
yönlendiricisinde 47023 portunu açmış olması).

| Fonksiyon | Ne yapar |
|---|---|
| `redDisponible() → bool [, motivo]` | Bu oyunun ağı kullanıp kullanamayacağı. Kullanamıyorsa nedeni: `"sin permiso"` (editör bu projeye izin vermedi) ya da `"sin red"` (bu platformda henüz ağ yok). |
| `redDispositivo() → texto` | Bu cihazın adı (diğerlerinin oturum ararken gördüğü ad). Editörde değiştirilir: **Ayarlar** kartı, *Cihaz adı*. |
| `redDireccion() → texto` | Bu cihazın yerel ağdaki IP'si (internet üzerinden ya da yazarak katılmak için). |
| `redHostear(nombre [, maximo [, juego]]) → bool [, error]` | Bir oturum oluşturur ve oyuncu bekler (`maximo` ev sahibi de sayılarak: varsayılan 2). `juego` oyununun adıdır: yalnızca aynı oyunun oturumları görünür. |
| `redAbrir(bool)` | Ev sahibi aramalarda görünür ve oyuncu kabul eder (`true`) ya da etmez (örneğin oturum başladığında). |
| `redBuscar([juego]) → bool` | Yerel ağda oturum aramaya başlar. Onu yeniden çağırmak listeyi **günceller**. |
| `redPartidas() → { {nombre, dispositivo, jugadores, maximo, direccion}, … }` | Yanıt veren oturumlar (sen ararken liste kendiliğinden güncellenir). |
| `redUnirse(direccion) → bool` | Bir oturuma girer: `redPartidas()`'tan gelen `direccion` ya da elle yazılmış bir adres (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `redEstado() → estado, error` | `"nada"`, `"buscando"`, `"esperando"` (ev sahibi sensin), `"conectando"`, `"conectado"` ya da `"error"` (nedeniyle birlikte). |
| `redYo() → id` | Oturumdaki numaran (0 = ev sahibi; oturum yoksa -1). |
| `redJugadores() → { {id, nombre, yo}, … }` | Oturumdakiler (ad, cihazlarının adıdır). |
| `redSilencio([id]) → segundos, limite` | O oyuncudan ne zamandır hiçbir şey gelmediği (`id` olmadan: en uzun süredir sessiz olandan). Katılan yalnızca ev sahibini duyar, bu yüzden onun için bu, ev sahibinin sessizliğidir. `limite` (8 sn) aşılınca gitmiş sayılır: ev sahibi onu oturumdan çıkarır ve katılan için `redEstado()` `"error"` verir. |
| `redEnviar(valor [, para [, confiable]]) → bool` | Bir Lua değerini (sayı, metin, boolean ya da içinde tablolar olabilen bir tablo) bir oyuncuya (`para` = onun id'si) ya da herkese (`nil`) gönderir. **Güvenilir** (varsayılan): kesin olarak ve sırayla ulaşır. `false` ile kaybolursa yeniden gönderilmez: bir sonrakinin öncekinin yerini aldığı, her karede gönderdiğin şeyler için (konumlar, kumanda). |
| `redRecibir() → { {de, datos}, … }` | Son seferden beri gelen her şey, sırayla. |
| `redCerrar()` | Oturumdan çıkar (ev sahibi için: oturumu herkes için kapatır). Editörün Durdur düğmesi de onu kapatır. |

## İzin

Oturum oluşturan, oturum arayan ya da bir oturuma katılan bir proje, kullanıcı izin verene kadar ağa çıkmaz. Projeyi
açarken (ya da ağı sonradan kullanmaya başladıysa Oynat'a basınca) editör dosya başına **yalnızca bir kez** sorar ve
yanıtı hatırlar. Sonradan **Dosya** kartından değiştirilir: *Ağ erişimi*. İzin olmadan oyun yine çalışır ve
`redDisponible()` `false, "sin permiso"` döndürür: ağda oynama seçeneği yerine bir mesaj göster.

## Bir örnek: lobi

```lua
local JUEGO = "mi_juego"

function inicio()
  if not redDisponible() then info("sin red") return end
  if propiedad("anfitrion") then redHostear(redDispositivo(), 2, JUEGO) else redBuscar(JUEGO) end
end

function actualizar(dt)
  local estado = redEstado()
  if estado == "buscando" then
    local p = redPartidas()
    if p[1] then redUnirse(p[1].direccion) end
  end
  if teclaApretada("enter") then redEnviar({t = "listo"}) end
  for _, m in ipairs(redRecibir()) do
    if m.datos.t == "listo" then info("el jugador " .. m.de .. " esta listo") end
  end
end
```

## İpuçları

- **Tek bir hesaplama**: en basiti ve farklı cihazlar (bir PC ve bir telefon) arasında en iyi çalışanı, oyunu ev
  sahibinin hesaplaması ve diğerlerine göstermeleri gerekeni göndermesidir; diğerleri ona yalnızca kumandalarını
  gönderir. Dragon Ball demosu (`dbz2w3d/lua/pelea.lua`) bunu böyle yapar.
- Her karede gönderilenler **güvenilmez**; bir kez olanlar (bir uyarı, "başla") **güvenilir**.
- **Ağ takıldığında**: bir dövüşte `redSilencio()` saniyenin birkaç onda birini aşarsa, öbür taraf göndermeyi bırakmıştır
  (Wi-Fi kesildi, telefon arka plana geçti). Oyunu duraklat ve haber ver ("…bekleniyor", `limite`'e kalan süreyle):
  geri gelirse kaldığı yerden devam eder; gelmezse `redEstado()` ya da `redJugadores()` bunu söyler.
- Tam gerektiği kadar gönder: sayılar ve kısa metinler. Bir mesaj 60000 baytı geçemez.
- Tarayıcıda henüz ağ yok (UDP'si yok). Android `INTERNET` iznini ister (derlenmiş oyun bunu içerir); Symbian
  `NetworkServices` yeteneğini ister (telefon hangi erişim noktasından çıkılacağını sorar).

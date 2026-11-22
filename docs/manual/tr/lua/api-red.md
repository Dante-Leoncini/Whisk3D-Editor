# Ağda oynama (LAN, Wi-Fi ya da internet)

Ağdaki bir oturumun bir **ev sahibi** (onu oluşturan: 0 numaralı oyuncu) ve ona **katılanlar** (1, 2…) vardır. Her şey ev
sahibinden geçer: bir oyuncu ev sahibine gönderir, ev sahibi de bunu dağıtır. Aynı ağdakiler (aynı LAN ya da aynı
Wi-Fi) birbirini kendiliğinden bulur; internet üzerinden ev sahibinin adresiyle katılmak gerekir (ve ev sahibinin
yönlendiricisinde 47023 portunu açmış olması).

| Fonksiyon | Ne yapar |
|---|---|
| `netAvailable() → bool [, reason]` | Bu oyunun ağı kullanıp kullanamayacağı. Kullanamıyorsa nedeni: `"no permission"` (editör bu projeye izin vermedi) ya da `"no network"` (bu platformda henüz ağ yok). |
| `netDevice() → text` | Bu cihazın adı (diğerlerinin oturum ararken gördüğü ad). Editörde değiştirilir: **Ayarlar** kartı, *Cihaz adı*. |
| `netAddress() → text` | Bu cihazın yerel ağdaki IP'si (internet üzerinden ya da yazarak katılmak için). |
| `netHost(name [, max [, game]]) → bool [, error]` | Bir oturum oluşturur ve oyuncu bekler (`max` ev sahibi de sayılarak: varsayılan 2). `game` oyununun adıdır: yalnızca aynı oyunun oturumları görünür. |
| `netOpen(bool)` | Ev sahibi aramalarda görünür ve oyuncu kabul eder (`true`) ya da etmez (örneğin oturum başladığında). |
| `netFind([game]) → bool` | Yerel ağda oturum aramaya başlar. Onu yeniden çağırmak listeyi **günceller**. |
| `netMatches() → { {name, device, players, max, address}, … }` | Yanıt veren oturumlar (sen ararken liste kendiliğinden güncellenir). |
| `netJoin(address) → bool` | Bir oturuma girer: `netMatches()`'tan gelen `address` ya da elle yazılmış bir adres (`"192.168.0.10"`, `"x.x.x.x:puerto"`). |
| `netState() → state, error` | `"none"`, `"searching"`, `"waiting"` (ev sahibi sensin), `"connecting"`, `"connected"` ya da `"error"` (nedeniyle birlikte). |
| `netSelf() → id` | Oturumdaki numaran (0 = ev sahibi; oturum yoksa -1). |
| `netPlayers() → { {id, name, isSelf}, … }` | Oturumdakiler (ad, cihazlarının adıdır). |
| `netSilence([id]) → seconds, limite` | O oyuncudan ne zamandır hiçbir şey gelmediği (`id` olmadan: en uzun süredir sessiz olandan). Katılan yalnızca ev sahibini duyar, bu yüzden onun için bu, ev sahibinin sessizliğidir. `limite` (8 sn) aşılınca gitmiş sayılır: ev sahibi onu oturumdan çıkarır ve katılan için `netState()` `"error"` verir. |
| `netSend(value [, para [, reliable]]) → bool` | Bir Lua değerini (sayı, metin, boolean ya da içinde tablolar olabilen bir tablo) bir oyuncuya (`para` = onun id'si) ya da herkese (`nil`) gönderir. **Güvenilir** (varsayılan): kesin olarak ve sırayla ulaşır. `false` ile kaybolursa yeniden gönderilmez: bir sonrakinin öncekinin yerini aldığı, her karede gönderdiğin şeyler için (konumlar, kumanda). |
| `netReceive() → { {from, data}, … }` | Son seferden beri gelen her şey, sırayla. |
| `netClose()` | Oturumdan çıkar (ev sahibi için: oturumu herkes için kapatır). Editörün Durdur düğmesi de onu kapatır. |

## İzin

Oturum oluşturan, oturum arayan ya da bir oturuma katılan bir proje, kullanıcı izin verene kadar ağa çıkmaz. Projeyi
açarken (ya da ağı sonradan kullanmaya başladıysa Oynat'a basınca) editör dosya başına **yalnızca bir kez** sorar ve
yanıtı hatırlar. Sonradan **Dosya** kartından değiştirilir: *Ağ erişimi*. İzin olmadan oyun yine çalışır ve
`netAvailable()` `false, "no permission"` döndürür: ağda oynama seçeneği yerine bir mesaj göster.

## Bir örnek: lobi

```lua
local GAME = "mi_juego"

function start()
  if not netAvailable() then info("sin red") return end
  if property("anfitrion") then netHost(netDevice(), 2, GAME) else netFind(GAME) end
end

function update(dt)
  local state = netState()
  if state == "searching" then
    local p = netMatches()
    if p[1] then netJoin(p[1].address) end
  end
  if keyPressed("enter") then netSend({t = "listo"}) end
  for _, m in ipairs(netReceive()) do
    if m.data.t == "listo" then info("el jugador " .. m.from .. " esta listo") end
  end
end
```

## İpuçları

- **Tek bir hesaplama**: en basiti ve farklı cihazlar (bir PC ve bir telefon) arasında en iyi çalışanı, oyunu ev
  sahibinin hesaplaması ve diğerlerine göstermeleri gerekeni göndermesidir; diğerleri ona yalnızca kumandalarını
  gönderir. Dragon Ball demosu (`dbz2w3d/lua/pelea.lua`) bunu böyle yapar.
- Her karede gönderilenler **güvenilmez**; bir kez olanlar (bir uyarı, "başla") **güvenilir**.
- **Ağ takıldığında**: bir dövüşte `netSilence()` saniyenin birkaç onda birini aşarsa, öbür taraf göndermeyi bırakmıştır
  (Wi-Fi kesildi, telefon arka plana geçti). Oyunu duraklat ve haber ver ("…bekleniyor", `limite`'e kalan süreyle):
  geri gelirse kaldığı yerden devam eder; gelmezse `netState()` ya da `netPlayers()` bunu söyler.
- Tam gerektiği kadar gönder: sayılar ve kısa metinler. Bir mesaj 60000 baytı geçemez.
- Tarayıcıda henüz ağ yok (UDP'si yok). Android `INTERNET` iznini ister (derlenmiş oyun bunu içerir); Symbian
  `NetworkServices` yeteneğini ister (telefon hangi erişim noktasından çıkılacağını sorar).

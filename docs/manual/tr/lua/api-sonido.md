# Ses

| Fonksiyon | Ne yapar |
|---|---|
| `beep(freq [, ms, vol])` | Bir bip sesi (dosyasız). |
| `sonido("sonidos/x.wav" [, vol, tono, loop, pan]) → h` | Bir ses çalar; bir tanımlayıcı döndürür. |
| `sonido("sonidos/x.wav", {vol, tono, loop, pan, ataque, decaimiento, sostenido, caida, liberacion, duracion, reverb}) → h` | Aynısı adlarla, artı zarf (atak, düşüş, 0 ile 1 arası tutma seviyesi, `caida`: tutma o kadar saniyede 60 dB düşer, bırakma ve `duracion`: nota o kadar saniye sonra kendiliğinden bırakılır) ve sesin yankıya giden payı (0 ile 1 arası). |
| `pararSonido(h [, fundido])`, `pararSonidos()`, `ajustarSonido(h, vol [, tono])` | Sesleri kesme ya da ayarlama. |
| `soltarSonido(h)` | Notayı bırakır: zarf bırakma aşamasına geçer. |
| `reverb(sala, amortiguacion, nivel)` | Genel yankı: oda boyutu, sönümleme ve seviye (her biri 0 ile 1 arası; seviye 0 kapatır). |
| `silenciar(bool)`, `estaMudo()` | Genel sessize alma. |
| `tocarSonido(obj [, seg])`, `callarSonido(obj)` | Bir **Sound nesnesini** yeniden başlatır (bölümünün `seg` saniyesinden) ya da susturur. |
| `sonidoObj(obj) → t`, `setSonidoObj(obj, t)` | Bir Sound nesnesini okur / değiştirir: `archivo` (kaynak), `activo`, `volumen`, `tono`, `reverb`, `paneo`, `inicio`, `recorteIni`, `recorteFin`, `bucle`, `posicional`, `radio`, `alcance` (okurken `duracion`, `sonando` da). |
| `crearSonido("sonidos/x.wav" [, padre]) → obj` | Yeni bir Sound nesnesi (`destruir(obj)` onu kaldırır). Bir prefab içinde her nesne gibi örneklenir. |

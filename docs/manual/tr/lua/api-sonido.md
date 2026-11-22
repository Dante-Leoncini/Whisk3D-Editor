# Ses

| Fonksiyon | Ne yapar |
|---|---|
| `beep(freq [, ms, vol])` | Bir bip sesi (dosyasız). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Bir ses çalar; bir tanımlayıcı döndürür. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | Aynısı adlarla, artı zarf (atak, düşüş, 0 ile 1 arası tutma seviyesi, `fall`: tutma o kadar saniyede 60 dB düşer, bırakma ve `duration`: nota o kadar saniye sonra kendiliğinden bırakılır) ve sesin yankıya giden payı (0 ile 1 arası). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Sesleri kesme ya da ayarlama. |
| `releaseSound(h)` | Notayı bırakır: zarf bırakma aşamasına geçer. |
| `reverb(room, damping, level)` | Genel yankı: oda boyutu, sönümleme ve seviye (her biri 0 ile 1 arası; seviye 0 kapatır). |
| `mute(bool)`, `isMuted()` | Genel sessize alma. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Bir **Ses nesnesini** yeniden başlatır (bölümünün `seg` saniyesinden) ya da susturur. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Bir Ses nesnesini okur / değiştirir: `file` (kaynak), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (okurken `duration`, `playing` da). |
| `createSound("sounds/x.wav" [, parent]) → obj` | Yeni bir Ses nesnesi (`destroy(obj)` onu kaldırır). Bir prefab içinde her nesne gibi örneklenir. |

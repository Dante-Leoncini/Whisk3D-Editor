# Zaman, rastgelelik ve yardımcı işlevler

| Fonksiyon | Ne yapar |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Rastgele sayılar. |
| `clockMs() → ms` | Bir şeyin ne kadar sürdüğünü ölçmek için milisaniye. |
| `unixTime() → seconds, ms` | Sistem saati, Unix zamanı olarak: 1/1/1970 UTC'den bu yana geçen saniyeler ve ayrıca milisaniyeler (0–999). |
| `unixTimeMs() → ms`, `unixTimeSec() → seconds` | Aynı Unix zamanı tek bir sayı olarak: milisaniye (tam sayı) ya da ondalıklı saniye. Ölçmek bir çıkarmadır: `t = unixTimeSec() - baslangic`. 64 bit Lua ile tam doğrudur (Dreamcast hariç tüm platformlar; orada `unixTime()`). |
| `date([seconds [, utc]]) → table` | Bir Unix zamanının tarihi ve saati (verilmezse şu an), yerel saatle ya da `utc = true` ile UTC olarak: `year`, `month`, `day`, `hour`, `min`, `sec`, `wday` (1 = pazar), `yday`. |
| `luaProfile(frames [, label])` | Script'in hangi kısımlarının yavaş olduğunu ölçer (`luaProfile(0)` onu kapatır). |
| `createLine(name, points)`, `linePoint(id, s)` | Motorun hesapladığı kapalı, yumuşak bir eğri (güzergâhlar). |

Başlangıçtan bu yana geçen süre: `start()` içinde `unixTimeSec()`'i sakla ve sonra çıkar (gerçek zaman). Ya da `dt`'yi bir değişkende topla (oyun zamanı).

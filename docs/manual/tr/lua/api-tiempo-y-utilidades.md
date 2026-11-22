# Zaman, rastgelelik ve yardımcı işlevler

| Fonksiyon | Ne yapar |
|---|---|
| `random() → 0..1`, `random(min, max) → integer` | Rastgele sayılar. |
| `clockMs() → ms` | Bir şeyin ne kadar sürdüğünü ölçmek için milisaniye. |
| `luaProfile(frames [, label])` | Script'in hangi kısımlarının yavaş olduğunu ölçer (`luaProfile(0)` onu kapatır). |
| `createLine(name, points)`, `linePoint(id, s)` | Motorun hesapladığı kapalı, yumuşak bir eğri (güzergâhlar). |

"Başlangıçtan bu yana geçen süre" diye bir fonksiyon yoktur: `dt`'yi bir değişkende topla.

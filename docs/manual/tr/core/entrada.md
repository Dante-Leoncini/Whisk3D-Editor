# Girdi

Neredeyse tüm sistemlerde klavyeyi, fareyi, parmakları ve oyun kumandalarını **SDL2** sağlar; editör ve oyunlar da bu olayların üzerine yazılmıştır. Core, SDL'in karşılamadıklarını tamamlar:

| Ne | Nasıl gelir |
|---|---|
| Dokunmatik ekran | Android'de, tarayıcıda, Vita'da ve 3DS'te SDL. Symbian'da `W3dTactil`, SDL ile aynı dönüşümü yapar (ilk parmak bir fareyi hareket ettirir, diğerleri parmak olarak bildirilir): S60 5th'te tek parmak, N8'de çoklu dokunma |
| Eğim | N95'te, açılışta yüklenen Nokia Sensor API (o olmadan da oyun aynı şekilde çalışır). Android'de ve PC'de SDL ile gelir |
| Oyun kumandaları | Diğerlerinde SDL. Symbian'da Bluetooth ile bağlanan PS3 ve PS4 kumandaları (bthid-s60 sürücüsü), SDL ile aynı düğme sırasıyla. En fazla 4 |
| Titreşim | Android'de telefonun titreşim motoru. Tarayıcıda `navigator.vibrate` (iOS'ta, bir hileyle sistemin "tık" sesi). Masaüstünde: hiçbir şey. API az şey vaat eder: "mümkünse kısa bir dokunuş" |
| Pano | SDL (tarayıcınınki de); Symbian'da `CClipboard` |

Dokunmatik ekranı olmayan konsollarda (PSP, Dreamcast) kumanda, fare gibi bir imleci hareket ettirir.

Neden önemli: Symbian'daki dokunmatik hareketler Android'dekilerle aynı yoldan geçer, bu yüzden Android'de hareketlere yapılan bir iyileştirme telefona kendiliğinden ulaşır. Lua'dan: [Klavye, oyun kumandası, dokunmatik ekran ve fare](../lua/api-entrada.md).

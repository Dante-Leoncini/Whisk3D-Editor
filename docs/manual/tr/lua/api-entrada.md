# Klavye, oyun kumandası, dokunmatik ekran ve fare

| Fonksiyon | Ne yapar |
|---|---|
| `key(name) → bool` | Tuş basılı (basılı tuttuğun sürece). |
| `keyPressed(name) → bool` | Tam bu karede basıldı (her basışta yalnızca bir kez). |
| `button(name [, gamepad])`, `buttonPressed(name [, gamepad])` | Aynısı, oyun kumandasının bir düğmesi için. `gamepad` (1'den 4'e) verilirse o kumandanınki; verilmezse herhangi birininki. |
| `stick(["left"\|"right"] [, gamepad]) → x, y` | Kumandanın analog çubuğu, -1'den 1'e (yukarı -1'dir). |
| `trigger(["left"\|"right"] [, gamepad]) → 0..1` | Analog tetik. |
| `controls() → n`, `control([n]) → name, tipo` | Kaç kumanda olduğu ve türleri (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `tap() → x, y, touching` | Ekrandaki parmak (ya da fare tıklaması). |
| `finger([n]) → x, y, active` | Çoklu dokunma: n numaralı parmak (1'den 4'e). |
| `touch() → bool` | Ekrana dokunuluyor: bir telefon ya da tablet (Android, bir iPhone'un tarayıcısı…) veya ekranına daha önce dokunulmuş herhangi bir cihaz. Ekran kontrollerini yalnızca gereken yerde göstermek için. |
| `tilt() → x, y, z` | Telefonun ivmeölçeri, g cinsinden, ekranın görüldüğü hâldeki (zaten döndürülmüş) eksenlerinde: `x`, bir direksiyon gibi sağa ne kadar döndürüldüğüdür (açının sinüsü); dik ve düz tutulduğunda `y = 1`, yüzü yukarı bakacak şekilde yatırıldığında `z = 1` verir. Cihazda ivmeölçer yoksa `nil`. |
| `vibrate(ms [, force])` | `ms` milisaniye boyunca, 0'dan 1'e `force` ile titreştirir: telefonun titreşim motoru (Android, tarayıcı) ve kumandaların motoru (PC). Telefonda güç, darbeyi kısaltır. Titreştirecek bir şey olmayan yerde ya da oyuncu titreşimi kapattıysa hiçbir şey yapmaz. |
| `pressed(obj) → bool` | Bu karede o 2D öğeye dokunuldu ya da tıklandı (düğmeler). |
| `mouse() → x, y, dentro` | Farenin nerede olduğu. |
| `captureMouse(bool)`, `mouseRel() → dx, dy`, `mouseCaptured()` | Fareyle etrafa bakmak için yakalanmış fare (birinci şahıs oyunları). PC'de ve tarayıcıda; tarayıcıda fare, oyundaki ilk tıklamayla ya da ilk tuşla yakalanır. <kbd>Esc</kbd> onu serbest bırakır ve oyunu duraklatır. |

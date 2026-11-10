# Klavye, oyun kumandası, dokunmatik ekran ve fare

| Fonksiyon | Ne yapar |
|---|---|
| `tecla(nombre) → bool` | Tuş basılı (basılı tuttuğun sürece). |
| `teclaApretada(nombre) → bool` | Tam bu karede basıldı (her basışta yalnızca bir kez). |
| `boton(nombre [, mando])`, `botonApretado(nombre [, mando])` | Aynısı, oyun kumandasının bir düğmesi için. `mando` (1'den 4'e) verilirse o kumandanınki; verilmezse herhangi birininki. |
| `stick(["izq"\|"der"] [, mando]) → x, y` | Kumandanın analog çubuğu, -1'den 1'e (yukarı -1'dir). |
| `gatillo(["izq"\|"der"] [, mando]) → 0..1` | Analog tetik. |
| `controles() → n`, `control([n]) → nombre, tipo` | Kaç kumanda olduğu ve türleri (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `toque() → x, y, tocando` | Ekrandaki parmak (ya da fare tıklaması). |
| `dedo([n]) → x, y, activo` | Çoklu dokunma: n numaralı parmak (1'den 4'e). |
| `tactil() → bool` | Ekrana dokunuluyor: bir telefon ya da tablet (Android, bir iPhone'un tarayıcısı…) veya ekranına daha önce dokunulmuş herhangi bir cihaz. Ekran kontrollerini yalnızca gereken yerde göstermek için. |
| `inclinacion() → x, y, z` | Telefonun ivmeölçeri, g cinsinden, ekranın görüldüğü hâldeki (zaten döndürülmüş) eksenlerinde: `x`, bir direksiyon gibi sağa ne kadar döndürüldüğüdür (açının sinüsü); dik ve düz tutulduğunda `y = 1`, yüzü yukarı bakacak şekilde yatırıldığında `z = 1` verir. Cihazda ivmeölçer yoksa `nil`. |
| `vibrar(ms [, fuerza])` | `ms` milisaniye boyunca, 0'dan 1'e `fuerza` ile titreştirir: telefonun titreşim motoru (Android, tarayıcı) ve kumandaların motoru (PC). Telefonda güç, darbeyi kısaltır. Titreştirecek bir şey olmayan yerde ya da oyuncu titreşimi kapattıysa hiçbir şey yapmaz. |
| `apretado(obj) → bool` | Bu karede o 2D öğeye dokunuldu ya da tıklandı (düğmeler). |
| `raton() → x, y, dentro` | Farenin nerede olduğu. |
| `capturarRaton(bool)`, `ratonRel() → dx, dy`, `ratonCapturado()` | Fareyle etrafa bakmak için yakalanmış fare (birinci şahıs oyunları). PC'de ve tarayıcıda; tarayıcıda fare, oyundaki ilk tıklamayla ya da ilk tuşla yakalanır. <kbd>Esc</kbd> onu serbest bırakır ve oyunu duraklatır. |

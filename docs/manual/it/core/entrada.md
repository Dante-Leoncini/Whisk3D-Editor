# Input

Su quasi tutti i sistemi è **SDL2** a fornire la tastiera, il mouse, le dita e i controller, e l'editor e i giochi sono scritti sopra questi eventi. Il Core completa ciò che SDL non copre:

| Cosa | Come arriva |
|---|---|
| Touch | SDL su Android, nel browser, sulla Vita e sul 3DS. Su Symbian, `W3dTactil` fa la stessa conversione di SDL (il primo dito muove un mouse, gli altri vengono segnalati come dita): un dito su S60 5th, multitouch sull'N8 |
| Inclinazione | Sull'N95, la Sensor API di Nokia, caricata all'avvio (senza di essa il gioco funziona lo stesso). Su Android e sul PC arriva tramite SDL |
| Controller | SDL negli altri sistemi. Su Symbian, i joystick di PS3 e PS4 via Bluetooth (il driver bthid-s60), con lo stesso ordine dei pulsanti di SDL. Fino a 4 |
| Vibrazione | Android, con il vibratore del telefono. Browser, `navigator.vibrate` (su iOS, il "tic" del sistema con un trucco). Desktop: niente. L'API promette poco: "un tocco breve, se possibile" |
| Appunti | SDL (anche quelli del browser); `CClipboard` su Symbian |

Sulle console senza schermo touch (PSP, Dreamcast) il controller muove un cursore, come un mouse.

Perché è importante: i gesti touch di Symbian seguono la stessa strada di quelli di Android, quindi un miglioramento dei gesti su Android arriva da solo al telefono. Da Lua: [Tastiera, controller, touch e mouse](../lua/api-entrada.md).

# Tastiera, controller, touch e mouse

| Funzione | Cosa fa |
|---|---|
| `key(name) → bool` | Il tasto è premuto (finché lo tieni premuto). |
| `keyPressed(name) → bool` | È stato premuto proprio in questo fotogramma (una sola volta per pressione). |
| `button(name [, gamepad])`, `buttonPressed(name [, gamepad])` | Lo stesso con un pulsante del controller. Con `gamepad` (da 1 a 4), quello di quel controller; senza, quello di uno qualsiasi. |
| `stick(["left"\|"right"] [, gamepad]) → x, y` | Levetta del controller, da -1 a 1 (in alto è -1). |
| `trigger(["left"\|"right"] [, gamepad]) → 0..1` | Grilletto analogico. |
| `controls() → n`, `control([n]) → name, tipo` | Quanti controller ci sono e di che tipo (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `tap() → x, y, touching` | Il dito (o il clic del mouse) sullo schermo. |
| `finger([n]) → x, y, active` | Multitouch: il dito numero n (da 1 a 4). |
| `touch() → bool` | Lo schermo si tocca: un telefono o un tablet (Android, il browser di un iPhone…) o qualsiasi dispositivo in cui lo schermo è già stato toccato. Serve per mostrare i controlli su schermo solo dove servono. |
| `tilt() → x, y, z` | L'accelerometro del telefono, in g, negli assi dello schermo così come lo si vede (già ruotato): `x` è quanto è girato a destra, come un volante (il seno dell'angolo); in verticale e dritto dà `y = 1`, sdraiato a faccia in su `z = 1`. `nil` se il dispositivo non ha un accelerometro. |
| `vibrate(ms [, force])` | Vibra per `ms` millisecondi con `force` da 0 a 1: il vibratore del telefono (Android, il browser) e il motore dei controller (PC). Su un telefono la forza accorcia l'impulso. Non fa nulla dove manca l'hardware, né se il giocatore ha disattivato la vibrazione. |
| `pressed(obj) → bool` | Quell'elemento 2D è stato toccato o cliccato in questo fotogramma (pulsanti). |
| `mouse() → x, y, dentro` | Dove si trova il mouse. |
| `captureMouse(bool)`, `mouseRel() → dx, dy`, `mouseCaptured()` | Mouse catturato per guardarsi intorno con il mouse (giochi in prima persona). Su PC e nel browser; nel browser il mouse viene catturato al primo clic o tasto nel gioco. <kbd>Esc</kbd> lo rilascia e mette in pausa il gioco. |

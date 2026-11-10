# Keyboard, gamepad, touch and mouse

| Function | What it does |
|---|---|
| `tecla(nombre) → bool` | The key is pressed (for as long as you hold it). |
| `teclaApretada(nombre) → bool` | It was pressed in this very frame (only once per press). |
| `boton(nombre [, mando])`, `botonApretado(nombre [, mando])` | The same, with a gamepad button. With `mando` (1 to 4), that gamepad's button; without it, any gamepad's. |
| `stick(["izq"\|"der"] [, mando]) → x, y` | Gamepad stick, from -1 to 1 (up is -1). |
| `gatillo(["izq"\|"der"] [, mando]) → 0..1` | Analog trigger. |
| `controles() → n`, `control([n]) → nombre, tipo` | How many gamepads there are and of what type (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `toque() → x, y, tocando` | The finger (or the mouse click) on the screen. |
| `dedo([n]) → x, y, activo` | Multi-touch: finger number n (1 to 4). |
| `tactil() → bool` | The screen is touch: a phone or a tablet (Android, the browser on an iPhone…) or any device where the screen has already been touched. To show on-screen controls only where they're needed. |
| `inclinacion() → x, y, z` | The phone's accelerometer, in g, on the axes of the screen as you see it (already rotated): `x` is how far it's turned to the right, like a steering wheel (the sine of the angle); standing upright gives `y = 1`, lying face up `z = 1`. `nil` if the device has no accelerometer. |
| `vibrar(ms [, fuerza])` | Vibrates for `ms` milliseconds with `fuerza` from 0 to 1: the phone's vibration motor (Android, the browser) and the gamepads' rumble motor (PC). On a phone the strength shortens the pulse. It does nothing where there's nothing to vibrate, or if the player turned vibration off. |
| `apretado(obj) → bool` | That 2D element was touched or clicked in this frame (buttons). |
| `raton() → x, y, dentro` | Where the mouse is. |
| `capturarRaton(bool)`, `ratonRel() → dx, dy`, `ratonCapturado()` | Captured mouse for mouse look (first-person games). On PC and in the browser; in the browser the mouse is captured with the first click or key press in the game. <kbd>Esc</kbd> releases it and pauses the game. |

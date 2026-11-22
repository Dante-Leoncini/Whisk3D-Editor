# Keyboard, gamepad, touch and mouse

| Function | What it does |
|---|---|
| `key(name) → bool` | The key is pressed (for as long as you hold it). |
| `keyPressed(name) → bool` | It was pressed in this very frame (only once per press). |
| `button(name [, gamepad])`, `buttonPressed(name [, gamepad])` | The same, with a gamepad button. With `gamepad` (1 to 4), that gamepad's button; without it, any gamepad's. |
| `stick(["left"\|"right"] [, gamepad]) → x, y` | Gamepad stick, from -1 to 1 (up is -1). |
| `trigger(["left"\|"right"] [, gamepad]) → 0..1` | Analog trigger. |
| `controls() → n`, `control([n]) → name, tipo` | How many gamepads there are and of what type (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `tap() → x, y, touching` | The finger (or the mouse click) on the screen. |
| `finger([n]) → x, y, active` | Multi-touch: finger number n (1 to 4). |
| `touch() → bool` | The screen is touch: a phone or a tablet (Android, the browser on an iPhone…) or any device where the screen has already been touched. To show on-screen controls only where they're needed. |
| `tilt() → x, y, z` | The phone's accelerometer, in g, on the axes of the screen as you see it (already rotated): `x` is how far it's turned to the right, like a steering wheel (the sine of the angle); standing upright gives `y = 1`, lying face up `z = 1`. `nil` if the device has no accelerometer. |
| `vibrate(ms [, force])` | Vibrates for `ms` milliseconds with `force` from 0 to 1: the phone's vibration motor (Android, the browser) and the gamepads' rumble motor (PC). On a phone the strength shortens the pulse. It does nothing where there's nothing to vibrate, or if the player turned vibration off. |
| `pressed(obj) → bool` | That 2D element was touched or clicked in this frame (buttons). |
| `mouse() → x, y, dentro` | Where the mouse is. |
| `captureMouse(bool)`, `mouseRel() → dx, dy`, `mouseCaptured()` | Captured mouse for mouse look (first-person games). On PC and in the browser; in the browser the mouse is captured with the first click or key press in the game. <kbd>Esc</kbd> releases it and pauses the game. |

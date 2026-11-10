# Input

On almost every system, **SDL2** delivers the keyboard, the mouse, fingers and gamepads, and the editor and games are written on top of those events. The Core fills in what SDL doesn't cover:

| What | How it arrives |
|---|---|
| Touch | SDL on Android, the browser, the Vita and the 3DS. On Symbian, `W3dTactil` does the same conversion as SDL (the first finger moves a mouse, the others report as fingers): one finger on S60 5th, multi-touch on the N8 |
| Tilt | On the N95, Nokia's Sensor API, loaded at startup (without it the game works the same). On Android and PC it comes through SDL |
| Gamepads | SDL everywhere else. On Symbian, PS3 and PS4 controllers over Bluetooth (the bthid-s60 driver), with the same button order as SDL. Up to 4 |
| Vibration | Android, with the phone's vibration motor. Browser, `navigator.vibrate` (on iOS, the system's "tick", with a trick). Desktop: nothing. The API promises little: "a short buzz, if possible" |
| Clipboard | SDL (the browser's too); `CClipboard` on Symbian |

On consoles without a touch screen (PSP, Dreamcast) the controller moves a cursor, like a mouse.

Why it matters: Symbian's touch gestures go through the same path as Android's, so an improvement to the gestures on Android reaches the phone on its own. From Lua: [Keyboard, gamepad, touch and mouse](../lua/api-entrada.md).

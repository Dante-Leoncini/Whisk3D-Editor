# Entrada

En casi todos los sistemas, **SDL2** entrega el teclado, el mouse, los dedos y los mandos, y el editor y los juegos están escritos sobre esos eventos. El Core completa lo que SDL no cubre:

| Qué | Cómo llega |
|---|---|
| Táctil | SDL en Android, el navegador, la Vita y la 3DS. En Symbian, `W3dTactil` hace la misma conversión que SDL (el primer dedo mueve un mouse, los demás avisan como dedos): un dedo en S60 5th, multitáctil en el N8 |
| Inclinación | En el N95, la Sensor API de Nokia, cargada al arrancar (sin ella el juego anda igual). En Android y la PC llega por SDL |
| Mandos | SDL en el resto. En Symbian, los joysticks de PS3 y PS4 por Bluetooth (el driver bthid-s60), en el mismo orden de botones que SDL. Hasta 4 |
| Vibración | Android, con el vibrador del teléfono. Navegador, `navigator.vibrate` (en iOS, el "tic" del sistema con un truco). Escritorio: nada. La API promete poco: "un toque corto, si se puede" |
| Portapapeles | SDL (también el del navegador); `CClipboard` en Symbian |

En las consolas sin pantalla táctil (PSP, Dreamcast) el control mueve un cursor, como un mouse.

Por qué importa: los gestos del táctil de Symbian pasan por el mismo camino que los de Android, así que una mejora de los gestos en Android llega sola al teléfono. Desde Lua: [Teclado, mando, táctil y mouse](../lua/api-entrada.md).

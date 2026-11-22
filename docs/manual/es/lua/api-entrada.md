# Teclado, mando, táctil y mouse

| Función | Qué hace |
|---|---|
| `key(name) → bool` | La tecla está apretada (mientras la mantenés). |
| `keyPressed(name) → bool` | Se apretó justo en este cuadro (una sola vez por pulsación). |
| `button(name [, gamepad])`, `buttonPressed(name [, gamepad])` | Lo mismo con un botón del mando. Con `gamepad` (1 a 4), el de ese mando; sin él, el de cualquiera. |
| `stick(["left"\|"right"] [, gamepad]) → x, y` | Palanca del mando, de -1 a 1 (arriba es -1). |
| `trigger(["left"\|"right"] [, gamepad]) → 0..1` | Gatillo analógico. |
| `controls() → n`, `control([n]) → name, tipo` | Cuántos mandos hay y de qué tipo (`"xbox"`, `"playstation"`, `"nintendo"`, `"generico"`). |
| `tap() → x, y, touching` | El dedo (o el click del mouse) en la pantalla. |
| `finger([n]) → x, y, active` | Multitáctil: el dedo número n (1 a 4). |
| `touch() → bool` | La pantalla se toca: un teléfono o una tableta (Android, el navegador de un iPhone…) o cualquier equipo donde ya se tocó la pantalla. Para mostrar controles en pantalla solo donde hacen falta. |
| `tilt() → x, y, z` | El acelerómetro del teléfono, en g, en los ejes de la pantalla como se la ve (ya girada): `x` es cuánto está girado a la derecha, como un volante (el seno del ángulo); parado y derecho da `y = 1`, acostado boca arriba `z = 1`. `nil` si el equipo no tiene acelerómetro. |
| `vibrate(ms [, force])` | Vibra `ms` milisegundos con `force` de 0 a 1: el vibrador del teléfono (Android, el navegador) y el motor de los mandos (PC). En un teléfono la fuerza acorta el pulso. No hace nada donde no hay con qué, ni si el jugador apagó la vibración. |
| `pressed(obj) → bool` | Se tocó o clickeó ese elemento 2D en este cuadro (botones). |
| `mouse() → x, y, dentro` | Dónde está el mouse. |
| `captureMouse(bool)`, `mouseRel() → dx, dy`, `mouseCaptured()` | Mouse capturado para mirar con el mouse (juegos en primera persona). En PC y en el navegador; en el navegador el mouse se captura con el primer click o tecla en el juego. <kbd>Esc</kbd> lo suelta y pausa el juego. |

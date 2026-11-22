# Nombres de teclas y botones

## Para `key` y `keyPressed`

| Dónde | Nombres |
|---|---|
| PC, Android y Web | `"a"` … `"z"` (en minúscula), `"0"` … `"9"`, `"up"`, `"down"`, `"left"`, `"right"`, `"space"`, `"enter"`, `"shift"`, `"ctrl"` |
| Nokia N95 (teclado del teléfono) | `"up"`, `"down"`, `"left"`, `"right"`, `"enter"` (el centro de las flechas también manda `"space"`), `"softleft"`, `"softright"`, `"0"` … `"9"`, `"*"`, `"#"` |

> [!NOTE]
> No hay nombre para Esc: en el editor, Esc pone el juego en pausa y suelta el mouse. Tampoco hay teclas F ni Tab.

## Para `button` y `buttonPressed` (mando)

`"a"`, `"b"`, `"x"`, `"y"`, `"up"`, `"down"`, `"left"`, `"right"` (la cruz). Las flechas de la cruz también mueven `stick("left")` cuando la palanca está quieta.

Además: `"l1"`, `"r1"` (los hombros), `"l2"`, `"r2"` (los gatillos como botones: apretados a más de la mitad; para el valor analógico está `trigger`), `"l3"`, `"r3"` (los sticks apretados), `"start"` y `"select"`. Los nombres son los de un mando de PlayStation; en uno de Xbox son LB / RB, LT / RT y Back. Las letras siguen la posición del botón y no el dibujo: `"a"` es el de abajo (la cruz ×), `"b"` el de la derecha (el círculo ○), `"x"` el de la izquierda (el cuadrado □) e `"y"` el de arriba (el triángulo △).

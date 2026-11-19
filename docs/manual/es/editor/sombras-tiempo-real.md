# Sombras en tiempo real

Las sombras que se calculan en cada cuadro: una pelota que rueda lleva su sombra, un personaje que camina la proyecta sobre el piso. Es lo que en Unity está en el panel de sombras de la calidad. Se ven en la **vista Render**, en las imágenes que se renderizan y en el juego.

Las da la **primera luz Sol** de la escena (ver [Luz](objeto-luz.md)). Las luces puntuales y los focos no dan sombras en tiempo real.

## La tarjeta Sombras en tiempo real

Está en la pestaña **Render** (el disquete), debajo de la tarjeta Render. Viene **apagada**: tildá **Habilitado** y aparecen las opciones.

| Propiedad | Qué hace |
|---|---|
| Habilitado | Prende las sombras en tiempo real. |
| Distancia máxima | Hasta dónde hay sombras, en metros desde la cámara. Más lejos no se calculan. Cuanto más corta, más nítidas se ven. |
| Cascadas | En cuántos tramos se corta la distancia (de 1 a 4). Cada tramo tiene su propio mapa de sombras: los de cerca cubren poco y se ven nítidos; los de lejos cubren mucho y se ven más gruesos. Más cascadas, sombras más lindas cerca de la cámara, pero cada una dibuja otra vez lo que proyecta. |
| Corte 1, 2, 3 | Hasta dónde llega cada cascada, en metros (aparecen las que hacen falta según la cantidad). La última siempre llega a la distancia máxima. |
| Borde final | Los últimos metros antes de la distancia máxima, donde las sombras se van desvaneciendo en vez de cortarse de golpe. |
| Desenfoque lejano | Borronea las sombras a medida que se alejan de la cámara (de 0 a 100%). Cerca quedan nítidas. |
| Resolución | Los píxeles de cada cascada: 256, 512, 1024, 2048 o 4096. Más, más nítidas y más memoria de video. |
| Sombras suaves | Bordes suaves (destildado: duros, con escalones, un poco más rápido). |
| Intensidad | Qué tan oscura es la sombra: 100% saca toda la luz del sol; 50%, la mitad. |
| Sesgo | Si una superficie se hace sombra a sí misma (rayas o manchas sobre lo iluminado), subilo. Si las sombras se despegan de los objetos, bajalo. |

Abajo puede aparecer un aviso: que no hay luz solar en la escena, o que en este equipo las calcula la CPU (ver más abajo).

## Qué objetos proyectan y cuáles reciben

En la pestaña **Objeto** de cada malla hay dos casillas, las dos tildadas de fábrica:

- **Proyectar sombras**: la malla tapa la luz del sol.
- **Recibir sombras**: la malla se oscurece donde algo le tapa el sol.

Un piso que no hace falta que proyecte (no tiene nada abajo) puede ir sin **Proyectar sombras**: es menos para dibujar. Un objeto que brilla solo (un cartel luminoso) puede ir sin **Recibir sombras**.

Un material **sin luz** (la casilla **Luz** destildada) no recibe sombras.

## Sobre la luz precalculada

Un escenario con la luz [precalculada](objeto-malla.md#precalcular-iluminación) ya tiene las sombras de lo **estático** en su textura. Ahí, las sombras en tiempo real solo agregan las de lo que **se mueve** (los objetos con la casilla **Estático** destildada): la esfera que cae sobre el piso horneado hace su sombra, y la del cubo estático no se duplica. Sobre la luz precalculada la sombra oscurece hasta un 75%, porque la luz que rebota también está en la textura.

## Rendimiento

- Cada cascada vuelve a dibujar lo que proyecta, visto desde el sol. Para juegos en teléfonos, una o dos cascadas y 1024 de resolución suelen alcanzar.
- Achicar la **Distancia máxima** es lo que más mejora la nitidez sin costar nada.
- El **Desenfoque lejano** cuesta más en lo que está lejos (lee el mapa más veces ahí).

## En el Nokia N95 (OpenGL ES 1.1)

Sin shaders no se puede comparar la profundidad de cada píxel con el mapa de sombras. Ahí las sombras se arman de otra manera, pensada para hacer **renders** en el teléfono en un tiempo razonable, no para un juego en tiempo real:

1. La GPU dibuja desde el sol cada objeto de un color distinto, con su profundidad.
2. La CPU lee esos píxeles y arma, para cada objeto que recibe, una textura oscura donde lo que ve el sol es **otro** objeto.
3. Esa textura se proyecta sobre el objeto en una pasada más, que oscurece.

Si nada se mueve, no se vuelve a calcular. Un objeto no se hace sombra a sí mismo (el brazo de un personaje no oscurece su cuerpo), y la resolución está limitada por el tamaño de la pantalla (hasta 512). Las cascadas, el desenfoque lejano y el sesgo no se usan; **Sombras suaves** promedia los bordes.

Lo mismo pasa en una GPU con ES 2.0 que no tiene lugar para el programa completo (los Android 2.3 más chicos).

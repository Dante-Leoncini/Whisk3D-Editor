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
| Penumbra | Qué tan grande es el sol (de 0 a 100%). La sombra es **nítida donde toca lo que la proyecta** y se va difuminando a medida que se aleja: la base de un poste queda nítida y la punta de su sombra, borrosa (la técnica de NVIDIA, *PCSS*). En 0, nítida siempre. |
| Resolución | Los píxeles de cada cascada: 256, 512, 1024, 2048 o 4096. Más, más nítidas y más memoria de video. |
| Sombras suaves | Bordes suaves (destildado: duros, con escalones, un poco más rápido). |
| Color de la sombra | Cuánto de la luz del sol llega igual a la sombra, y de qué color. **Negro** (el de fábrica): nada, y en la sombra queda la **luz del ambiente** (ver abajo). Un azul: como la luz del cielo o de las nubes, sumada al ambiente. |
| Intensidad | Qué tan fuerte es la sombra: 100% le saca toda la luz del sol; 50%, la mitad. |
| Sesgo | Si una superficie se hace sombra a sí misma (rayas o manchas sobre lo iluminado), subilo. Si las sombras se despegan de los objetos, bajalo. |

Abajo puede aparecer un aviso: que no hay luz solar en la escena, o que en este equipo las calcula la CPU (ver más abajo).

## Qué objetos proyectan y cuáles reciben

En la pestaña **Objeto** de cada malla hay dos casillas, las dos tildadas de fábrica:

- **Proyectar sombras**: la malla tapa la luz del sol.
- **Recibir sombras**: la malla se oscurece donde algo le tapa el sol.

Un piso que no hace falta que proyecte (no tiene nada abajo) puede ir sin **Proyectar sombras**: es menos para dibujar. Un objeto que brilla solo (un cartel luminoso) puede ir sin **Recibir sombras**.

Un material **sin luz** (la casilla **Luz** destildada) no recibe sombras.

## El color de las sombras: la luz del ambiente

Como en Unity, lo que está en sombra no queda negro: le llega la **luz del ambiente**, y de ella sale su color. En Whisk3D el ambiente es el **fondo** de la escena (su color o su imagen 360) por la **Luz del fondo** de la tarjeta Render. Esa misma luz aclara el lado de los modelos que no ve el sol y entra en la [luz precalculada](objeto-malla.md#precalcular-iluminación) y en las [sondas de luz](objeto-sonda-luz.md).

Si las sombras se ven negras, subí la **Luz del fondo** (o aclará el fondo): las sombras toman su color. Con un fondo azulado, las sombras salen azuladas. Si el escenario tiene la luz precalculada, volvé a hornearlo para que sus sombras tomen el ambiente nuevo.

En las vistas **Sólido** y **Material** el ambiente es un gris fijo; el del fondo se ve en la vista **Render**.

## Sobre la luz precalculada

Un escenario con la luz [precalculada](objeto-malla.md#precalcular-iluminación) ya tiene las sombras de lo **estático** en su textura. Ahí, las sombras en tiempo real solo agregan las de lo que **se mueve** (los objetos con la casilla **Estático** destildada): la esfera que cae sobre el piso horneado hace su sombra, y la del cubo estático no se duplica.

Al hornear, la textura guarda además **cuánto de la luz de cada punto viene del sol** (en su canal alfa: como el *Shadowmask* de Unity). La sombra de lo que se mueve le saca a la luz horneada **solo esa parte**: queda la luz del ambiente y de los rebotes, la misma que hay en las sombras horneadas. Por eso, donde la sombra de un personaje toca la sombra horneada de una pared, las dos tienen el mismo tono y se juntan sin borde, y donde se pisan no se oscurece dos veces. Aunque el cubo estático le tape el sol a la esfera, la sombra de la esfera sigue entera sobre la penumbra horneada del cubo: el mapa de sombras guarda aparte lo que se mueve.

El **Color de la sombra** y la **Intensidad** también se hornean (con las sombras en tiempo real prendidas): donde algo tapa al sol, la textura recibe esa parte de su luz, igual que la sombra que se mueve. Si los cambiás, volvé a hornear para que las dos sigan con el mismo tono.

Una textura horneada **antes** de esta versión no tiene esa información: ahí la sombra de lo que se mueve baja hasta el ambiente, sin oscurecer lo que ya estaba en sombra. Para que coincidan del todo, volvé a hornear.

Lo que se movía cuando se horneó queda **pintado** en la textura: si después le sacás **Estático** a un objeto (un personaje), su sombra vieja sigue en el piso hasta que vuelvas a hornear.

La luz precalculada se guarda **por material**: si dos objetos comparten un material, al hornear uno el otro mostraría su luz (con sus propias UV, como una sombra que no es suya). Por eso, al hornear un objeto cuyo material usan otros, ese objeto pasa a una **copia** del material.

## Rendimiento

- Cada cascada vuelve a dibujar lo que proyecta, visto desde el sol. Para juegos en teléfonos, una o dos cascadas y 1024 de resolución suelen alcanzar.
- Achicar la **Distancia máxima** es lo que más mejora la nitidez sin costar nada.
- La **Penumbra** cuesta más: cada píxel busca lo que le tapa el sol y lee el mapa unas 25 veces. En un teléfono, mejor en 0.

## En el Nokia N95 (OpenGL ES 1.1)

Sin shaders no se puede comparar la profundidad de cada píxel con el mapa de sombras. Ahí las sombras se arman de otra manera, pensada para hacer **renders** en el teléfono en un tiempo razonable, no para un juego en tiempo real (y sobre la luz precalculada oscurecen el color como antes: la información del sol de la textura se usa con shaders):

1. La GPU dibuja desde el sol cada objeto de un color distinto, con su profundidad.
2. La CPU lee esos píxeles y arma, para cada objeto que recibe, una textura oscura donde lo que ve el sol es **otro** objeto.
3. Esa textura se proyecta sobre el objeto en una pasada más, que oscurece.

Si nada se mueve, no se vuelve a calcular. Un objeto no se hace sombra a sí mismo (el brazo de un personaje no oscurece su cuerpo), y la resolución está limitada por el tamaño de la pantalla (hasta 512). Las cascadas, el desenfoque lejano y el sesgo no se usan; **Sombras suaves** promedia los bordes.

Lo mismo pasa en una GPU con ES 2.0 que no tiene lugar para el programa completo (los Android 2.3 más chicos).

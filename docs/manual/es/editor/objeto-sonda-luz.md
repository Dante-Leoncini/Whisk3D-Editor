# Sonda de luz

Una **sonda de luz** guarda la **luz indirecta** de un lugar de la escena: la que llega del fondo y la que rebota en lo que hay alrededor (un piso verde tiñe de verde lo que está encima; una pared roja, lo que tiene al lado). Es lo que en Unity se llama *Light Probes* y en Blender *Irradiance Volume*.

Es una **caja con una grilla de puntos**. Al **hornear**, desde cada punto se mira en todas las direcciones con el trazado de rayos y se guarda la luz que llega. Después, cada malla que está adentro de la caja (o cerca) toma la luz de los puntos que la rodean y la usa como luz ambiente, en lugar de la luz del fondo. La luz **directa** de las lámparas no se guarda: esa la siguen calculando las luces de siempre.

Se agrega con Añadir → Luces → Sonda de luz, en el cursor 3D. Se ve como una caja amarilla con una cruz en cada punto; después de hornear, cada cruz toma el color de la luz que le llega.

## La tarjeta Sonda de luz

| Propiedad | Qué hace |
|---|---|
| Tamaño X, Y, Z | El tamaño de la caja, en metros. Lo que queda adentro toma su luz. |
| Puntos X, Y, Z | Cuántos puntos tiene la grilla en cada eje (de 2 a 16). Más puntos siguen mejor los cambios de luz, pero el horneado tarda más. Cambiar los puntos borra lo horneado. |
| Rayos | Cuántos rayos se mandan desde cada punto al hornear. Más rayos, menos ruido. |
| Hornear | Calcula la luz de todos los puntos. Hay que volver a hornear cuando cambia la escena o las luces. |

Abajo dice si la sonda está horneada y cuántos puntos tiene.

## Qué objetos cuentan

Al hornear, la sonda solo ve los objetos **estáticos** (la casilla **Estático** de la pestaña Objeto, que viene tildada): el piso, las paredes, lo que no se mueve. Destildala en lo que se mueve (un personaje, una pelota): ese objeto no tapa ni rebota luz en la sonda, y es el que **recibe** su luz al moverse.

La sonda ilumina **solo a lo que no es estático**. Lo estático precalcula su propia luz en una textura: ver [Precalcular iluminación](objeto-malla.md#precalcular-iluminación).

La luz rebotada toma el color de lo que la rebota y se multiplica por el color del material que la recibe: sobre un piso rojo, una esfera blanca se tiñe de rojo abajo; una esfera verde pura no, porque el verde no refleja nada de rojo.

Con shaders llega a **todos** los materiales con luz, con la luz por píxel o por vértice (un personaje importado suele venir por vértice): cada vértice recibe la luz de la sonda según hacia dónde mira. En un personaje con **esqueleto**, la sonda toma la luz de donde está su pose en ese momento, así lo sigue mientras camina por la escena.

## Consejos

- Poné la caja de manera que **envuelva** los objetos que se mueven (personajes, cosas que se caen) y las zonas donde la luz cambia mucho.
- Un punto que queda **adentro** de una malla (una pared, el piso) no ve la luz: se rellena solo con el promedio de sus vecinos.
- Si hay varias sondas, una malla toma la luz de la más chica que la contiene.
- Un objeto fuera de toda sonda usa la luz del fondo (la **Luz del fondo** de la tarjeta Render).

## En el Nokia N95 (OpenGL ES 1.1)

Sin luz por píxel, la sonda llega igual como la luz ambiente de cada objeto (su promedio), sin costo. Para que la luz siga la forma del objeto, tildá **Luz indirecta** en el material: la CPU calcula la luz de cada vértice (las lámparas más la luz de la sonda según hacia dónde mira el vértice). Usala en los objetos donde se nota; cuesta CPU en cada cuadro.

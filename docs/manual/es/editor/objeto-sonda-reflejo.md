# Sonda de reflejo

Una **cámara de 360 grados** puesta en un lugar de la escena (como el *Reflection Probe* de Unity). Guarda en una **imagen 360** todo lo que se ve desde ahí, y los materiales con reflejo la usan: un auto que pasa por un garaje refleja el garaje y no el cielo.

Se crea con **Añadir → Luces → Sonda de reflejo**, en el cursor 3D. Seleccionada se ve su **caja** (lo que está adentro usa su reflejo) y, en el centro de la captura, una **esfera espejo** con la imagen de la sonda: lo que se horneó, o lo que está viendo la de tiempo real.

## La tarjeta Sonda de reflejo

| Propiedad | Qué hace |
|---|---|
| Tipo | **Horneada**: la imagen se calcula en el editor con **Hornear**, con trazado de rayos (la misma lógica que la [luz precalculada](objeto-malla.md#precalcular-iluminación) y la [Sonda de luz](objeto-sonda-luz.md): solo los objetos **Estático**, si proyectan o no sombras, las lámparas, la luz que rebota y el fondo). **Personalizada**: una imagen 360 que elegís vos (**Imagen 360**). **Tiempo real**: la dibuja la placa de video (OpenGL / Vulkan) como cualquier cuadro, con todo lo que se ve, también lo que se mueve. Anda en el editor y en el juego. No usa rayos: no tiene **Muestras**, **Rebotes** ni el botón **Hornear**. Ver [La sonda de tiempo real](#la-sonda-de-tiempo-real). |
| Importancia | Si un objeto está adentro de dos sondas, usa la de más importancia (con la misma, la más chica). |
| Intensidad | Qué tan brillante sale lo reflejado. |
| Proyección en caja | Corrige el reflejo por la caja de la sonda: lo que se refleja está en sus paredes y no en el infinito (una habitación, un pasillo). Sin ella el reflejo es como el de algo muy lejano. |
| Tamaño X / Y / Z | La caja: su alcance y, con la proyección en caja, las paredes. |
| Origen X / Y / Z | Desde dónde se saca la foto, corrido del centro de la caja (para que no quede adentro de un objeto). |
| Resolución | El tamaño de la imagen 360 (el doble de ancho que de alto). Más grande, más nítido y más tiempo para hornear. |
| Cerca | Lo que está más cerca que esto del origen no se ve (por si la sonda queda adentro de algo). |
| Muestras | (Horneada) Los rayos por cada píxel de la imagen. Más muestras, menos granulado y más tiempo. Vienen 32. |
| Rebotes | (Horneada) Las veces que rebota la luz. Con 0, lo que no ve ninguna lámpara sale negro. Vienen 3. |
| Actualizar cada | (Tiempo real) Cada cuántos segundos se vuelve a dibujar. **0** = en cada cuadro (hasta 60 veces por segundo). |
| Hornear | (Horneada) Calcula la imagen, con el fondo de la escena tal cual (su color o su 360). Queda como una textura del proyecto y se guarda con el .w3d. |

## La sonda de tiempo real

La placa de video dibuja la escena desde el origen de la sonda hacia los 6 lados (las caras de un cubo) y las junta en la imagen 360. Como es la misma forma de dibujar que la vista Render, se ven las luces, el fondo, los materiales y lo que se mueve. La imagen no se guarda con el proyecto: el editor y el juego la vuelven a dibujar.

- **Que siga a algo**: hacé la sonda hija del objeto (un auto, una esfera) y se mueve con él. Su imagen se vuelve a dibujar donde esté.
- **Adentro de un objeto**: desde adentro se vería la carrocería. Destildá **Se ve en las sondas** en la pestaña Objeto de ese objeto (ver [la malla](objeto-malla.md)) o subí **Cerca** hasta pasar su borde.
- **Lo que cuesta**: dibujar la escena 6 veces más. Con **Actualizar cada** en 0 se hace en cada cuadro. Si pesa, subí el tiempo o bajá la **Resolución** (cada cara es de un cuarto del ancho de la imagen).
- Lo que ve no tiene sombras en tiempo real ni pasa por las rutinas (su cámara, limpiar la pantalla): es la escena tal cual.
- Necesita una placa con shaders: en el pipeline fijo (N95, PSP, 3DS, Dreamcast) y en las GPU chicas (el Galaxy Ace) no se dibuja.

## Los materiales PBR la usan solos

Un material con la luz **por píxel** (los valores PBR: metálico, rugosidad) toma sus reflejos de la sonda que lo **contiene**, sin elegir nada, como en Unity: una esfera 100% metálica y lisa adentro de la caja refleja lo que se horneó, y más rugosa lo refleja borroso. La sonda también le da la luz del ambiente que llega desde cada lado. Afuera de todas las sondas refleja el fondo de la escena. (Sin una sonda ni un fondo 360, un metal liso no tiene nada que reflejar y se ve oscuro.)

## Usarla en un material

En la tarjeta Material, el selector de la **Textura** tiene el submenú **Sondas de reflejo** (separado del resto de las texturas):

- **La sonda más cercana (automático)**: cada objeto usa la sonda de su lugar, y el reflejo cambia mientras se mueve de una sonda a otra.
- **Una sonda en particular**: ese reflejo siempre.

Elegir una sonda prende el **Reflejo** del material en modo **Equirectangular 360**.

## Prenderla y apagarla (el antes y después)

El **ojo** de la sonda en el outliner la apaga: los materiales PBR de adentro vuelven a reflejar el fondo de la escena en el mismo momento, y la imagen horneada no se pierde. El ojo de la **colección** que la contiene apaga todas las sondas de adentro (de reflejo y de luz). La **cámara** del outliner (no renderizar) la apaga solo en el render final.

(Un material con **Una sonda en particular**, o con **La sonda más cercana** cuando no queda ninguna prendida, sigue usando la imagen que se eligió: es su textura.)

Sin tocar la escena: en el menú **Overlays** de la Vista 3D, **Sondas de reflejo** (y **Sondas de luz**) destildada apaga todas las sondas de ese tipo solo en esa vista. Ver [la sonda de luz](objeto-sonda-luz.md#prenderla-y-apagarla-el-antes-y-después).

## En el Nokia N95 (OpenGL ES 1.1)

El reflejo 360 se calcula por vértice en la CPU, así que también anda en el N95 (más lento con muchas caras). La sonda de tiempo real no se dibuja ahí: usá una horneada.

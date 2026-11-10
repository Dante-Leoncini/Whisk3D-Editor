# Modelar una puerta

Vamos a hacer nuestro primer modelo simple: ¡una puerta! Para empezar nos sirve el cubo que viene en la escena, que mide 2 × 2 × 2 metros.

Yo (Dante) uso mucho las puertas para crear espacios: es algo que aprendí estudiando construcción. Las puertas casi siempre miden 2 metros de alto, y una persona (normalmente) mide menos que una puerta. Con una puerta en la escena tenés enseguida la escala de todo lo demás.

## Apoyarla en el piso

El cubo tiene su centro en el piso, así que la mitad queda abajo. Lo subimos 1 metro en el eje Z para que quede apoyado. Con el cubo elegido:

![PC](img/icono-pc.svg) Apretá <kbd>G</kbd>: el cubo sigue al mouse. Después <kbd>Z</kbd>: queda atado al eje Z (aparece la línea azul). Escribí `1` y apretá <kbd>Enter</kbd>.

![Con G el cubo sigue al mouse; con Z queda en la línea azul del eje vertical; con 1 sube exactamente 1 metro y Enter lo deja ahí, apoyado en el piso](img/modelar-subir-raton.webp)

![Android](img/icono-android.svg) Tocá **Mover** en la barra de herramientas, después **Z**, un toque en la barra de arriba para el teclado numérico, `1`, **OK** y la tilde verde.

![Se toca Mover y Z en la barra de herramientas; un toque en la barra de arriba abre el teclado numérico, 1 y OK suben el cubo y la tilde lo deja hecho; abajo aparece la ficha Mover, que se despliega con un toque y se cierra con Aceptar](img/modelar-subir-dedo.webp)

![Nokia](img/icono-nokia.svg) <kbd>1</kbd> (mover), <kbd>3</kbd> (el eje Z), el **lápiz** mantenido y <kbd>1</kbd>, y <kbd>OK</kbd>.

![Un Nokia N95: el 1 empieza a mover, el 3 lo deja en el eje Z, el lápiz y el 1 escriben el valor y el OK lo confirma](img/modelar-subir-nokia.webp)

Cómo se escriben los números en cada dispositivo está en [Con números exactos](modo-objeto.md#con-números-exactos).

### Corregir la última operación

Después de mover, rotar o escalar aparece abajo a la izquierda una ficha chica, **Ajustar la última operación**, en lugar de la barra de herramientas. Plegada solo dice qué hiciste (por ejemplo, **Mover**); un toque o un click en su título la despliega:

![La ficha Mover desplegada: X 0, Y 0 y Z 1 metro, la orientación Global, la casilla de edición proporcional y los botones Cancelar y Aceptar](img/modelar-ficha.webp)

Cada valor se puede corregir: si en vez de 1 metro querías 1,2, tocás Z, escribís `1.2` y la operación se vuelve a hacer con el valor nuevo. **Aceptar** la cierra y vuelve la barra de herramientas; **Cancelar** deshace la operación.

## Darle forma

Hay dos formas de modelar la puerta.

### La más fácil, que no recomiendo: escalar el objeto

En el Modo Objeto, escalá el cubo en el eje X y en el eje Y hasta que tenga la forma de la puerta. Por ejemplo, <kbd>S</kbd> <kbd>X</kbd> `0.45` <kbd>Enter</kbd> (90 cm de ancho) y <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd> (10 cm de grueso).

No la recomiendo porque el objeto queda con una escala distinta en cada eje (0,45, 0,05 y 1), y lo ideal es que tenga escala **1** en sus tres ejes (ver [Mejor con la escala en 1](modo-objeto.md#mejor-con-la-escala-en-1)). Si llegás a hacerlo así, **siempre aplicá la transformación**: <kbd>Ctrl</kbd> + <kbd>A</kbd> → **Escalar** (o Objeto → Aplicar → Escalar). La forma no cambia y la escala vuelve a 1.

![En el Modo Objeto, S X 0.45 y S Y 0.05 dejan el cubo con forma de puerta, y Propiedades muestra la escala distinta en cada eje; Ctrl+A y Escalar la aplican y la escala vuelve a 1](img/modelar-escalar-raton.webp)

![Android](img/icono-android.svg) En una pantalla táctil: **Escalar**, **X**, la barra de arriba, `0.45` y OK, y lo mismo con **Y** y `0.05`. Después, **Objeto → Aplicar → Escalar**.

![Escalar en X con 0.45 y en Y con 0.05 desde la barra de herramientas y el teclado numérico; después Objeto, Aplicar, Escalar, y la escala vuelve a 1](img/modelar-escalar-dedo.webp)

### La que recomiendo: el Modo Edición

Con el cubo elegido, entrá al Modo Edición: apretá <kbd>Tab</kbd> o elegí **Modo Edición** en el botón del modo de la Vista 3D. Al entrar, todos los vértices quedan elegidos.

En el Modo Edición se eligen **vértices**, **aristas** y **caras**: eso es lo básico. Y, al igual que en el Modo Objeto, se pueden mover, rotar y escalar (ver [Modo Edición](../editor/modo-edicion.md)).

Con todo elegido, achicá en X y en Y hasta tener la puerta como la querés. En Z no hace falta, ¡porque ya mide 2 metros! Como lo que cambia es la malla y no el objeto, la escala sigue en 1.

**A ojo, con el gizmo:** un click (o un toque) en el centro del gizmo lo pasa a Rotar, y otro a **Escalar**. Agarrá la punta roja (X) y arrastrala hacia el centro hasta el ancho que quieras; después la verde (Y), hasta que quede finita como una puerta.

![En el Modo Edición, dos clicks en el centro del gizmo lo pasan a escalar; la punta roja se arrastra hacia el centro y el cubo se angosta; después la verde, y queda finito como una puerta](img/modelar-gizmo-raton.webp)

![Lo mismo con el dedo: dos toques en el centro del gizmo y las puntas roja y verde arrastradas hacia el centro](img/modelar-gizmo-dedo.webp)

**Con números:** <kbd>S</kbd>, <kbd>X</kbd>, `0.45`, <kbd>Enter</kbd> y <kbd>S</kbd>, <kbd>Y</kbd>, `0.05`, <kbd>Enter</kbd>.

![Con Tab el cubo pasa al Modo Edición, todo en verde; S, X, 0.45 y Enter lo angostan, y S, Y, 0.05 y Enter lo afinan; la escala del objeto sigue en 1](img/modelar-edicion-raton.webp)

![Android](img/icono-android.svg) En una pantalla táctil: el botón del modo → **Modo Edición**; después **Escalar**, **X**, un toque en la barra de arriba, `0.45`, **OK** y la tilde, y lo mismo con **Y** y `0.05`.

![El botón del modo pasa al Modo Edición; Escalar, X, la barra de arriba, 0.45 y OK lo angostan; lo mismo con Y y 0.05](img/modelar-edicion-dedo.webp)

![Nokia](img/icono-nokia.svg) En el Nokia N95: <kbd>OK</kbd> pasa al Modo Edición. <kbd>3</kbd> (escalar), <kbd>1</kbd> (el eje X), el lápiz mantenido y `0` <kbd>*</kbd> `45` (el <kbd>*</kbd> es el punto), y <kbd>OK</kbd>. Lo mismo con <kbd>2</kbd> (el eje Y) y `0.05`.

![Un Nokia N95: OK pasa al Modo Edición; 3 y 1 escalan en X, el lápiz y 0.45 escriben el valor y OK lo confirma; lo mismo con 3, 2 y 0.05](img/modelar-edicion-nokia.webp)

## Con medidas exactas

Si en vez de hacerlo a ojo querés, por ejemplo, una puerta de exactamente **90 cm de ancho**: en el botón **Vértice / Arista / Cara** elegí **Cara** y hacé click en uno de los costados. Sabemos que el cubo mide 2 metros, así que tenemos que restarle lo que sobra para que quede en 0,9: hay que correr ese costado **1,1 metros**. <kbd>G</kbd> <kbd>X</kbd> `-1.1` <kbd>Enter</kbd>.

![En el Modo Edición se elige Cara, un click en el costado derecho lo elige solo a él, y G, X, -1.1 y Enter lo corren: el cubo queda de 0,9 metros de ancho](img/modelar-exacto-raton.webp)

![Con el dedo: Cara en el botón de selección, un toque en el costado, Mover, X, la barra de arriba, -1.1 y OK; el cubo queda de 0,9 metros de ancho](img/modelar-exacto-dedo.webp)

Al movernos, el eje **X** bloquea el movimiento en un solo eje, para que el cubo no se deforme:

- ![PC](img/icono-pc.svg) Con la tecla <kbd>X</kbd>.
- ![Android](img/icono-android.svg) Con un toque en **X** en la barra de herramientas. El `-1.1` se escribe con el teclado numérico, tocando la barra de arriba.
- ![Nokia](img/icono-nokia.svg) Con <kbd>1</kbd> mientras se mueve. El número se escribe con el lápiz mantenido: <kbd>#</kbd> le cambia el signo y <kbd>*</kbd> es el punto. La cara se elige con el mouse virtual (la tecla de selección derecha, las flechas y <kbd>OK</kbd>).

Para el grosor, con todo elegido, <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>, como antes.

¡Y ya tenemos nuestro primer modelo 3D! Felicitaciones.

En el próximo tutorial vamos a hacer que la puerta se abra desde la bisagra: [Cambiar el origen: una puerta](puerta.md).

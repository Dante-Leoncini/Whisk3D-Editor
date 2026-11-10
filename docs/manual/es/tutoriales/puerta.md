# Cambiar el origen: una puerta

En este tutorial vas a hacer que una puerta se abra desde la bisagra. Para eso hay que cambiar su **origen**: el punto desde donde gira el objeto (ver [El origen](modo-objeto.md#el-origen) en el tutorial del Modo Objeto).

Partimos de la puerta del tutorial anterior, [Modelar una puerta](modelar-puerta.md), con el origen en el centro. Si la hiciste corriendo un costado, el origen quedó corrido: **Objeto → Definir Origen → Origen a Geometría** lo vuelve al centro.

## La puerta gira mal

Probá girarla. La puerta gira sobre su centro, como una puerta giratoria, y no desde la bisagra:

![PC](img/icono-pc.svg) Con el gizmo: un click en su centro lo pasa a **Rotar**, y el anillo azul la gira sobre el eje vertical.

![Un click en el centro del gizmo lo pasa a Rotar; el anillo azul se arrastra y la puerta gira sobre su centro, como una puerta giratoria, y vuelve](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) Con el dedo: un toque en el centro del gizmo lo pasa a **Rotar**, y el anillo azul se arrastra.

![El dedo toca el centro del gizmo y lo pasa a Rotar; después arrastra el anillo azul y la puerta gira sobre su centro, y vuelve](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) En el Nokia N95 el gizmo viene apagado: <kbd>2</kbd> (rotar), <kbd>3</kbd> (el eje Z, el vertical) y las flechas; <kbd>OK</kbd> confirma.

![Un Nokia N95: 2 y 3 empiezan a rotar sobre el eje vertical; la flecha derecha gira la puerta sobre su centro, la izquierda la vuelve y OK confirma](img/puerta-gira-mal-nokia.webp)

Gira mal porque su origen está en el centro. Vamos a llevarlo a la bisagra.

## Llevar el origen a la bisagra

1. **Entrá al Modo Edición.** Un click en el botón del modo y otro en **Modo Edición** (en la PC, también con <kbd>Tab</kbd>).

   ![El botón del modo abierto, con Modo Edición marcado](img/puerta-paso-1.webp)

2. **Elegí las caras.** En el Modo Edición aparece el botón **Vértice / Arista / Cara**: un click y elegí **Cara**.

   ![El botón Vértice, Arista, Cara abierto, con Cara marcado](img/puerta-paso-2.webp)

3. **Elegí la cara de la bisagra.** Un click en la cara angosta del costado de la puerta, del lado donde va la bisagra: se pone verde.

   ![La cara angosta del costado de la puerta, elegida en verde y marcada con un círculo](img/puerta-paso-3.webp)

4. **Llevá el cursor 3D ahí.** En el menú **Malla**, **Imán** → **Cursor a lo seleccionado**: el cursor 3D queda sobre la bisagra.

   ![El menú Malla abierto, con el submenú Imán y Cursor a lo seleccionado marcado](img/puerta-paso-4.webp)

5. **Volvé al Modo Objeto.** Un click en el botón del modo y otro en **Modo Objeto** (o <kbd>Tab</kbd>).

   ![El botón del modo abierto, con Modo Objeto marcado; el cursor 3D ya está en la bisagra](img/puerta-paso-5.webp)

6. **Llevá el origen al cursor 3D.** En el menú **Objeto**, **Definir Origen** → **Origen al Cursor 3D**. La puerta no se mueve: el que se mueve es su origen, y con él el gizmo, que queda en la bisagra.

   ![El menú Objeto abierto, con el submenú Definir Origen y Origen al Cursor 3D marcado](img/puerta-paso-6.webp)

## Ahora gira bien

Girala otra vez con el gizmo: ahora se abre y se cierra desde la bisagra.

![Con el origen en la bisagra, el anillo azul del gizmo abre la puerta hasta quedar de costado y la vuelve a cerrar](img/puerta-gira-bien-raton.webp)

![Con el dedo, el anillo azul abre la puerta desde la bisagra y la vuelve a cerrar](img/puerta-gira-bien-dedo.webp)

![Un Nokia N95: 2, 3 y la flecha derecha abren la puerta desde la bisagra; la izquierda la cierra y OK confirma](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) En una pantalla táctil, los pasos de arriba son iguales, con toques: los menús se abren y se eligen con un toque.

![Nokia](img/icono-nokia.svg) En el Nokia N95, <kbd>OK</kbd> pasa del Modo Objeto al Modo Edición y los menús se abren con la soft izquierda.

## Las opciones de Definir Origen

| Opción | Qué hace |
|---|---|
| **Origen a Geometría** | Lleva el origen al centro de la malla. |
| **Origen al Cursor 3D** | Lleva el origen a donde está el cursor 3D. |
| **Geometría al Origen** | Al revés: deja el origen y mueve la malla hasta que su centro quede en él. |

En el próximo tutorial le hacemos el picaporte: [Modelar el picaporte](modelar-picaporte.md).

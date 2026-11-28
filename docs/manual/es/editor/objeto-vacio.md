# Vacío

Un **Vacío** es un objeto sin malla: no se ve al renderizar ni en el juego. Sirve como punto de referencia: un pivote para girar o escalar otros objetos que cuelgan de él, un lugar donde apuntar una cámara o una restricción, un control para animar. Se agrega con **Añadir → Vacío**, en el cursor 3D. Cómo se agregan y se mueven los objetos está en el tutorial [El Modo Objeto](../tutoriales/modo-objeto.md).

En la Vista 3D se dibuja con líneas. Un Vacío nuevo es un **círculo acostado en el piso**, fácil de ver y de agarrar. La forma, el tamaño y el giro del dibujo se cambian en su tarjeta. Es **solo cómo se ve**: no cambia el objeto, ni su transformación, ni a sus hijos.

![Seis Vacíos: la cruz, el cuadrado acostado en el piso, el cubo, el círculo acostado en el piso, la esfera de tres anillos y un círculo girado 90 grados en X, parado](../tutoriales/img/objeto-vacio-formas.webp)

## Las pestañas

| Pestaña | Qué tiene |
|---|---|
| Objeto | Posición, rotación, escala, nombre y visibilidad, igual que en todos los objetos: ver [Malla 3D](objeto-malla.md#la-pestaña-objeto). Escalar el Vacío escala también a sus hijos. |
| Vacío (la cruz) | La tarjeta de abajo. |
| Restricciones | Ver [Restricciones](restricciones.md). |
| Scripts | Los scripts Lua del objeto. Ver [Lua en Whisk3D](../lua/README.md). |

## La tarjeta Vacío

| Propiedad | Qué hace |
|---|---|
| Forma | Cómo se dibuja, cada una con su ícono: **Cruz** (los tres ejes), **Cuadrado**, **Cubo**, **Círculo**, **Esfera** (tres círculos, uno por eje) o **Imagen** (la referencia: ver abajo). El cuadrado y el círculo nacen acostados en el piso. En las pantallas chicas (el Nokia N95, la PSP, la 3DS y la Dreamcast) el círculo y la esfera se dibujan con la mitad de líneas: ahí no se nota la diferencia y el teléfono dibuja menos. |
| Imagen | (Forma **Imagen**) El archivo de la imagen. Tocarlo abre el selector para cambiarla. |
| Opacidad | (Forma **Imagen**) Qué tan opaca se ve, de 0 a 1. Por debajo de 1 se ve lo que hay detrás. |
| Tamaño | Lo que mide el dibujo, en metros, de punta a punta. 1 es un metro. En la imagen, su lado largo. |
| Rotación X, Y, Z | Gira el dibujo, en grados. Por ejemplo, el círculo con **X** en 90 queda parado. |

Nada de esto mueve ni gira el objeto: el Vacío sigue en el mismo lugar y sus hijos no se enteran. Se guarda con el proyecto.

## La referencia (forma Imagen)

**Añadir → Referencia** crea un Vacío con forma **Imagen** y abre el selector de la imagen: una imagen para calcar encima mientras modelás, como el *Image Empty* de Blender. La imagen sale **parada, mirando al frente**, con su proporción (no se deforma: el lado largo mide lo que dice **Tamaño**, 2 m al crearla). Como en Blender, la imagen va acostada en el plano X-Y del objeto: por eso la referencia nace con **90° en X** (con la rotación en 0, 0, 0 queda acostada mirando para arriba, cómoda para la vista de arriba). Se ve en **todos los modos de vista** —sólido, material, render, alambre y encima del trazado de rayos—, así se pueden dibujar las aristas sobre ella en alambre. Elegida, muestra su marco; clickeando la imagen se la elige.

No es una malla: no aparece en **Mallas 3D** de la biblioteca, no sale en el render ni en el juego, y no proyecta sombras. La imagen se guarda con el proyecto (adentro del `.w3d`, o afuera si es externa), con su tamaño y su opacidad.

## Para qué sirve

- **Un pivote:** un objeto hijo de un Vacío gira y se escala desde el Vacío. Por ejemplo, una pelota emparentada a un Vacío puesto en su punto de abajo se aplasta contra el piso al escalar el Vacío.
- **Ordenar la escena:** varios objetos colgados de un Vacío se mueven juntos moviendo solo el Vacío.
- **Un blanco:** las [restricciones](restricciones.md) pueden apuntar a un Vacío, por ejemplo para que una cámara lo siga mirando.

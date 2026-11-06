# Navegar en la Vista 3D

> [!NOTE]
> ![Nokia](img/icono-nokia.svg) El Nokia N95 soporta mouse y teclado (por Bluetooth). Las partes de este tutorial sobre el Nokia explican cómo se usa sin mouse ni teclado, solo con las teclas del teléfono.

Este tutorial explica cómo navegar en la Vista 3D. Navegar es mover tu punto de vista: girar alrededor de la escena, correrte para un costado y acercarte o alejarte.

Saber navegar es importante: para hacer modelos 3D y animaciones de forma cómoda y rápida, tenés que saber moverte por la escena y colocarte en una posición cómoda para trabajar.

Como se explicó en el [tutorial anterior](viewports.md#el-viewport-activo), para usar los comandos que siguen, la Vista 3D tiene que ser el viewport activo, el del borde verde.

Las tres cosas que más vas a usar son **orbitar**, **desplazar** y hacer **zoom**. Cada una se puede hacer con el mouse, con los dedos en una pantalla táctil o con el teclado del Nokia N95. En los videos de la PC, el mouse de abajo a la derecha muestra en verde el botón que se está apretando.

## Orbitar

Orbitar es girar la vista alrededor de la escena, como si caminaras alrededor de una mesa para ver lo que hay arriba desde todos los lados.

![PC](img/icono-pc.svg) **Con el mouse:** mantené apretado el **botón del medio** (se aprieta hundiendo la rueda) y arrastrá. En un trackpad, o en un mouse sin botón del medio, mantené <kbd>Alt</kbd> y arrastrá con el **click primario** (el izquierdo).

![El puntero sobre la Vista 3D: con el botón del medio apretado, el arrastre gira la vista alrededor del cubo](img/navegacion-orbitar-raton.webp)

![Android](img/icono-android.svg) **Con el dedo:** arrastrá un dedo sobre la Vista 3D. Empezá sobre el fondo: si lo apoyás sobre el gizmo (las flechas de colores del objeto), lo vas a mover.

![Un dedo se arrastra sobre el fondo de la Vista 3D y la vista gira alrededor del cubo](img/navegacion-orbitar-dedo.webp)

![Nokia](img/icono-nokia.svg) **En el Nokia N95:** mantené apretada una **flecha** del pad. La vista gira para ese lado mientras la tengas apretada.

![Un Nokia N95 parado: con las flechas derecha, izquierda, arriba y abajo apretadas, la vista gira alrededor del cubo](img/navegacion-orbitar-nokia.webp)

## Desplazar

Desplazar (o *panear*) corre la vista para un costado, para arriba o para abajo, sin girarla.

![PC](img/icono-pc.svg) **Con el mouse:** mantené <kbd>Shift</kbd> y arrastrá con el **botón del medio**. En un trackpad: <kbd>Alt</kbd> + <kbd>Shift</kbd> y arrastrá con el click primario.

![Con Shift y el botón del medio apretados, el puntero hace un círculo y la vista se desplaza con él](img/navegacion-panear-raton.webp)

![Android](img/icono-android.svg) **Con los dedos:** apoyá **dos dedos** y movelos juntos para el mismo lado.

![Dos dedos se mueven juntos en círculo y la vista se desplaza](img/navegacion-panear-dedo.webp)

![Nokia](img/icono-nokia.svg) **En el Nokia N95:** mantené apretada la tecla <kbd>\*</kbd> y usá las flechas.

![Un Nokia N95 con la tecla asterisco apretada: con cada flecha la vista se desplaza para ese lado](img/navegacion-panear-nokia.webp)

## Zoom

El zoom te acerca o te aleja. El paso se adapta a la distancia: cerca de algo chico avanza despacio y de lejos avanza rápido, así que sirve igual para mirar toda la escena o un vértice.

![PC](img/icono-pc.svg) **Con el mouse:** girá la **rueda**. Para adelante acerca y para atrás aleja. En un trackpad, deslizá dos dedos para arriba o para abajo.

![El puntero sobre el cubo: la rueda para adelante acerca la vista y para atrás la aleja](img/navegacion-zoom-raton.webp)

![Android](img/icono-android.svg) **Con los dedos:** pellizcá. Separar los dedos acerca y juntarlos aleja.

![Dos dedos se separan y la vista se acerca al cubo; se juntan y se aleja](img/navegacion-zoom-dedo.webp)

![Nokia](img/icono-nokia.svg) **En el Nokia N95:** mantené apretado el <kbd>0</kbd> y usá la flecha **arriba** para acercar o la flecha **abajo** para alejar.

![Un Nokia N95 con el 0 apretado: con arriba la vista se acerca al cubo y con abajo se aleja](img/navegacion-zoom-nokia.webp)

## Encuadrar lo seleccionado

Si te perdiste, o querés trabajar sobre algo en particular, **encuadralo**: la vista se centra en lo que está seleccionado y ajusta el zoom para que entre justo. Desde ahí, orbitar gira alrededor de eso.

| | Cómo se encuadra |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num .</kbd> (el punto del teclado numérico) |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Un toque del <kbd>0</kbd>, sin flechas (mantenido con una flecha es zoom) |
| ![Android](img/icono-android.svg) Pantalla táctil | Doble toque con dos dedos |
| Menú | Vista → Encuadrar Selección |

![La vista está lejos y corrida, con el cubo chiquito en una punta; con Num . el cubo queda centrado y llena la vista](img/navegacion-encuadrar.webp)

![Un Nokia N95: con un toque del 0, el cubo que estaba chiquito en una punta queda centrado y encuadrado](img/navegacion-encuadrar-nokia.webp)

### En el Modo Edición

En el [Modo Edición](../editor/modo-edicion.md) se encuadra la parte seleccionada de la malla: los vértices, aristas o caras que elegiste, no el objeto entero. Sirve para trabajar sobre un detalle de un modelo grande: lo seleccionás, lo encuadrás y orbitás alrededor de él.

![Una esfera en Modo Edición con seis caras seleccionadas (en verde): con Num . la vista se acerca y esas caras llenan la pantalla](img/navegacion-encuadrar-edicion.webp)

## La vista local

Cuando hay muchas cosas en la escena, trabajar sobre una es incómodo: las de adelante tapan, y al orbitar se meten en el medio. La **vista local** deja a la vista solo lo que está seleccionado (uno o varios objetos) y lo encuadra. El resto no se borra ni se oculta en la escena: solo deja de verse en este viewport. Arriba a la izquierda aparece el cartel "Vista local".

Para volver, se repite lo mismo: todo vuelve a aparecer y la vista queda como la tenías antes.

| | Cómo se entra y se sale |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>/</kbd> |
| ![Nokia](img/icono-nokia.svg) ![Android](img/icono-android.svg) Nokia N95 y pantalla táctil | Menú Vista → Vista local |

![Una escena llena de cajas, esferas, cilindros y conos, con un cono seleccionado: con / queda solo el cono, encuadrado, y con / otra vez vuelve todo](img/navegacion-vista-local.webp)

## Las vistas de frente, de costado y de arriba

Para mirar la escena derecho desde un lado, sin acomodar la órbita a mano:

| Vista | ![PC](img/icono-pc.svg) PC | Menú |
|---|---|---|
| Frontal / Posterior | <kbd>Num 1</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 1</kbd> | Vista → Punto de Vista → Frontal / Posterior |
| Derecha / Izquierda | <kbd>Num 3</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 3</kbd> | Vista → Punto de Vista → Derecha / Izquierda |
| Superior / Inferior | <kbd>Num 7</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 7</kbd> | Vista → Punto de Vista → Superior / Inferior |

![PC](img/icono-pc.svg) Con <kbd>Ctrl</kbd>, cada tecla da la vista opuesta.

![La escena llena desde la vista frontal, la derecha, la superior y, con Ctrl, sus opuestas](img/navegacion-vistas.webp)

![Nokia](img/icono-nokia.svg) **En el Nokia N95:** mantené apretada la tecla <kbd>#</kbd> y apretá una flecha. Izquierda y derecha giran la vista de a 90° alrededor de la escena. Arriba pasa a la vista de arriba; abajo vuelve a la de costado, y otra vez abajo va a la de abajo. Si estabas orbitando libre, la primera flecha solo endereza la vista a la más cercana.

![Un Nokia N95 con el numeral apretado: con la flecha derecha la vista gira de a 90 grados, con arriba pasa a la vista de arriba y con abajo vuelve](img/navegacion-vistas-nokia.webp)

## Perspectiva y ortográfica

La Vista 3D tiene dos maneras de mostrar la escena:

- **Perspectiva:** como la ve el ojo o una cámara. Lo que está lejos se ve más chico, y las líneas paralelas (los bordes de una calle, las filas de cajas del video) se juntan en un punto del horizonte: el **punto de fuga**. Así se ve de verdad, con profundidad.
- **Ortográfica:** sin perspectiva. Todo mantiene su tamaño, esté cerca o lejos, y las líneas paralelas nunca se tocan. Es como un plano de arquitectura o un dibujo técnico.

La ortográfica sirve para modelar con precisión: alinear cosas, comparar tamaños y medir sin que la distancia engañe. Combinada con las vistas de frente, de costado y de arriba, es como trabajar sobre un plano. La perspectiva sirve para ver cómo va a quedar.

| | Cómo se cambia |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num 5</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Un toque del <kbd>#</kbd>, sin flechas |
| Menú | Vista → Perspectiva/Orto |

![Dos filas de cajas iguales: en perspectiva las del fondo se ven chicas; con Num 5, en ortográfica, todas tienen el mismo tamaño](img/navegacion-perspectiva.webp)

![Un Nokia N95: con un toque del numeral, las filas de cajas pasan de perspectiva a ortográfica y vuelven](img/navegacion-perspectiva-nokia.webp)

## Bloquear la órbita

Con la **órbita bloqueada**, la vista deja de girar: lo que antes orbitaba ahora la desplaza, como si movieras una hoja sobre una mesa. Es ideal para trabajar cómodo en modelos que se dibujan como un plano 2D, como el plano de una casa o una pieza mecánica: mirando de arriba y en ortográfica, te movés por todo el plano sin que se tuerza nunca.

| | Cómo se bloquea y se desbloquea |
|---|---|
| ![PC](img/icono-pc.svg) PC | Menú Vista → Bloquear Órbita |
| ![Android](img/icono-android.svg) Pantalla táctil | Menú Vista → Bloquear Órbita |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>9</kbd> |

Al bloquearla o desbloquearla aparece un aviso, y mientras está bloqueada, Bloquear Órbita se ve en verde en el menú.

![El plano de una casa visto de arriba: se elige Vista → Bloquear Órbita y, con el botón del medio, el plano se desplaza sin girar](img/navegacion-bloquear-orbita-raton.webp)

![El plano de una casa visto de arriba: se toca Vista → Bloquear Órbita y el dedo desplaza el plano sin girarlo](img/navegacion-bloquear-orbita-dedo.webp)

![Un Nokia N95 con el plano de una casa: con el 9 se bloquea la órbita y las flechas desplazan el plano sin girarlo](img/navegacion-bloquear-orbita-nokia.webp)

## La cámara activa

La escena puede tener varias cámaras, pero una sola es la **cámara activa**: la que sirve para el juego y para los renders. Lo que ve esa cámara es lo que se ve al jugar y lo que sale cuando hacés el render de una imagen o de un video. La vista con la que navegás es solo para trabajar: no aparece ni en el juego ni en los renders.

Para mirar por la cámara activa:

| | Cómo se entra y se sale |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num 0</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>8</kbd> (fuera del Modo Edición: ahí el 8 es Loop Cut) |
| Menú | Vista → Cámaras → Cámara Activa |

![Con Num 0 la Vista 3D pasa a mirar por la cámara, con el marco de lo que va a salir en el render, y con Num 0 otra vez vuelve](img/navegacion-camara.webp)

![Un Nokia N95: con el 8 la vista pasa a la cámara activa y con el 8 otra vez vuelve](img/navegacion-camara-nokia.webp)

El rectángulo es el encuadre: lo que queda adentro es lo que sale. Abajo al centro hay un **candado**:

- **Abierto** (como arranca): el zoom y el desplazamiento solo te acercan para revisar el encuadre, sin tocar la cámara. Orbitar te saca de la vista de cámara y seguís navegando desde donde estaba.
- **Cerrado:** orbitar, desplazar y hacer zoom mueven la **cámara**. Así se acomoda la toma.

### Bloquear la cámara a la vista

Con el candado cerrado, la cámara activa queda bloqueada a la vista: orbitás, desplazás y hacés zoom como en la navegación normal, y la cámara se mueve con vos. Es la forma más cómoda de colocar una cámara exactamente donde querés: mirás por ella, la bloqueás y navegás hasta que la toma quede como la buscás.

| | Cómo se bloquea y se desbloquea |
|---|---|
| ![PC](img/icono-pc.svg) PC | Un click en el candado |
| ![Android](img/icono-android.svg) Pantalla táctil | Un toque en el candado |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | La soft izquierda abre la barra de menús (como en el [tutorial anterior](viewports.md#cuando-la-barra-no-entra)); ahí, Vista → Mover la cámara con la vista |
| Menú | Vista → Mover la cámara con la vista |

![Con Num 0 la vista pasa a la cámara; un click en el candado lo cierra y, al orbitar y girar la rueda, la cámara se mueve hasta que el cubo queda encuadrado](img/navegacion-camara-bloquear-raton.webp)

![Mirando por la cámara, un toque en el candado lo cierra y, al arrastrar el dedo, la cámara se mueve hasta que el cubo queda encuadrado](img/navegacion-camara-bloquear-dedo.webp)

![Un Nokia N95: el 8 pasa a la cámara; con la soft izquierda se abre el menú Vista y se elige Mover la cámara con la vista; las flechas y el 0 mueven la cámara hasta encuadrar el cubo](img/navegacion-camara-bloquear-nokia.webp)

Para que otra cámara pase a ser la activa: seleccionala y apretá <kbd>Ctrl</kbd> + <kbd>Num 0</kbd> (o Vista → Cámaras → Objeto Activo como Cámara).

## Más opciones de la vista

| Opción | Qué hace | ![PC](img/icono-pc.svg) PC | ![Nokia](img/icono-nokia.svg) Nokia N95 | Menú |
|---|---|---|---|---|
| Girar la vista | La gira 15° sobre su propio eje, como inclinar la cabeza. | <kbd>Num 4</kbd> / <kbd>Num 6</kbd> | — | — |

Todos los atajos del editor están en [Atajos](../editor/atajos.md), y la [Navegación](../editor/navegacion.md) del manual los resume en tablas.

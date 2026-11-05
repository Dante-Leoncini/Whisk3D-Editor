# Los viewports

La ventana de Whisk3D se divide en **viewports**: paneles, cada uno con su editor (la Vista 3D, el Outliner, Propiedades...). Los podés dividir, cambiarles el tamaño y elegir qué muestra cada uno. Funciona igual con mouse, con el dedo en una pantalla táctil y con el teclado de un Nokia.

Esto es lo que se ve cuando inicia el programa por primera vez:

![Whisk3D recién abierto: la Vista 3D con un cubo, una cámara y una luz; a la derecha el Outliner y Propiedades; abajo la Línea de tiempo](img/inicio.webp)

## La barra de menús y el botón de la esquina

Todos los viewports tienen arriba una **barra de menús** (en verde en la imagen) con los menús y botones de ese editor. Arriba a la izquierda está siempre el **botón de la esquina** (en naranja): es el que cambia o modifica el viewport.

![Los cuatro viewports de Whisk3D, con la barra de menús de cada uno resaltada en verde y el botón de la esquina en naranja](img/viewports-barras.webp)

## Cambiar el tamaño

**Mantené presionado** el botón de la esquina (con el dedo o con el mouse) y arrastralo para cambiar el tamaño: eso mueve el borde izquierdo y el de arriba de ese viewport, y los de al lado se acomodan. Un **toque simple**, sin arrastrar, abre el menú desplegable con las opciones del viewport. Acá, el de abajo a la derecha se arrastra en círculos y los tres cambian de tamaño:

![Un dedo arrastra en círculos el botón de la esquina del viewport de abajo a la derecha, y los tres viewports cambian de tamaño](img/viewports-arrastrar.webp)

**En el Nokia:** mantené apretada la **tecla verde** de llamada ![](img/tecla-verde.svg) y apretá una flecha: el borde del viewport activo (el que tiene el marco verde) se corre para ese lado, y así lo agrandás o lo achicás. La tecla verde sola, sin flechas, pasa al viewport siguiente.

## Las opciones del viewport

Un toque en el botón de la esquina despliega este menú:

![Un dedo va hasta el botón de la esquina de la Vista 3D, lo toca y se despliega el menú: los tipos de viewport arriba y, debajo de la línea, Expandir, Dividir en columnas, Dividir en filas y Maximizar](img/viewports-menu.webp)

Arriba están los **tipos de viewport**: elegir uno convierte el viewport en ese editor. Debajo de la línea está lo que se le puede hacer:

| Opción | Qué hace |
|---|---|
| Expandir | El viewport se queda con el lugar del de al lado, que se cierra (si el de al lado está dividido, se cierra todo ese grupo). |
| Dividir en columnas | Lo parte en dos, uno al lado del otro. |
| Dividir en filas | Lo parte en dos, uno arriba del otro. |
| Maximizar | Lo agranda a toda la ventana. Mientras está maximizado, el menú tiene solo **Minimizar**, que lo devuelve a su lugar. |

### Cambiar el tipo

La Vista 3D convertida en Editor UV:

![Se toca el botón de la esquina de la Vista 3D, se elige Editor UV y el viewport pasa a ser un Editor UV](img/viewports-cambiar-tipo.webp)

### Dividir en columnas y en filas

![Dividir en columnas: la Vista 3D queda partida en dos vistas, una al lado de la otra](img/viewports-dividir-columnas.webp)

![Dividir en filas: la Vista 3D queda partida en dos vistas, una arriba de la otra](img/viewports-dividir-filas.webp)

### Expandir

El Outliner se queda con el lugar de Propiedades:

![Se elige Expandir en el Outliner y ocupa también el lugar de Propiedades](img/viewports-expandir.webp)

### Maximizar y minimizar

![La Vista 3D se maximiza a toda la ventana y después, con Minimizar, vuelve a su lugar](img/viewports-maximizar.webp)

## Cuando la barra no entra

Si el viewport es angosto, su barra no entra entera. Lo mismo pasa con la barra de herramientas de abajo de la Vista 3D. Para ver el resto:

- **Con el dedo:** arrastrala de costado.
- **Con el mouse:** mantené apretado el click sobre la barra y arrastrá, o girá la **rueda** encima.
- **Con el teclado:** <kbd>M</kbd> en la PC, o la **soft izquierda** en el Nokia, entra a la barra del viewport activo. Con las flechas izquierda y derecha la recorrés, y cada menú se abre al llegar. Con arriba y abajo te movés por el menú, y con OK (<kbd>Enter</kbd> en la PC) elegís. La barra se desplaza sola para mostrar dónde estás.

![Un dedo arrastra de costado la barra del Outliner y aparecen los botones que no entraban](img/viewports-barra-dedo.webp)

![El puntero del mouse sobre la barra del Outliner: con la rueda, la barra se desplaza](img/viewports-barra-rueda.webp)

En un Nokia N95: la soft izquierda entra a la barra y las flechas la recorren.

![La interfaz del Nokia N95: la soft izquierda abre el menú Modo y la flecha derecha va pasando por los menús de la barra, que se desplaza sola](img/viewports-barra-nokia.webp)

## Los tipos de viewport

| Viewport | Para qué sirve |
|---|---|
| [Vista 3D](../editor/vista-3d.md) | Donde se ve y se edita la escena: modelar, mover objetos, pintar, posar y animar. |
| [Outliner](../editor/outliner.md) | La lista de todo lo que hay en la escena. |
| [Biblioteca](../editor/outliner.md#vista-biblioteca) | Los recursos del proyecto: mallas, materiales, texturas, animaciones, sonidos, scripts y más. |
| [Propiedades](../editor/propiedades.md) | Los datos del objeto activo, en pestañas. |
| [Editor UV](../editor/editor-uv.md) | Cómo se apoya la textura sobre la malla. |
| [Línea de tiempo](../editor/linea-de-tiempo.md) | La animación: los keyframes y las curvas. |
| [Editor 2D](../editor/editor-2d.md) | Interfaces de juego, HUD y juegos 2D. |
| [Consola](../editor/consola.md) | El registro del motor: errores, avisos e información. |
| [IDE](../editor/editor-de-scripts.md) | El editor de scripts Lua y de shaders. |

Cada viewport va a tener su propio tutorial con más detalle. Mientras tanto, el [Manual del editor](../editor/README.md) explica cada uno.

Whisk3D sigue creciendo: con el tiempo se van a sumar viewports nuevos y los de ahora pueden cambiar. Todos van a funcionar igual, con la barra arriba y el botón en la esquina.

# Modo Objeto

| Función | Descripción | Cómo se usa |
|---|---|---|
| Seleccionar | Elige un objeto. Click en el vacío deselecciona. | Click; <kbd>Shift</kbd>+click suma o quita |
| Mover / Rotar / Escalar | Ver [Mover, rotar y escalar](transformar.md). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd>, gizmo |
| Añadir | Crea un objeto nuevo en el cursor 3D (ver [menú Añadir](#menú-añadir)). | <kbd>Shift+A</kbd> o botón + |
| Aplicar | Hornea la posición, la rotación, la escala o todo en la malla (el objeto queda en 0, 0, 0 sin rotar y a escala 1). | <kbd>Ctrl+A</kbd> |
| Definir origen | Geometría al origen, Origen a la geometría, Origen al cursor 3D. | Menú Objeto |
| Imán | Lleva el cursor 3D a lo seleccionado, al activo, al origen del mundo o a la cuadrícula, y lo seleccionado al cursor, al activo o a la cuadrícula. | <kbd>Shift+S</kbd> o menú Objeto → Imán |
| Duplicar objetos | Copia independiente; queda agarrada para moverla. | <kbd>Shift+D</kbd> |
| Duplicar vinculado | Copia que comparte la malla con el original (editar una edita las dos). | <kbd>Alt+D</kbd> |
| Unir | Junta las mallas seleccionadas en el objeto activo. | <kbd>Ctrl+J</kbd> |
| Emparentar | Lo seleccionado pasa a ser hijo del activo (con o sin mantener el transform). | <kbd>Ctrl+P</kbd> |
| Desemparentar | Quita el padre (manteniendo o no la posición). | <kbd>Alt+P</kbd> |
| Borrar | Borra los objetos seleccionados (pide confirmación). | <kbd>X</kbd> o <kbd>Supr</kbd> |
| Ocultar / Mostrar | Alterna la visibilidad del objeto activo. | <kbd>H</kbd> |
| Crear prefab / Desempaquetar prefab | Convierte la selección en un prefab reutilizable, o vuelve a separar una instancia. | Menú Objeto |
| Insertar keyframe | Guarda la posición, rotación y/o escala en el frame actual (menú de canales). <kbd>Shift+I</kbd> guarda todo sin preguntar. | <kbd>I</kbd> |
| Borrar / Limpiar keyframes | Borra el keyframe del frame actual o toda la animación. | Menú Animación |
| Rastro de movimiento | Dibuja el camino que recorren los objetos animados. | Menú Animación |
| Cambiar de modo | Malla: Objeto ↔ Edición. Esqueleto: Objeto ↔ Pose. Curva: edición de nodos. | <kbd>Tab</kbd> |

## Menú Añadir

Solo en Modo Objeto. Lo nuevo aparece en el cursor 3D y queda seleccionado. Las primitivas abren un panel para ajustar sus medidas.

| Función | Descripción |
|---|---|
| Malla | Plano, Cubo, Círculo, Esfera UV, Cono, Cilindro (con tamaño, radio, segmentos, sombreado suave). También Vértice (un punto suelto para empezar a modelar) y Referencia (un plano con una imagen, para calcar). |
| Vacío | Un objeto sin malla, para agrupar o como punto de referencia. |
| Cámara / Luz / Niebla | Cámara, luz y niebla de la escena. La luz tiene **Tipo**: Punto, Sol (direccional) o Foco (un cono, que se dibuja en la vista); Agregar también ofrece Sol y Foco directo. **Intensidad** la hace más fuerte (animable); **Distancia** (m) es hasta dónde llega una luz puntual o foco y el largo de su cono (0 = sin límite; el trazado de rayos la apaga suave ahí). Con la luz seleccionada, un foco muestra su cono y una puntual una esfera de esa distancia; un sol muestra siempre una línea hasta el piso, hacia donde alumbra. |
| Sonido | Un sonido puesto en la escena (icono de parlante). Suena con la línea de tiempo en **segundos**, no en frames; con **Posicional (3D)** se escucha desde donde está (más fuerte cuanto más cerca de la cámara, del lado en que está); sin eso es como una música. Soltar un .wav en la vista 3D crea uno ahí. |
| Esqueleto | Un esqueleto con un hueso, para animar personajes. |
| Ruta | Una curva de dos nodos (se edita como malla: <kbd>E</kbd> agrega nodos). |
| Colección | Una carpeta para ordenar objetos. |
| UI | Una interfaz 2D para juegos (se edita en el Editor 2D). |
| Hitbox | Una caja de detección de choques; si hay un objeto activo nace como hijo, de su tamaño. |
| Physics | No crea objeto: le agrega un cuerpo rígido al objeto activo. |
| Partículas | Un emisor de partículas. |
| LOD / Culling / Grid Cull | Objetos de optimización: niveles de detalle y ocultamiento de lo que no se ve. |
| Limpiar Z / Recorte | Herramientas de dibujo: limpiar la profundidad o recortar una zona de pantalla. |
| Prefab / Escena / Proxy W3D | Insertar un prefab del proyecto, otra escena, o algo de otro .w3d (ver [Proxy W3D](#proxy-w3d)). |
| Importar | OBJ, FBX, glTF, GLB, STL o una interfaz Whisk3D UI. El STL (impresión 3D, CAD) entra con Z arriba girado para que quede parado, con los vértices iguales soldados. |
| Núcleo (avanzado) | Rutinas: listas de pasos de dibujo que se arman a mano (transformar, dibujar, texturas, luces, niebla, mezcla, profundidad, control, Llamar Lua). Incluye **Bucle de rutina**: una rutina que repite sus pasos varias veces (por ejemplo "trasladar, dibujar" 20 veces), como un array pero más rápido. |

## Proxy W3D

Trae a la escena algo de **otro .w3d**: un objeto (con todo lo que cuelga de él: sus hijos, su malla, sus materiales y su esqueleto con las animaciones de su animset), una colección, un prefab o una escena entera. Los keyframes que el objeto tiene en la escena del otro archivo todavía no vienen. El otro archivo no se modifica: lo que se trae es de solo lectura (sus hijos llevan un candado en el Outliner) y se actualiza solo si el otro .w3d cambia.

1. **Añadir → Proxy W3D** abre el explorador de archivos: elegí el .w3d. Queda vinculado como librería del proyecto (si ya lo estaba, se usa ese).
2. Después se abre **ese .w3d por dentro**: en la raíz están sus escenas (se recorren como carpetas) y sus prefabs. Adentro de una escena están sus objetos; los que tienen hijos también se recorren. Los objetos llevan el ícono del **cubo** (no se confunden con una malla 3D); las luces y las cámaras, el suyo.
3. Elegí un objeto (o un prefab) y apretá **Añadir**. Sin nada elegido, **Añadir** trae lo que estás mirando: la escena entera o el objeto con todos sus hijos.

El proxy nace en el cursor 3D y se llama como lo que trae. Moverlo, rotarlo o animarlo es del proyecto que lo llama: esas animaciones quedan en este .w3d y no tocan el original. En la tarjeta del proxy (Propiedades), el desplegable del elemento tiene **Explorar...**, que abre el mismo explorador para cambiar lo que trae.

### Los recursos de una librería

En la Biblioteca, el botón de la vista lista las librerías vinculadas. Al elegir un recurso de una, Propiedades muestra su tarjeta **de solo lectura**: cuántos objetos de este proyecto lo usan (**0** en rojo), el tipo, la entrada, de qué librería es, si está **cargado** y, si este proyecto ya lo usa, sus datos (vértices, resolución...). **Seleccionar usuarios** elige los objetos que lo usan.

La cuadrícula muestra sus **vistas previas** (materiales, mallas 3D, texturas) si hay memoria. Con poca memoria libre, las vistas previas son lo primero que se suelta, en este orden: sonidos, texturas, mallas 3D, materiales, prefabs y escenas. Si aun así no alcanza, se ven los íconos de cada tipo y aparece el aviso **No hay memoria para cargar las vistas previas**.


# Vista 3D

La Vista 3D es donde se ve y se edita la escena. Trabaja en distintos **modos**: lo que hacen los clicks, las teclas y los menús depende del modo actual. El modo se elige con el selector de la barra superior (solo aparece si el objeto activo es una malla o un esqueleto) o con <kbd>Tab</kbd>.

| Modo | Para qué objetos | Para qué sirve |
|---|---|---|
| [Modo Objeto](modo-objeto.md) | Todos | Crear, mover, rotar, escalar, duplicar, emparentar y borrar objetos enteros. |
| [Modo Edición](modo-edicion.md) | Mallas y curvas | Editar vértices, aristas y caras (modelar). |
| [Modo Edición de esqueleto](esqueletos.md#modo-edición-de-esqueleto) | Esqueletos | Crear y acomodar los huesos. |
| [Modo Pose](esqueletos.md#modo-pose) | Esqueletos | Posar y animar los huesos. |
| [Pintura de pesos](pintura.md#pintura-de-pesos) | Solo mallas 3D | Pintar cuánto mueve cada hueso a cada vértice. |
| [Pintura de vértices](pintura.md#pintura-de-vértices) | Solo mallas 3D | Pintar colores directamente sobre los vértices. |
| [Pintura de textura](pintura.md#pintura-de-textura) | Solo mallas 3D | Pintar sobre la textura, directamente encima del objeto. |

## Barra superior de la Vista 3D

| Botón | Descripción |
|---|---|
| Selector de modo | Cambia de modo (Objeto, Edición, Pose, Pintura…). |
| Vértice / Arista / Cara | Solo en Modo Edición: qué se selecciona. |
| Punto de pivote | Alrededor de qué se rota y escala: Cursor 3D, Orígenes individuales, Punto medio o Elemento activo. Con la casilla Bloquear normales. |
| Orientación | Los ejes del transform: Global, Local, Normal (la dirección de la cara) o Vista. |
| Imán (Snap) | Opciones de imán. Se pone verde si está activo (<kbd>Shift+Tab</kbd>). |
| Edición proporcional | Al mover algo, lo que está cerca lo acompaña con suavidad. Se pone verde si está activa (<kbd>O</kbd>). |
| Vista | Cámaras, puntos de vista, encuadrar, vista local, perspectiva, bloquear órbita. |
| Seleccionar | Todo, ninguno, invertir, caja, vinculado, loops y caminos. |
| Añadir (+) | Solo en Modo Objeto: crear objetos nuevos. |
| Malla | Solo en Modo Edición: las operaciones de modelado. |
| Guardar textura (diskette) | Solo en Pintura de textura. Verde si hay pintura sin guardar. |
| Animación (rombo) | Insertar, borrar y limpiar keyframes; rastro de movimiento. |
| Contexto | El menú del modo: Objeto, Vértice/Arista/Cara, Armature o Pose. |
| Superposiciones (ojo) | Qué ayudas se dibujan: piso, ejes, gizmo, cursor 3D, normales, estadísticas, rayos X. |
| Render (cámara) | El modo de sombreado. |
| Escena | Elegir qué escena, juego o prefab del proyecto se edita, y crear nuevos. |
| UV | Solo en Modo Edición: desplegar y proyectar UVs. |

## Menús comunes a varios modos

### Seleccionar

| Función | Descripción | Atajo |
|---|---|---|
| Todo | Selecciona todo. | <kbd>A</kbd> |
| Ninguno | Deselecciona todo. | <kbd>Alt+A</kbd> |
| Invertir | Lo seleccionado se deselecciona y al revés. | <kbd>Ctrl+I</kbd> |
| Selección por caja | Arrastrar de izquierda a derecha (verde) toma solo lo que queda entero adentro; de derecha a izquierda (azul) alcanza con rozar. <kbd>Shift</kbd> suma. | <kbd>B</kbd> |
| Ver selección | Muestra u oculta el contorno de lo seleccionado. | Menú |

### Render (modos de sombreado)

| Función | Descripción |
|---|---|
| Vista Render | Con las luces de la escena. Si el trazado de rayos está activo, muestra la imagen trazada de a poco. |
| Vista Material | La de fábrica: una luz neutra desde la vista, sin depender de las luces de la escena. |
| Vista Sólido / Alámbrica / ZBuffer | Gris sólido, solo las aristas, o la profundidad. |
| Vista Normal / Vista Alpha | Las normales en color, o la transparencia en blanco y negro. |
| Vectores de movimiento | Cuánto se movió cada píxel en la pantalla desde el cuadro anterior (rojo = X, verde = Y; lavanda = quieto, verde oliva = el fondo). La misma imagen que el pase `_vectores` del render. |
| Alambre: solo lo visible | La vista alámbrica muestra solo lo que se dibuja de verdad. |

### Superposiciones

| Función | Descripción | Atajo |
|---|---|---|
| Mostrar superposiciones | Prende o apaga todas las ayudas visuales de una vez. | <kbd>K</kbd> |
| Piso, Eje X, Eje Y, Orígenes, Cursor 3D, Líneas de relación | Cada ayuda por separado. | Menú |
| Gizmo | El gizmo de mover/rotar/escalar. | Menú |
| Objetos | Mostrar u ocultar esqueletos, luces, cámaras, vacíos, partículas, curvas, hitboxes. | Menú |
| Normales | Dibuja las normales de vértice, personalizadas o de cara, con su tamaño. | Menú |
| Estadísticas | FPS, vértices, caras, llamadas de dibujo y tiempos. | Menú |
| Rayos X | La malla en edición se ve semitransparente y se pueden elegir los elementos de atrás. | Menú |

### Imán (Snap)

| Función | Descripción | Atajo |
|---|---|---|
| Activar imán | Lo que se mueve se pega a vértices, aristas, caras o sus centros. | <kbd>Shift+Tab</kbd> |
| Base y destino de snap | Qué punto de lo movido se pega (más cercano, centro, mediana, activo) y a qué. | Menú |
| Proyectar en cara / Cara más cercana | Para retopología: los vértices se apoyan sobre otra superficie. | Menú |
| Afectar mover / rotar / escalar | En qué operaciones actúa el imán. | Menú |
| Menú Snap del cursor | Selección a la cuadrícula, al cursor o al activo; Cursor a lo seleccionado, al origen, a la cuadrícula o al activo. | <kbd>Shift+S</kbd> |

### Edición proporcional

| Función | Descripción | Atajo |
|---|---|---|
| Activar | Al mover, rotar o escalar, los vecinos acompañan según la distancia. | <kbd>O</kbd> |
| Solo conectados | Mide la distancia por las aristas: no arrastra piezas sueltas. | Menú |
| Caída | Forma de la influencia: suave, esfera, raíz, cuadrado inverso, aguda, lineal, constante, aleatoria. | Menú |
| Tamaño proporcional | El radio de influencia. Durante el transform se cambia con la rueda o con <kbd>RePág</kbd> / <kbd>AvPág</kbd>. | Menú / rueda |

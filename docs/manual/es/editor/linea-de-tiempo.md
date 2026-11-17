# Línea de tiempo

Muestra y edita la animación: hoja de keyframes (dope sheet) o editor de curvas. Muestra lo animado de lo **seleccionado**.

## Barra superior

| Función | Descripción |
|---|---|
| Transporte | Ir al inicio, keyframe anterior, fotograma anterior, reproducir hacia atrás, reproducir / pausa, fotograma siguiente, keyframe siguiente, ir al final. |
| Inicio / Fin / FPS | Rango y velocidad de la animación (click para tipear). |
| Animación | Qué animación se edita (escena, clips, esqueletos, objetos, flipbooks, Juego, Mix) y crear nuevas. La que se está editando sale en **verde**, y una lista larga (un esqueleto con cientos de clips) se abre mostrándola. Se guarda con el proyecto: al abrirlo, la línea de tiempo vuelve a esa animación. |
| Auto Key | En rojo: cada transform graba keyframes solo. |
| Curvas / Dope Sheet | Cambia entre la hoja de keyframes y el editor de curvas. |
| Seleccionar, Key, Vista, Pivote | Menús de selección, operaciones sobre keyframes, encuadre y panel lateral, y el pivote de la <kbd>S</kbd>, la <kbd>R</kbd> y el gizmo: centro, punto medio (el promedio de los keyframes elegidos), frame actual o keyframe activo. |

## Navegar el tiempo

| Función | Descripción | Atajo |
|---|---|---|
| Cambiar de frame | Click o arrastre sobre los números de frame (scrub), o arrastrando el botón verde del frame actual. Un click en el botón verde, sin arrastrar, permite tipear el frame. | Click |
| Fotograma anterior / siguiente | Un frame para atrás o adelante. | <kbd>Izquierda</kbd> / <kbd>Derecha</kbd> |
| Keyframe anterior / siguiente | Salta al keyframe más cercano. | <kbd>Abajo</kbd> / <kbd>Arriba</kbd> |
| Ir al inicio / al final | Primer o último frame. | <kbd>Inicio</kbd> / <kbd>Fin</kbd> |
| Reproducir / Pausa | Desde cualquier panel. | <kbd>Espacio</kbd> |
| Zoom y desplazar | Rueda = zoom (en curvas, <kbd>Ctrl</kbd> = solo tiempo, <kbd>Shift</kbd> = solo valor); botón del medio = desplazar. | Rueda / botón del medio |
| Panel de la izquierda con el dedo | Un dedo lo desplaza para arriba y abajo (las filas) y para los costados (los nombres largos). | Un dedo |
| Zoom y desplazar con el dedo | Dos dedos desplazan para los costados y, en curvas, para arriba y abajo. Abrir o cerrar los dedos hace zoom alrededor de los dedos; en curvas, abrirlos en horizontal estira el tiempo, en vertical el valor, y en diagonal los dos. | Dos dedos |
| Encuadrar selección | Ajusta la vista a lo seleccionado. | <kbd>Num.</kbd> |

## Editar keyframes (dope sheet y curvas)

| Función | Descripción | Atajo |
|---|---|---|
| Seleccionar | Click en un rombo, o en una fila para tomar todos sus keyframes; <kbd>Shift</kbd> suma. | Click; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Mover keyframes | Corre los keyframes en el tiempo (también arrastrando un rombo). | <kbd>G</kbd> |
| Escalar keyframes | Estira o comprime desde el pivote. Las manijas se escalan con los keyframes: escalar en el valor (Y) hasta 0 deja la curva chata, manijas incluidas. | <kbd>S</kbd> |
| Rotar manijas | Solo en curvas. | <kbd>R</kbd> |
| Editar una manija | En curvas, las manijas de los keyframes elegidos se agarran y se arrastran. Le ganan a todo lo demás: si una manija queda debajo del gizmo, se agarra la manija. Con el dedo, arrastrar sobre una manija la edita (aunque el monitor esté prendido). | Arrastrar la manija |
| Mostrar manijas | En curvas, el menú Vista las muestra u oculta: ocultas no se ven ni se agarran. | Menú Vista |
| Gizmo | Aparece sobre el pivote de los keyframes elegidos. Mover: la flecha roja corre en el tiempo, la verde en el valor y el círculo del centro, libre. Rotar: el anillo azul (solo en curvas). Escalar: puntas cuadradas por eje y el círculo del centro, parejo. En el dope sheet solo hay eje de tiempo. Un toque sin arrastrar pasa al modo siguiente; soltar confirma. Se prende y apaga con el gizmo de la Vista 3D (Superposiciones → Gizmo). | Arrastrar el gizmo |
| Duplicar | Copia los keyframes y los deja agarrados. | <kbd>Shift+D</kbd> |
| Menú de los keyframes | Deshacer, Rehacer, Cortar, Copiar, Pegar, Duplicar y Borrar. | Click derecho; con el dedo, pulsación larga |
| Borrar | Borra los keyframes (o las curvas de las filas) seleccionados. | <kbd>X</kbd> |
| Interpolación | Constante, lineal o Bezier. | <kbd>T</kbd> |
| Tipo de manija | Libre, alineada, vector, automática, auto limitada (solo curvas). | <kbd>V</kbd> |
| Euler inteligente | Quita las vueltas de más en las rotaciones seleccionadas. | Menú Key |
| Ocultar una curva | El ojo de cada fila la oculta. | Click en el ojo |
| Valores exactos | Frame, valor y manijas de los keyframes elegidos, y la interpolación y las manijas de las curvas elegidas. Con varios elegidos, lo que no coincide se ve con un guion y lo que se escribe va a todos. | Propiedades → Animación → Curva / Keyframe |
| Modificadores de curva | Ciclos (repetir la curva antes y después de sus keyframes), ruido, escalonado y suavizado sobre la curva activa. Ver [Modificadores de curva](modificadores-de-curva.md). | Propiedades → Modificadores |

## Ajustar la última operación

Después de mover, rotar o escalar keyframes (con el teclado, el gizmo o arrastrando), abajo a la izquierda aparece una tarjeta plegada con lo que se hizo. Desplegada se puede cambiar el valor y la operación se rehace desde el principio (queda un solo paso de deshacer):

| Operación | Valores |
|---|---|
| Mover | Frames y, en curvas, el valor. |
| Rotar | El ángulo (solo en curvas). |
| Escalar | X (el tiempo) y, en curvas, Y (el valor). |

Abajo dice con qué pivote se hizo. **Cancelar** deshace la operación; **Aceptar**, un click o un toque afuera o <kbd>Esc</kbd> la dejan como está (el toque de afuera además le llega a lo que tocaste). Mientras arrastrás un valor, la tarjeta se vuelve translúcida y solo queda opaca la fila que estás editando.

## Barra de herramientas

| Botón | Qué hace |
|---|---|
| Deshacer / Rehacer | Las flechas curvas: siempre están (grises si no hay nada que deshacer o rehacer). |
| Shift | Con el dedo: prendido, cada toque **suma** a la selección en vez de reemplazarla (como tener apretada la tecla Shift). |
| Selección / Monitor | Con el dedo, qué hace arrastrar un dedo sobre las curvas o los rombos. **Monitor** (el de fábrica): mueve la vista. **Selección**: arma la caja de selección y agarra keyframes, como con el mouse. Uno o el otro. |
| Interpolación, manijas y easing | Aparecen con keyframes elegidos: cambian la interpolación, el tipo de manija y el easing de lo elegido. |

## Sonidos

| Función | Descripción | Atajo |
|---|---|---|
| Tira del sonido | Cada objeto Sonido seleccionado tiene su tira: dónde arranca y cuánto dura, en **segundos** (cambiar los FPS no la corre). Con bucle se repite hasta el final. | — |
| Mover | Arrastrar la tira, o <kbd>G</kbd> sin keyframes elegidos (corre todos los sonidos seleccionados). <kbd>Ctrl</kbd> engancha a los frames. | Arrastre / <kbd>G</kbd> |
| Recortar | Agarrar una punta de la tira. | Arrastrar una punta |
| Forma de onda / Línea de volumen | Menú Vista: la forma de onda y la curva del volumen adentro de la tira (la tira ocupa tres filas). Sin eso es solo una tira. | Menú Vista |

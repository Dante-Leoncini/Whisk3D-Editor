# Línea de tiempo

Muestra y edita la animación: hoja de keyframes (dope sheet) o editor de curvas. Muestra lo animado de lo **seleccionado**.

## Barra superior

| Función | Descripción |
|---|---|
| Transporte | Ir al inicio, keyframe anterior, fotograma anterior, reproducir hacia atrás, reproducir / pausa, fotograma siguiente, keyframe siguiente, ir al final. |
| Inicio / Fin / FPS | Rango y velocidad de la animación (click para tipear). |
| Animación | Qué animación se edita (escena, clips, esqueletos, objetos, flipbooks, Juego, Mix) y crear nuevas. |
| Auto Key | En rojo: cada transform graba keyframes solo. |
| Curvas / Dope Sheet | Cambia entre la hoja de keyframes y el editor de curvas. |
| Seleccionar, Key, Vista, Pivote | Menús de selección, operaciones sobre keyframes, encuadre y panel lateral, y desde dónde escala la <kbd>S</kbd> (centro o frame actual). |

## Navegar el tiempo

| Función | Descripción | Atajo |
|---|---|---|
| Cambiar de frame | Click o arrastre sobre los números de frame (scrub). El frame actual (verde) se puede tipear. | Click |
| Fotograma anterior / siguiente | Un frame para atrás o adelante. | <kbd>Izquierda</kbd> / <kbd>Derecha</kbd> |
| Keyframe anterior / siguiente | Salta al keyframe más cercano. | <kbd>Abajo</kbd> / <kbd>Arriba</kbd> |
| Ir al inicio / al final | Primer o último frame. | <kbd>Inicio</kbd> / <kbd>Fin</kbd> |
| Reproducir / Pausa | Desde cualquier panel. | <kbd>Espacio</kbd> |
| Zoom y desplazar | Rueda = zoom (en curvas, <kbd>Ctrl</kbd> = solo tiempo, <kbd>Shift</kbd> = solo valor); botón del medio = desplazar. | Rueda / botón del medio |
| Encuadrar selección | Ajusta la vista a lo seleccionado. | <kbd>Num.</kbd> |

## Editar keyframes (dope sheet y curvas)

| Función | Descripción | Atajo |
|---|---|---|
| Seleccionar | Click en un rombo, o en una fila para tomar todos sus keyframes; <kbd>Shift</kbd> suma. | Click; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Mover keyframes | Corre los keyframes en el tiempo (también arrastrando un rombo). | <kbd>G</kbd> |
| Escalar keyframes | Estira o comprime desde el pivote. | <kbd>S</kbd> |
| Rotar manijas | Solo en curvas. | <kbd>R</kbd> |
| Duplicar | Copia los keyframes y los deja agarrados. | <kbd>Shift+D</kbd> |
| Borrar | Borra los keyframes (o las curvas de las filas) seleccionados. | <kbd>X</kbd> |
| Interpolación | Constante, lineal o Bezier. | <kbd>T</kbd> |
| Tipo de manija | Libre, alineada, vector, automática, auto limitada (solo curvas). | <kbd>V</kbd> |
| Euler inteligente | Quita las vueltas de más en las rotaciones seleccionadas. | Menú Key |
| Ocultar una curva | El ojo de cada fila la oculta. | Click en el ojo |
| Valores exactos | Frame, valor y manijas del keyframe elegido. | Propiedades → Animación → Keyframe |

## Sonidos

| Función | Descripción | Atajo |
|---|---|---|
| Tira del sonido | Cada objeto Sonido seleccionado tiene su tira: dónde arranca y cuánto dura, en **segundos** (cambiar los FPS no la corre). Con bucle se repite hasta el final. | — |
| Mover | Arrastrar la tira, o <kbd>G</kbd> sin keyframes elegidos (corre todos los sonidos seleccionados). <kbd>Ctrl</kbd> engancha a los frames. | Arrastre / <kbd>G</kbd> |
| Recortar | Agarrar una punta de la tira. | Arrastrar una punta |
| Forma de onda / Línea de volumen | Menú Vista: la forma de onda y la curva del volumen adentro de la tira (la tira ocupa tres filas). Sin eso es solo una tira. | Menú Vista |

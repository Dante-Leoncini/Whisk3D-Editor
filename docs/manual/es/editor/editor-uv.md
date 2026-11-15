# Editor UV

Muestra cómo se apoya la textura sobre la malla. Para editar, la malla tiene que estar en **Modo Edición en la Vista 3D**; si no, el editor solo muestra la textura. El modo del Editor UV es propio: no cambia el modo de la Vista 3D. Las herramientas para desplegar y proyectar UVs están en la Vista 3D (Modo Edición, tecla <kbd>U</kbd>).

## Navegación y barra superior

| Función | Descripción | Cómo se usa |
|---|---|---|
| Zoom / Desplazar | Acerca hacia el puntero / mueve la vista. Con dos dedos, el zoom es alrededor de los dedos. | Rueda / botón del medio o flechas; táctil: 1 dedo desplaza, 2 dedos zoom |
| Modo | Modo Objeto, Modo Edición, Pintura de pesos, Editar huesos y Modo Pose (estos dos si la malla tiene esqueleto 2D). | Botón de modo, <kbd>Tab</kbd> |
| Vértice / Arista / Cara | Qué se selecciona en Modo Edición. | Botón |
| Pivote | Cursor, orígenes individuales o punto medio. | Botón |
| Cursor | Cursor a la selección, selección al cursor, cursor al centro. | Menú |
| Vista | Sincronizar selección (la selección UV es la misma que la de la malla), Repetir textura (mosaico), Mostrar chrome UV, Encuadrar selección, Encuadrar todo. | Menú; <kbd>Num.</kbd> |
| Textura | Qué textura se muestra: Nueva textura…, Auto (la de la parte activa), o cualquiera del proyecto. | Menú |
| Nueva textura | Crea una imagen (nombre, tamaño, color, transparencia; lisa, grilla UV o grilla de colores), la guarda en el proyecto y la asigna al material. | Textura → Nueva textura… |
| Mipmap | Muestra un nivel de mipmap en particular, para revisarlo. | Botón Mip |
| Editar | Entra y sale de la pintura de textura sobre la imagen. | Botón Editar |
| Guardar (diskette, siempre visible) | Guardar, Guardar como…, Guardar fuera del w3d…, Guardar en el w3d, Exportar mapa UV… | Menú |
| Exportar mapa UV | Guarda un PNG con fondo transparente: caras semitransparentes (más opacas donde se superponen) y líneas opacas. Se elige nombre, carpeta y resolución (de fábrica 512, 256 en el N95, o el tamaño de la textura mostrada). | Guardar → Exportar mapa UV… |
| Animación | Insertar, borrar o limpiar keyframes de la capa UV, o de la pose 2D. | Menú |

## Modo Objeto (UV)

Click elige qué se edita: un esqueleto 2D (si se toca un hueso) o la geometría. <kbd>Tab</kbd> entra a editar lo elegido.

## Modo Edición (UV)

| Función | Descripción | Atajo |
|---|---|---|
| Seleccionar | Vértices, aristas o caras UV. Sin sincronizar, la Vista 3D filtra: solo se ven las caras seleccionadas en 3D. | Click; <kbd>Shift</kbd>+click |
| Todo / Ninguno / Invertir | Sobre lo visible. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Seleccionar vinculado | La isla UV bajo el puntero (<kbd>Shift</kbd> suma). | <kbd>L</kbd> |
| Selección por caja | Rectángulo; <kbd>Shift</kbd> suma. | <kbd>B</kbd> |
| Mover / Rotar / Escalar | Transforma los UV. <kbd>X</kbd> / <kbd>Y</kbd> bloquean un eje; se puede tipear el valor (1.0 = una baldosa). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Gizmo | Aparece sobre el pivote de los UV elegidos (punto medio, o el cursor 2D). Mover: flecha roja = U, verde = V, círculo del centro = libre. Rotar: el anillo azul. Escalar: puntas cuadradas por eje, el círculo del centro escala parejo. Un toque sin arrastrar pasa al modo siguiente; soltar confirma. | Arrastrar el gizmo |
| Ubicar el cursor 2D | Pone el cursor donde se clickea. | Click derecho |
| Insertar keyframe | Guarda la capa UV en la animación de vértices activa. | <kbd>I</kbd> |

## Pintura de pesos (UV)

Pinta los pesos de los huesos 2D sobre el mapa UV, con los mismos controles que en la Vista 3D (modo +/−/=, grupo, marcas, caída, solo lo seleccionado). <kbd>Ctrl</kbd>+click sobre un hueso lo elige para pintar.

## Editar huesos y Modo Pose (esqueleto 2D)

| Función | Descripción | Atajo |
|---|---|---|
| Añadir esqueleto 2D / hueso | Crea el esqueleto 2D de la malla (con su grupo UV) o agrega un hueso. | Menú Añadir |
| Seleccionar | Una punta o el hueso entero; arrastrar lo mueve. | Click; <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>L</kbd> / <kbd>B</kbd> |
| Mover / Rotar / Escalar | En Editar huesos cambia la posición de reposo; en Pose, posa (los UV pesados siguen al hueso). | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Extruir / Duplicar / Borrar | Solo en Editar huesos. | <kbd>E</kbd> / <kbd>Shift+D</kbd> / <kbd>X</kbd> |
| Emparentar / Desemparentar | Mantener offset o conectado / desconectar. | <kbd>Ctrl+P</kbd> / <kbd>Alt+P</kbd> |
| Insertar keyframe | En Pose: menú de canales, o todo con <kbd>Shift+I</kbd>. | <kbd>I</kbd> |

## Pintura de textura (UV)

Con el botón **Editar** se pinta directamente sobre la imagen, con color, caída, radio y fuerza. Se guarda desde el diskette.

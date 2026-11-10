# Mover, rotar y escalar

Funciona igual en Modo Objeto, Modo Edición, Pose y en la edición de huesos.

| Función | Descripción | Atajo |
|---|---|---|
| Mover (trasladar) | Lo seleccionado sigue al puntero. | <kbd>G</kbd> |
| Rotar | Gira alrededor del pivote. Otra <kbd>R</kbd> alterna entre giro libre (trackball) y orbital. | <kbd>R</kbd> |
| Escalar | Agranda o achica desde el pivote. | <kbd>S</kbd> |
| Restringir a un eje | Durante el transform. Apretar otra vez cicla Global → Local → Vista → libre. | <kbd>X</kbd> / <kbd>Y</kbd> / <kbd>Z</kbd> |
| Restringir a un plano | Excluye ese eje (mueve en los otros dos). | <kbd>Shift+X</kbd> / <kbd>Shift+Y</kbd> / <kbd>Shift+Z</kbd> |
| Valor exacto | Tipear un número o una cuenta (`2*3`, `(1+0.5)/2`). <kbd>-</kbd> invierte el signo, <kbd>Retroceso</kbd> borra. | Números |
| Confirmar | Acepta la operación. | Click izquierdo, <kbd>Enter</kbd>, tilde verde |
| Cancelar | Todo vuelve a como estaba. | <kbd>Esc</kbd>, click derecho, cruz roja |
| Pasos finos | Mueve, rota o escala de a poco. | Flechas |

Mientras dura, la barra superior muestra lo que se está haciendo, por ejemplo "Mover X 1.5 en X global". En táctil, tocar esa barra abre un teclado numérico.

## El gizmo

El gizmo aparece sobre el pivote en Modo Objeto y en Modo Edición (se prende y apaga en Superposiciones → Gizmo).

| Función | Descripción |
|---|---|
| Gizmo de mover | Flechas = un eje; cuadrados = un plano; círculo central = libre. |
| Gizmo de rotar | Un semicírculo de color por eje, un anillo blanco para girar de cara a la vista, y el interior para girar libre. |
| Gizmo de escalar | Puntas cuadradas por eje; el círculo central escala parejo. |
| Cambiar de gizmo | Un click con el mouse o un toque con el dedo sobre el gizmo, sin arrastrar, pasa de Mover a Rotar, a Escalar y vuelta a Mover. Funciona igual en PC, navegador y pantallas táctiles. |

## Ajustar la última operación

Después de mover, rotar o escalar (en Modo Objeto o Edición) aparece abajo a la izquierda un panel con lo que se hizo: los valores X, Y, Z (o el ángulo), la orientación y la edición proporcional con sus opciones. Cambiar un valor rehace la operación con el valor nuevo. **Cancelar** la deshace y **Aceptar** la deja. Un click afuera del panel también acepta. Mientras el panel está abierto, la barra de herramientas de la Vista 3D se oculta.

El panel tiene borde verde porque es la ventana con el foco. Si no entra en la mitad del alto de la pantalla, se achica a esa mitad y se desplaza arrastrando el dedo (sin tocar las opciones), con la rueda o con las flechas. Con el teclado o el teléfono: las flechas recorren las filas, <kbd>OK</kbd> / <kbd>Enter</kbd> edita el valor sin cerrar el panel, **Aceptar**, <kbd>Esc</kbd> o <kbd>Supr</kbd> (la <kbd>C</kbd> en el N95) lo cierran dejando el cambio, y solo **Cancelar** lo deshace.

## Barra de herramientas

Tiene Deshacer y Rehacer, las últimas acciones usadas y el botón de edición proporcional. Durante un transform muestra la tilde (aceptar), la cruz (cancelar), la orientación y los ejes X / Y / Z (dos ejes prendidos = un plano). En táctil, después de extruir aparece **Repetir**, que acepta y vuelve a extruir.

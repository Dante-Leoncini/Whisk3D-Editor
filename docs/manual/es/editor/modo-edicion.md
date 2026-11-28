# Modo Edición (mallas)

Se entra con <kbd>Tab</kbd> sobre una malla. Se trabaja sobre **vértices**, **aristas** o **caras**: el botón alargado de la barra superior tiene los tres íconos, el activo en verde, y un click (o un toque; en Symbian las flechas y OK) cambia de uno a otro. Los menús Vértice, Arista y Cara están adentro de **Malla**, en <kbd>W</kbd> y en el click derecho.

## Seleccionar

| Función | Descripción | Atajo |
|---|---|---|
| Seleccionar | Elige un vértice, arista o cara. | Click; <kbd>Shift</kbd>+click alterna |
| Seleccionar vinculado | Toda la isla conectada bajo el puntero, **sumada** a lo que ya estaba elegido. | <kbd>L</kbd> |
| Seleccionar loop | Un anillo de aristas o de caras. Se suma a lo que ya estaba elegido (en los tres modos): varios clicks seguidos juntan varios loops. | <kbd>Shift+Alt</kbd>+click |
| Camino más corto | Selecciona el camino entre el activo y el clickeado. Sin activo, sale del seleccionado más cercano. Lo que se **extruye** queda como activo: extruir un vértice y hacer Ctrl+click arma el camino desde el nuevo. | <kbd>Ctrl</kbd>+click |
| Camino más corto (rellenar región) | Selecciona toda la región entre los dos. | <kbd>Ctrl+Shift</kbd>+click |
| Todo / Ninguno / Invertir / Caja | Como en Modo Objeto, sobre los elementos de la malla. | <kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> / <kbd>B</kbd> |
| Ocultar lo seleccionado / lo no seleccionado | Esconde parte de la malla para trabajar en el resto: no se ve ni se puede elegir. Un vértice oculto se lleva sus aristas y caras. Es solo de la edición: en Modo Objeto la malla se ve entera. <kbd>Ctrl+Z</kbd> lo deshace. También en Malla > Mostrar/Ocultar. | <kbd>H</kbd> / <kbd>Shift+H</kbd> |
| Mostrar lo oculto | Vuelve a mostrar todo lo oculto (y lo deja seleccionado). | <kbd>Alt+H</kbd> |

## Modelar

| Función | Descripción | Atajo |
|---|---|---|
| Mover / Rotar / Escalar | Transforma lo seleccionado. Con un mover en curso, otra <kbd>G</kbd> pasa a deslizar. | <kbd>G</kbd> / <kbd>R</kbd> / <kbd>S</kbd> |
| Encoger / Engordar | Mueve cada vértice por su normal (infla o desinfla). | <kbd>Alt+S</kbd> |
| Extruir | Saca geometría nueva desde lo seleccionado (las caras salen por su normal). La cara que sale de una arista sigue el sentido de la cara de al lado; si la arista está suelta, al soltarla mira hacia la vista. Lo extruido queda como activo. | <kbd>E</kbd> |
| Loop Cut y Deslizar | Corta un anillo de aristas. La rueda cambia la cantidad de cortes; click aplica y después se desliza. | <kbd>Ctrl+R</kbd> |
| Cuchillo | Dibuja un corte sobre la malla, click a click. Pasando el mouse engancha un **vértice** (recuadro verde), una **arista** (se pinta de amarillo: el corte cae justo sobre ella) o queda en la **cara**, también en el medio. Los puntos rojos son los vértices que van a nacer. <kbd>Enter</kbd> o <kbd>Espacio</kbd> corta, <kbd>Esc</kbd> o click derecho cancela, <kbd>Retroceso</kbd> saca el último punto y <kbd>Z</kbd> corta también lo de atrás (lo que no se ve). La cara que el corte cruza de lado a lado se parte; lo que empieza o termina en el medio de una cara queda como arista suelta. Un solo <kbd>Ctrl+Z</kbd> deshace el corte. | <kbd>K</kbd> |
| Nueva arista / cara desde vértices | Une los vértices seleccionados con una arista o una cara. | <kbd>F</kbd> |
| Conectar camino de vértices | Corta en línea recta entre dos vértices. | <kbd>J</kbd> |
| Deslizar vértices | Desliza los vértices a lo largo de sus aristas. | <kbd>Shift+V</kbd> |
| Rasgar | Separa la malla por la selección. | <kbd>V</kbd> |
| Duplicar | Copia lo seleccionado y lo deja agarrado. | <kbd>Shift+D</kbd> |
| Separar | Pasa las caras seleccionadas a un objeto nuevo. | <kbd>P</kbd> |
| Fusionar | Suelda vértices: en el centro, en el cursor, colapsar o por distancia. También en el menú de la <kbd>W</kbd> (vértice, arista o cara). **Por distancia** avisa abajo cuántos vértices fusionó (0 = no había nada encimado). | <kbd>M</kbd> |
| Auto fusionar | Al terminar de mover, suelda los vértices que quedaron más cerca que el umbral. | Menú Malla |
| Borrar | Vértices, aristas, caras o loops de aristas. **Aristas** también borra las aristas sueltas elegidas. | <kbd>X</kbd> o <kbd>Supr</kbd> |
| Recalcular normales / Voltear | Corrige hacia dónde miran las caras, o las da vuelta. | <kbd>Shift+N</kbd> |
| Normales de los vértices | Editando, solo se recalculan las normales de las caras que tocan lo que se edita (lo elegido y lo nuevo): el resto queda como estaba (las normales importadas, suaves y con sus filos, no se rehacen). Borrar, disolver y fusionar no las tocan. **Malla > Normales > Bloquear normales** no recalcula ni eso (solo lo nuevo recibe una normal); **Recalcular normales de vértices** rehace las de lo elegido. | Menú Malla > Normales |
| Triangular caras | Parte en triángulos las caras de más de 3 lados. | <kbd>Ctrl+T</kbd> |
| Sombreado suave / plano | Cómo se ilumina la superficie. | Menú Cara |
| Marcar / limpiar afilado | Aristas que se ven duras aunque la malla sea suave. | Menú Arista |
| Marcar / limpiar costura | Por dónde se corta la malla al desplegar las UV. | Menú Arista o UV |
| Menú de contexto | Las operaciones del sub-modo actual (vértice, arista o cara) en el puntero. | <kbd>W</kbd> o click derecho |
| Insertar keyframe | Guarda la forma de la malla en el frame actual (animación de vértices). | <kbd>I</kbd> |

## Menú UV

| Función | Descripción | Atajo |
|---|---|---|
| Desplegar | Basado en ángulos, conforme o de mínimo estiramiento: abre la malla por sus costuras. | <kbd>U</kbd> |
| Proyección UV inteligente | Corta y despliega sola según los ángulos entre caras. | <kbd>U</kbd> |
| Empaquetar lightmap / Seguir quads activos | Islas ordenadas sin solaparse / UV ordenadas siguiendo la cara activa. | <kbd>U</kbd> |
| Proyección de cubo, cilindro, esfera, desde la vista | Proyecciones simples. | <kbd>U</kbd> |
| Restablecer | Cada cara ocupa toda la imagen. | <kbd>U</kbd> |

## Edición de curvas (rutas)

Con <kbd>Tab</kbd> sobre una curva se editan sus nodos como si fuera una malla de puntos: <kbd>G</kbd> mueve, <kbd>E</kbd> agrega el nodo siguiente, <kbd>X</kbd> borra.

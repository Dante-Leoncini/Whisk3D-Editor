# Editor de scripts (IDE)

Editor de texto para los scripts Lua y los [shaders escritos](shaders.md) (`.glsl`), con números de línea, colores de sintaxis, guías de sangría y soporte para la ñ y las tildes. Mientras tiene el foco, las letras escriben (no disparan atajos del editor). En Android, tocar el texto abre el teclado. En el N95 se escribe con T9.

| Función | Descripción | Atajo |
|---|---|---|
| Script | Elegir qué .lua del proyecto se edita, o Cargar .lua… (un archivo externo). | Menú Script |
| Guardar | Guarda el script. Si vive dentro del .w3d, guarda también el proyecto. | <kbd>Ctrl+S</kbd> o Archivo → Guardar |
| Guardar como… | Otro nombre, adentro o afuera del proyecto, en una carpeta a elección. | Archivo |
| Guardar en el w3d | Solo si el script es externo: lo mete en el proyecto. | Archivo |
| Guardar fuera del w3d… | Solo si el script es interno: lo saca a un archivo; los objetos pasan a usar ese archivo. | Archivo |
| Refrescar | Trae el archivo tal como está en el disco (lo que se editó con otro editor). Si hay cambios en el editor sin guardar, pregunta antes de descartarlos (y se pueden recuperar con <kbd>Ctrl+Z</kbd>). Con el juego corriendo, recarga en vivo los objetos que usan el script. | Botón |
| Shaders (.glsl) | Se abren desde el material (el botoncito de la fila **Shader**), desde la Biblioteca o desde el selector, y se pintan como GLSL. Al lado del nombre aparece **Compilar**: compila lo que estás escribiendo (si un material lo usa, lo guarda y compila ese material). Guardar un shader lo compila en todos los materiales que lo usan y avisa si compiló o el error con su renglón. | <kbd>Ctrl+S</kbd> |
| Protección al guardar | Si el archivo externo cambió afuera desde que se abrió, Guardar pregunta antes de sobrescribirlo. Guardar también recarga el script en vivo si el juego está corriendo. | Automático |
| Sangría (Tab) | <kbd>Tab</kbd> completa hasta el próximo múltiplo de 4; con varias líneas seleccionadas les suma un nivel y <kbd>Shift+Tab</kbd> lo saca. Las líneas verticales marcan cada nivel; la del bloque del cursor se ve más clara. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Menú Editar | Deshacer, Rehacer, Cortar, Copiar, Pegar, Buscar… y Reemplazar… | Botón Editar |
| Menú Seleccionar | Seleccionar todo. | Botón Seleccionar |
| Deshacer / Rehacer | Sobre el texto del script (no sobre la escena). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> o <kbd>Ctrl+Shift+Z</kbd> |
| Buscar / Reemplazar | Abre un panel arriba a la derecha: el texto a buscar, cuántas coincidencias hay ("3 de 7"), distinguir mayúsculas y minúsculas, palabra completa, Anterior / Siguiente, y en Reemplazar el texto nuevo con Reemplazar y Reemplazar todo (un solo paso de deshacer). Las coincidencias se resaltan en el código. El panel no bloquea el editor: con el puntero sobre el script se sigue escribiendo. Se cierra con Cerrar o <kbd>Esc</kbd>. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copiar / Cortar / Pegar | Con el portapapeles del sistema. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Seleccionar todo / Deshacer | Deshacer afecta el texto, no la escena. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Seleccionar con el dedo | Doble toque selecciona la palabra, triple toque el renglón entero. Con una selección aparecen dos agarraderas (círculos verdes) en sus puntas: se arrastran para ajustarla. | Toques |
| Lupa (táctil) | Dejar el dedo quieto un momento sobre el código muestra arriba del dedo el renglón agrandado con el cursor; arrastrando, el cursor sigue al dedo y queda donde se suelta. | Pulsación larga |
| Seleccionar en el N95 | Las flechas mueven el cursor; manteniendo el lápiz, las flechas seleccionan desde donde está el cursor. Mientras se mantiene el lápiz aparece abajo una barra: la tecla de selección izquierda **copia** y la derecha **pega**. | Lápiz + flechas / softkeys |
| Moverse por el texto | Flechas, Inicio, Fin, RePág, AvPág (con <kbd>Shift</kbd> seleccionan); <kbd>Ctrl+Inicio</kbd> / <kbd>Ctrl+Fin</kbd> van al principio o al final. | Teclado |

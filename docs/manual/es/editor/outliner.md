# Outliner

La lista de todo lo que hay en la escena, y también la biblioteca de recursos del proyecto. En Android no se muestra (se trabaja con la Vista 3D y Propiedades).

## Menú Vista (ojo)

| Función | Descripción |
|---|---|
| Escena / Biblioteca / librerías | Qué se lista: el árbol de la escena, todos los recursos del proyecto, o los de otro .w3d vinculado (solo lectura). |
| Constructor / Rutinas | Muestran u ocultan la rutina Constructor (lo que corre una sola vez al empezar) y las demás rutinas. |
| Íconos de visibilidad / Íconos de render | Muestran u ocultan las columnas del ojo y de la cámara. |
| Cuadrícula | En Biblioteca: miniaturas en vez de lista. |
| Vincular .w3d… / Desvincular | Usa otro proyecto como librería, o lo suelta. |

## Vista Escena

| Función | Descripción | Cómo se usa |
|---|---|---|
| Seleccionar | Elige el objeto. <kbd>Shift</kbd>+click toma un rango; <kbd>Ctrl</kbd>+click suma o quita. | Click |
| Desplegar / plegar | Muestra u oculta los hijos. | Flecha; <kbd>Izquierda</kbd> / <kbd>Derecha</kbd> |
| Visible / Renderizable | El ojo oculta el objeto (es animable); la cámara lo excluye del render. | Click en el ojo / la cámara |
| Reordenar y emparentar | Soltar antes, después o encima de otra fila (como hijo). | Arrastrar la fila |
| Mover (reordenar) | Con el teclado: arriba/abajo reordena, izquierda saca del padre, derecha mete bajo el anterior, <kbd>Enter</kbd> confirma. | <kbd>G</kbd> |
| Renombrar | La fila se vuelve un campo de texto. | Doble click o <kbd>F2</kbd> |
| Duplicar / Duplicar vinculado | Como en la Vista 3D. | <kbd>Shift+D</kbd> / <kbd>Alt+D</kbd> |
| Ocultar / Mostrar | Alterna la visibilidad. | <kbd>H</kbd> |
| Borrar | Con confirmación. | <kbd>X</kbd> o <kbd>Supr</kbd> |
| Crear / Desempaquetar prefab | Desde el menú Objeto. | Click derecho |
| Mostrar el activo | Desplaza la lista hasta el objeto activo. | <kbd>Num.</kbd> |
| Desplazar la lista | Rueda, o arrastrar (también con el dedo). | Rueda / arrastrar |

## Vista Biblioteca

| Función | Descripción |
|---|---|
| Recursos | Mallas, materiales, texturas, animaciones, prefabs, escenas, sonidos, scripts, [shaders](shaders.md), fuentes y videos, en carpetas. Un material o un shader arrastrado sobre una malla del 3D se ve en la parte bajo el puntero antes de soltarlo. Tilde verde = en uso; cruz roja = huérfano; `*` = sin guardar. |
| Filtro / Lista o cuadrícula | Mostrar un solo tipo; ver miniaturas. |
| Arrastrar un recurso | A una carpeta (lo mueve), a la Vista 3D (malla = objeto nuevo, material o textura = se asigna a lo tocado, prefab = instancia) o a un desplegable de Propiedades. |
| Carpeta nueva / Mover a carpeta | Organizar (las carpetas son solo para ordenar). |
| Duplicar recurso / Seleccionar usuarios / Purgar huérfanos | Copiar, encontrar quién lo usa, borrar lo que nadie usa. |
| Dentro del .w3d / Externo | Desde Propiedades (tarjeta Recurso): mueve el recurso adentro o afuera del proyecto. |

## Escenas, juegos y prefabs

El botón con el nombre de la escena (en el Outliner y en la Vista 3D) lista las escenas, juegos y prefabs del proyecto. Desde ahí se crean nuevos, se elige la escena inicial y se convierte una escena en juego (o al revés).

# El proyecto (.w3d) y los archivos

| Función | Descripción | Dónde |
|---|---|---|
| Pantalla de inicio | Al abrir el editor sin un archivo aparece una tarjeta al medio, con el editor oscurecido detrás: el logo (el engranaje gira) con el nombre a la derecha, la página y la versión, **Nuevo archivo**, **Abrir...** y los últimos 5 **Archivos recientes** (un toque abre uno; si todavía no hay ninguno, no aparecen). Un toque afuera o <kbd>Esc</kbd> la cierran. | Al arrancar |
| Nuevo proyecto | Deja la escena con la que arranca el editor, sin archivo: un cubo de 2 m apoyado en un piso de 8 × 8 m, con su cámara y su luz, y la vista mirándolo. Si hay algo sin guardar, primero pregunta (guardar, seguir sin guardar o cancelar). | Tarjeta Archivo, pantalla de inicio |
| Abrir proyecto | Abre un .w3d (también con doble click desde el sistema). | Propiedades → Archivo/Render → Archivo |
| Guardar / Guardar como | Guarda en la carpeta y con el nombre de la tarjeta; Guardar como pide otro. Si falta el nombre, el campo se pone rojo. | Tarjeta Archivo, <kbd>Ctrl+S</kbd> |
| Guardar versión | Guarda y además deja una copia numerada (`proyecto_v03.w3d`). | Tarjeta Archivo |
| Guardar como (un recurso) | Una copia del archivo de un recurso (una textura, un sonido, un script...) donde elijas y con el nombre que quieras. La copia no cambia el proyecto: el recurso sigue donde estaba. | Tarjeta Recurso (elegirlo en la Biblioteca) |
| Extraer / Importar | Lleva el archivo de un recurso afuera del .w3d (Extraer: el proyecto pasa a usar ese archivo) o adentro (Importar: eligiendo la carpeta de la Biblioteca y el nombre). | Tarjeta Recurso, desplegable **Ubicación** |
| Malla en texto | Guarda esa malla como texto legible (.w3dm) en vez de binario (.w3db). Es de cada malla. | Tarjeta Recurso de la malla |
| Importar | OBJ, FBX, glTF, GLB e interfaces .w3dui. La malla importada viaja adentro del proyecto. Una textura, un sonido, un script o una fuente que se carga de un archivo queda **afuera**: el proyecto guarda su ruta (para meterlo adentro: la Biblioteca). | Vista 3D → Añadir → Importar |
| Exportar | OBJ, glTF o GLB del objeto seleccionado (con o sin modificadores y transformaciones). | Propiedades → Exportar |
| Cambios sin guardar | Lo editado y no guardado (scripts, texturas pintadas) se marca con `*`, también en el título de la ventana. Al cerrar, abrir otro proyecto o empezar uno nuevo con algo sin guardar aparece el cartel **Se perderán los cambios en:** con la lista, para guardar, seguir sin guardar o cancelar. | Automático |
| Cerrar en Android | El botón (o el gesto) **Atrás** cierra lo que esté abierto (un menú, un cartel, la pantalla de inicio). Si no hay nada abierto, es cerrar Whisk3D: con algo sin guardar aparece el cartel **Se perderán los cambios en:**; si no, pregunta **¿Cerrar Whisk3D?**. Cerrarlo desde el administrador de aplicaciones de Android no pregunta: lo no guardado se pierde. | Atrás |
| Recurso adentro o afuera | Un recurso externo no se copia: se guarda su ruta. Si al guardar su archivo no está, el aviso dice **cuál** falta (y está en Biblioteca > Archivos que faltan). El archivo original de un modelo importado (el .glb o el .obj de donde vino) no cuenta: la malla ya viaja adentro, así que que falte no es un error. | Tarjeta Recurso; menú de la Biblioteca; "Guardar en / fuera del w3d" en IDE y Editor UV |
| Dónde estabas | El proyecto guarda el cuadro, la selección y **qué animación estaba en la línea de tiempo** (un clip de un esqueleto, la animación de una malla, un flipbook...). Al abrirlo seguís ahí. | Automático |

El guardado nunca deja el archivo a medias: primero escribe una copia temporal y después la renombra.

## En el navegador (versión web)

El navegador no deja que una página lea ni escriba los archivos de la PC por su cuenta. Por eso, en la versión web:

- **Abrir proyecto**, **Importar** y elegir una textura abren el explorador de archivos del navegador: aparece un cartel con el botón **Elegir archivo**. Para un OBJ se eligen juntos el .obj, su .mtl y sus texturas; para un glTF, el .gltf y su .bin.
- **Guardar** (<kbd>Ctrl+S</kbd>), **Guardar como**, **Guardar versión**, **Extraer** y el **Guardar como** de un recurso descargan el archivo a la carpeta de descargas del navegador. Lo mismo pasa con **Exportar** (el OBJ baja con su .mtl y sus texturas al lado), el render, el mapa UV, las texturas pintadas guardadas afuera y los scripts externos.
- Cuando el editor pide una carpeta para guardar, no se abre ningún explorador: el archivo se descarga directamente.
- Si el navegador pregunta si la página puede descargar varios archivos, hay que aceptar. Si no, solo baja el primero.
- Lo abierto vive en la memoria de la página: al recargarla se pierde todo lo que no se descargó.

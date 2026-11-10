# Qué es Whisk3D

Whisk3D es un **editor 3D y motor de juegos** libre, hecho desde cero. La idea es poder crear lo que quieras con lo que tengas: funciona en una PC, en un teléfono y también en consolas y equipos viejos, sin depender de hardware moderno ni de software privativo.

## Qué se puede hacer

Con el **Whisk3D Editor** se puede:

- **Modelar** en 3D: mallas con vértices, aristas y caras, y [modificadores](objeto-malla.md#la-pestaña-modificadores) que trabajan sin destruir la malla.
- Hacerles un **rig**: armar el [esqueleto](esqueletos.md) de un personaje y pintar cuánto mueve cada hueso.
- **Animar**: keyframes, el editor de curvas y animaciones por esqueleto ([Línea de tiempo](linea-de-tiempo.md)).
- **Iluminar** la escena con luces puntuales, de sol y focos.
- **Renderizar**, en tiempo real o con **trazado de rayos** (sombras suaves, reflejos y transparencias), imágenes y videos.
- **Pintar** los colores de los vértices y las texturas, directamente sobre el modelo ([Modos de pintura](pintura.md)).
- **Crear juegos**: la lógica se escribe en [Lua](../lua/README.md), con el [editor de scripts](editor-de-scripts.md) integrado; las interfaces (menús, HUD) se arman en el [Editor 2D](editor-2d.md), y el juego se compila desde el mismo editor para cada sistema.
- Importar y exportar modelos en OBJ, FBX, glTF, GLB y STL.

Whisk3D tiene además su **propio formato libre** para guardar los proyectos y los juegos: el **`.w3d`**. Un juego en `.w3d` se puede abrir directamente en cualquier sistema donde funcione Whisk3D, o **compilar** para un sistema específico, para aprovechar al máximo su hardware. Ver el artículo [W3D: un formato libre para que los videojuegos no mueran](https://leoncini.com.ar/articulo.php?id=w3d-formato-abierto&lang=es).

Los juegos también se pueden hacer **sin el editor**, programando directamente sobre **Whisk3D Core** en C y C++. Ese camino es solo para usuarios con experiencia, que ya saben programar en C y C++: ver el [Manual de Whisk3D Core](../core/empezar.md).

## Dónde funciona

| Sistema | |
|---|---|
| **PC** | Windows, Linux y macOS |
| **Teléfonos** | Android (desde la 2.3) y los Nokia N95 y N8 (Symbian) |
| **Navegador** | Cualquier navegador moderno (WebGL) |
| **Consolas** | Sega Dreamcast, Sony PSP, PlayStation Vita y Nintendo 3DS |

## Cómo está armado

Whisk3D son tres proyectos, uno encima del otro, como tres engranajes que se mueven juntos:

- **Whisk3D Editor** (el engranaje verde): el editor, con todo lo de arriba.
- **[Whisk3D UI](https://leoncini.com.ar/proyecto.php?id=whisk3d-ui&lang=es)** (el amarillo): se encarga de la **interfaz**: los botones, los menús, las tarjetas de propiedades, el texto y los íconos. El editor está hecho con ella. Ver el [Manual de Whisk3D UI](../ui/README.md).
- **[Whisk3D Core](https://leoncini.com.ar/proyecto.php?id=whisk3d-core&lang=es)** (el rojo): la **capa de abstracción**. Whisk3D UI la usa para todo lo que hace.

Un juego o un proyecto hecho en el editor vive **adentro** del editor, con su lógica escrita en Lua. Pero el Editor y Whisk3D UI son **opcionales**: se pueden usar herramientas propias. Un juego en C o C++ puede usar Whisk3D UI para su interfaz, sin el editor, o hablarle **directamente al Core**, con su propia interfaz y sus propias herramientas. Lo único que no puede faltar es el Core.

![La arquitectura de Whisk3D en tres capas: arriba el Editor (verde), con tu juego o proyecto en Lua adentro; en el medio Whisk3D UI (amarillo); abajo Whisk3D Core (rojo). El Editor y la UI son opcionales: desde afuera, un juego en C++ puede usar la UI y otro el Core directamente. El Core habla con cada sistema por su propio backend](img/whisk3d-capas.webp)

El Core es el **ingrediente secreto**. Cada sistema dibuja, suena, lee archivos y recibe las teclas a su manera: una PC usa OpenGL, la Dreamcast tiene su propio chip gráfico, la PSP y la 3DS sus propias bibliotecas, el navegador WebGL. Ni el editor ni los juegos le hablan a cada sistema: le piden las cosas al Core, siempre de la misma forma ("dibujá esta malla", "reproducí este sonido", "abrí este archivo"), y el Core sabe cómo hacerlo en cada uno. Por eso el **mismo código** funciona en máquinas tan distintas, y sumar un sistema nuevo es enseñarle al Core a hablar con él, no reescribir el editor.

Cómo lo hace, con todo detalle (los gráficos, el sonido, los archivos, la entrada, la red, los logs): [Manual de Whisk3D Core](../core/README.md).

## El proyecto

Todo el trabajo se guarda en un archivo de proyecto `.w3d`. Adentro viajan la escena y también los recursos: texturas (`texturas/`), scripts (`scripts/`), sonidos, escenas, prefabs. Un recurso puede vivir **dentro** del .w3d o **fuera**, como un archivo suelto en el disco; los menús "Guardar en el w3d" y "Guardar fuera del w3d" lo mueven de un lado al otro. Ver [El proyecto](proyecto.md).

> [!NOTE]
> Este manual describe lo que Whisk3D sabe hacer hoy. Los nombres de las funciones son los que se ven en el editor en español. Las teclas indicadas son las de fábrica: se pueden cambiar en [Atajos de teclado](atajos.md).

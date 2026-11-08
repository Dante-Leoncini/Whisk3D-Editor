# Whisk3D en Symbian (Nokia N95 y S60)

Dos variantes, desde el mismo proyecto:

- **OpenGL ES 1.1** (pipeline fijo): el N95 y todo S60 3rd/5th. El objetivo insignia del proyecto: correr en un teléfono de 2007.
- **OpenGL ES 2.0** (shaders): el N8, C7, E7 y todo Symbian^3/Anna/Belle. Usa el mismo backend de shaders que Android y la web (`libs/Whisk3DCore/graphics/gles2/w3dGraphicsGLES2.cpp`).

Las dos tienen la misma UID: instalar una reemplaza a la otra.

## Instalar (usuario final)

La última versión está siempre en la carpeta `sis` como `Whisk3D_gcce.sisx` (puede no funcionar del todo; está en desarrollo constante). La versión **verificada** está en `releases`, con la fecha en el nombre, referenciando los videos/imágenes de demostración de [Instagram](https://www.instagram.com/dante_leoncini).

> Ojo: algunas features van y vienen entre versiones. Por ejemplo el modelado 3D se sacó en las últimas; si querés esa función, usá una versión anterior como `Whisk3D_gcce_beta_24-08-15_FERNET.sisx`.

Si el instalador falla, probablemente tengas que "hackear" el teléfono para ignorar los certificados de Symbian: [Tutorial Hack](https://www.youtube.com/watch?v=UJJICzbk3TA).

## Compilar

El proyecto de Symbian está en esta carpeta:

- `group/Whisk3D.mmp` — el proyecto ES 1.1 (fuentes, includes, libs). Comparte casi todo el código con el resto de las plataformas (`main/`, `libs/Whisk3DCore`, `libs/WhiskUI`); lo específico del teléfono está en `src/`.
- `group/Whisk3D_gles2.mmp` — la variante ES 2.0. Define `W3D_MMP_GLES2` e incluye `Whisk3D.mmp`: la lista de fuentes es una sola. Lo que cambia está en los `#ifdef W3D_MMP_GLES2` de `Whisk3D.mmp` (el ejecutable `whisk3D_gles2.exe`, el backend, `libglesv2` + `libegl` + `accmonitor`). En el código, la variante se reconoce por `W3D_GLES2`.
- `sis/Whisk3D_armv5.pkg` (ES 1.1) y `sis/Whisk3D_gles2_armv5.pkg` (ES 2.0, instala `whisk3D_gles2.exe` como `Whisk3D.exe`).
- `src/` — la capa de Symbian: app framework (`Whisk3D*.cpp`), input (teclado/keypad, mouse virtual, HID bluetooth), carga de texturas por ICL, y el reloj/glue.
- `data/`, `sis/` — recursos e instaladores.

Se compila con el SDK de S60 3rd Edition + **Carbide.c++** (o `bldmake` + `abld` a mano). La toolchain es vieja (**C++03**: sin `nullptr`, `auto`, range-for, `vector::data`...) y no todo el subsistema nuevo (FBX, skinning) está portado — mirá los `#ifndef W3D_SYMBIAN` del código.

## Pantalla táctil

Los gestos son los mismos de Android y la web, con el mismo código:

- S60 5th (5800, N97): un dedo. Symbian^3 (N8 y los que siguen; la variante ES 2.0): multitáctil.
- Los dedos los sigue el Core (`libs/Whisk3DCore/input/W3dTactil.cpp`, y `W3dTactilSymbian.cpp` para el `TPointerEvent`). Arma los mismos eventos que SDL en Android: el primer dedo mueve y aprieta el mouse, los demás solo avisan como dedos.
- El editor los convierte en `SDL_Event` (`main/app/W3dTactilEditor.cpp`) y entran por `controles.cpp`, como en Android. Para eso `controles.cpp` compila en Symbian contra `main/app/W3dSdlMinimo.h`.
- Una mejora de los gestos en Android llega sola al teléfono. En la PC se prueba con `tactiltest` (`tools/pruebas/prueba_tactil.w3s`), que pasa por el mismo camino del Core.
- En Symbian los handlers de PC del viewport 3D y de los paneles corren solo con el dedo o el mouse Bluetooth (`W3D_SOLO_POR_CONTROLES`): el keypad y el cursor virtual del N95 siguen como antes.
- En un teléfono táctil la barra de herramientas del 3D sale prendida: con el dedo, el transform se confirma con el tilde.
- Todavía no: el editor UV y el Editor 2D con el dedo (su input sigue apagado en Symbian).

## Pantallas externas (TV-out y HDMI)

Al conectar una pantalla, el editor pregunta qué hacer con ella:

- **Clonar pantalla**: no hace nada; el sistema copia la pantalla del teléfono, como siempre.
- **Usar nuevo viewport**: la pantalla externa muestra un viewport 3D propio, a su resolución. Sigue la vista del 3D del teléfono, porque la TV no tiene entrada.
- **Viewport de propiedades**: la pantalla externa muestra un panel de propiedades propio.

Cómo funciona:

- La lógica es compartida (`main/UI/ViewPorts/PantallasExternas.cpp`). Se prueba en la PC con el comando `pantalla` del harness.
- La parte del teléfono es `src/w3dpantallas.cpp`. Revisa una vez por segundo las pantallas que expone wserv (`RWsSession::NumberOfScreens`). En la variante ES 2.0, además, confirma con el AccMonitor que haya un cable de video (HDMI o compuesto).
- Si se elige un viewport, crea en esa pantalla un window group, una ventana y una superficie EGL que comparte el contexto de la principal.
- El N8 tiene salida RCA de 480i y HDMI de 720p. La variante ES 1.1 dibuja la pantalla externa a 320x240 como máximo, porque el TV-out del N95 muestra 240p.

Todo queda en el log con el prefijo `[pantallas]`: cuántas pantallas hay, sus tamaños, el cable y los errores de EGL.

## Diagnosticar un cierre en el teléfono

La app escribe `E:\whisk3d.log` línea por línea, con flush a disco. Si se cierra, el log llega hasta el último paso que hizo. Tres tipos de líneas sirven para saber por qué se cerró:

- `[cierre] el hilo principal termino con un PANIC: <categoria> <numero>`: lo escribe un hilo vigía (`Whisk3DAppUi.cpp`) cuando muere el hilo principal. Por ejemplo, `KERN-EXEC 3` es un acceso a memoria inválida o la pila desbordada, y `USER 47` es el heap roto. Si sale con `[cierre] ... tipo 0` es una salida normal.
- `[mem] SIN MEMORIA: fallo un pedido de N bytes ...`: el `operator new` (`newdel_compat.cpp`) devuelve NULL cuando no hay memoria. Antes, el código que seguía escribía en NULL y no quedaba rastro.
- `[mem] <hito>: heap X KB en uso (Y KB tomados), RAM libre Z KB`: los hitos de una carga grande (contenedor montado, JSON leído, cada animset y cada prefab, proyecto abierto, texturas del Play). "Tomados" es lo que el chunk del heap ya le sacó al teléfono: los huecos que deja lo liberado no vuelven.

Pila del hilo principal: 80 KB (`EPOCSTACKSIZE 0x14000`, el máximo de EKA2).

Al arrancar, las líneas `[gl]` registran lo que expone el driver: `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, las extensiones de GL y de EGL en tramos, los límites (luces, unidades de textura, `GL_MAX_PALETTE_MATRICES_OES`, `GL_MAX_VERTEX_UNITS_OES`) y si `eglGetProcAddress` encuentra funciones que el driver no anuncia (programas de vértices de `IMG_vertex_program`, la paleta de matrices, multi-draw).

## GPU del N95: lo que se sabe

- **El chip.** El N95 usa un OMAP2420 con PowerVR **MBX + VGP**, según la hoja de datos de TI ("2D/3D hardware MBX/VGP accelerators, up to 2 Million polygons/sec"). El VGP es el procesador de vértices programable. El OMAP2430 trae, en cambio, MBX Lite + VGP Lite.
- **El acceso al VGP.** Desde OpenGL ES 1.1 se programa con la extensión `GL_IMG_vertex_program` (ensamblador al estilo de `ARB_vertex_program`, compilado con el VGP Compiler del SDK de PowerVR). Según Imagination (foro de PowerVR, 2008), **el driver de Nokia no la expone en el N95 ni en ningún Nokia con OMAP2**, aunque otros equipos con el mismo chip sí (Samsung i8510 e i7710, Motorola Z8 y Z10). Sin esa extensión no hay programas de vértices propios.


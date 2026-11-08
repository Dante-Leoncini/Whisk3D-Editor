# Whisk3D para PSP

Es el mismo editor de la PC, compilado para el MIPS Allegrex de la PSP con el kit libre pspdev (nada de Sony):

- **pspdev** es el compilador (`psp-gcc`) con el PSPSDK (sceGu, sceCtrl, el cargador) y las herramientas que arman el `EBOOT.PBP`.
- **El dibujo es propio**: la PSP no tiene OpenGL. El Core tiene un backend para su GPU (el GE), en `libs/Whisk3DCore/graphics/gu/`, con la misma API que el de OpenGL (`graphics/w3dGraphics.h`), como el de citro3d para la 3DS.
- **SDL2 de la PSP** maneja los eventos, el control y el sonido. Entra por `main.cpp` y `controles.cpp`, igual que en la PC.

Lo propio de la consola va con `#ifdef W3D_PSP`, y lo que no tiene lugar en el código compartido, en `w3d_psp.cpp`. El `GL/gl.h` de `include/` trae los tipos de GL (el PSPGL de pspdev no se usa).

## El backend del GE (libs/Whisk3DCore/graphics/gu)

- `w3dGraphicsGu.cpp`: el dibujo.
- `w3dGLgu.cpp`: las pocas funciones de GL que el Core llama directo, que son las texturas y las luces.
- `w3dGu.h`: lo que usa la plataforma (iniciar, empezar y terminar el cuadro).

El GE tiene pipeline fijo por hardware y el backend se lo pasa casi derecho:

- **Por hardware:** la transformación, cuatro luces, la niebla lineal, las mezclas, el alpha test, el estencil y el z.
- **Por la CPU:** los arreglos de vértices de la RAM se leen en cada draw y se escriben entrelazados en el formato del GE, en un anillo de memoria que el GE lee.

Lo que el GE no hace como GL:

- **El recorte.** El GE no recorta contra los costados, y un triángulo con un vértice fuera de su espacio de 4096x4096 se descarta entero. Por eso cada vértice se pasa al espacio de recorte en la CPU, y lo que cruza el plano cercano, el lejano o ese borde se recorta ahí. Si no, la grilla del piso, que pasa por debajo de la cámara, desaparecería.
- **Las líneas anchas y los puntos.** El GE los dibuja de un píxel. Se arman en la CPU como rectángulos en pantalla: los puntos son el cursor 3D, los orígenes y los vértices del modo edición.
- **El z.** Es de 16 bits y va al revés (65535 cerca, 0 lejos), así que los test de z se dan vuelta. Con 16 bits la precisión cae mucho con la distancia: el visor usa un plano cercano proporcional a la distancia de órbita (el 2%, como en el N95), y el polygon offset vale un nivel del z por unidad (y dos por el factor de pendiente). Con el plano cercano fijo en 0,01 y un offset más grande, las líneas que estaban detrás de una cara se veían a través.
- **El estencil.** Es el alfa del framebuffer, y su test queda siempre prendido para que lo que se dibuja no lo pise.
- **Las texturas.** Son potencia de dos hasta 512 y van "swizzled" (en bloques), en 565 las opacas y en 8888 las que tienen alfa. Las de 1024 se achican a la mitad.
- **Sin sphere map por hardware ni dos texturas a la vez.** Esos caminos van por software, como en la Dreamcast.

## El control

La PSP no tiene pantalla táctil: el control mueve un cursor (como el mouse en la Dreamcast) y la interfaz es la táctil del editor. Las transformaciones se hacen con la barra de abajo (Mover, Rotar, Escalar, el tilde) y los números con el teclado numérico en pantalla. Cuando un campo de texto toma el foco, se abre el teclado QWERTY de Whisk3D, que trae la ñ.

| Botón | En el editor |
|---|---|
| Analógico | Mover el cursor |
| Cruceta | El cursor de a un píxel (mantenida, avanza) |
| Cruz | Click (mantenida, arrastra: en la vista 3D orbita) |
| Círculo | Click derecho |
| Cuadrado | Botón del medio (mantenido + analógico: orbitar) |
| Triángulo | Enter (confirmar) |
| Start | Esc (cancelar) |
| Select | Deshacer |
| L / R | Alejar / acercar (la rueda) |

Mientras el juego corre (Play), los botones son del juego.

## Preparar (una vez)

Todo lo que hace falta fuera del proyecto queda en `~/psp` (o en `RAIZPSP`), sin root:

```bash
bash platform/psp/preparar_psp.sh
```

El script hace esto:

- Baja pspdev ya compilado de su página de releases. Trae el compilador, el PSPSDK y SDL2.
- Baja el emulador PPSSPP (el AppImage oficial) y lo desarma. Su configuración y su Memory Stick quedan en `~/psp/emu/home`.
- Para probar sin ventana: baja Xvfb y xdotool con `apt download` (sin instalarlos).

Si algo ya está hecho, lo saltea.

## Compilar

```bash
bash platform/psp/build_psp.sh
```

Genera dos cosas en `platform/psp/build/`:

- `PSP/GAME/Whisk3D/`: la carpeta con el `EBOOT.PBP` y los recursos (`res/`) al lado.
- `whisk3d-psp.zip`: esa misma carpeta, para bajar (unos 5 MB).

Hacen falta ImageMagick (`convert`, para el ícono y el fondo del menú) y `zip`.

## En la consola

Hace falta una PSP con firmware modificado (o un cargador de homebrew). Se descomprime `whisk3d-psp.zip` en la raíz del Memory Stick: queda `PSP/GAME/Whisk3D/EBOOT.PBP` y Whisk3D aparece en Juego > Memory Stick. En la PSP Go también puede ir en la memoria interna, con la misma estructura; ahí el explorador de archivos empieza en `ef0:/` en vez de `ms0:/`.

Cargado ocupa unos 14 MB (el ejecutable, unos 10,6, y el heap al arrancar). Entra en la PSP 1000, que tiene 24 MB para el programa; desde la 2000, el `PARAM.SFO` pide la memoria grande (52 MB).

El log (`whisk3d.log`, se pisa en cada sesión) y la configuración quedan en la carpeta del juego. Los proyectos se guardan donde se elija; el explorador de archivos empieza en `ms0:/`.

## Probar en PPSSPP

Con ventana:

```bash
~/psp/emu/ppsspp.sh platform/psp/build/PSP/GAME/Whisk3D/EBOOT.PBP
```

Sin ventana, para ver lo que dibuja:

```bash
bash platform/psp/captura_ppsspp.sh                              # -> build/captura.png
TOQUES="60,12 200,200>300,180" bash platform/psp/captura_ppsspp.sh
```

El script vuelve a compilar y copia el juego a un Memory Stick propio (`~/psp/emu/home-captura`). Corre el emulador en un X virtual de 480x272 con el Vulkan por software del sistema (lavapipe), espera y captura la pantalla. El log de Whisk3D y los errores del emulador quedan en `build/ppsspp.log`.

Como la PSP no tiene pantalla táctil, los toques los hace Whisk3D mismo: `TOQUES` se compila adentro (`make PRUEBA=...`). Va un paso por segundo desde los 3 segundos de arrancar, en píxeles de la PSP (0 a 479, 0 a 271). Cada paso puede ser:

- `x,y`: un click.
- `x,y>x2,y2`: un arrastre.
- `z+n` / `z-n`: n pasos de la rueda donde esté el cursor (acercar o alejar).

`build_psp.sh CAPTURA=n` guarda la pantalla en `captura.ppm`, en la carpeta del juego, después del cuadro n. Sirve en la consola de verdad, donde no hay emulador que capture.

## Estado

Probado en PPSSPP, con su renderer por software (el más parecido a la consola) y con Vulkan. Falta la consola de verdad.

- **El editor entero:** la UI, el texto, los íconos, la grilla, el gizmo, la luz del modo Solid, el modo edición (los vértices, las aristas, las caras seleccionadas) y el cursor.
- **El control:** el click abre menús y el arrastre orbita la vista.
- **El teclado:** el campo Nombre abre el QWERTY de Whisk3D.
- **Abrir y guardar:** "Guardar" deja el `.w3d` en `ms0:/` y "Abrir proyecto" lo vuelve a abrir.

Pendiente:

- La consola de verdad (la velocidad, la memoria en la PSP 1000).
- La red (está apagada, como en la 3DS y la Vita).
- Las capturas de pantalla de los juegos (`CopiarPantalla`): todavía no.

# Whisk3D para PS Vita

Es el mismo editor de la PC, compilado para el ARM (Cortex-A9) de la Vita con el kit libre VitaSDK (nada de Sony):

- **VitaSDK** es el compilador (`arm-vita-eabi-gcc`) y trae las herramientas que arman el `.vpk`.
- **vitaGL** es el OpenGL: emula el pipeline fijo sobre la GPU de la Vita (sceGxm). Whisk3D dibuja con su backend fijo de siempre (`gfx/w3dGraphics.cpp`), como en la Dreamcast con GLdc.
- **SDL2 de la Vita, con vitaGL** abre el contexto de GL y maneja el táctil, los botones y el sonido. Entra por `main.cpp` y `controles.cpp`, igual que en la PC.

Lo propio de la consola va con `#ifdef W3D_VITA`, y lo que no tiene lugar en el código compartido, en `w3d_vita.cpp`. El `GL/gl.h` de `include/` apunta a vitaGL y completa lo que no trae: `glLightf` y `glLightModelf`, y los enums de los focos.

## El compilador de shaders (hace falta aparte)

vitaGL arma los shaders del pipeline fijo en la consola, mientras corre: cada combinación de estado (luces, texturas, niebla) se compila la primera vez y queda guardada en `ux0:data/shader_cache`. El compilador es el de Sony, **`libshacccg.suprx`**. No viene con la Vita ni con VitaSDK: se instala aparte, una vez, y lo usan casi todos los juegos y programas caseros hechos con vitaGL.

- **En la consola:** con la aplicación ShaRKBR33D, que lo deja en `ur0:data/external/libshacccg.suprx`.
- **En Vita3K:** se copia ese mismo archivo a `~/vita/emu/home/data/Vita3K/Vita3K/ur0/data/external/libshacccg.suprx`.

Sin ese archivo, Whisk3D avisa con un cartel y se cierra (en vez de colgarse en el primer cuadro).

## La pantalla y los botones

La pantalla (960x544) es el editor entero, a escala 2. Se toca como en el celular: un toque es un click, un dedo en el viewport orbita, y dos dedos hacen zoom (pellizcar) y panean. El panel táctil de atrás no se usa.

| Botón | En el editor |
|---|---|
| Stick izquierdo | Orbitar la vista |
| Stick derecho | Panear |
| Cruceta | Flechas |
| L / R | Shift / Ctrl (mantenidos: L + toque agrega a la selección) |
| Cruz | Enter (confirmar) |
| Círculo | Esc (cancelar) |
| Cuadrado | Tab (modo objeto / edición) |
| Triángulo | Deshacer |
| Select | Rehacer |

Mientras el juego corre (Play), los botones son del juego.

## Preparar (una vez)

Todo lo que hace falta fuera del proyecto queda en `~/vita` (o en `RAIZVITA`), sin root:

```bash
bash platform/vita/preparar_vita.sh
```

El script hace esto:

- Instala VitaSDK con su gestor de paquetes (vdpm).
- Instala SDL2 con vitaGL (`vdpm install sdl2_vitagl`, que trae vitaGL y lo que pide).
- Baja el emulador Vita3K y lo desarma. Su configuración y la memoria de la consola quedan en `~/vita/emu/home`.
- Para probar sin ventana: baja Xvfb y xdotool con `apt download` (sin instalarlos).

Si algo ya está hecho, lo saltea. El compilador de shaders no lo baja (ver arriba).

## Compilar

```bash
bash platform/vita/build_vita.sh
```

Genera `platform/vita/build/whisk3d.vpk`, unos 5 MB, con los recursos de `res/` adentro (`app0:res`). Hace falta ImageMagick (`convert`) para el ícono y la pantalla de la burbuja.

## En la consola

Hace falta una Vita con HENkaku (o Ensō) y VitaShell. Se copia `whisk3d.vpk` a la Vita y se instala desde VitaShell. Aparece la burbuja de Whisk3D (`WHSK00001`).

Los datos del editor y el log (`whisk3d.log`, se pisa en cada sesión) van en `ux0:data/Whisk3D`.

## Probar en Vita3K

Con ventana (instala el `.vpk` y lo abre):

```bash
~/vita/emu/vita3k.sh platform/vita/build/whisk3d.vpk
```

Sin ventana, para ver lo que dibuja (por ejemplo, en una prueba automática):

```bash
bash platform/vita/captura_vita3k.sh            # -> build/captura.png
TOQUES="40,20 480,300>600,300" bash platform/vita/captura_vita3k.sh
```

Corre el emulador en un X virtual de 960x544, con el Vulkan por software del sistema (lavapipe: el X virtual no tiene placa de video), espera a que arranque y captura la pantalla. Los toques van en píxeles de la Vita (0 a 959, 0 a 543); `x,y>x2,y2` arrastra el dedo. Lo que escribe Whisk3D en su log y los errores del emulador quedan en `build/vita3k.log`.

Vita3K corre Whisk3D sin el firmware de la Vita: solo le faltan las fuentes del sistema, que usan los carteles (el de error sale sin texto).

`build_vita.sh CAPTURA=n` guarda la pantalla en `ux0:data/Whisk3D/captura.ppm` después del cuadro n. Sirve en la consola de verdad, donde no hay emulador que capture.

## Estado

Probado en Vita3K (falta la consola de verdad):

- **Dibuja el editor entero**, con la luz del modo Solid.
- **El táctil anda:** un toque abre menús y arrastrar orbita. SDL arma el click del mouse a partir del toque solo si se le dice de qué panel (`SDL_HINT_VITA_TOUCH_MOUSE_DEVICE`, en `main.cpp`); sin eso el editor no veía los toques.
- **Sin VBO:** las mallas van por arreglos en RAM, como en la Dreamcast. Desde VBO la luz salía mal (el cubo blanco con un driver de video y negro con otro).
- **Los sprites de punto** (el cursor 3D, los orígenes): vitaGL usa la textura solo si el arreglo de coordenadas de textura está prendido, y un sprite no lo necesita. `DrawPoints` lo prende con coordenadas de relleno; sin eso, el cursor 3D salía como un cuadrado verde al arrastrar un objeto.
- **Abrir y guardar:** el explorador de archivos empieza en `ux0:` y lista sus carpetas. El nombre se escribe con el teclado propio de Whisk3D (no hace falta el de la consola), y "Save" deja el `.w3d` en la carpeta elegida. "Open project" lo vuelve a abrir.

Pendiente:

- Volver a probar los VBO en la consola de verdad (puede ser cosa del emulador).
- Un trapecio verde oscuro abajo a la izquierda del viewport en la vista de inicio: parece una línea de la grilla que pasa por debajo de la cámara.
- La red (está apagada, como en la 3DS).
- El sphere map por hardware: vitaGL no tiene `glTexGen`, así que el matcap va por software, como en la Dreamcast.

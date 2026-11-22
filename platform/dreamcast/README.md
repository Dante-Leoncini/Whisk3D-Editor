# Whisk3D para Sega Dreamcast

Es el mismo editor de la PC, compilado para el SH-4 de la Dreamcast:

- **KallistiOS** es el sistema.
- **GLdc** hace de OpenGL 1.x sobre la PVR. Usa el backend fijo `w3dGraphics.cpp`, el mismo de la PC y del N95.
- **SDL2 de Dreamcast** maneja la ventana GL, el mouse, el teclado y el control. Entra por `main.cpp` y `controls.cpp`, igual que en la PC.

Lo propio de la consola va con `#ifdef W3D_DREAMCAST`.

Se modela con el **mouse y el teclado de la Dreamcast**, como en una PC. La consola no tiene cursor del sistema: Whisk3D dibuja una flecha apenas se mueve el mouse.

Sin mouse, el **control** maneja el cursor. Mientras el juego corre (Play), el control vuelve a ser del juego.

| Control | En el editor |
|---|---|
| Analógico | Mover el cursor (más rápido cuanto más se inclina; al fondo, como la cruceta mantenida) |
| Cruceta | Mover el cursor de a un píxel por toque, para agarrar bordes (mantenida, acelera) |
| A | Click izquierdo (mantenido: arrastrar) |
| B | Click derecho |
| X | Botón del medio (mantenido + analógico: orbitar la vista) |
| R / L | Rueda: acercar / alejar |
| Y | Enter |
| Start | Esc |

## Preparar (una vez)

Todo lo que hace falta fuera del proyecto queda en `~/dc` (o en `DC_RAIZ`):

```bash
bash platform/dreamcast/preparar_dreamcast.sh
```

El script hace esto:

- Compila KallistiOS. Necesita la toolchain SH-4 ya armada en `~/dc/toolchains/dc/sh-elf`; se arma con `kos/utils/kos-chain`.
- Instala GLdc con `parches/gldc.patch`.
- Compila SDL2 de Dreamcast (rama `dreamcastSDL2` de github.com/GPF/SDL) con `parches/sdl2-dreamcast.patch`.
- Arma `mkdcdisc`, que hace el disco `.cdi`.

Si algo ya está hecho, lo saltea.

### Los parches

**GLdc** (`parches/gldc.patch`):

- **Normales en doble precisión:** `_glPackNormal` usaba `127.5` en doble. El FPU llegaba a la instrucción `FTRV` en modo doble, y en la consola real eso es una instrucción ilegal.
- **Normales con signo:** las normales `GL_BYTE` se leían sin signo, y la luz salía mal.
- **Luces direccionales:** la dirección hacia la luz estaba negada y no se rotaba con la modelview.
- **Líneas:**
  - se armaban antes de dividir por `w`, así que salían más finas cuanto más lejos estaban y quedaban punteadas;
  - una línea que pasaba por detrás de la cámara (el eje al mover un objeto) se recortaba recién después de hacerla rectángulo y salía como una franja enorme; ahora se recorta antes;
  - van apenas hacia la cámara, porque GLdc no tiene `glPolygonOffset`: las aristas del modo edición ya no pelean con sus caras;
  - una línea que en pantalla mide cero (las dos puntas en el mismo píxel) no tiene dirección: el ancho salía de `0 × ∞` y le mandaba NaN a la PVR. Ahora queda un cuadrilátero sin área.
- **Puntos:** ahora llevan la textura entera (point sprites).
- **Scissor:**
  - si nada toca los bordes no se recorta nada (antes cada triángulo de la interfaz pasaba por el recorte de 4 planos y era la mitad del cuadro);
  - también se recortan las líneas y los puntos: antes solo los recortaba la PVR, por bloques de 32x32, y la grilla del viewport 3D se pasaba a los paneles de al lado;
  - el recorte escribía la salida encima de la entrada, así que pisaba las primitivas siguientes (desaparecían los quads y se rompían los triángulos del borde de un panel).
- **Fin de cuadro:** no espera a que la PVR termine de dibujar. Así la PVR dibuja un cuadro mientras la CPU arma el siguiente.
- **Actualizar parte de una textura** (`glTexSubImage2D`): si había que convertir el formato, borraba el resto de la textura. El trazado de rayos, que sube la imagen de a tiles, quedaba negro salvo el tile en curso.

**SDL2 de Dreamcast** (`parches/sdl2-dreamcast.patch`):

- **Teclas:** van por `SDL_SendKeyboardKey`, para que funcionen Ctrl, Shift y Alt, y el estado del teclado. Además, una tecla suelta ya no cuenta como otra pulsación.
- **Texto:** sale con la distribución del teclado (ñ y acentos en el español).

## Compilar

```bash
bash platform/dreamcast/build_dreamcast.sh
```

Genera en `platform/dreamcast/build/`:

- `whisk3d.elf`: el ejecutable, unos 5,3 MB. Flycast lo abre directo.
- `whisk3d.cdi`: el disco autoarrancable, unos 7,5 MB. Sirve para emuladores, un CD-R grabado o una unidad que emula el lector, tipo GDEMU.
- `whisk3d_debug.elf`: el mismo con símbolos, para traducir direcciones de un cuelgue con `sh-elf-addr2line`.

Los recursos de `res/` van adentro del ejecutable, en un romdisk que se monta en `/rd`.

## En la consola

- **Por el adaptador de red (BBA), con dcload-ip:** se manda el ELF desde la PC. El log sale en la consola de dc-tool:

  ```bash
  dc-tool-ip -t <ip-de-la-dreamcast> -x platform/dreamcast/build/whisk3d.elf
  ```

- **Sin cable:** `whisk3d.cdi` grabado en un CD-R o cargado en una unidad que emula el lector, tipo GDEMU.

## Probar en Flycast

Flycast emula el teclado y el mouse de la Dreamcast, pero hay que decirle a qué puerto de la consola van el teclado y el mouse de la PC. Por defecto los manda al puerto A, como si fueran el control, y entonces las teclas no le llegan al teclado de la Dreamcast. En su `emu.cfg` (de paso, con el cable VGA):

```ini
[config]
Dreamcast.Cable = 0        ; VGA: 640x480 progresivo a 60 Hz (con el de TV, que es el de fábrica, sale PAL entrelazado a 50 Hz)

[input]
device1 = 0                ; puerto A: control
device2 = 5                ; puerto B: teclado de Dreamcast
device3 = 6                ; puerto C: mouse de Dreamcast
maple_sdl_keyboard = 1     ; el teclado de la PC va al puerto B
maple_sdl_mouse = 2        ; el mouse de la PC va al puerto C
maple_sdl_mouse_0 = 2
```

Para capturar el mouse dentro de la ventana se aprieta **Ctrl izquierdo + Alt izquierdo**, y lo mismo para soltarlo. Sin capturar, el mouse igual se mueve mientras el puntero está sobre la ventana.

Con el teclado en el puerto B, el control del puerto A se maneja con un gamepad de la PC. Para probar el control-como-mouse sin gamepad, volvé el teclado al puerto A (`maple_sdl_keyboard = 0`); ahí las teclas hacen de control.

`~/dc/emu/flycast.sh` corre el Flycast de `~/dc/emu` con su propia configuración, que ya trae todo eso y la consola serie a la terminal:

```bash
~/dc/emu/flycast.sh platform/dreamcast/build/whisk3d.cdi
```

Para ver lo que dibuja sin ventana, por ejemplo en una prueba automática:

```bash
bash platform/dreamcast/captura_flycast.sh      # -> platform/dreamcast/build/captura.png
```

Ese script compila con `CAPTURA=n`: en el cuadro n, la pantalla sale en base64 por la consola serie y el script la pasa a PNG.

Con `PRUEBA=n` delante, antes de capturar empuja eventos de teclado y mouse, para verificar que la interfaz responde en la consola:

- `PRUEBA=1`: Tab y un click en el menú del modo.
- `PRUEBA=2`: mover el cubo con G, X y el mouse, y confirmar con un click. Con captura en el cuadro 16 se ve la línea del eje.
- `PRUEBA=3`: seleccionar con click: el vacío, la luz y el cubo.
- `PRUEBA=4`: arrastrar el borde entre el viewport 3D y los paneles.
- `PRUEBA=5`: modo edición y click sobre un vértice.
- `PRUEBA=6`: trazado de rayos en el viewport.
- `PRUEBA=7`: pintura de textura, con un trazo sobre el cubo.
- `PRUEBA=8`: orbitar como con el control (X mantenido y el cursor a la derecha), pasando varias veces el borde del viewport, donde el editor lleva el cursor al otro lado para que el arrastre siga.

Con cualquier `PRUEBA`, cada 8 cuadros sale por la consola cuánto tardaron y en qué (lógica, viewport 3D, render, swap).

### Si se cuelga

Si una excepción de C++ queda sin atrapar, antes de abortar sale por la consola serie cuál fue y la pila de llamadas:

```
[cuelgue] excepcion de C++ sin atrapar: ...
[cuelgue] pila (sh-elf-addr2line -Cfpe whisk3d_debug.elf ...): 0x8c0... 0x8c0...
```

Después la consola se reinicia (lo mismo al salir del editor), salvo que se haya arrancado con dcload: ahí vuelve al cargador.

Las direcciones se traducen a funciones y líneas con:

```bash
sh-elf-addr2line -Cfpe platform/dreamcast/build/whisk3d_debug.elf 0x8c0... 0x8c0...
```

## Lo que hay que saber

- **Memoria:** la consola tiene 16 MB para todo. Con la escena de fábrica quedan unos 8,9 MB libres; el log lo dice al dibujar el primer cuadro.
- **Guardar:** lo único escribible es `/ram`, que se pierde al apagar. Por ahora sirve para una sesión; lo que viene es guardar en una tarjeta SD por el adaptador del puerto serie, que KallistiOS lee.
- **Texturas:** van en potencia de 2, de 8 a 1024 por lado, y en 16 bits (lo hace GLdc): RGB565 si la imagen es opaca, ARGB4444 si tiene transparencia. Sin mipmaps, porque GLdc solo los hace en texturas twiddled.
- **Velocidad:** en Flycast, la escena de fábrica tarda unos 30 ms por cuadro: unos 20 ms en armar la interfaz y la escena, y unos 8 ms en mandarlas a la PVR. La interfaz es lo más caro.
- **Leer la pantalla:** la PVR arma el cuadro entero recién al cerrarlo, así que no hay pantalla para leer a mitad de cuadro (en GLdc, `glReadPixels` corta el programa).
  - La selección con click, que lee los colores de los IDs, se dibuja por software en una zona chica alrededor del cursor (`ReadPixelsBegin` en `w3dGraphics.cpp`).
  - Al pintar no hay mapa de oclusión: vale solo el descarte de las caras de espaldas, como con X-Ray. Un vértice de una cara que mira a la cámara pero está tapada por otra también se pinta.
  - Las capturas de pantalla que piden los juegos quedan como estaban.
- **Lo que la PVR no tiene:**
  - planos de recorte;
  - combinación de texturas en una pasada (las capas van en varias pasadas);
  - sphere map por hardware (el reflejo se calcula por software);
  - VBOs.
- **Desactivado:** el sonido y "Compilar juego", que lanza cmake o emcc en la PC.
- **Idioma:** sale del menú de la consola. Con la BIOS emulada de Flycast la flash no lo trae y arranca en inglés; se cambia en Ajustes.

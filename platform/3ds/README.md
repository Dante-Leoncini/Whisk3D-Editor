# Whisk3D para Nintendo 3DS

Es el mismo editor de la PC, compilado para el ARM11 de la 3DS con los kits libres de devkitPro (nada de Nintendo):

- **devkitARM** es el compilador; **libctru**, el sistema de la consola.
- **citro3d** maneja la GPU (PICA200). Whisk3D la usa con su propio backend, `libs/Whisk3DCore/graphics/citro3d`, en vez del de OpenGL.
- **SDL2 de 3DS** (el de `thirdparty/SDL2`, que trae el port oficial) maneja el táctil, los botones y los eventos. Entra por `main.cpp` y `controles.cpp`, igual que en la PC.

Lo propio de la consola va con `#ifdef W3D_3DS`, y lo que no tiene lugar en el código compartido, en `w3d_3ds.cpp`.

## Las dos pantallas

- **Abajo, la táctil (320x240):** el editor entero. Se toca como en el celular: un toque es un click, arrastrar el dedo en el viewport orbita, y la barra de abajo trae Shift y Ctrl.
- **Arriba (400x240):** la vista 3D activa en grande, solo la escena (sin barras ni menús). Sigue a la de abajo: misma cámara, misma selección.

La 3DS tiene un solo punto de toque, así que no hay pellizco: el zoom y el paneo van por los botones.

| Botón | En el editor |
|---|---|
| Circle pad | Orbitar la vista |
| C-stick (New 3DS) | Panear |
| ZL / ZR (New 3DS) | Alejar / acercar |
| Cruceta | Flechas |
| L / R | Shift / Ctrl (mantenidos: L + toque agrega a la selección) |
| A | Enter (confirmar) |
| B | Esc (cancelar) |
| X | Tab (modo objeto / edición) |
| Y | Deshacer |
| Select | Rehacer |

Mientras el juego corre (Play), los botones son del juego.

## Cómo dibuja

La PICA200 no tiene el pipeline fijo de OpenGL. El backend `c3d` lo emula con lo que la GPU trae por hardware:

- **Vértices en la GPU:** la transformación corre en un vertex shader (`graphics/citro3d/w3d.v.pica`, lo ensambla picasso).
- **Luz por fragmento:** hasta 8 luces, con la especular por tabla.
- **El combiner:** modulate, replace, DOT3, la mezcla de dos texturas.
- **Lo demás por hardware:** scissor, estencil, alpha test, mezclas y z.

Los arreglos de vértices se copian en cada cuadro a un anillo de memoria lineal (la que lee la GPU). Para que vaya rápido:

- **Cada draw manda solo lo que cambió.** citro3d vuelve a mandar todo lo que se toca aunque quede igual (el combiner, la textura y el vaciado de su caché, las matrices), y eso era lo que más tardaba.
- **Los draws chicos van juntos.** La interfaz son muchos draws de 6 vértices (un rectángulo, un ícono, un texto), y a la PICA cada draw le cuesta lo mismo tenga 6 vértices o 600. Los draws chicos sin luz se pasan al espacio de recorte en la CPU y los seguidos con el mismo estado salen en un solo draw. Con la escena de inicio, de 191 draws por cuadro quedan 18.
- **No se vacía toda la caché en cada cuadro.** Lo que lee la GPU ya está vaciado: el anillo al terminar el cuadro y cada textura al subirla.

Lo que la PICA no tiene se arma en la CPU: las líneas y los puntos son triángulos. Se recortan a un margen de dos pantallas para cada lado: la PICA solo recorta contra el plano cercano, y una línea que lo cruza (la grilla debajo de la cámara) llegaba con la punta a cientos de miles de píxeles de distancia.

**La selección por color** (tocar un objeto, un vértice, una arista) se dibuja aparte, en una superficie del tamaño de la pantalla de abajo que no se ve. El motor de transferencias de la GPU la copia a memoria lineal y la CPU lee ahí esos píxeles (la CPU no lee la VRAM directo: en el emulador andaba, en la consola no es seguro).

**Los atributos de los vértices** van empaquetados y cada uno ocupa exactamente lo que se declara: las normales de 3 bytes van como 4 (la cuarta en cero). Con 3 bytes y lugar para 4, en la consola el cubo salía sombreado como con las normales soldadas.

## Preparar (una vez)

Todo lo que hace falta fuera del proyecto queda en `~/3ds` (o en `RAIZ3DS`), sin root:

```bash
bash platform/3ds/preparar_3ds.sh
```

El script hace esto:

- Compila devkitARM con los buildscripts de devkitPro. Las fuentes salen de los servidores de GNU y de sourceware, y las reglas de GitHub: no hace falta downloads.devkitpro.org (que no siempre responde) ni pacman (que pide root).
- Compila las herramientas: picasso, 3dsxtool, smdhtool y bin2s.
- Instala libctru (de la rama principal: la v2.7.0 no compila con el gcc 16 de hoy) y citro3d 1.7.1.
- Compila SDL2 de 3DS.
- Baja el emulador Azahar (fork de Citra), con su propia configuración en `~/3ds/emu/home`.
- Para probar sin ventana: baja Xvfb y xdotool con `apt download` (sin instalarlos).

Si algo ya está hecho, lo saltea. Compilar gcc lleva un buen rato.

## Compilar

```bash
bash platform/3ds/build_3ds.sh
```

Genera `platform/3ds/build/whisk3d.3dsx`, unos 7 MB, con los recursos de `res/` adentro (romfs).

## En la consola

Hace falta una 3DS con el Homebrew Launcher. Se copia `whisk3d.3dsx` a la carpeta `3ds/` de la tarjeta SD y se abre desde el launcher.

Los datos del editor van en la carpeta `3ds/Whisk3D` de la SD (la crea al arrancar).

## Si se cuelga en la consola

Whisk3D escribe su log en la SD, en `3ds/Whisk3D/whisk3d.log` (se pisa en cada sesión). La primera línea dice si es una New 3DS o una 3DS vieja.

Además vigila la GPU: si un cuadro no termina en 2 segundos (en la consola de verdad, la GPU colgada), anota en ese log la lista de draws del cuadro que la colgó (qué eran, cuántos vértices, con qué estado y hasta dónde llegaban en la pantalla). El emulador nunca se cuelga así, por eso hace falta.

Si se cuelga, sirve:

- **El log** de la SD (después de reiniciar la consola).
- **Si Luma3DS muestra una pantalla de excepción** ("An exception occurred"): una foto, o el archivo que guarda en `luma/dumps/arm11/`. Con eso y `build/whisk3d.elf` (que tiene los símbolos) se sabe en qué función se cayó.

## Probar en Azahar

Con ventana:

```bash
~/3ds/emu/azahar.sh platform/3ds/build/whisk3d.3dsx
```

Un click en la pantalla de abajo es un toque. Los botones van por el teclado de la PC (se cambian en la configuración del emulador):

| Tecla | Botón |
|---|---|
| A / S / Z / X | A / B / X / Y |
| Q / W | L / R |
| 1 / 2 | ZL / ZR |
| M / N | Start / Select |
| T G F H | Cruceta |
| Flechas | Circle pad |
| I K J L | C-stick |

Sin ventana, para ver lo que dibuja (por ejemplo, en una prueba automática):

```bash
bash platform/3ds/captura_azahar.sh             # -> build/captura_arriba.png y build/captura_abajo.png
```

Corre el emulador en un X virtual, espera a que arranque y captura las dos pantallas. Lo que escribe Whisk3D en su log queda en `build/azahar.log`. Delante del comando se pueden pedir toques y botones:

```bash
TOQUES="45,12 70,45" bash platform/3ds/captura_azahar.sh       # el menú del modo y "Edit Mode"
TOQUES="85,105>140,150" bash platform/3ds/captura_azahar.sh     # arrastrar el dedo de un punto a otro (de golpe)
TOQUES="30,170>>310,170" bash platform/3ds/captura_azahar.sh    # arrastrarlo de a poco, como un dedo: orbita
TECLAS="z Right:700 2:600" bash platform/3ds/captura_azahar.sh  # X, circle pad a la derecha 0,7 s, ZR 0,6 s
```

Los toques van en píxeles de la pantalla de abajo (0 a 319, 0 a 239).

### Rendimiento

```bash
bash platform/3ds/build_3ds.sh PRUEBA=1 && bash platform/3ds/captura_azahar.sh 30
grep "prueba\]" ~/3ds/emu/home/data/azahar-emu/log/azahar_log.txt
```

La vista gira sin parar y el log anota, cada 30 cuadros, cuánto tarda el cuadro y en qué: la lógica, la escena, el viewport 3D, lo que tarda la CPU en armar los comandos de citro3d, la GPU y los draws del backend.

En el emulador, con la escena de inicio en las dos pantallas, la CPU arma el cuadro en unos 6,6 ms, y sale a 60 cuadros por segundo. El tiempo del emulador es optimista: en una 3DS de verdad la CPU tarda más, sobre todo en la vieja (268 MHz).

### Captura en la consola

`build_3ds.sh CAPTURA=n` guarda la pantalla de abajo en la SD (`whisk3d_captura.ppm`) después del cuadro n. Sirve en la consola de verdad, donde no hay emulador que capture.

## Pendiente

- **El teclado de la consola** al escribir (renombrar, un número): lo abre SDL y no está probado. El emulador no lo muestra sin los archivos del sistema.
- **Guardar y abrir** proyectos en la SD: no está probado (pasa por el teclado de la consola para el nombre).
- **La niebla** (la vista Z-Buffer) y el **sphere map** por hardware.
- **El 3D estereoscópico** de la pantalla de arriba.
- **Las mallas fijas en memoria de la GPU** (VBO), para no copiarlas en cada cuadro.

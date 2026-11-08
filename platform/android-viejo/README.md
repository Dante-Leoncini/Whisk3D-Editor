# Whisk3D para Android 2.3

Es el mismo editor, en un APK aparte para los teléfonos de Android 2.3 (API 10) en adelante: ARMv6 o más nuevo y una GPU con OpenGL ES 2.0. Está probado en un **Samsung Galaxy Ace** (GT-S5830M, 2011) con Android 2.3.6: un ARM11 de 832 MHz, 278 MB de RAM, una GPU Broadcom VideoCore IV y una pantalla de 320x480.

Para Android 5.0 o más nuevo está el [APK de siempre](../android/README.md). Este se llama `com.whisk3d.viejo`, así que se pueden tener los dos instalados.

## Qué cambia

- **SDL 2.0.7**, el último SDL que anda en Android 2.3 (desde 2.0.8 piden la API 14), y el **NDK r15c**, el último que compila para ARMv5/v6. Lo que el editor usa de SDL más nuevo está en `jni/shim/w3d_sdl207.h`.
- **Instrucciones ARM con la FPU (VFP)**: sin eso, `armeabi` hace las cuentas de punto flotante por software.
- El compilador de ese NDK no trae `<filesystem>`: hay uno mínimo en `jni/shim`.
- **La interfaz a escala 1** (la densidad de estas pantallas es la del N95) y **sin antialiasing**.
- Los permisos se otorgan al instalar (Android 2.3 no los pide aparte).

Lo propio de esta plataforma va con `#ifdef W3D_ANDROID_VIEJO`.

## Cómo dibuja

Usa el backend de shaders de la web y el Android de siempre (`libs/Whisk3DCore/graphics/gles2`). El driver de la VideoCore IV tiene sus mañas, y el backend se adapta solo cuando las detecta:

- **El programa entero no le entra.** Al arrancar, el backend prueba si el shader con todas las funciones dibuja. En esta GPU no, así que cada draw usa un programa armado con lo que necesita (sin luz, sin niebla, sin recorte...). Se arman la primera vez que se usan.
- **Cada programa con una sola combinación de estado.** El driver traduce el programa según la primitiva, el blending y cómo llega cada atributo, y guarda pocas traducciones: usado con más combinaciones, lo traducía de nuevo en cada cuadro (unos 35 ms cada vez). Esa combinación también es parte de la clave del programa.
- **Los vértices de cada draw van como arreglos del cliente**, no subidos a un buffer: un `glBufferData` por draw al mismo buffer a veces le dejaba a un draw los datos del siguiente.
- **Las mallas, en la GPU.** Se suben una vez a buffers de vértices y cada draw solo dice dónde están.
- **La interfaz, junta.** Los textos, íconos y fondos son un draw cada uno; los seguidos con el mismo estado se juntan (ya transformados en la CPU) y salen en un solo draw: de 179 draws por cuadro quedan 16.
- **Lo que GL ya tiene no se vuelve a pedir**: cada llamada a GL es un mensaje al chip.
- **Hasta 4 luces a la vez** en la escena.

Con la escena de inicio, orbitar con el cubo seleccionado lleva unos 27 ms por cuadro más el swap (el primer intento tardaba 325 ms).

## Preparar (una vez)

```bash
bash platform/android-viejo/preparar_android_viejo.sh
```

Deja en `~/android-viejo` (o en `W3D_ANDROID_VIEJO`) el NDK r15c y las fuentes de SDL 2.0.7, sin root. Del sistema hacen falta además el SDK de Android (las build-tools y cualquier plataforma, en `~/Android/Sdk` o `ANDROID_HOME`), un JDK e ImageMagick.

## Compilar

```bash
bash platform/android-viejo/build_android_viejo.sh            # -> build/whisk3d-android-viejo.apk
bash platform/android-viejo/build_android_viejo.sh instalar   # y lo instala en el teléfono conectado (adb) y lo abre
```

Compila lo nativo con `ndk-build`, el Java con `javac` y `d8`, y arma el APK a mano con `aapt` (sin Gradle), firmado con la llave de debug.

Al agregar un `.cpp` al Core hay que sumarlo en `jni/Android.mk`, igual que en el del Android de siempre.

## En el teléfono

Con la depuración USB prendida: `adb install -r build/whisk3d-android-viejo.apk`. Sin cable, se copia el APK al teléfono y se abre con permiso para instalar de **Orígenes desconocidos** (Ajustes > Aplicaciones).

Lo que escribe Whisk3D sale en el logcat: `adb logcat -s Whisk3D`.

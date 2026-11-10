# Sonido y música

**W3dAudio** es un **mezclador por software** escrito en C++03, idéntico en todos los sistemas (la misma idea que usaba Quake). Mezcla hasta 64 voces a un flujo estéreo de 16 bits, con interpolación lineal (los cambios de tono no suenan granulados), un limitador de picos, envolventes ADSR, reverb y un reloj propio para que un secuenciador de música no dependa de cuándo corre el cuadro.

Cada sistema aporta solo la **salida**: cuatro funciones (`W3dAudioBackendInit`, `Shutdown`, `Lock`, `Unlock`, en `W3dAudioBackend.h`) que abren el dispositivo y le piden al mezclador el próximo pedazo.

| Sistema | Salida |
|---|---|
| PC, Android, navegador | SDL2 (44.100 Hz) |
| Symbian | `CMdaAudioOutputStream` en su propio hilo, con doble buffer (22.050 Hz en el N95) |
| Consolas | El editor compilado para consola va sin sonido. Un juego compilado con "Usar sonido" lleva el mezclador con la salida de SDL |
| Dreamcast | Además, `w3dAICA`: un módulo opcional que usa el chip de sonido directo (64 canales por hardware y ADPCM de 4 bits). No convive con W3dAudio |

Hay tres piezas más:

- **W3dMusic:** la música larga (mp3, ogg, aac) no se pasa a PCM: la decodifica el sistema, casi siempre por hardware. Hoy tiene backend para el navegador (un elemento `<audio>`); los de Symbian y escritorio están pendientes.
- **W3dClip:** sonidos cortos de interfaz. En el navegador van por WebAudio, porque en iOS dispararlos con `<audio>` traba el teléfono. En el resto son sonidos del mezclador.
- **W3dVolumen:** el volumen general y el silencio valen para todo lo que suena y se guardan solos en la configuración.

Por qué importa: el juego de Android salía **mudo** con los .wav bien empaquetados adentro del APK, porque se abrían con `fopen` y ahí no son archivos. Desde que el sonido carga por `ReadFileBytes`, anda en todos lados. Desde Lua: [Sonido](../lua/api-sonido.md).

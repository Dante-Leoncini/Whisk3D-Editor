# Cada sistema, de un vistazo

| Sistema | Gráficos | Sonido | Ventana y entrada | Archivos |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 o Vulkan, a elección | SDL2 | SDL2 | Disco |
| Windows | OpenGL, OpenGL ES 2.0 o Vulkan, a elección | SDL2 | SDL2 | Disco |
| macOS | OpenGL (fijo) | SDL2 | SDL2 | Disco |
| Android 5 o más | OpenGL ES 2.0 | SDL2 | SDL2 y JNI (vibración) | APK y disco |
| Android 2.3 | OpenGL ES 2.0 (ajustado a la VideoCore IV) | SDL2 (2.0.7) | SDL 2.0.7 | APK y disco |
| Navegador | WebGL | SDL2 sobre WebAudio | Emscripten y SDL2 | `/res` empaquetado, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (fijo) | `CMdaAudioOutputStream` | Symbian (keypad, mandos Bluetooth, sensor; táctil en S60 5th) | Tarjeta `E:` |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (multitáctil) | Tarjeta `E:` |
| Dreamcast | GLdc (fijo); `pvr` opcional | Mudo en el editor; `w3dAICA` opcional | KallistiOS y SDL2 | Romdisk y `/ram` |
| PSP | sceGu (GE) | Mudo en el editor; SDL2 en juegos | pspdev y SDL2 | Memory Stick |
| PS Vita | vitaGL (fijo) | Mudo en el editor; SDL2 en juegos | VitaSDK y SDL2 | `app0:` y `ux0:` |
| Nintendo 3DS | citro3d | Mudo en el editor; SDL2 en juegos | libctru y SDL2 | romfs y SD |

# Ogni sistema, in breve

| Sistema | Grafica | Suono | Finestra e input | File |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 o Vulkan, a scelta | SDL2 | SDL2 | Disco |
| Windows | OpenGL, OpenGL ES 2.0 o Vulkan, a scelta | SDL2 | SDL2 | Disco |
| macOS | OpenGL (fisso) | SDL2 | SDL2 | Disco |
| Android 5 o superiore | OpenGL ES 2.0 | SDL2 | SDL2 e JNI (vibrazione) | APK e disco |
| Android 2.3 | OpenGL ES 2.0 (adattato alla VideoCore IV) | SDL2 (2.0.7) | SDL 2.0.7 | APK e disco |
| Browser | WebGL | SDL2 su WebAudio | Emscripten e SDL2 | `/res` impacchettato, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (fisso) | `CMdaAudioOutputStream` | Symbian (tastierino, controller Bluetooth, sensore; touch su S60 5th) | Scheda `E:` |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (multitouch) | Scheda `E:` |
| Dreamcast | GLdc (fisso); `pvr` opzionale | Muto nell'editor; `w3dAICA` opzionale | KallistiOS e SDL2 | Romdisk e `/ram` |
| PSP | sceGu (GE) | Muto nell'editor; SDL2 nei giochi | pspdev e SDL2 | Memory Stick |
| PS Vita | vitaGL (fisso) | Muto nell'editor; SDL2 nei giochi | VitaSDK e SDL2 | `app0:` e `ux0:` |
| Nintendo 3DS | citro3d | Muto nell'editor; SDL2 nei giochi | libctru e SDL2 | romfs e SD |

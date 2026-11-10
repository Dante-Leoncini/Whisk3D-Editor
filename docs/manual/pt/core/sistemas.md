# Cada sistema, de relance

| Sistema | Gráficos | Som | Janela e entrada | Arquivos |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 ou Vulkan, à escolha | SDL2 | SDL2 | Disco |
| Windows | OpenGL, OpenGL ES 2.0 ou Vulkan, à escolha | SDL2 | SDL2 | Disco |
| macOS | OpenGL (fixo) | SDL2 | SDL2 | Disco |
| Android 5 ou superior | OpenGL ES 2.0 | SDL2 | SDL2 e JNI (vibração) | APK e disco |
| Android 2.3 | OpenGL ES 2.0 (ajustado para a VideoCore IV) | SDL2 (2.0.7) | SDL 2.0.7 | APK e disco |
| Navegador | WebGL | SDL2 sobre WebAudio | Emscripten e SDL2 | `/res` empacotado, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (fixo) | `CMdaAudioOutputStream` | Symbian (teclado, controles Bluetooth, sensor; touch no S60 5th) | Cartão `E:` |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (multitoque) | Cartão `E:` |
| Dreamcast | GLdc (fixo); `pvr` opcional | Mudo no editor; `w3dAICA` opcional | KallistiOS e SDL2 | Romdisk e `/ram` |
| PSP | sceGu (GE) | Mudo no editor; SDL2 nos jogos | pspdev e SDL2 | Memory Stick |
| PS Vita | vitaGL (fixo) | Mudo no editor; SDL2 nos jogos | VitaSDK e SDL2 | `app0:` e `ux0:` |
| Nintendo 3DS | citro3d | Mudo no editor; SDL2 nos jogos | libctru e SDL2 | romfs e SD |

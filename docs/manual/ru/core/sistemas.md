# Все системы с первого взгляда

| Система | Графика | Звук | Окно и ввод | Файлы |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 или Vulkan, на выбор | SDL2 | SDL2 | Диск |
| Windows | OpenGL, OpenGL ES 2.0 или Vulkan, на выбор | SDL2 | SDL2 | Диск |
| macOS | OpenGL (фиксированный) | SDL2 | SDL2 | Диск |
| Android 5 и выше | OpenGL ES 2.0 | SDL2 | SDL2 и JNI (вибрация) | APK и диск |
| Android 2.3 | OpenGL ES 2.0 (с настройкой под VideoCore IV) | SDL2 (2.0.7) | SDL 2.0.7 | APK и диск |
| Браузер | WebGL | SDL2 поверх WebAudio | Emscripten и SDL2 | Упакованный `/res`, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (фиксированный) | `CMdaAudioOutputStream` | Symbian (клавиатура, геймпады Bluetooth, датчик; сенсорный экран на S60 5th) | Карта `E:` |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (мультитач) | Карта `E:` |
| Dreamcast | GLdc (фиксированный); `pvr` по желанию | Без звука в редакторе; `w3dAICA` по желанию | KallistiOS и SDL2 | Romdisk и `/ram` |
| PSP | sceGu (GE) | Без звука в редакторе; SDL2 в играх | pspdev и SDL2 | Memory Stick |
| PS Vita | vitaGL (фиксированный) | Без звука в редакторе; SDL2 в играх | VitaSDK и SDL2 | `app0:` и `ux0:` |
| Nintendo 3DS | citro3d | Без звука в редакторе; SDL2 в играх | libctru и SDL2 | romfs и SD |

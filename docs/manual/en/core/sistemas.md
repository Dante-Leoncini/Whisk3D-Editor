# Each system at a glance

| System | Graphics | Sound | Window and input | Files |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 or Vulkan, your choice | SDL2 | SDL2 | Disk |
| Windows | OpenGL, OpenGL ES 2.0 or Vulkan, your choice | SDL2 | SDL2 | Disk |
| macOS | OpenGL (fixed) | SDL2 | SDL2 | Disk |
| Android 5 or later | OpenGL ES 2.0 | SDL2 | SDL2 and JNI (vibration) | APK and disk |
| Android 2.3 | OpenGL ES 2.0 (tuned for the VideoCore IV) | SDL2 (2.0.7) | SDL 2.0.7 | APK and disk |
| Browser | WebGL | SDL2 on top of WebAudio | Emscripten and SDL2 | Packed `/res`, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (fixed) | `CMdaAudioOutputStream` | Symbian (keypad, Bluetooth gamepads, sensor; touch on S60 5th) | `E:` card |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (multi-touch) | `E:` card |
| Dreamcast | GLdc (fixed); `pvr` optional | Silent in the editor; `w3dAICA` optional | KallistiOS and SDL2 | Romdisk and `/ram` |
| PSP | sceGu (GE) | Silent in the editor; SDL2 in games | pspdev and SDL2 | Memory Stick |
| PS Vita | vitaGL (fixed) | Silent in the editor; SDL2 in games | VitaSDK and SDL2 | `app0:` and `ux0:` |
| Nintendo 3DS | citro3d | Silent in the editor; SDL2 in games | libctru and SDL2 | romfs and SD |

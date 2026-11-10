# Bir bakışta her sistem

| Sistem | Grafik | Ses | Pencere ve girdi | Dosyalar |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 ya da Vulkan, seçime göre | SDL2 | SDL2 | Disk |
| Windows | OpenGL, OpenGL ES 2.0 ya da Vulkan, seçime göre | SDL2 | SDL2 | Disk |
| macOS | OpenGL (sabit) | SDL2 | SDL2 | Disk |
| Android 5 ve üzeri | OpenGL ES 2.0 | SDL2 | SDL2 ve JNI (titreşim) | APK ve disk |
| Android 2.3 | OpenGL ES 2.0 (VideoCore IV'e göre ayarlanmış) | SDL2 (2.0.7) | SDL 2.0.7 | APK ve disk |
| Tarayıcı | WebGL | WebAudio üzerinde SDL2 | Emscripten ve SDL2 | Paketlenmiş `/res`, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (sabit) | `CMdaAudioOutputStream` | Symbian (tuş takımı, Bluetooth kumandalar, sensör; S60 5th'te dokunmatik ekran) | `E:` kartı |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (çoklu dokunma) | `E:` kartı |
| Dreamcast | GLdc (sabit); isteğe bağlı `pvr` | Editörde sessiz; isteğe bağlı `w3dAICA` | KallistiOS ve SDL2 | Romdisk ve `/ram` |
| PSP | sceGu (GE) | Editörde sessiz; oyunlarda SDL2 | pspdev ve SDL2 | Memory Stick |
| PS Vita | vitaGL (sabit) | Editörde sessiz; oyunlarda SDL2 | VitaSDK ve SDL2 | `app0:` ve `ux0:` |
| Nintendo 3DS | citro3d | Editörde sessiz; oyunlarda SDL2 | libctru ve SDL2 | romfs ve SD |

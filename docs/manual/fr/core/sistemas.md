# Chaque système en un coup d'œil

| Système | Graphismes | Son | Fenêtre et entrées | Fichiers |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 ou Vulkan, au choix | SDL2 | SDL2 | Disque |
| Windows | OpenGL, OpenGL ES 2.0 ou Vulkan, au choix | SDL2 | SDL2 | Disque |
| macOS | OpenGL (fixe) | SDL2 | SDL2 | Disque |
| Android 5 ou plus | OpenGL ES 2.0 | SDL2 | SDL2 et JNI (vibration) | APK et disque |
| Android 2.3 | OpenGL ES 2.0 (adapté au VideoCore IV) | SDL2 (2.0.7) | SDL 2.0.7 | APK et disque |
| Navigateur | WebGL | SDL2 sur WebAudio | Emscripten et SDL2 | `/res` empaqueté, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (fixe) | `CMdaAudioOutputStream` | Symbian (keypad, manettes Bluetooth, capteur ; tactile sur S60 5th) | Carte `E:` |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (multitouch) | Carte `E:` |
| Dreamcast | GLdc (fixe) ; `pvr` facultatif | Muet dans l'éditeur ; `w3dAICA` facultatif | KallistiOS et SDL2 | Romdisk et `/ram` |
| PSP | sceGu (GE) | Muet dans l'éditeur ; SDL2 dans les jeux | pspdev et SDL2 | Memory Stick |
| PS Vita | vitaGL (fixe) | Muet dans l'éditeur ; SDL2 dans les jeux | VitaSDK et SDL2 | `app0:` et `ux0:` |
| Nintendo 3DS | citro3d | Muet dans l'éditeur ; SDL2 dans les jeux | libctru et SDL2 | romfs et SD |

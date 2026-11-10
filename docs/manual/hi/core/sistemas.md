# हर सिस्टम, एक नज़र में

| सिस्टम | ग्राफ़िक्स | साउंड | विंडो और इनपुट | फ़ाइलें |
|---|---|---|---|---|
| Linux | OpenGL, OpenGL ES 2.0 या Vulkan, अपनी पसंद से | SDL2 | SDL2 | डिस्क |
| Windows | OpenGL, OpenGL ES 2.0 या Vulkan, अपनी पसंद से | SDL2 | SDL2 | डिस्क |
| macOS | OpenGL (फ़िक्स्ड) | SDL2 | SDL2 | डिस्क |
| Android 5 या उससे ऊपर | OpenGL ES 2.0 | SDL2 | SDL2 और JNI (वाइब्रेशन) | APK और डिस्क |
| Android 2.3 | OpenGL ES 2.0 (VideoCore IV के हिसाब से एडजस्ट) | SDL2 (2.0.7) | SDL 2.0.7 | APK और डिस्क |
| ब्राउज़र | WebGL | WebAudio पर SDL2 | Emscripten और SDL2 | पैक किया हुआ `/res`, `localStorage` |
| Nokia N95 (S60) | OpenGL ES 1.1 (फ़िक्स्ड) | `CMdaAudioOutputStream` | Symbian (कीपैड, Bluetooth कंट्रोलर, सेंसर; S60 5th पर टच) | `E:` कार्ड |
| Nokia N8 (Symbian^3) | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian (मल्टी-टच) | `E:` कार्ड |
| Dreamcast | GLdc (फ़िक्स्ड); `pvr` वैकल्पिक | एडिटर में बिना आवाज़; `w3dAICA` वैकल्पिक | KallistiOS और SDL2 | Romdisk और `/ram` |
| PSP | sceGu (GE) | एडिटर में बिना आवाज़; गेम में SDL2 | pspdev और SDL2 | Memory Stick |
| PS Vita | vitaGL (फ़िक्स्ड) | एडिटर में बिना आवाज़; गेम में SDL2 | VitaSDK और SDL2 | `app0:` और `ux0:` |
| Nintendo 3DS | citro3d | एडिटर में बिना आवाज़; गेम में SDL2 | libctru और SDL2 | romfs और SD |

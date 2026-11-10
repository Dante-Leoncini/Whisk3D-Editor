# 各系统一览

| 系统 | 图形 | 声音 | 窗口和输入 | 文件 |
|---|---|---|---|---|
| Linux | OpenGL、OpenGL ES 2.0 或 Vulkan，可选 | SDL2 | SDL2 | 磁盘 |
| Windows | OpenGL、OpenGL ES 2.0 或 Vulkan，可选 | SDL2 | SDL2 | 磁盘 |
| macOS | OpenGL（固定管线） | SDL2 | SDL2 | 磁盘 |
| Android 5 及以上 | OpenGL ES 2.0 | SDL2 | SDL2 和 JNI（振动） | APK 和磁盘 |
| Android 2.3 | OpenGL ES 2.0（针对 VideoCore IV 调整） | SDL2（2.0.7） | SDL 2.0.7 | APK 和磁盘 |
| 浏览器 | WebGL | 基于 WebAudio 的 SDL2 | Emscripten 和 SDL2 | 打包的 `/res`、`localStorage` |
| Nokia N95（S60） | OpenGL ES 1.1（固定管线） | `CMdaAudioOutputStream` | Symbian（键盘、蓝牙手柄、传感器；S60 5th 上支持触屏） | `E:` 卡 |
| Nokia N8（Symbian^3） | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian（多点触控） | `E:` 卡 |
| Dreamcast | GLdc（固定管线）；可选 `pvr` | 编辑器中无声；可选 `w3dAICA` | KallistiOS 和 SDL2 | Romdisk 和 `/ram` |
| PSP | sceGu（GE） | 编辑器中无声；游戏中用 SDL2 | pspdev 和 SDL2 | Memory Stick |
| PS Vita | vitaGL（固定管线） | 编辑器中无声；游戏中用 SDL2 | VitaSDK 和 SDL2 | `app0:` 和 `ux0:` |
| Nintendo 3DS | citro3d | 编辑器中无声；游戏中用 SDL2 | libctru 和 SDL2 | romfs 和 SD |

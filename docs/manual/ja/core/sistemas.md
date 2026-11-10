# 各システムの概要

| システム | グラフィックス | サウンド | ウィンドウと入力 | ファイル |
|---|---|---|---|---|
| Linux | OpenGL、OpenGL ES 2.0、Vulkanから選択 | SDL2 | SDL2 | ディスク |
| Windows | OpenGL、OpenGL ES 2.0、Vulkanから選択 | SDL2 | SDL2 | ディスク |
| macOS | OpenGL（固定機能） | SDL2 | SDL2 | ディスク |
| Android 5以降 | OpenGL ES 2.0 | SDL2 | SDL2とJNI（振動） | APKとディスク |
| Android 2.3 | OpenGL ES 2.0（VideoCore IV向けに調整） | SDL2（2.0.7） | SDL 2.0.7 | APKとディスク |
| ブラウザー | WebGL | WebAudio上のSDL2 | EmscriptenとSDL2 | まとめられた`/res`、`localStorage` |
| Nokia N95（S60） | OpenGL ES 1.1（固定機能） | `CMdaAudioOutputStream` | Symbian（キーパッド、Bluetoothゲームパッド、センサー。S60 5thではタッチ） | `E:`カード |
| Nokia N8（Symbian^3） | OpenGL ES 2.0 | `CMdaAudioOutputStream` | Symbian（マルチタッチ） | `E:`カード |
| Dreamcast | GLdc（固定機能）。`pvr`はオプション | エディターでは無音。`w3dAICA`はオプション | KallistiOSとSDL2 | Romdiskと`/ram` |
| PSP | sceGu（GE） | エディターでは無音。ゲームではSDL2 | pspdevとSDL2 | メモリースティック |
| PS Vita | vitaGL（固定機能） | エディターでは無音。ゲームではSDL2 | VitaSDKとSDL2 | `app0:`と`ux0:` |
| Nintendo 3DS | citro3d | エディターでは無音。ゲームではSDL2 | libctruとSDL2 | romfsとSD |

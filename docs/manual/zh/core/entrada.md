# 输入

在几乎所有系统上，键盘、鼠标、手指和手柄都由 **SDL2** 提供，编辑器和游戏都基于这些事件编写。SDL 不覆盖的部分由 Core 补全：

| 内容 | 来源 |
|---|---|
| 触屏 | 在 Android、浏览器、Vita 和 3DS 上由 SDL 提供。在 Symbian 上，`W3dTactil` 做与 SDL 相同的转换（第一根手指移动鼠标，其余手指作为手指上报）：S60 5th 上是单点触控，N8 上是多点触控 |
| 倾斜 | 在 N95 上使用 Nokia 的 Sensor API，在启动时加载（没有它游戏也照常运行）。在 Android 和 PC 上由 SDL 提供 |
| 手柄 | 其他系统由 SDL 提供。在 Symbian 上，支持通过蓝牙连接的 PS3 和 PS4 手柄（bthid-s60 驱动），按钮顺序与 SDL 相同。最多 4 个 |
| 振动 | Android 使用手机的振动器。浏览器使用 `navigator.vibrate`（在 iOS 上借助一个技巧使用系统的“嗒”声反馈）。桌面系统：无。这个 API 的承诺很少：“如果可以的话，短短震一下” |
| 剪贴板 | SDL（包括浏览器的剪贴板）；Symbian 上是 `CClipboard` |

在没有触摸屏的主机上（PSP、Dreamcast），由手柄移动一个光标，就像鼠标一样。

为什么重要：Symbian 的触屏手势和 Android 的手势走同一条路径，所以 Android 上手势的改进会自动惠及手机。在 Lua 中：[键盘、手柄、触屏和鼠标](../lua/api-entrada.md)。

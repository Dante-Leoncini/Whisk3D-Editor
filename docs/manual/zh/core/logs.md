# 日志

三个级别，printf 风格：`w3dLogf`（信息）、`w3dLogfW`（警告）和 `w3dLogfE`（错误）。`g_w3dLogNivel` 在运行时过滤（3 = 全部，0 = 不输出），用 `W3D_DEV_LOG=0` 编译则会把这些调用从可执行文件中去掉。最近的若干行还会保存在内存中：编辑器的 [Console](../editor/consola.md) 显示的就是这些内容。

| 系统 | 输出到哪里 |
|---|---|
| PC | `whisk3d.log`，每行带有毫秒时间戳以及 `[INFO]`、`[WARN]` 或 `[ERROR]` |
| Android | logcat，标签为 `Whisk3D`（`adb logcat -s Whisk3D`） |
| 浏览器 | 控制台：按级别使用 `console.log`、`console.warn` 和 `console.error` |
| Symbian | `E:\whisk3d.log` |
| Dreamcast | 串口（dc-tool 或 Flycast 的控制台） |
| PSP | 标准输出（PPSSPP、psplink），以及 `EBOOT.PBP` 旁边的 `whisk3d.log` |
| PS Vita | `sceClibPrintf` 和 `ux0:data/Whisk3D/whisk3d.log` |
| 3DS | `svcOutputDebugString` 和 `sdmc:/3ds/Whisk3D/whisk3d.log` |

为什么重要：真正的主机上没有终端，死机之后，存储卡上的日志就是唯一留下的东西。所以在 3DS、Vita 和 PSP 上，每一行都会立即写入。在 N95 上情况正好相反：每一行都立即写入要花几十毫秒，会让游戏卡顿。所以在那里，信息会先攒起来，每满 3 KB 写一次，警告立即写入，错误则强制写到磁盘。在 Lua 中：[消息和调试](../lua/api-mensajes-y-depuracion.md)。

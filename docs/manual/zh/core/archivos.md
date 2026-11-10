# 文件

读写只用一套 API：`w3dFileSystem::ReadFileBytes`、`ReadTextFile`、`WriteTextFile`、`ReadFileRange`（读取一部分，用于分段加载网格）、`ListDir`、`FileExists`。没有人会单独调用 `fopen`。

`ReadFileBytes` 按以下顺序查找：

1. **链接的库**（`lib:<nombre>/...` 路径）。
2. **内嵌的 pak：** 用“Assets: Packed”编译的游戏会把所有文件放在可执行文件内部。
3. **挂载的容器：** 即打开的 .w3d。容器**是挂载的，而不是解压的**：纹理、字体、脚本和界面直接从 zip 内部加载，谁都不用修改代码。
4. **APK 的 assets**（Android）：相对路径通过 NDK 的 `AAssetManager` 从 APK 中读取。
5. **磁盘。**

查找之前会先规范化路径（去掉 `.` 和 `..`）。曾经有个 .mtl 引用了 `modelos/crash/../../texturas/sombra_blob.png`，这个路径不在任何索引中，结果在 APK 里纹理显示成了白色。

| 系统 | 资源（`res/`） | 写入位置 |
|---|---|---|
| Windows、Linux | 在可执行文件旁边（在 Linux 上还有 `<prefijo>/share/Whisk3d/res`） | 用户文件夹；设置保存在 SDL 的文件夹中（`SDL_GetPrefPath`） |
| Android | 在 APK 内部 | `/storage/emulated/0`；导出的内容保存在“下载”中 |
| 浏览器 | 打包在 `.data` 中，挂载在 `/res` | 配置保存在 `localStorage` 中；保存会下载文件（[方法](../editor/proyecto.md)） |
| Symbian | 手机自己的路径 | `E:/`（存储卡）或 `C:/` |
| Dreamcast | 可执行文件内部的 romdisk（`/rd`） | `/ram`，关机后会清空 |
| PSP | 在 `EBOOT.PBP` 旁边 | `ms0:/`（PSP Go 上是 `ef0:/`） |
| PS Vita | 在 `.vpk` 内部（`app0:res`） | `ux0:` |
| 3DS | `.3dsx` 的 romfs（`romfs:`） | SD 卡（`sdmc:/`） |

该模块还有两个部分：

- **W3dRecursos** 记录每一份大数据（纹理、网格、动画）：谁在用它，以及什么时候释放。不是马上需要的东西会分片加载，每帧加载一点。
- **W3dConfig** 以 `clave=valor` 的形式保存设置。在浏览器中保存到 `localStorage`；在其他系统中保存到一个文本文件。它不申请动态内存，所以在 Symbian 上同样适用。在 Lua 中：[保存的配置](../lua/api-configuracion.md)。

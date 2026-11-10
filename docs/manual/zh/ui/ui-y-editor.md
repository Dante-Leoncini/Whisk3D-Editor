# 哪些属于 UI，哪些属于编辑器

编辑器是用 Whisk3D UI 做的，但在编辑器中看到的很多东西是编辑器自己的。属于 UI 的在 `libs/WhiskUI/`；属于编辑器的在 Whisk3D 仓库的 `main/` 中。

## 属于 UI

- 2D 绘制、带边框的卡片和矩形。
- 图标表和图集打包器。
- 位图字体、扩展图集，以及使用烘焙好的 TTF 字体的文本。
- 调色板和各项尺寸，以及它们的缩放。
- 按钮、选项卡、复选框和下拉菜单。
- 属性卡片及其各行，包括动画菱形。
- 文本框的状态及其绘制。

## 属于编辑器

| 内容 | 位置 |
|---|---|
| 视口：分割屏幕、视口的边框、菜单栏和工具栏 | `main/ui/ViewPorts/`（`ViewPorts.cpp`、`WithBorder.cpp`、`ToolbarBase.cpp`） |
| 属性面板：有哪些卡片、它们的选项卡、在行上的单击和拖动 | `main/ui/ViewPorts/Properties.cpp` |
| 模态窗口：颜色选择器、文件浏览器、确认框、进度条、屏幕键盘 | `main/ui/ViewPorts/PopUp/` |
| 通知 | `main/ui/ViewPorts/Notificaciones.cpp` |
| 面板的滚动条 | `main/ui/ViewPorts/ScrollBar.cpp` |
| 帮助提示框。文本由 `Button::ayuda` 保存 | `W3dAyudaRender`，位于 `main/ui/ViewPorts/LayoutInput.cpp` |
| 哪个按键、单击或触摸发给哪个控件，以及 N95 的按键 | `main/ui/ViewPorts/LayoutInput.cpp`、`main/app/controles.cpp`、`platform/symbian/src/` |
| 文本框的焦点函数 | `main/ui/ViewPorts/NumInput.cpp` |
| 翻译 `T()`。UI 只有 `W3dMenuTraducir` 这个钩子 | `main/config/W3dLang.cpp` |
| UI 的启动 | `main/app/W3dInitUI.cpp` |
| 用单独的图标拼出皮肤图集 | `main/io/SkinAtlas.cpp` |
| 重新计算缩放 | `SetGlobalScale`，位于 `main/ui/ViewPorts/ViewPorts.cpp` |
| 在 PC 和 Android 上读取 `skin.ini` | `loadColors`，位于 `main/app/main.cpp` |
| 编辑器自己的颜色：地面、坐标轴、法线 | `main/ui/W3dColors.cpp` |
| 皮肤文件：`font.png`、`font-ext.png`、`atlas/iconos/`、`skin.ini` | `res/Skins/Whisk3D/` |
| 扩展图集的生成器 | `tools/fuente_ext.py` |
| [2D Editor](../editor/editor-2d.md) 以及游戏的 `.w3dui` 界面：文本、图片、矩形、容器、九宫格（slice 9）、按钮和视频 | `main/objects/`（`UI.h`、`Texto2D.h`、`Boton2D.h`……）、`main/render/UIOverlay.cpp`、`main/io/UI2DFormato.cpp`、`main/io/Fuente2D.cpp`、`main/ui/ViewPorts/Editor2D.cpp` |

2D Editor 用 `W3dTextAtlas` 绘制文本，它属于 UI。在运行时用 stb_truetype 烘焙 TTF 则属于编辑器（`main/io/Fuente2D.cpp`）。

## 在 UI 中，但属于编辑器

位于 `libs/WhiskUI/` 中、但仍然属于编辑器的东西：

- `PropListMeshParts` 读取网格、骨架、动画、约束和脚本，并调用属性面板的 `RebindMaterialMeshPart()`。
- `PropColor` 保存颜色所来自的调色板和对象（`palRef`、`palObj`），供颜色选择器使用。
- `GroupProperties` 是属性面板的全局卡片列表。
- `core/UI.h` 中有编辑器的状态：Edit Mode 的触屏视图模式（`g_viewEditMode`）、鼠标移动量（`dx`、`dy`）、Alt、Shift 和 Ctrl 键（`LAltPressed`、`LShiftPressed`、`LCtrlPressed`），以及视口的尺寸（`paddingViewport`、`MinViewportHeight`、`MinViewportWidth`）。
- 大多数图标都是编辑器的工具。

还有哪些需要分离，以及库的现状：见[直接使用 UI](empezar.md) 的结尾。

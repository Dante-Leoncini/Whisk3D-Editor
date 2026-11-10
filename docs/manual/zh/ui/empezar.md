# 直接使用 UI

面向使用 C 或 C++ 编程、想不借助编辑器使用这套界面的人。你需要：

- **Core，以及它的场景模型。** UI 用 `w3dEngine` 绘制，有些行会读取场景中的对象（`objects/`、`animation/`、`script/`）。参见[直接使用 Core](../core/empezar.md)。
- **UI 的源代码。** 即 `libs/WhiskUI/` 中所有的 `.cpp`。包含目录是 `libs/`（用于 `"WhiskUI/draw/icons.h"`），以及子文件夹 `widgets/`、`text/`、`draw/`、`theme/` 和 `core/`，因为它们内部互相包含时不带文件夹名（`"icons.h"`、`"card.h"`）。
- **皮肤文件。** `font.png`（字母和卡片的边框）、`font-ext.png`（扩展图集）、`skin.ini`（颜色），以及新图标所在的 `atlas/iconos/` 文件夹。它们不随库提供：放在编辑器的仓库中，位于 `res/Skins/Whisk3D/`。

## 启动

和编辑器启动时做的一样，只是去掉了编辑器自己的部分：

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // 没有这个文件时，使用代码中的颜色

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // 圆角边框
CrearIconos(w, h);         // font.png 内部的图标
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // 直到需要时才加载
```

`GlobalScale` 初始为 1。要使用其他缩放，参见[颜色和缩放](tema.md)。关于 `atlas/iconos/` 中的单独图标，参见[绘制和图集](dibujo.md)。

每次窗口大小改变时：

```cpp
W3dPantallaAlto = alto;    // 用于把界面的 Y 转换为 Scissor 和 Viewport 的 Y
MenuPantallaW = ancho;     // 让菜单不超出屏幕
MenuPantallaH = alto;
```

## 2D 状态

UI 以像素为单位绘制，Y 轴向下，原点在左上角。每个控件都绘制在当前矩阵的原点：用 `Translatef` 来摆放它。绘制之前：

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Y 轴向下
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // 清晰的像素
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## 一张卡片

一张带有一个数字、一个复选框和一个按钮的卡片。每一行都指向你的一个变量：

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // 高度由卡片自己计算

// 每一帧，在设置好 2D 状态之后
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

UI 不读取键盘和鼠标：要由你传给它。用键盘时：

```cpp
// 下方向键
if (opciones->editando) opciones->button_down();   // 修改数值
else opciones->NextSelect();                        // 下面一行
// OK 或 Enter：在标题上折叠卡片；在某一行上，编辑它或按下它
opciones->EnterPropertieSelect();
// 取消：让这一行恢复原样
opciones->Cancel();
```

用鼠标时，指针下方是哪一行要由你自己计算：参见[卡片和属性](propiedades.md)。菜单则会接收坐标：参见[下拉菜单](menus.md)。

## 目前的状态，直说吧

- **还不能用于生产环境。** 仓库的 `README.md` 里是这么说的，事实也是如此：它内部还有编辑器的代码，正在逐步分离。
- **没有单独使用它的示例。** [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples) 中 `ui/` 下的示例是带 Lua 的 `.w3dui` 界面，由游戏运行时运行：那属于编辑器。本页的代码是用编辑器所用的相同调用拼出来的，并非来自经过测试的示例。
- **目前无法单独链接。** `PropList.cpp` 会调用 `RebindMaterialMeshPart()`，而它位于编辑器的属性面板中。`TextField.h` 的焦点函数（`TextFieldEnfocar`、`TextFieldCancelar`、`TextFieldInputChar`、`TextFieldFinDeCuadro`）定义在编辑器中，位于 `main/ui/ViewPorts/NumInput.cpp`。
- **依赖 Core 的场景模型。** `PropertieBase.h` 包含了 `objects/Objects.h`，属性列表会读取网格、骨架、动画和脚本。
- **缩放由编辑器重新计算。** `SetGlobalScale` 位于 `main/ui/ViewPorts/ViewPorts.cpp`。单独使用时，需要手动重新计算各项尺寸（[颜色和缩放](tema.md)）。
- **一切都是全局的。** 每个程序只有一个字体（`WhiskFont`）、一张图标表（`IconsUV`）、一个打开的菜单（`MenuAbierto`）和一个获得焦点的文本框（`g_textFieldActivo`）。
- **头文件暴露了 OpenGL 的类型**（`GLfloat`、`GLshort`、`GLuint`），尽管已经不再调用 OpenGL。
- **接下来的计划**，根据 `README.md`：清理属于编辑器的代码，并编写使用示例。

逐项说明哪些属于 UI、哪些属于编辑器：[哪些属于 UI，哪些属于编辑器](ui-y-editor.md)。有疑问可以到 [Telegram](https://t.me/Whisk3D) 群组提问。

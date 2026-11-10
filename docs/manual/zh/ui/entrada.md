# 输入和文本框

## UI 不读取事件

Whisk3D UI 不打开窗口，也不监听鼠标、键盘或触摸屏。事件由程序接收（在几乎所有系统上通过 SDL2，参见 Core 的[输入](../core/entrada.md)），再把相应的内容传给每个控件：

| 事件 | 传给谁 |
|---|---|
| 鼠标或手指移动 | `PopupMenu::MouseMove(x, y)`；按钮和选项卡的 `hover`；卡片中的 `PropHoverGroup` 和 `PropHoverFila` |
| 单击或触摸 | `PopupMenu::Click(x, y)`；`Button::Contains` 和 `Tab::Contains`；在卡片中，传给程序计算出的那一行 |
| 滚轮 | `PopupMenu::Wheel(dir)` |
| 方向键 | 菜单、卡片和行的 `button_up`、`button_down`、`button_left` 和 `button_right` |
| OK 或 Enter | `PopupMenu::Enter()`、`GroupPropertie::EnterPropertieSelect()` |
| 取消或 Esc | `PopupMenu::Cerrar()`、`GroupPropertie::Cancel()` |
| 一个字母 | 获得焦点的文本框 |

坐标是屏幕像素，Y 轴向下。

## 没有鼠标时

一切都可以只用四个方向键、OK 和取消来操作：在 N95 上就是这样用的。UI 为此提供了：

- 卡片可以逐行浏览，并跳过不能选择的内容。
- 菜单可以用方向键浏览，高亮的选项永远不会移出视野。
- 数字可以用方向键修改，无需输入。
- 键盘焦点是可见的：按钮的 `focoMenu` 和选项卡的 `foco` 使用 `accent`，菱形上则有 `g_kfFocoProp` 的白色边框。

手机上的哪个键对应哪一项（软键或 OK）不属于 UI：由编辑器和各个系统负责转换。

## 触摸屏

- 一次触摸就是一次单击。
- 打开子菜单的那一下触摸不会选中子菜单中的任何内容（[下拉菜单](menus.md)）。
- `UITactil` 会为手指放大按钮和栏（[颜色和缩放](tema.md)）。目前没有任何地方会开启它。

拖动滚动、长按或双击，由使用 UI 的一方处理。

## 文本框

`TextField`（`widgets/TextField.h`）是文本框的状态：文本（`text`）、光标位置（`caret`）以及是否全部选中（`selectAll`）。它不绘制，也不读取按键：只是数据。

| 函数 | 作用 |
|---|---|
| `SetText(t)` | 设置文本，光标放在末尾 |
| `SelectAll()` | 全部选中：按下的第一个键会替换掉它 |
| `InsertChar(c)` | 在光标处写入一个字节 |
| `Backspace()`、`DelForward()` | 向后或向前删除 |
| `CaretIzq()`、`CaretDer()` | 移动光标。全部选中时，会取消选择 |

删除和移动光标时，会把多字节的字母（例如 `ñ` 或西里尔字母）当作一个字母处理。要输入这样的字母，就把每个字节分别传给 `InsertChar`。

同一时间只有一个文本框获得焦点：`g_textFieldActivo`，没有时为 `NULL`。按键会发给它。

```cpp
TextField nombre;
nombre.SetText("Jugador 1");
nombre.SelectAll();
g_textFieldActivo = &nombre;

// 每输入一个字母
if (g_textFieldActivo) g_textFieldActivo->InsertChar(c);

// 绘制时，先设置好文本颜色
RenderTextField(nombre, g_textFieldActivo == &nombre, textAlign::left, 200);
```

`RenderTextField(caja, foco, alineación, ancho)` 在当前原点绘制它：

- 没有焦点时，只显示文本。
- 有焦点且全部选中时，文本以白色显示在 `accentDark` 上。
- 有焦点且没有选中时，会出现光标：一条每 530 ms 闪烁一次的竖线。

`TextFieldParpadeoTick()` 在竖线状态改变时返回 `true`：这时就该重绘了。

`TextField.h` 还声明了 `TextFieldEnfocar`、`TextFieldCancelar`（Esc：恢复为获得焦点之前的文本）、`TextFieldInputChar` 和 `TextFieldFinDeCuadro`。它们目前定义在编辑器中，位于 `main/ui/ViewPorts/NumInput.cpp`：单独的 UI 并不包含它们。

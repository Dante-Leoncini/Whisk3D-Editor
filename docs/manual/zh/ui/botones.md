# 按钮、选项卡和复选框

## 按钮

`Button`（`widgets/Button.h`）是一张带文本、左侧可带一个图标的卡片。深色边框让它看起来像按钮。鼠标悬停时（`hover`），边框变成白色，背景稍微变亮。

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // 宽度随文本而定
guardar->Resize(200);                                         // 最宽不超过这个值

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // 它的位置，供鼠标使用
guardar->hover = guardar->Contains(mx, my);
```

`Render` 在当前原点绘制，并不知道自己在屏幕上的位置：`sx` 和 `sy` 由绘制它的一方设置，`Contains` 拿它们来比较。构造函数的第三个参数是 `adaptar`：为 `false` 时，按钮占满 `Resize` 给它的全部宽度。

| 字段 | 作用 |
|---|---|
| `centrado` | 图标和文本居中，就像 Accept 或 Cancel 按钮那样 |
| `cuadrado` | 宽度等于高度：只有图标的按钮 |
| `altoRenglon` | 使用属性行的高度，而不是栏的高度 |
| `caretMenu` | 右侧一个小箭头：提示它会打开一个菜单 |
| `tinte` | 背景颜色，代替 `gris` |
| `colorTexto`、`colorBorde` | 文本和图标的颜色，以及边框的颜色 |
| `focoMenu` | 通过键盘获得焦点：边框和文本使用 `accent` |
| `alpha` | 背景和边框的不透明度，让后面的东西透出来 |
| `iconFlip` | 图标镜像：1 为水平，2 为垂直 |
| `barra` | 一个 0 到 1 的数：按钮按这个比例用 `accentDark` 填充，文本居中 |
| `relleno` | 内部一个该颜色的方块：用于选择颜色 |
| `editField` | 一个 `TextField`：按钮绘制成文本框，用于就地重命名 |
| `visible` | 为 `false` 时不绘制 |

有些字段按钮会保存但不绘制：由包含它的一方使用。`desplegable` 表示它会打开一个菜单。`separador` 要求在栏中它前面画一条线。`rol` 用来在栏中找到它，而不依赖顺序。`ayuda` 是鼠标停留在它上面时出现的提示框文本；提示框本身不属于 UI。

高度是 `UIBotonAltura()`（[颜色和缩放](tema.md)），使用 `altoRenglon` 时为 `RenglonHeightGS + 2 * GlobalScale`。它永远不会比一行文字更窄。

## 选项卡

`Tab`（`widgets/Tab.h`）是一个没有底边框的按钮：与它下方的内容融为一体。活动的选项卡（`activa`）稍高一些，颜色与内容相同；其他的颜色更深。文本在获得键盘焦点（`foco`）时使用 `accent`，是活动选项卡或在鼠标下方时为白色，否则为灰色。

用法与按钮相同：`Resize(maxW)`、`Render()` 和 `Contains(mx, my)`，`sx` 和 `sy` 由绘制它的一方设置。

## 复选框和颜色样块

复选框的外观只由一个地方决定，即 `widgets/card.h`，这样菜单中的复选框和卡片中的复选框就不可能不一样。

| 函数 | 作用 |
|---|---|
| `DibujarCheckbox(lado, prendida)` | 绘制方框，如果已勾选，再画上勾号 |
| `DibujarMuestraColor(lado, rgba)` | 绘制一个该颜色的方块，带有复选框的圆角 |
| `CheckboxLado()` | 边长：一行文字的高度 |
| `CheckboxColorCaja(prendida)`、`CheckboxColorTilde()` | 方框和勾号的颜色 |
| `CheckboxTilde()` | 勾号字符 `✔` |

勾选时，方框为 `grisUI`，勾号为背景色。未勾选时，方框为背景色，没有勾号。

```cpp
DibujarCheckbox(CheckboxLado(), sonidoPrendido);
```

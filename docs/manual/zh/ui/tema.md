# 颜色和缩放

## 调色板

UI 的颜色是角色，而不是固定的颜色：即枚举 `ColorID`（`theme/colores.h`）。

| 角色 | 用途 |
|---|---|
| `background` | 背景。未勾选复选框的方框，以及静止状态下按钮的边框 |
| `blanco` | 鼠标下方或被选中元素的文本和边框 |
| `accent` | 焦点：菜单的边框、正在编辑的内容、活动的选项、滑块的填充 |
| `accentDark` | 选中文本的背景，以及数值按钮的填充条 |
| `negro` | `PropSeparator` 的线 |
| `gris` | 卡片、按钮和菜单的主体 |
| `headerColor` | 菜单中高亮的行，以及滑块的轨道 |
| `negroTransparente` | 25 % 的黑色。UI 不使用它 |
| `grisUI` | 普通文本，以及已勾选复选框的方框 |
| `grisLinea` | 菜单中分隔各组的线 |

`ListaColores` 以 `float` 保存它们，`ListaColoresUbyte` 以字节保存。`SetColorID` 把其中一个设为绘制颜色，alpha 可选：

```cpp
SetColorID(ColorID::accent);         // 不透明
SetColorID(ColorID::grisUI, 0.5f);   // 半透明
```

`loadColorsW3d(ruta)` 从一个文本文件中读取它们，每行一个颜色：名称和 0 到 1 的四个分量，例如 `accent 0.0 1.0 0.0 1.0`。它会忽略不认识的名称，所以同一个 `skin.ini` 可以带有其他程序的颜色。如果打不开文件，就返回 `false`。没有这个文件时，使用代码中的颜色（`theme/colores.cpp`）；Whisk3D 的皮肤会改变它们，例如 `accent` 会变成绿色。

有些颜色是写死在代码里的，不来自调色板：未保存修改的菱形所用的绿色、错误 `PropLabel` 的红色、未填完的 `PropText` 的红色边框，以及禁用的 `PropButton` 的灰色。

## 缩放

基础尺寸在 `core/UI.h` 中，以像素为单位：

| 尺寸 | 值 |
|---|---|
| `margin`、`padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`、`CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale` 把它们相乘，而且总是乘以整数：这样像素字母才能保持清晰。例如，在 N95 的 240 像素屏幕上为 1，在 PC 上为 3。

每个尺寸都有一个已经乘好的版本，名称以 `GS` 结尾：`RenglonHeightGS`、`gapGS`、`borderGS`、`bordersGS`（边框的两倍）、`CharacterWidthGS`、`LetterHeightGS`……控件使用的就是这些。

UI 不带重新计算它们的函数：编辑器的是 `SetGlobalScale`，位于 `main/ui/ViewPorts/ViewPorts.cpp`。单独使用时，这样做：

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

每个控件的尺寸在它的 `Resize` 中计算：改变缩放之后，需要对每张卡片、每个按钮和每个菜单重新调用它。

## 紧凑或触屏

`UIBotonAltura()` 给出按钮的高度，`UIBarPadding()` 给出栏中按钮上下的留白。两者都取决于 `UITactil`：

- `false`，紧凑模式，为 N95 设计：按钮高度为一行文字加上边框，再减去 `2 * GlobalScale`，栏在上下各留 `GlobalScale`。
- `true`：按钮和栏都多加一个 `gap`，方便手指点按。边框不会变粗。

`UITactil` 初始为 `false`，目前没有任何地方会开启它。

# 文本

## 位图字体

界面的字体是在 `font.png` 中手绘的。每个字符是一个 **5x11** 像素的格子，前进宽度为 **6**（`CharacterWidth`）。一行高 **12**（`RenglonHeight`）。它是等宽字体：所有字符占用的宽度相同。缩放时，所有数值都乘以 `GlobalScale`。

`Font`（`text/font.h`）保存每个字符的坐标。只有一个，全局的：`WhiskFont`。用图集的尺寸和它的纹理来创建（[直接使用 UI](empezar.md)）。

`font.png` 包含字母、数字和 ASCII 中几乎所有的符号，另外还有 `ñ`、`Ñ`、`¡`、`°` 和勾号 `✔`。有两个是由别的字符翻转得到的：`\` 是镜像的 `/`，`¿` 是旋转过来的 `?`，位置稍微低一些。

## 重音符号

带重音的字母并没有画出来：而是由基础字母加上上方的符号组合而成。

- **锐音符**和**波浪符**取自 `font.png`：锐音符是从逗号截取的一部分，波浪符是 `ñ` 上的那个。它们是一条 2 像素高的条带，在大写字母上方的位置更高。
- 其他符号（抑音符、扬抑符、分音符、上点、短音符、下加符和反尾形符）在扩展图集中。

## 扩展图集

`font-ext.png` 大小为 128x64，存放 `font.png` 中放不下的内容：

- 各种重音符号，
- `ß`、`ı`、`ł`、`Ł`、`œ`、`Œ`、`æ` 和 `Æ`，
- `;`、`^`、`~`、`` ` ``、`±`、`«`、`»`、`–`、`—` 和 `…`，
- **西里尔字母**。与拉丁字母相同的那些（`А В Е К М Н О Р С Т Х` 和 `а е о р с у х`）使用 `font.png` 中的字形。

它直到某段文本第一次需要时才加载：`SetAtlasExtendido` 只保存路径。在英语或西班牙语中，它可能永远不会被加载，而在 N95 上，内存很宝贵。如果缺少这个文件，组合字母会显示为没有符号的样子，扩展图集中的字符会显示为 `?`。

这些表（每个字符在哪个格子、每个字母如何组合）在 `text/FuenteExt.h` 中。不要手工编辑：它们由 `tools/fuente_ext.py` 生成，这个工具也会绘制 PNG，位于编辑器的仓库中。

有了这两张纹理，可以显示西班牙语、葡萄牙语、法语、德语、意大利语、波兰语、土耳其语和俄语。日语、中文和印地语则不行：任何图集中都没有的字符会显示为 `?`。`WhiskFont->Soporta(cp)` 告诉你某个字符能否绘制。

文本使用 UTF-8。`UTF8_Char` 读取 1 到 3 字节的序列。4 字节的序列（例如表情符号）无法读取：会显示为问号。

## 绘制文本

```cpp
SetColorID(ColorID::blanco);
RenderBitmapText("Hola", textAlign::left, 200);   // 200：最大宽度，以像素为单位
```

它在当前矩阵的原点、用当前颜色绘制。每段文本每个图集一次绘制调用：一次，如果用到扩展图集则是两次。结束时会重新绑定 `font.png` 的纹理，让之后绘制的图标能找到它。

## 测量和截断

文本的宽度等于字母数乘以 `CharacterWidthGS`。`UTF8_Largo(texto)` 统计的是字母而不是字节：一个 `ñ` 占两个字节，但只算一个。

如果超出最大宽度，`RenderBitmapText` 会截断它：

| 对齐方式 | 位置 | 截掉什么 |
|---|---|---|
| `textAlign::left` | 从原点开始 | 结尾 |
| `textAlign::right` | 在原点结束 | 开头：在输入框中能看到最后输入的内容 |
| `textAlign::center` | 在最大宽度内居中 | 结尾 |

`RenderBitmapEtiqueta(texto, ancho)` 用于行的标签：它右对齐，但如果放不下，会截掉结尾并加上 `..`（“Motion vecto..”）。

`RenderBitmapFloat(valor, alineación, ancho, unidad)` 绘制一个带四位小数、后面紧跟单位的数字：`90.0000°`。它不使用 `%f`，因为 Symbian 的 `printf` 不能格式化 `float`。

## 使用 TTF 字体的文本

如果需要任意大小的字母，可以用 `w3dui::W3dTextAtlas`（`text/W3dTextAtlas.h`）。它只有一个头文件。它绘制一种已经烘焙到图集中的 TTF 字体，每个字母都会自行缩放和定位：适用于标题、分数和动画文字。它不会在运行时光栅化 TTF。

图集在 PC 上用 `text/bake_atlas.py` 烘焙，需要 Pillow：`bake_atlas.py <fuente.ttf> <lado del atlas> <alto de letra> <salida>`。图集的边长为 128、256、512 或 1024。它烘焙 ASCII 和 `áéíóúÁÉÍÓÚñÑüÜ¿¡`。输出 `<salida>.png`（灰度覆盖率，无 alpha）和 `<salida>.w3dfnt`（每个字母的尺寸）。

```cpp
#include "WhiskUI/text/W3dTextAtlas.h"
#include "io/w3dFilesystem.h"

std::vector<unsigned char> fnt, png;
w3dFileSystem::ReadFileBytes("fuentes/titulo.w3dfnt", fnt);
w3dFileSystem::ReadFileBytes("fuentes/titulo.png", png);

w3dui::W3dTextAtlas titulo;
titulo.Load(&fnt[0], fnt.size(), &png[0], png.size());

titulo.Begin();                                                // 图集、混合和状态
titulo.DrawText("Puntaje: 120", 10, 10, 24, 1, 1, 1, 1);      // x、y、高度、r、g、b、a
float ancho = titulo.TextWidth("Puntaje: 120", 24);
```

`(x, y)` 是这一行的顶部，Y 轴向下。`DrawText` 把所有字母合并成一次绘制。`DrawGlyph` 只绘制一个字母，用于大小或颜色不同的字母。`LineHeight(alto)` 给出一行的高度。默认使用预乘 alpha 混合（`MezclaPremult`）：文本可以是任何颜色。

## 不涉及 GPU 的表

`text/W3dFont.h` 中有同一套位图字体的表，但不涉及 GPU：每个字母在 `font.png` 中的位置（`W3dFontGetGlyphPx`）及其坐标（`W3dFontGetGlyph`）。可用于用同样的字体拼出另一张图集。

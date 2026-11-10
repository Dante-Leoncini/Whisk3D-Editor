# 绘制和图集

## 如何绘制

一切都通过 `w3dEngine`（[图形](../core/graficos.md)）。UI 绘制三角形（`DrawTrianglesArray`、`DrawTrianglesByte`）和线，顶点使用 `float`。它不用 `TRIANGLE_STRIP` 绘制，也不用 `short` 顶点：N95 的驱动在帧的 2D 阶段画不出它们。

坐标是屏幕像素，Y 轴向下，原点在左上角。每个控件都绘制在当前矩阵的原点。要摆放它，就在 `PushMatrix` 和 `PopMatrix` 之间使用 `Translatef`。

## `glesdraw` 的函数

`draw/glesdraw.h` 是最底层的绘制，其他部分都在用它：

| 函数 | 绘制什么 |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | 一个四边形：4 个 `GLshort` 顶点，按三角形带的顺序排列，带 8 个 UV。它会把它转换成两个三角形 |
| `W3dDrawElemsB(verts, uvs, idx, n)` | 一个使用 `GLubyte` 索引的小网格，最多 64 个 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | 最多 21 条线，使用 3 分量的 `GLshort` 顶点 |
| `W3dDrawLinesF(verts, idx, n)` | 使用 3 分量 `float` 顶点的线，每次绘制 40 个顶点 |

转换成 `float` 的顶点会放进一个有 256 个位置、循环使用的固定缓冲区，而绝不放在栈上：N95 的驱动读取指针的时机很晚。

`W3dPantallaAlto` 是屏幕的高度。由使用 UI 的一方在窗口大小改变时设置，用于把界面的 Y 转换为 `Scissor` 和 `Viewport` 的 Y，后两者是从下往上计数的。

## 节点和矩形

`Object2D`（`draw/object2D.h`）是一个带子节点（`Childrens`）的节点：它有尺寸、颜色、不透明度和 `visible`。`Render()` 先绘制它自己，再绘制它的子节点。删除它时，也会删除子节点。

`Rec2D`（`draw/rectangle.h`）是一个单色矩形。`SetSize(x, y, w, h)` 设置它的位置，`SetColor(r, g, b)` 设置它的颜色。它不读取纹理：要在关闭 `Texture2D` 的情况下绘制。

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false：使用当前颜色，而不是它自己的颜色
gfx::Enable(gfx::Texture2D);
```

## 卡片

`Card`（`widgets/card.h`）是按钮、菜单和属性卡片所用的圆角矩形。它是一个**九宫格（9-patch）**：16 个顶点，拉伸中间部分，保留 `2 * GlobalScale` 大小的角。角和中间部分取自 `font.png` 的一个角落。`CalcCardUV(anchoAtlas, altoAtlas)` 计算这些坐标：用纹理的尺寸调用一次即可。

| 函数 | 作用 |
|---|---|
| `Resize(w, h)` | 改变尺寸 |
| `RenderObject(propio)` | 绘制填充 |
| `RenderBorder(propio)` | 只绘制边框 |
| `RenderObjectTab()`、`RenderBorderTab()` | 同上，但底边是直的：用于选项卡 |

`propio` 为 `true` 时使用它自己的颜色（`color` 和 `opacity`）；为 `false` 时使用当前颜色。

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## 皮肤图集

整个界面只用一张纹理。`font.png` 大小为 128x128，包含字母、卡片的边框和旧图标。只有它，UI 也能运行：`CrearIconos` 会从 `font.png` 内部取出图标。

新图标是单独的文件，放在 `atlas/iconos/` 中。编辑器在启动时会拼出一张更大的纹理（`main/io/SkinAtlas.cpp`）：把整张 `font.png` 放在 (0,0)，这样它的坐标不会改变，再把每个图标打包到剩余的空间里。然后通过 `CrearIconosDesde` 告诉 UI 每个图标的位置。拼这张图集是编辑器的事；打包器和图标表属于 UI。参见[图标](iconos.md)。

## 打包器

`W3dAtlasPack`（`draw/W3dAtlasPacker.h`）把多张 RGBA 图像合并成一张。它不碰 GPU：输入像素，输出一张画布，由调用方上传。它同样可以用于游戏：把一个关卡的纹理合并成一张，可以减少纹理切换。

```cpp
std::vector<W3dAtlasItem> items(n);       // 每张图像的：rgba、w、h
items[0].fijo = true;                     // 这一张必须放在指定的位置
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // 图像之间留 2 像素的间隙
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x、items[i].y：每张图像的位置
}
```

它会尝试越来越大的画布：128x128、256x128、256x256、512x256、512x512 和 1024x512。如果连最后一个都放不下，就返回 `false`。非固定的图像按行从左到右排列，跳过固定的图像。图像之间的间隙可以避免线性过滤把一张图像与相邻的图像混在一起。

## 遗留下来的东西

`draw/sprites.h`（`Sprite`）和 `draw/image.h`（`Image`）没有被任何地方包含。它们使用 C++11 的初始化器，在 Symbian 上无法编译。

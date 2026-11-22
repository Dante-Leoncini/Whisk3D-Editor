# 图标

## 文件

每个图标都是一张 **10x10** 的 PNG，位于编辑器仓库的 `res/Skins/Whisk3D/atlas/icons/` 中。目前共有 106 个。像素是透明背景上的白色：图标会采用绘制时所用的颜色。这样同一个图标可以用灰色、白色或强调色显示。

## 枚举和名称

`draw/icons.h` 中有枚举 `IconType` 和总数 `ICON_TOTAL`（106）。在 `draw/icons.cpp` 中，`KNames` 按同样的顺序给出每个图标的名称：即去掉 `.png` 的文件名。

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta，不存在时为 -1
```

几乎所有图标的名称都与枚举中的相同。例外是 `lockOpen`，它的文件是 `candado_abierto.png`。

很多图标属于编辑器：建模工具、对象类型、动画曲线。通用的图标也有：`arrow` 和 `arrowRight`（折叠箭头）、`visible` 和 `hidden`、`folder`、`save`、`trash`、`plus` 和 `minus`、`list` 和 `grid`、`lock`、`notifOk` 和 `notifError`、`scissors`、`copy` 和 `paste`。

## 坐标

`IconsUV` 是一个 `std::vector<IconUV*>`，保存每个图标在图集中的坐标（`uvs`，8 个数）。有两种填充方式：

- `CreateIcons(width, height)`：使用记录每个图标在 `font.png` 内部位置的表（`TableLegacy`，位于 `icons.cpp`）。这是没有 `atlas/icons/` 文件夹时的方式。
- `CreateIconsFrom(rects, width, height)`：使用自己的矩形列表，即拼好的图集的列表。`x = -1` 的矩形会使用 `font.png` 中的那个：没有 PNG 的图标仍然会显示。

`SetIconScale(scale)` 调整 `IconSizeGS` 和两个网格：`IconMesh`（一个图标的方块）和 `IconLineMesh`（一个一行文字高的方块，用于树状结构的连线）。

## 绘制一个图标

设置好图集后，在当前原点绘制：

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`、`Tab`、`PopupMenu` 和 `GroupPropertie` 以枚举的 `int` 值接收图标，`-1` 表示没有图标。

## 添加一个图标

1. 一张 10x10 的 PNG，放在 `atlas/icons/` 中。
2. 在枚举 `IconType` 末尾加一行，并把 `ICON_TOTAL` 加一。
3. 在 `KNames` 末尾加上它的名称。
4. 在 `TableLegacy` 末尾加一行，填上缺少 PNG 时用来替代它的 `font.png` 图标。

没有第 4 步，在没有拼好的图集时，这个图标就没有坐标。

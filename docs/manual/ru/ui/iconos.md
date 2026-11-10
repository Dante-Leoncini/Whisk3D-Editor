# Иконки

## Файлы

Каждая иконка — это PNG размером **10x10** в `res/Skins/Whisk3D/atlas/iconos/`, в репозитории редактора. Сейчас их 106. Пиксели белые на прозрачном фоне: иконка принимает цвет, которым её рисуют. Поэтому одна и та же иконка годится и серой, и белой, и акцентного цвета.

## Enum и имена

В `draw/icons.h` есть enum `IconType` и количество, `ICON_TOTAL` (106). В `draw/icons.cpp` массив `KNombres` даёт имя каждой иконки, в том же порядке: это имя файла без `.png`.

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, или -1, если такой нет
```

Почти все называются так же, как в enum. Исключение — `candadoAbierto`, это файл `candado_abierto.png`.

Многие относятся к редактору: инструменты моделирования, типы объектов, кривые анимации. Общие тоже есть: `arrow` и `arrowRight` (стрелки сворачивания), `visible` и `hidden`, `carpeta`, `guardar`, `borrar`, `mas` и `menos`, `lista` и `cuadricula`, `candado`, `notifOk` и `notifError`, `tijera`, `copiar` и `pegar`.

## Координаты

`IconsUV` — это `std::vector<IconUV*>` с координатами каждой иконки в атласе (`uvs`, 8 чисел). Он заполняется двумя способами:

- `CrearIconos(ancho, alto)`: по таблице, где лежит каждая иконка внутри `font.png` (`TablaLegacy`, в `icons.cpp`). Это способ без папки `atlas/iconos/`.
- `CrearIconosDesde(rects, ancho, alto)`: по собственному списку прямоугольников — списку собранного атласа. Прямоугольник с `x = -1` использует прямоугольник из `font.png`: иконка без своего PNG всё равно появляется.

`SetIconScale(escala)` настраивает `IconSizeGS` и оба меша: `IconMesh`, квадрат одной иконки, и `IconLineMesh`, квадрат высотой в строку текста, для линий деревьев.

## Нарисовать иконку

С установленным атласом, в текущем начале координат:

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` и `GroupPropertie` получают иконку как `int` из enum или `-1`, если иконки нет.

## Добавить иконку

1. PNG размером 10x10 — в `atlas/iconos/`.
2. Строку в конец enum `IconType` и плюс один к `ICON_TOTAL`.
3. Её имя в конец `KNombres`.
4. Строку в конец `TablaLegacy` с иконкой из `font.png`, которая заменит её, если PNG нет.

Без шага 4 иконка остаётся без координат, когда собранного атласа нет.

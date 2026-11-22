# Íconos

## Los archivos

Cada ícono es un PNG de **10x10** en `res/Skins/Whisk3D/atlas/icons/`, en el repositorio del editor. Hoy son 106. Los píxeles son blancos sobre transparente: el ícono toma el color con que se dibuja. Así el mismo ícono sirve en gris, en blanco o en el color de acento.

## El enum y los nombres

`draw/icons.h` tiene el enum `IconType` y la cantidad, `ICON_TOTAL` (106). En `draw/icons.cpp`, `KNames` da el nombre de cada uno, en el mismo orden: es el nombre del archivo sin `.png`.

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta, o -1 si no existe
```

Casi todos se llaman igual que en el enum. La excepción es `lockOpen`, que es el archivo `candado_abierto.png`.

Muchos son del editor: herramientas de modelado, tipos de objeto, curvas de animación. Los generales también están: `arrow` y `arrowRight` (las flechas de plegar), `visible` y `hidden`, `folder`, `save`, `trash`, `plus` y `minus`, `list` y `grid`, `lock`, `notifOk` y `notifError`, `scissors`, `copy` y `paste`.

## Las coordenadas

`IconsUV` es un `std::vector<IconUV*>`, con las coordenadas de cada ícono en el atlas (`uvs`, 8 números). Se llena de dos formas:

- `CreateIcons(width, height)`: con la tabla de dónde está cada ícono adentro de `font.png` (`TableLegacy`, en `icons.cpp`). Es la forma sin la carpeta `atlas/icons/`.
- `CreateIconsFrom(rects, width, height)`: con una lista propia de rectángulos, la del atlas armado. Un rectángulo con `x = -1` usa el de `font.png`: un ícono sin su PNG sigue apareciendo.

`SetIconScale(scale)` ajusta `IconSizeGS` y las dos mallas: `IconMesh`, el cuadrado de un ícono, y `IconLineMesh`, un cuadrado del alto de un renglón, para las líneas de los árboles.

## Dibujar un ícono

Con el atlas puesto, en el origen actual:

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`, `Tab`, `PopupMenu` y `GroupPropertie` reciben el ícono como un `int` del enum, o `-1` para ninguno.

## Agregar uno

1. El PNG de 10x10, en `atlas/icons/`.
2. Una fila al final del enum `IconType`, y sumarle uno a `ICON_TOTAL`.
3. Su nombre al final de `KNames`.
4. Una fila al final de `TableLegacy`, con el ícono de `font.png` que lo reemplaza si falta el PNG.

Sin la fila 4, el ícono queda sin coordenadas cuando no hay atlas armado.

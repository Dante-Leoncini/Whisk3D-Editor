# Íconos

## Los archivos

Cada ícono es un PNG de **10x10** en `res/Skins/Whisk3D/atlas/iconos/`, en el repositorio del editor. Hoy son 106. Los píxeles son blancos sobre transparente: el ícono toma el color con que se dibuja. Así el mismo ícono sirve en gris, en blanco o en el color de acento.

## El enum y los nombres

`draw/icons.h` tiene el enum `IconType` y la cantidad, `ICON_TOTAL` (106). En `draw/icons.cpp`, `KNombres` da el nombre de cada uno, en el mismo orden: es el nombre del archivo sin `.png`.

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta, o -1 si no existe
```

Casi todos se llaman igual que en el enum. La excepción es `candadoAbierto`, que es el archivo `candado_abierto.png`.

Muchos son del editor: herramientas de modelado, tipos de objeto, curvas de animación. Los generales también están: `arrow` y `arrowRight` (las flechas de plegar), `visible` y `hidden`, `carpeta`, `guardar`, `borrar`, `mas` y `menos`, `lista` y `cuadricula`, `candado`, `notifOk` y `notifError`, `tijera`, `copiar` y `pegar`.

## Las coordenadas

`IconsUV` es un `std::vector<IconUV*>`, con las coordenadas de cada ícono en el atlas (`uvs`, 8 números). Se llena de dos formas:

- `CrearIconos(ancho, alto)`: con la tabla de dónde está cada ícono adentro de `font.png` (`TablaLegacy`, en `icons.cpp`). Es la forma sin la carpeta `atlas/iconos/`.
- `CrearIconosDesde(rects, ancho, alto)`: con una lista propia de rectángulos, la del atlas armado. Un rectángulo con `x = -1` usa el de `font.png`: un ícono sin su PNG sigue apareciendo.

`SetIconScale(escala)` ajusta `IconSizeGS` y las dos mallas: `IconMesh`, el cuadrado de un ícono, y `IconLineMesh`, un cuadrado del alto de un renglón, para las líneas de los árboles.

## Dibujar un ícono

Con el atlas puesto, en el origen actual:

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`, `Tab`, `PopupMenu` y `GroupPropertie` reciben el ícono como un `int` del enum, o `-1` para ninguno.

## Agregar uno

1. El PNG de 10x10, en `atlas/iconos/`.
2. Una fila al final del enum `IconType`, y sumarle uno a `ICON_TOTAL`.
3. Su nombre al final de `KNombres`.
4. Una fila al final de `TablaLegacy`, con el ícono de `font.png` que lo reemplaza si falta el PNG.

Sin la fila 4, el ícono queda sin coordenadas cuando no hay atlas armado.

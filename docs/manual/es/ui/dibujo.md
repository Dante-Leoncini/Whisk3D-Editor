# Dibujo y atlas

## Cómo dibuja

Todo pasa por `w3dEngine` ([Gráficos](../core/graficos.md)). La UI dibuja triángulos (`DrawTrianglesArray`, `DrawTrianglesByte`) y líneas, con los vértices en `float`. No dibuja con `TRIANGLE_STRIP` ni con vértices `short`: el driver del N95 no los dibuja en la fase 2D del cuadro.

Las coordenadas son píxeles de pantalla, con la Y para abajo y el origen arriba a la izquierda. Cada widget se dibuja en el origen de la matriz actual. Para ubicarlo se usa `Translatef`, entre `PushMatrix` y `PopMatrix`.

## Las funciones de `glesdraw`

`draw/glesdraw.h` es el dibujo de abajo de todo, el que usan los demás:

| Función | Qué dibuja |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | Un cuadrado: 4 vértices `GLshort` en el orden de una tira, con sus 8 UV. Lo pasa a dos triángulos |
| `W3dDrawElemsB(verts, uvs, idx, n)` | Una malla chica con índices `GLubyte`, hasta 64 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | Hasta 21 líneas, con vértices `GLshort` de 3 componentes |
| `W3dDrawLinesF(verts, idx, n)` | Líneas con vértices `float` de 3 componentes, de a 40 vértices por dibujo |

Los vértices convertidos a `float` van a un depósito fijo de 256 lugares que se usa en círculo, nunca a la pila: el driver del N95 lee los punteros tarde.

`W3dPantallaAlto` es el alto de la pantalla. Lo pone quien usa la UI al cambiar el tamaño, y sirve para pasar la Y de la interfaz a la de `Scissor` y `Viewport`, que cuentan desde abajo.

## Nodos y rectángulos

`Object2D` (`draw/object2D.h`) es un nodo con hijos (`Childrens`): tiene tamaño, color, opacidad y `visible`. `Render()` lo dibuja a él y después a sus hijos. Al borrarlo, borra los hijos.

`Rec2D` (`draw/rectangle.h`) es un rectángulo de un color. `SetSize(x, y, w, h)` lo ubica y `SetColor(r, g, b)` le pone el color. No lee la textura: se dibuja con `Texture2D` apagado.

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false: con el color actual, no con el suyo
gfx::Enable(gfx::Texture2D);
```

## Tarjetas

`Card` (`widgets/card.h`) es el rectángulo con esquinas redondeadas de los botones, los menús y las tarjetas de propiedades. Es un **9-patch**: 16 vértices que estiran el centro y dejan las esquinas de `2 * GlobalScale`. Las esquinas y el centro salen de un rincón de `font.png`. `CalcCardUV(anchoAtlas, altoAtlas)` calcula esas coordenadas: se llama una vez, con el tamaño de la textura.

| Función | Qué hace |
|---|---|
| `Resize(w, h)` | Cambia el tamaño |
| `RenderObject(propio)` | Dibuja el relleno |
| `RenderBorder(propio)` | Dibuja solo el borde |
| `RenderObjectTab()`, `RenderBorderTab()` | Lo mismo, con el lado de abajo recto: para las pestañas |

Con `propio` en `true` usa su color (`color` y `opacity`); con `false`, el color actual.

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## El atlas del skin

Una sola textura para toda la interfaz. `font.png`, de 128x128, trae las letras, los bordes de las tarjetas y los íconos viejos. Con eso solo, la UI anda: `CrearIconos` toma los íconos de adentro de `font.png`.

Los íconos nuevos son archivos sueltos, en `atlas/iconos/`. El editor arma al arrancar una textura más grande (`main/io/SkinAtlas.cpp`): pone `font.png` entero en el (0,0), así sus coordenadas no cambian, y empaqueta cada ícono en el lugar que sobra. Después le pasa a la UI dónde quedó cada uno, con `CrearIconosDesde`. Armar ese atlas es del editor; el empaquetador y la tabla de íconos son de la UI. Ver [Íconos](iconos.md).

## El empaquetador

`W3dAtlasPack` (`draw/W3dAtlasPacker.h`) junta varias imágenes RGBA en una. No toca la GPU: entran píxeles y sale un lienzo, que sube quien llama. Sirve igual para un juego: las texturas de un nivel juntas en una bajan los cambios de textura.

```cpp
std::vector<W3dAtlasItem> items(n);       // de cada imagen: rgba, w, h
items[0].fijo = true;                     // esta va sí o sí donde se pide
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // 2 píxeles de aire entre imágenes
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x, items[i].y: dónde quedó cada una
}
```

Prueba lienzos cada vez más grandes: 128x128, 256x128, 256x256, 512x256, 512x512 y 1024x512. Si no entran ni en el último, devuelve `false`. Las imágenes sueltas van en filas, de izquierda a derecha, salteando las fijas. El aire entre imágenes evita que el filtrado lineal mezcle una con su vecina.

## Lo que quedó de antes

`draw/sprites.h` (`Sprite`) y `draw/image.h` (`Image`) no los incluye nadie. Usan inicializadores de C++11, que no compilan en Symbian.

# Texto

## La fuente bitmap

La letra de la interfaz está dibujada a mano en `font.png`. Cada carácter es una celda de **5x11** píxeles y avanza **6** (`CharacterWidth`). Un renglón mide **12** (`RenglonHeight`). Es monoespaciada: todos los caracteres ocupan lo mismo. Con la escala, todo se multiplica por `GlobalScale`.

`Font` (`text/font.h`) guarda las coordenadas de cada carácter. Hay una sola, global: `WhiskFont`. Se crea con el tamaño del atlas y su textura ([Usar la UI directamente](empezar.md)).

`font.png` trae las letras, los números y casi todos los signos del ASCII, más `ñ`, `Ñ`, `¡`, `°` y el tilde `✔`. Dos salen de otro carácter dado vuelta: la `\` es la `/` espejada y el `¿` es el `?` girado, un poco más abajo.

## Los acentos

Las letras con acento no están dibujadas: se arman con la letra base y la marca encima.

- El **agudo** y la **tilde** salen de `font.png`: el agudo es un recorte de la coma, y la tilde es la de la `ñ`. Son una franja de 2 píxeles, más arriba sobre una mayúscula.
- Las otras marcas (grave, circunflejo, diéresis, punto, breve, cedilla y ogonek) están en el atlas extendido.

## El atlas extendido

`font-ext.png`, de 128x64, tiene lo que no entra en `font.png`:

- las marcas de los acentos,
- `ß`, `ı`, `ł`, `Ł`, `œ`, `Œ`, `æ` y `Æ`,
- `;`, `^`, `~`, `` ` ``, `±`, `«`, `»`, `–`, `—` y `…`,
- el **cirílico**. Las letras iguales a una latina (`А В Е К М Н О Р С Т Х` y `а е о р с у х`) usan la de `font.png`.

Se carga recién la primera vez que un texto lo necesita: `SetAtlasExtendido` solo guarda la ruta. En inglés o en castellano puede no cargarse nunca, y en el N95 la memoria vale. Si falta el archivo, una letra compuesta sale sin la marca y un carácter del extendido sale como `?`.

Las tablas, con qué celda tiene cada carácter y cómo se arma cada letra, están en `text/FuenteExt.h`. No se editan a mano: las genera `tools/fuente_ext.py`, que también dibuja el PNG y está en el repositorio del editor.

Con las dos texturas entran el castellano, el portugués, el francés, el alemán, el italiano, el polaco, el turco y el ruso. No entran el japonés, el chino ni el hindi: lo que no está en ningún atlas sale como `?`. `WhiskFont->Soporta(cp)` dice si un carácter se puede dibujar.

El texto va en UTF-8. `UTF8_Char` lee secuencias de 1 a 3 bytes. Una de 4 bytes, como un emoji, no se lee: sale como signos de pregunta.

## Dibujar un texto

```cpp
SetColorID(ColorID::blanco);
RenderBitmapText("Hola", textAlign::left, 200);   // 200: el ancho máximo, en píxeles
```

Se dibuja en el origen de la matriz actual, con el color actual. Cada texto es una llamada de dibujo por atlas: una, o dos si usa el extendido. Al terminar deja puesta otra vez la textura de `font.png`, para que la encuentren los íconos que vienen después.

## Medir y cortar

El ancho de un texto es la cantidad de letras por `CharacterWidthGS`. `UTF8_Largo(texto)` cuenta letras, no bytes: una `ñ` ocupa dos bytes y cuenta una.

Si no entra en el ancho máximo, `RenderBitmapText` lo corta:

| Alineación | Dónde queda | Qué se corta |
|---|---|---|
| `textAlign::left` | Empieza en el origen | El final |
| `textAlign::right` | Termina en el origen | El principio: en un campo se ve lo último que se escribió |
| `textAlign::center` | Centrado en el ancho máximo | El final |

`RenderBitmapEtiqueta(texto, ancho)` es para la etiqueta de una fila: va alineada a la derecha, pero si no entra corta el final y agrega `..` ("Vectores de movimi..").

`RenderBitmapFloat(valor, alineación, ancho, unidad)` dibuja un número con cuatro decimales y la unidad pegada: `90.0000°`. No usa `%f`, porque el `printf` de Symbian no formatea `float`.

## Texto con fuentes TTF

Para letras de tamaño libre está `w3dui::W3dTextAtlas` (`text/W3dTextAtlas.h`). Es solo un header. Dibuja una fuente TTF ya horneada en un atlas, y cada letra se escala y se ubica sola: sirve para títulos, puntajes y letras animadas. No rasteriza la TTF mientras corre.

El atlas se hornea en la PC con `text/bake_atlas.py`, que necesita Pillow: `bake_atlas.py <fuente.ttf> <lado del atlas> <alto de letra> <salida>`. El lado del atlas es 128, 256, 512 o 1024. Hornea el ASCII y `áéíóúÁÉÍÓÚñÑüÜ¿¡`. Salen `<salida>.png`, con la cobertura en gris y sin alfa, y `<salida>.w3dfnt`, con las medidas de cada letra.

```cpp
#include "WhiskUI/text/W3dTextAtlas.h"
#include "io/w3dFilesystem.h"

std::vector<unsigned char> fnt, png;
w3dFileSystem::ReadFileBytes("fuentes/titulo.w3dfnt", fnt);
w3dFileSystem::ReadFileBytes("fuentes/titulo.png", png);

w3dui::W3dTextAtlas titulo;
titulo.Load(&fnt[0], fnt.size(), &png[0], png.size());

titulo.Begin();                                                // atlas, mezcla y estado
titulo.DrawText("Puntaje: 120", 10, 10, 24, 1, 1, 1, 1);      // x, y, alto, r, g, b, a
float ancho = titulo.TextWidth("Puntaje: 120", 24);
```

`(x, y)` es la parte de arriba del renglón, con la Y para abajo. `DrawText` junta todas las letras en un solo dibujo. `DrawGlyph` dibuja una sola, para letras de distinto tamaño o color. `LineHeight(alto)` da el alto de un renglón. Por defecto mezcla con alfa premultiplicado (`MezclaPremult`): el texto puede ser de cualquier color.

## La tabla sin GPU

`text/W3dFont.h` tiene la misma tabla de la fuente bitmap, sin tocar la GPU: dónde está cada letra en `font.png` (`W3dFontGetGlyphPx`) y sus coordenadas (`W3dFontGetGlyph`). Sirve para armar otro atlas con la misma letra.

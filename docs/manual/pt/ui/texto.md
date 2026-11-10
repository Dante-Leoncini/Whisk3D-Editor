# Texto

## A fonte bitmap

A letra da interface é desenhada à mão em `font.png`. Cada caractere é uma célula de **5x11** pixels e avança **6** (`CharacterWidth`). Uma linha de texto mede **12** (`RenglonHeight`). É monoespaçada: todos os caracteres ocupam o mesmo espaço. Com a escala, tudo é multiplicado por `GlobalScale`.

`Font` (`text/font.h`) guarda as coordenadas de cada caractere. Há uma só, global: `WhiskFont`. Ela é criada com o tamanho do atlas e a sua textura ([Usar a UI diretamente](empezar.md)).

`font.png` traz as letras, os números e quase todos os sinais do ASCII, mais `ñ`, `Ñ`, `¡`, `°` e a marca de seleção `✔`. Dois saem de outro caractere virado: a `\` é a `/` espelhada e o `¿` é o `?` girado, um pouco mais abaixo.

## Os acentos

As letras com acento não estão desenhadas: elas são montadas com a letra base e o sinal por cima.

- O **agudo** e o **til** vêm de `font.png`: o agudo é um recorte da vírgula, e o til é o do `ñ`. São uma faixa de 2 pixels, mais alta sobre uma maiúscula.
- Os outros sinais (grave, circunflexo, trema, ponto, breve, cedilha e ogonek) estão no atlas estendido.

## O atlas estendido

`font-ext.png`, de 128x64, tem o que não cabe em `font.png`:

- os sinais dos acentos,
- `ß`, `ı`, `ł`, `Ł`, `œ`, `Œ`, `æ` e `Æ`,
- `;`, `^`, `~`, `` ` ``, `±`, `«`, `»`, `–`, `—` e `…`,
- o **cirílico**. As letras iguais a uma latina (`А В Е К М Н О Р С Т Х` e `а е о р с у х`) usam a de `font.png`.

Ele só é carregado na primeira vez que um texto precisa dele: `SetAtlasExtendido` só guarda o caminho. Em inglês ou em espanhol, ele pode nunca ser carregado, e no N95 a memória conta. Se o arquivo faltar, uma letra composta sai sem o sinal e um caractere do estendido sai como `?`.

As tabelas, com a célula de cada caractere e o modo de montar cada letra, estão em `text/FuenteExt.h`. Elas não são editadas à mão: são geradas por `tools/fuente_ext.py`, que também desenha o PNG e está no repositório do editor.

Com as duas texturas, cabem o espanhol, o português, o francês, o alemão, o italiano, o polonês, o turco e o russo. Não cabem o japonês, o chinês nem o hindi: o que não está em nenhum atlas sai como `?`. `WhiskFont->Soporta(cp)` diz se um caractere pode ser desenhado.

O texto vai em UTF-8. `UTF8_Char` lê sequências de 1 a 3 bytes. Uma de 4 bytes, como um emoji, não é lida: sai como pontos de interrogação.

## Desenhar um texto

```cpp
SetColorID(ColorID::blanco);
RenderBitmapText("Hola", textAlign::left, 200);   // 200: a largura máxima, em pixels
```

Ele é desenhado na origem da matriz atual, com a cor atual. Cada texto é uma chamada de desenho por atlas: uma, ou duas se usar o estendido. Ao terminar, deixa definida de novo a textura de `font.png`, para que os ícones que vêm depois a encontrem.

## Medir e cortar

A largura de um texto é a quantidade de letras vezes `CharacterWidthGS`. `UTF8_Largo(texto)` conta letras, não bytes: um `ñ` ocupa dois bytes e conta como uma letra.

Se não couber na largura máxima, `RenderBitmapText` o corta:

| Alinhamento | Onde fica | O que é cortado |
|---|---|---|
| `textAlign::left` | Começa na origem | O final |
| `textAlign::right` | Termina na origem | O começo: num campo, vê-se o que foi escrito por último |
| `textAlign::center` | Centralizado na largura máxima | O final |

`RenderBitmapEtiqueta(texto, ancho)` é para o rótulo de uma linha: fica alinhado à direita, mas, se não couber, corta o final e acrescenta `..` ("Vetores de movimen..").

`RenderBitmapFloat(valor, alineación, ancho, unidad)` desenha um número com quatro casas decimais e a unidade colada: `90.0000°`. Não usa `%f`, porque o `printf` do Symbian não formata `float`.

## Texto com fontes TTF

Para letras de tamanho livre existe o `w3dui::W3dTextAtlas` (`text/W3dTextAtlas.h`). É só um header. Ele desenha uma fonte TTF já bakeada num atlas, e cada letra é escalada e posicionada individualmente: serve para títulos, pontuações e letras animadas. Ele não rasteriza a TTF em tempo de execução.

O atlas é bakeado no PC com `text/bake_atlas.py`, que precisa do Pillow: `bake_atlas.py <fuente.ttf> <lado del atlas> <alto de letra> <salida>`. O lado do atlas é 128, 256, 512 ou 1024. Ele bakeia o ASCII e `áéíóúÁÉÍÓÚñÑüÜ¿¡`. Saem `<salida>.png`, com a cobertura em cinza e sem alfa, e `<salida>.w3dfnt`, com as medidas de cada letra.

```cpp
#include "WhiskUI/text/W3dTextAtlas.h"
#include "io/w3dFilesystem.h"

std::vector<unsigned char> fnt, png;
w3dFileSystem::ReadFileBytes("fuentes/titulo.w3dfnt", fnt);
w3dFileSystem::ReadFileBytes("fuentes/titulo.png", png);

w3dui::W3dTextAtlas titulo;
titulo.Load(&fnt[0], fnt.size(), &png[0], png.size());

titulo.Begin();                                                // atlas, mesclagem e estado
titulo.DrawText("Puntaje: 120", 10, 10, 24, 1, 1, 1, 1);      // x, y, altura, r, g, b, a
float ancho = titulo.TextWidth("Puntaje: 120", 24);
```

`(x, y)` é a parte de cima da linha, com o Y para baixo. `DrawText` junta todas as letras num único desenho. `DrawGlyph` desenha uma só, para letras de tamanho ou cor diferentes. `LineHeight(alto)` dá a altura de uma linha. Por padrão, mescla com alfa pré-multiplicado (`MezclaPremult`): o texto pode ser de qualquer cor.

## A tabela sem GPU

`text/W3dFont.h` tem a mesma tabela da fonte bitmap, sem tocar na GPU: onde fica cada letra em `font.png` (`W3dFontGetGlyphPx`) e suas coordenadas (`W3dFontGetGlyph`). Serve para montar outro atlas com a mesma letra.

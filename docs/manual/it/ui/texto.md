# Testo

## Il font bitmap

Il font dell'interfaccia è disegnato a mano in `font.png`. Ogni carattere è una cella di **5x11** pixel e avanza di **6** (`CharacterWidth`). Una riga è alta **12** (`RenglonHeight`). È monospaziato: tutti i caratteri occupano lo stesso spazio. Con la scala, tutto si moltiplica per `GlobalScale`.

`Font` (`text/font.h`) conserva le coordinate di ogni carattere. Ce n'è uno solo, globale: `WhiskFont`. Si crea con la dimensione dell'atlas e la sua texture ([Usare la UI direttamente](empezar.md)).

`font.png` contiene le lettere, i numeri e quasi tutti i segni dell'ASCII, più `ñ`, `Ñ`, `¡`, `°` e il segno di spunta `✔`. Due derivano da un altro carattere capovolto: la `\` è la `/` specchiata e il `¿` è il `?` ruotato, un po' più in basso.

## Gli accenti

Le lettere accentate non sono disegnate: si compongono con la lettera base e il segno sopra.

- L'**acuto** e la **tilde** vengono da `font.png`: l'acuto è un ritaglio della virgola, e la tilde è quella della `ñ`. Sono una striscia di 2 pixel, più in alto sopra una maiuscola.
- Gli altri segni (grave, circonflesso, dieresi, punto, breve, cediglia e ogonek) sono nell'atlas esteso.

## L'atlas esteso

`font-ext.png`, di 128x64, contiene ciò che non entra in `font.png`:

- i segni degli accenti,
- `ß`, `ı`, `ł`, `Ł`, `œ`, `Œ`, `æ` e `Æ`,
- `;`, `^`, `~`, `` ` ``, `±`, `«`, `»`, `–`, `—` e `…`,
- il **cirillico**. Le lettere uguali a una latina (`А В Е К М Н О Р С Т Х` e `а е о р с у х`) usano quella di `font.png`.

Si carica solo la prima volta che un testo ne ha bisogno: `SetAtlasExtendido` si limita a salvare il percorso. In inglese o in spagnolo potrebbe non caricarsi mai, e sull'N95 la memoria è preziosa. Se il file manca, una lettera composta esce senza il segno e un carattere dell'atlas esteso esce come `?`.

Le tabelle, con la cella di ogni carattere e il modo in cui si compone ogni lettera, sono in `text/FuenteExt.h`. Non si modificano a mano: le genera `tools/fuente_ext.py`, che disegna anche il PNG e si trova nel repository dell'editor.

Con le due texture ci stanno lo spagnolo, il portoghese, il francese, il tedesco, l'italiano, il polacco, il turco e il russo. Non ci stanno il giapponese, il cinese né l'hindi: ciò che non è in nessun atlas esce come `?`. `WhiskFont->Soporta(cp)` dice se un carattere si può disegnare.

Il testo è in UTF-8. `UTF8_Char` legge sequenze da 1 a 3 byte. Una da 4 byte, come un'emoji, non viene letta: esce come punti interrogativi.

## Disegnare un testo

```cpp
SetColorID(ColorID::blanco);
RenderBitmapText("Hola", textAlign::left, 200);   // 200: el ancho máximo, en píxeles
```

Si disegna nell'origine della matrice attuale, con il colore attuale. Ogni testo è una chiamata di disegno per atlas: una, o due se usa quello esteso. Alla fine reimposta la texture di `font.png`, così la trovano le icone che vengono dopo.

## Misurare e tagliare

La larghezza di un testo è il numero di lettere per `CharacterWidthGS`. `UTF8_Largo(texto)` conta lettere, non byte: una `ñ` occupa due byte e conta come una.

Se non ci sta nella larghezza massima, `RenderBitmapText` lo taglia:

| Allineamento | Dove si trova | Cosa si taglia |
|---|---|---|
| `textAlign::left` | Inizia nell'origine | La fine |
| `textAlign::right` | Finisce nell'origine | L'inizio: in un campo si vede l'ultima cosa scritta |
| `textAlign::center` | Centrato nella larghezza massima | La fine |

`RenderBitmapEtiqueta(texto, ancho)` è per l'etichetta di una riga: è allineata a destra, ma se non ci sta taglia la fine e aggiunge `..` ("Vettori di movimen..").

`RenderBitmapFloat(valor, alineación, ancho, unidad)` disegna un numero con quattro decimali e l'unità attaccata: `90.0000°`. Non usa `%f`, perché il `printf` di Symbian non formatta i `float`.

## Testo con font TTF

Per lettere di dimensione libera c'è `w3dui::W3dTextAtlas` (`text/W3dTextAtlas.h`). È solo un header. Disegna un font TTF già prerenderizzato in un atlas, e ogni lettera si scala e si posiziona per conto suo: va bene per titoli, punteggi e lettere animate. Non rasterizza il TTF durante l'esecuzione.

L'atlas si prerenderizza sul PC con `text/bake_atlas.py`, che richiede Pillow: `bake_atlas.py <fuente.ttf> <lado del atlas> <alto de letra> <salida>`. Il lato dell'atlas è 128, 256, 512 o 1024. Prerenderizza l'ASCII e `áéíóúÁÉÍÓÚñÑüÜ¿¡`. Ne escono `<salida>.png`, con la copertura in grigio e senza alfa, e `<salida>.w3dfnt`, con le misure di ogni lettera.

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

`(x, y)` è la parte superiore della riga, con la Y verso il basso. `DrawText` riunisce tutte le lettere in un solo disegno. `DrawGlyph` ne disegna una sola, per lettere di dimensione o colore diversi. `LineHeight(alto)` dà l'altezza di una riga. Per impostazione predefinita fonde con alfa premoltiplicato (`MezclaPremult`): il testo può essere di qualsiasi colore.

## La tabella senza GPU

`text/W3dFont.h` contiene la stessa tabella del font bitmap, senza toccare la GPU: dove si trova ogni lettera in `font.png` (`W3dFontGetGlyphPx`) e le sue coordinate (`W3dFontGetGlyph`). Serve per costruire un altro atlas con le stesse lettere.

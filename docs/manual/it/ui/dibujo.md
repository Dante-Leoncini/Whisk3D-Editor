# Disegno e atlas

## Come disegna

Tutto passa per `w3dEngine` ([Grafica](../core/graficos.md)). La UI disegna triangoli (`DrawTrianglesArray`, `DrawTrianglesByte`) e linee, con i vertici in `float`. Non disegna con `TRIANGLE_STRIP` né con vertici `short`: il driver dell'N95 non li disegna nella fase 2D del fotogramma.

Le coordinate sono pixel dello schermo, con la Y verso il basso e l'origine in alto a sinistra. Ogni widget si disegna nell'origine della matrice attuale. Per posizionarlo si usa `Translatef`, tra `PushMatrix` e `PopMatrix`.

## Le funzioni di `glesdraw`

`draw/glesdraw.h` è il disegno di livello più basso, quello che usano tutti gli altri:

| Funzione | Cosa disegna |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | Un quadrato: 4 vertici `GLshort` nell'ordine di una strip, con le loro 8 UV. Lo converte in due triangoli |
| `W3dDrawElemsB(verts, uvs, idx, n)` | Una piccola mesh con indici `GLubyte`, fino a 64 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | Fino a 21 linee, con vertici `GLshort` a 3 componenti |
| `W3dDrawLinesF(verts, idx, n)` | Linee con vertici `float` a 3 componenti, 40 vertici per disegno |

I vertici convertiti in `float` vanno in un deposito fisso di 256 posti usato in modo circolare, mai sullo stack: il driver dell'N95 legge i puntatori in ritardo.

`W3dPantallaAlto` è l'altezza dello schermo. La imposta chi usa la UI quando cambia la dimensione, e serve per convertire la Y dell'interfaccia in quella di `Scissor` e `Viewport`, che contano dal basso.

## Nodi e rettangoli

`Object2D` (`draw/object2D.h`) è un nodo con figli (`Childrens`): ha dimensione, colore, opacità e `visible`. `Render()` disegna il nodo e poi i suoi figli. Quando lo si elimina, elimina anche i figli.

`Rec2D` (`draw/rectangle.h`) è un rettangolo di un solo colore. `SetSize(x, y, w, h)` lo posiziona e `SetColor(r, g, b)` gli assegna il colore. Non legge la texture: si disegna con `Texture2D` disattivato.

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false: con el color actual, no con el suyo
gfx::Enable(gfx::Texture2D);
```

## Riquadri

`Card` (`widgets/card.h`) è il rettangolo con gli angoli arrotondati dei pulsanti, dei menu e dei riquadri delle proprietà. È un **9-patch**: 16 vertici che allungano il centro e lasciano gli angoli di `2 * GlobalScale`. Gli angoli e il centro vengono da una zona di `font.png`. `CalcCardUV(anchoAtlas, altoAtlas)` calcola quelle coordinate: si chiama una volta, con la dimensione della texture.

| Funzione | Cosa fa |
|---|---|
| `Resize(w, h)` | Cambia la dimensione |
| `RenderObject(propio)` | Disegna il riempimento |
| `RenderBorder(propio)` | Disegna solo il bordo |
| `RenderObjectTab()`, `RenderBorderTab()` | Lo stesso, con il lato inferiore dritto: per le schede |

Con `propio` a `true` usa il proprio colore (`color` e `opacity`); con `false`, il colore attuale.

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## L'atlas della skin

Una sola texture per tutta l'interfaccia. `font.png`, di 128x128, contiene le lettere, i bordi dei riquadri e le icone vecchie. Già solo con questo la UI funziona: `CrearIconos` prende le icone da dentro `font.png`.

Le icone nuove sono file separati, in `atlas/iconos/`. All'avvio l'editor costruisce una texture più grande (`main/io/SkinAtlas.cpp`): mette `font.png` intero in (0,0), così le sue coordinate non cambiano, e impacchetta ogni icona nello spazio che avanza. Poi comunica alla UI dove è finita ciascuna, con `CrearIconosDesde`. Costruire quell'atlas spetta all'editor; il packer e la tabella delle icone sono della UI. Vedi [Icone](iconos.md).

## Il packer

`W3dAtlasPack` (`draw/W3dAtlasPacker.h`) unisce più immagini RGBA in una sola. Non tocca la GPU: entrano pixel ed esce un canvas, che poi carica chi chiama la funzione. Va bene anche per un gioco: le texture di un livello unite in una sola riducono i cambi di texture.

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

Prova canvas sempre più grandi: 128x128, 256x128, 256x256, 512x256, 512x512 e 1024x512. Se le immagini non entrano nemmeno nell'ultimo, restituisce `false`. Le immagini libere vanno in righe, da sinistra a destra, saltando quelle fisse. Lo spazio tra le immagini evita che il filtro lineare ne mescoli una con la vicina.

## Ciò che è rimasto da prima

`draw/sprites.h` (`Sprite`) e `draw/image.h` (`Image`) non li include nessuno. Usano inizializzatori del C++11, che non compilano su Symbian.

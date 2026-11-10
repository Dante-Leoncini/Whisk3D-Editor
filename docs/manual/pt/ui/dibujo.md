# Desenho e atlas

## Como ela desenha

Tudo passa por `w3dEngine` ([Gráficos](../core/graficos.md)). A UI desenha triângulos (`DrawTrianglesArray`, `DrawTrianglesByte`) e linhas, com os vértices em `float`. Ela não desenha com `TRIANGLE_STRIP` nem com vértices `short`: o driver do N95 não os desenha na fase 2D do quadro.

As coordenadas são pixels de tela, com o Y para baixo e a origem no canto superior esquerdo. Cada widget é desenhado na origem da matriz atual. Para posicioná-lo, usa-se `Translatef`, entre `PushMatrix` e `PopMatrix`.

## As funções de `glesdraw`

`draw/glesdraw.h` é o desenho mais básico de todos, o que os outros usam:

| Função | O que desenha |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | Um quadrado: 4 vértices `GLshort` na ordem de uma tira, com seus 8 UV. Converte para dois triângulos |
| `W3dDrawElemsB(verts, uvs, idx, n)` | Uma malha pequena com índices `GLubyte`, até 64 |
| `W3dDrawLinesS(ancho, n, verts, idx)` | Até 21 linhas, com vértices `GLshort` de 3 componentes |
| `W3dDrawLinesF(verts, idx, n)` | Linhas com vértices `float` de 3 componentes, de 40 em 40 vértices por desenho |

Os vértices convertidos para `float` vão para um depósito fixo de 256 posições usado de forma circular, nunca para a pilha: o driver do N95 lê os ponteiros tarde.

`W3dPantallaAlto` é a altura da tela. Quem usa a UI a define ao mudar o tamanho, e ela serve para converter o Y da interface para o de `Scissor` e `Viewport`, que contam de baixo para cima.

## Nós e retângulos

`Object2D` (`draw/object2D.h`) é um nó com filhos (`Childrens`): tem tamanho, cor, opacidade e `visible`. `Render()` desenha o próprio nó e depois seus filhos. Ao ser apagado, ele apaga os filhos.

`Rec2D` (`draw/rectangle.h`) é um retângulo de uma cor. `SetSize(x, y, w, h)` o posiciona e `SetColor(r, g, b)` define sua cor. Ele não lê a textura: é desenhado com `Texture2D` desligado.

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false: com a cor atual, não com a dele
gfx::Enable(gfx::Texture2D);
```

## Cartões

`Card` (`widgets/card.h`) é o retângulo com cantos arredondados dos botões, dos menus e dos cartões de propriedades. É um **9-patch**: 16 vértices que esticam o centro e mantêm os cantos de `2 * GlobalScale`. Os cantos e o centro vêm de um cantinho de `font.png`. `CalcCardUV(anchoAtlas, altoAtlas)` calcula essas coordenadas: é chamada uma vez, com o tamanho da textura.

| Função | O que faz |
|---|---|
| `Resize(w, h)` | Muda o tamanho |
| `RenderObject(propio)` | Desenha o preenchimento |
| `RenderBorder(propio)` | Desenha só a borda |
| `RenderObjectTab()`, `RenderBorderTab()` | O mesmo, com o lado de baixo reto: para as abas |

Com `propio` em `true`, usa a sua própria cor (`color` e `opacity`); com `false`, a cor atual.

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## O atlas do skin

Uma única textura para toda a interface. `font.png`, de 128x128, traz as letras, as bordas dos cartões e os ícones antigos. Só com isso a UI já funciona: `CrearIconos` pega os ícones de dentro de `font.png`.

Os ícones novos são arquivos avulsos, em `atlas/iconos/`. Ao iniciar, o editor monta uma textura maior (`main/io/SkinAtlas.cpp`): coloca `font.png` inteiro em (0,0), assim suas coordenadas não mudam, e empacota cada ícone no espaço que sobra. Depois informa à UI onde ficou cada um, com `CrearIconosDesde`. Montar esse atlas é do editor; o empacotador e a tabela de ícones são da UI. Veja [Ícones](iconos.md).

## O empacotador

`W3dAtlasPack` (`draw/W3dAtlasPacker.h`) junta várias imagens RGBA numa só. Ele não toca na GPU: entram pixels e sai um canvas, que quem chama envia para a GPU. Serve igualmente para um jogo: juntar as texturas de um nível numa só reduz as trocas de textura.

```cpp
std::vector<W3dAtlasItem> items(n);       // de cada imagem: rgba, w, h
items[0].fijo = true;                     // esta vai obrigatoriamente onde for pedido
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // 2 pixels de folga entre imagens
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x, items[i].y: onde ficou cada uma
}
```

Ele testa canvas cada vez maiores: 128x128, 256x128, 256x256, 512x256, 512x512 e 1024x512. Se as imagens não couberem nem no último, retorna `false`. As imagens avulsas vão em linhas, da esquerda para a direita, pulando as fixas. A folga entre imagens evita que a filtragem linear misture uma com a vizinha.

## O que sobrou de antes

`draw/sprites.h` (`Sprite`) e `draw/image.h` (`Image`) não são incluídos por ninguém. Usam inicializadores de C++11, que não compilam no Symbian.

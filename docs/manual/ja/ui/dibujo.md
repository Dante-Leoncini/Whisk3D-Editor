# 描画とアトラス

## 描画のしくみ

すべて`w3dEngine`（[グラフィックス](../core/graficos.md)）を通ります。UIは三角形（`DrawTrianglesArray`、`DrawTrianglesByte`）と線を描画し、頂点は`float`です。`TRIANGLE_STRIP`や`short`の頂点では描画しません：N95のドライバーは、フレームの2Dの段階ではそれらを描画しないからです。

座標は画面のピクセルで、Yは下向き、原点は左上です。各ウィジェットは現在の行列の原点に描画されます。位置を決めるには、`PushMatrix`と`PopMatrix`の間で`Translatef`を使います。

## `glesdraw`の関数

`draw/glesdraw.h`は、いちばん下の層の描画で、ほかのすべてが使っているものです：

| 関数 | 描画するもの |
|---|---|
| `W3dDrawStrip4(verts, uvs)` | 正方形：ストリップの順に並んだ`GLshort`の頂点4つと、そのUV 8つ。2つの三角形に変換します |
| `W3dDrawElemsB(verts, uvs, idx, n)` | `GLubyte`のインデックスを持つ小さなメッシュ（最大64） |
| `W3dDrawLinesS(ancho, n, verts, idx)` | 最大21本の線。頂点は3成分の`GLshort` |
| `W3dDrawLinesF(verts, idx, n)` | 3成分の`float`の頂点を持つ線。1回の描画で40頂点ずつ |

`float`に変換した頂点は、スタックではなく、256か所の固定のバッファーに入れて循環して使います：N95のドライバーはポインターを遅れて読むからです。

`W3dPantallaAlto`は画面の高さです。サイズが変わったときにUIを使う側が設定し、インターフェースのYを、下から数える`Scissor`と`Viewport`のYに変換するために使います。

## ノードと矩形

`Object2D`（`draw/object2D.h`）は子（`Childrens`）を持つノードです：サイズ、色、不透明度、`visible`を持っています。`Render()`は自分を描画し、続いて子を描画します。削除すると、子も削除されます。

`Rec2D`（`draw/rectangle.h`）は単色の矩形です。`SetSize(x, y, w, h)`で位置を決め、`SetColor(r, g, b)`で色を設定します。テクスチャは読まないので、`Texture2D`をオフにして描画します。

```cpp
Rec2D velo;
velo.SetSize(0, 0, ancho, alto);
gfx::Disable(gfx::Texture2D);
gfx::Color4f(0.0f, 0.0f, 0.0f, 0.5f);
velo.RenderObject(false);          // false：自分の色ではなく、現在の色で描く
gfx::Enable(gfx::Texture2D);
```

## カード

`Card`（`widgets/card.h`）は、ボタン、メニュー、プロパティのカードに使われる角丸の矩形です。**9パッチ**になっていて、16個の頂点で中央を伸ばし、角は`2 * GlobalScale`のまま残します。角と中央は`font.png`の隅の一部から取ります。`CalcCardUV(anchoAtlas, altoAtlas)`がその座標を計算します：テクスチャのサイズを渡して、1回だけ呼びます。

| 関数 | 働き |
|---|---|
| `Resize(w, h)` | サイズを変えます |
| `RenderObject(propio)` | 塗りを描画します |
| `RenderBorder(propio)` | 枠だけを描画します |
| `RenderObjectTab()`、`RenderBorderTab()` | 同じものを、下辺を直線にして描画します：タブ用 |

`propio`が`true`のときは自分の色（`color`と`opacity`）を使い、`false`のときは現在の色を使います。

```cpp
Card fondo(NULL, 120, 40);
SetColorID(ColorID::gris);
fondo.RenderObject(false);
SetColorID(ColorID::accent);
fondo.RenderBorder(false);
```

## スキンのアトラス

インターフェース全体で1枚のテクスチャです。128x128の`font.png`には、文字、カードの縁、古いアイコンが入っています。これだけでもUIは動きます：`CrearIconos`が`font.png`の中のアイコンを使います。

新しいアイコンは、`atlas/iconos/`にある個別のファイルです。エディターは起動時に、もっと大きなテクスチャを組み立てます（`main/io/SkinAtlas.cpp`）：`font.png`を丸ごと(0,0)に置くので、その座標は変わりません。そして、残った場所に各アイコンを詰め込みます。そのあと、それぞれがどこに置かれたかを`CrearIconosDesde`でUIに渡します。このアトラスを組み立てるのはエディターの仕事で、パッカーとアイコンのテーブルはUIのものです。[アイコン](iconos.md)を参照してください。

## パッカー

`W3dAtlasPack`（`draw/W3dAtlasPacker.h`）は、複数のRGBA画像を1枚にまとめます。GPUには触れません：ピクセルを受け取ってキャンバスを返し、それをアップロードするのは呼び出し側です。ゲームにもそのまま使えます：1つのレベルのテクスチャを1枚にまとめれば、テクスチャの切り替えが減ります。

```cpp
std::vector<W3dAtlasItem> items(n);       // 各画像のrgba、w、h
items[0].fijo = true;                     // これは必ず指定した位置に置く
items[0].x = 0; items[0].y = 0;

std::vector<unsigned char> lienzo; int W = 0, H = 0;
if (W3dAtlasPack(&items[0], n, 2, lienzo, &W, &H)) {    // 画像の間に2ピクセルの余白
    unsigned int tex = w3dEngine::UploadRGBA(&lienzo[0], W, H, true);
    // items[i].x, items[i].y：それぞれが置かれた位置
}
```

キャンバスを少しずつ大きくして試します：128x128、256x128、256x256、512x256、512x512、1024x512。最後のものにも収まらなければ、`false`を返します。固定でない画像は、固定の画像を避けながら、左から右へ行ごとに並べます。画像の間の余白は、リニアフィルタリングで隣の画像と色が混ざるのを防ぎます。

## 以前のものの名残

`draw/sprites.h`（`Sprite`）と`draw/image.h`（`Image`）は、どこからもインクルードされていません。C++11の初期化子を使っているので、Symbianではコンパイルできません。

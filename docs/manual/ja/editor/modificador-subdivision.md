# Subdivision Surface

**Subdivision Surface**モディファイアーは、各面をより小さな面に分割し、初期設定では形を丸くします。少ないポリゴンでモデリングし（編集が速く、軽い）、結果を滑らかに見るのに使います。立方体からは球に近い形ができ、ローポリのキャラクターからは丸みのある形のキャラクターができます。

![Modifiersタブ全体。立方体にSubdivision Surfaceモディファイアーが付いています](../tutoriales/img/modificador-subdivision.webp)

## 追加のしかた

1. メッシュを選択します。
2. Propertiesで**Modifiers**タブ（スパナのアイコン）を開きます。
3. **Add**をタップして、**Subdivision Surface**を選びます。

リストには内部名の**Subdivision Surface**で表示され、その下にパラメーターのカードが開きます。ほかのモディファイアーと同じく、メッシュを壊しません。オフにしたり、**Move Up**と**Move Down**で順番を変えたり、**Remove**で外したり、**Apply Modifier**でベイクしたりできます。

## パラメーター

| パラメーター | 働き |
|---|---|
| Display in Viewport | オフにすると、細分化されません。ビューでもレンダリングでもです。 |
| Display in Edit Mode | オフにすると、編集中は細分化していないメッシュが表示され、編集が速くなります。Object Modeに戻ると、また滑らかに表示されます。 |
| Simple | オフ（初期設定）のときは、レベルごとに形が滑らかになり、角が丸くなります。オンのときは、頂点を動かさずに面を分割するだけです。形は同じまま頂点が増えるので、たとえば頂点カラーをより細かくペイントしたいときに使います。 |
| Levels Viewport | ビューで何回細分化するかです。0から6まで（初期値は1）。レベルが1つ上がるごとに面の数が4倍になります。6面の立方体は、レベル1で24面、2で96面、3で384面になります。0では細分化しません。 |
| Render | **Render Image**や**Render Animation**のときに使うレベルです（初期値は2）。こうすると、ビューは軽いまま、最終的な画像はより滑らかになります。 |
| Apply Modifier | **Levels Viewport**の値で（Renderの値ではなく）、細分化をメッシュにベイクします。下の注を参照してください。 |

## 細分化を使った作業

- 四角形の面でいちばんよく働きます。三角形は3つの四角形に、五角形の面は5つに分割されます。
- 細分化してもシェーディングは変わりません。フラットなメッシュは角ばって見えたままです。滑らかに見せるには、Edit Modeで面を選択し、Faceメニュー → **Shade Smooth**を使います。
- 辺をよりはっきりさせるには、**Loop Cut and Slide**でその近くにカットを入れます（[Edit Mode](modo-edicion.md)を参照）。辺に近いほど硬くなります。
- 閉じていないメッシュの開いた縁は、縁に沿って滑らかになります。

## 順番が大切

モディファイアーは上から下へ適用されます。[Mirror](modificador-espejo.md)はSubdivision Surfaceより**上に**置きます。こうすると、モデル全体が一度に滑らかになります。Mirrorが下にあると、半分ずつ別々に滑らかになり、中央に折り目ができます。[Array](modificador-array.md)でも同じです。Mergeで結合したコピーは、まとめて滑らかになります。

![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95では、レベルごとに負荷がかかります。レベル1で描画する面は4倍に、レベル2では16倍になります。Levels Viewportは0か1にして、モデリング中はDisplay in Edit Modeをオフにし、高いレベルはRenderに回してください。

> [!NOTE]
> Subdivision SurfaceでApply Modifierを使うと、これだけでなく、リストにあるオンのモディファイアーが**すべて**ベイクされ、リストは空になります。オフになっていたモディファイアーや、[Armature](modificador-esqueleto.md)のようにジオメトリを生成しないモディファイアーも消えます。元に戻すことができます。

[3Dメッシュ](objeto-malla.md)に戻る

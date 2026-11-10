# 3Dメッシュ

**3Dメッシュ**は形を持つオブジェクトです。頂点、辺、面でできていて、モデリングし、ペイントし、アニメーションする対象です。立方体も、キャラクターも、ステージ全体もメッシュです。このページはメッシュのプロパティのリファレンスです。オブジェクトの作成、移動、ペアレントのしかたは、チュートリアル[Object Mode](../tutoriales/modo-objeto.md)で説明しています。

## タブ

メッシュを選択すると、Propertiesには、常にあるFile / RenderとAnimationに加えて、次のタブが表示されます。

| タブ | 内容 |
|---|---|
| Object | すべてのオブジェクトに共通するもの：位置、回転、スケール、名前、表示。 |
| Material | メッシュのパーツと、それぞれのマテリアル。 |
| 3D Mesh | メッシュのデータ：どのメッシュを使っているか、頂点グループ、UVマップ、カラーレイヤー。 |
| Modifiers | 形を壊さずに変えるモディファイアー。 |
| Transform | Edit Modeのみ：選択したものの中心を、数値で入力できます。 |
| Constraints | [コンストレイント](restricciones.md)を参照してください。 |
| Scripts | オブジェクトのLuaスクリプトです。[Whisk3DのLua](../lua/README.md)を参照してください。 |

## Objectタブ

![Objectタブ全体。Leonのメッシュを選んでいます](../tutoriales/img/objeto-malla-objeto.webp)

すべての種類のオブジェクトで同じです。カメラ、ライト、アーマチュア、サウンドにもこのタブがあります。

### Transform

| プロパティ | 働き |
|---|---|
| Location X, Y, Z ◆ | オブジェクトの位置です（メートル）。Zが高さです。 |
| Mode | 回転の書き方です：**XYZ Euler**（度単位の3つの角度。いちばん読みやすい）、**Quaternion (WXYZ)**、**Axis Angle**。後の2つでは**Rotation W**が表示されます。モードを変えてもオブジェクトは回転しません。数値の表示のしかたが変わるだけです。 |
| Rotation X, Y, Z ◆ | 各軸のまわりの回転です。 |
| Scale X, Y, Z ◆ | 各軸方向のサイズです。1が元のサイズ、2が2倍、0.5が半分です。 |
| Name | オブジェクトの名前で、Outlinerに表示されるものと同じです。同じ名前は使えません。すでにある名前なら、番号（`.001`）が付けられます。 |
| Visible ◆ | オブジェクトを表示するかどうかです。Outlinerの目のアイコンと同じです。 |
| Renderable ◆ | レンダリングに出るかどうかです。チェックを外すと、3D Viewportには表示されたままですが、画像や動画には出ません。 |
| Relationship Lines | オブジェクトとその親を結ぶ線です。子がたくさんあって線がビューを隠してしまうときは、オフにすると便利です。 |
| Static | オブジェクトが決して動かないことを示します。グリッド方式のCullingオブジェクトの中でだけ意味があります。静的なオブジェクトは、毎フレーム測る代わりに、一度だけ自分のセルに配置されます。 |

### AnimationとPalette

- **Animation：** 頂点アニメーションなど、オブジェクト自身のアニメーションと、そのクリップです。[Timeline](linea-de-tiempo.md)を参照してください。
- **Palette：** オブジェクトが使うカラーパレットです。**Same as parent**（初期設定）では親から受け継ぎます。プロジェクトのパレットを選ぶと、オブジェクトと、その下にぶら下がっているものすべてが塗り直されます。

## Materialタブ

![Materialタブ全体：メッシュのパーツと、選んだパーツのマテリアル](../tutoriales/img/objeto-malla-material.webp)

### Mesh Parts

メッシュは**パーツ**に分けることができ、パーツごとにマテリアルを持ちます。たとえば家なら、壁を1つのパーツに、窓のガラスを別のパーツにできます。

| プロパティ | 働き |
|---|---|
| リスト | メッシュのパーツです。選んだパーツが、下のMaterialカードに表示されます。 |
| New Mesh Part | 空のパーツを作成します。 |
| Assign | Edit Modeで選択している面を、選んだパーツに移します。 |
| Select / Deselect | そのパーツの面を選択、または選択解除します。必要なら、Selectは面を表示するためにEdit Modeに切り替えます。 |
| Delete / Rename | パーツを削除する（その面は1つ前のパーツに移ります）か、名前を変えます。Deleteは、パーツが2つ以上あるときに表示されます。 |
| Move Up / Move Down | 順番を変えます。パーツはこの順に描画されるので、透明なものは最後に置くとよいでしょう。 |

### Material

マテリアルは、パーツの見た目を決めます。色、テクスチャ、光の当たり方です。初期設定のマテリアルは編集できません。何かを変えるには、まず上のセレクターでマテリアルを作成します（**New Material**）。このセレクターでは、プロジェクトの別のマテリアルを選ぶこともできます。よく使うオプション：

| プロパティ | 働き |
|---|---|
| Rename Material | マテリアルの名前を変えます。 |
| Color | パーツのテクスチャ、**Base Color**の四角い色見本、テクスチャが使うUVマップ。 |
| Metallic、Roughness、Emission、Normal、Alpha | それぞれ、数値にもテクスチャにもできます（**Number / Texture**ボタン）。Alphaは透明度です。1未満にするか、アルファのテクスチャを使うと、マテリアルは透明になります。 |
| Alpha Test | アルファの低い部分を、混ぜ合わせずに切り抜きます。柵、金網、葉などに使います。0でオフになります。 |
| Transmission、IOR | ガラスと屈折です。**Ray Tracing**がオンのときだけ働きます（File / Renderタブ）。 |
| Lighting | シーンのライトが当たるかどうかです。オフにすると、色はフラットで、常に同じに見えます。グラフィックスのバックエンドにシェーダーがある場合は、**Shading**が表示されます：Per vertexまたはPer pixel。 |
| Vertex Color | ベースカラーの代わりに、メッシュにペイントした色を使います（[Vertex Paint](pintura.md#vertex-paint)を参照）。 |
| Filtering / Repeat | 近くで見たテクスチャを滑らかにします（チェックを外すとピクセルが見えます）／タイルのように繰り返します。 |
| Reflection | クロムのような、周囲の映り込みです。 |
| Texture Layers | 最初のテクスチャの上に、さらに4枚までのテクスチャを重ねます。それぞれにUVマップと合成方法があります。 |
| Lines / Line Width | パーツの辺を描画します。太さはピクセル単位です。 |
| Culling | 裏側の面を描画しません。 |
| Test、Depth Bias、Pass Order | 深度の高度なオプションです。デカールや、ほかのものの上に重ねるものに使います。 |
| Advanced: custom shader | マテリアルを独自に描画するファイルです。[自作シェーダー](shaders.md)を参照してください。 |

オブジェクトに物理がある場合（Add → Physics）、その**Physics**カードもこのタブに表示されます。

## 3D Meshタブ

![3D Meshタブ全体：使っているメッシュ、編集（Lock Normalsのチェックボックス付き）、頂点グループ（各ボーンの名前付き）、UVマップ、カラー](../tutoriales/img/objeto-malla-propiedades.webp)

### 3D Mesh

オブジェクトの形はプロジェクトのリソースで、複数のオブジェクトが同じメッシュを**共有**できます（Duplicate Linkedがそうします）。1つを編集すると、すべてが変わります。

| プロパティ | 働き |
|---|---|
| セレクター | オブジェクトが使うメッシュです（保存していない変更があると`*`が付きます）。ここから、プロジェクトの別のメッシュや**New Copy**を選べます。 |
| Users | シーンの中で、このメッシュを使っているオブジェクトの数です。 |
| Make Single User | メッシュが共有されているときに表示されます。オブジェクトに専用のコピーを持たせ、ほかのオブジェクトに影響を与えずに編集できるようにします。 |

メッシュがライブラリーのものである場合は、「It belongs to a library (read-only): open the library to edit it」という注意が表示され、メッシュを変更する項目はパネルから消えます。

### Editing

**Delete editing data**は、もうモデリングしないステージのためのものです。Edit Modeが使うデータをメッシュから取り除くので、読み込みが速くなり、メモリーも少なくて済みます（Nokia N95では重要です）。その間はEdit Modeに入れません。**Convert to editable mesh**で、データが再計算されます。

**Lock Normals**のチェックボックスをオンにすると、Edit Modeで頂点を動かしても、このメッシュの法線は再計算されません。こうすると、読み込んだモデルに付いていた法線や、手で修正した法線が保たれます。初期状態ではオフで、メッシュごとに設定します。

### Vertex Groups

**頂点グループ**は頂点のリストで、各頂点にウェイトがあります。[アーマチュア](objeto-esqueleto.md)は、これを使って、各ボーンがどの頂点を動かすかを知ります。グループはボーンと同じ名前を持ちます。ウェイトは[Weight Paint](pintura.md#weight-paint)でペイントします。

| プロパティ | 働き |
|---|---|
| Add Vertex Group / Rename | グループを作成する、または名前を変えます。 |
| Assign / Remove | Edit Modeで選択している頂点を、グループに入れる（ウェイト1）か、グループから外します。 |
| Select / Deselect | グループの頂点を選択、または選択解除して、グループがどこをつかんでいるかを確認します。 |
| Delete / Move Up / Move Down | グループを削除する、またはリストの中で移動します。 |

**UV Groups**も同じように働きますが、そのウェイトは[UV Editor](editor-uv.md)の2Dアーマチュア用です。

### UV Maps

**UVマップ**は、テクスチャをメッシュにどう貼るかを表します。メッシュは複数のUVマップを持てます。たとえば、カラーテクスチャ用に1つ、繰り返し方の違うテクスチャレイヤー用にもう1つです。マテリアルの各テクスチャが、どれを使うかを選びます。

**Add UV Map**で作成し、**Rename**で名前を変えます。Delete、Move Up、Move Downは、2つ以上あるときに表示されます。

### Color

**カラーレイヤー**は、Vertex Paintでメッシュにペイントした色を保存します。**Add Color Layer**で作成します。**Color Mode**では、**Per-Vertex**（頂点を共有する面は、その色も共有します）と**Per-Corner**（面ごとに別の色を持てるので、硬い境目を作れます）を切り替えます。Rename、Delete、Move Up、Move Downは、UVマップと同じです。

## Modifiersタブ

![Modifiersタブ全体。立方体にSubdivision Surfaceが付いています](../tutoriales/img/objeto-malla-modificadores.webp)

**モディファイアー**は、メッシュを壊さずに変更します。いつでもオフにしたり、並べ替えたり、削除したりでき、元のメッシュはそのまま残っています。

**Modifiers**カードには、リスト（上から下へ順に適用されます）、**Add**（種類のメニュー）、**Remove**、**Move Up / Move Down**があります。その下には、選んだモディファイアーのカード（名前がタイトルになります）があり、**Display in Viewport**（オフにすると計算されません）、**Display in Edit Mode**（オフにすると編集が速くなり、Edit Modeを出たときに再計算されます）、そのモディファイアーのプロパティ、そして**Apply Modifier**があります。Apply Modifierはモディファイアーをメッシュにベイクし、リストから外します。

モディファイアーごとに、それぞれのページがあります。

| モディファイアー | 用途 |
|---|---|
| [Screw](modificador-screw.md) | 断面を軸のまわりに回転させます：ネジ、花瓶、バネ。 |
| [Mirror](modificador-espejo.md) | メッシュを鏡映します。半分をモデリングすると、もう半分は自動でできます。 |
| [Array](modificador-array.md) | コピーを列状に並べます：階段、柵、鎖。 |
| [Subdivision Surface](modificador-subdivision.md) | メッシュを細分化して滑らかにします。 |
| [Boolean](modificador-booleano.md) | 別のオブジェクトと結合、交差、または差し引きします。 |
| [Culling](modificador-culling.md) | 最適化：カメラから見えないものを描画しません。 |
| [Armature](modificador-esqueleto.md) | アーマチュアでメッシュを変形します。 |

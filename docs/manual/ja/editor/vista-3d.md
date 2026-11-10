# 3D Viewport

**3D Viewport**は、シーンを表示して編集するビューポートです。モデリング、オブジェクトの移動、ペイント、ポーズ付け、アニメーションはここで行います。ほかのビューポートと同じく、上に**メニューバー**、下に**ツールバー**があります。

![Object Modeで立方体を選択した3D Viewport。上にメニューバー全体が見えます](../tutoriales/img/vista-3d-barra.webp)

- **メニューバー**のボタンとメニューを1つずつ説明したページ：[3D Viewportのメニューバー](vista-3d-barra.md)。
- **ナビゲーション**（オービット、平行移動、ズーム、ビュー、カメラ）：[3D Viewportでのナビゲーション](navegacion.md)とチュートリアル[3D Viewportのナビゲーション](../tutoriales/navegacion.md)。
- **移動、回転、拡大縮小**、ギズモ、ピボット、座標系：[移動・回転・拡大縮小](transformar.md)。

## モード

3D Viewportはいくつかの**モード**で動作し、クリック、キー、メニューの働きは現在のモードによって変わります。モードは、現在のモードが表示されているメニューバーの最初のボタンか、<kbd>Tab</kbd>で選びます（Object ↔ Edit。Nokia N95では<kbd>OK</kbd>）。

選べるモードは**アクティブなオブジェクト**によって変わります。**Object Mode**はすべてのオブジェクトにあります。3Dメッシュにはそれに加えてEdit Modeと3つのペイントモードがあり、アーマチュアにはEdit ModeとPose Modeがあります。それ以外のオブジェクト（ライト、カメラ、サウンド）では、モードはObject Modeだけなので、このボタンは表示されません。

![立方体を選択しているときのモードのメニュー：Object Mode、Edit Mode、Vertex Paint、Weight Paint、Texture Paint](../tutoriales/img/vista-3d-menu-modo.webp)

![アーマチュアを選択しているときのモードのメニュー：Object Mode、Edit Mode、Pose Mode](../tutoriales/img/vista-3d-modos-esqueleto.webp)

| モード | 対象のオブジェクト | 用途 |
|---|---|---|
| [Object Mode](modo-objeto.md) | すべて | オブジェクト全体を扱います：作成、移動、回転、拡大縮小、複製、ペアレント、削除。 |
| [Edit Mode](modo-edicion.md) | 3Dメッシュとカーブ | モデリング：メッシュの頂点、辺、面を動かし、追加し、削除します。 |
| [Edit Mode](esqueletos.md#edit-modeアーマチュア) | アーマチュア | アーマチュアを組み立てます：ボーンを作成し、レストポジションに配置します。 |
| [Pose Mode](esqueletos.md#pose-mode) | アーマチュア | ボーンにポーズを付けて、そのポーズを保存します。キャラクターはこうしてアニメーションさせます。 |
| [Weight Paint](pintura.md#weight-paint) | 3Dメッシュ | 各ボーンが各頂点をどれだけ動かすかをペイントします。 |
| [Vertex Paint](pintura.md#vertex-paint) | 3Dメッシュ | 頂点に直接色を塗ります。 |
| [Texture Paint](pintura.md#texture-paint) | 3Dメッシュ | オブジェクトの上から直接、テクスチャにペイントします。 |

### Object Mode

Whisk3Dを起動したときのモードで、すべてのオブジェクトにある唯一のモードです。オブジェクト全体を扱い、選択、移動、回転、拡大縮小、追加、削除をします。[Object Mode](modo-objeto.md)とチュートリアル[Object Mode](../tutoriales/modo-objeto.md)を参照してください。

### メッシュのEdit Mode

メッシュの**形**、つまり頂点、辺、面をモデリングします。メニューバーに**Vertex / Edge / Face**のボタン（クリックで何を選択するか）と、**Mesh**メニュー、**UV**メニューが表示されます。シーンのほかの部分は見えたままですが、触ることはできません。

![Edit Modeの立方体。1つの面が緑で選択され、その上にギズモがあります](../tutoriales/img/vista-3d-modo-edicion.webp)

### アーマチュアのEdit Mode

**アーマチュア**を組み立てます。ボーンを（別のボーンの先端から押し出して）作成し、移動し、ペアレントします。これがアーマチュアの**レスト**ポジション、つまりアニメーションしていないときの姿勢です。

![Edit ModeのLeonのアーマチュア。青いボーンがレストポジションにあります](../tutoriales/img/vista-3d-modo-edicion-esqueleto.webp)

### Pose Mode

アーマチュアに**ポーズを付けます**。アーマチュアそのものは変えずにボーンを回転、移動させ、メッシュがそれについてきます。いろいろなフレームに保存したポーズが、キャラクターのアニメーションになります。

![Pose ModeのLeonのアーマチュア。歩くアニメーションの途中で、ボーンが曲がっています](../tutoriales/img/vista-3d-modo-pose.webp)

### Weight Paint

**各ボーンがメッシュの各部分をどれだけ動かすか**をペイントします。選択したボーンが完全に動かす部分は赤、動かさない部分は青です。腕を曲げたときに、腰ではなく腕が曲がるのはこのおかげです。

![Weight PaintのLeonのメッシュ。選択したボーンは胸（赤と黄色）を動かし、ほかの部分（青）は動かしません](../tutoriales/img/vista-3d-modo-pesos.webp)

### Vertex Paint

テクスチャを使わずに、メッシュの**頂点に色を塗ります**。シンプルなモデルの色付け、手で描く影、シェーダーのエフェクトに使います。

![Vertex Paintの立方体。ブラシが端から端まで動き、触れた頂点が赤く染まります](../tutoriales/img/vista-3d-modo-vertices.webp)

### Texture Paint

人形に塗るように、オブジェクトの上から**直接テクスチャにペイントします**。塗ったものは、メニューバーに表示されるフロッピーディスクのボタンで保存します。

![Texture PaintのLeon。ブラシがジャケットに赤い線を描いています](../tutoriales/img/vista-3d-modo-textura.webp)

ペイントモードでは、ツールバーの上にブラシの行（**Radius**と**Value**）が表示されます。[ペイントモード](pintura.md)を参照してください。

## ツールバー

3D Viewportの下には、**Undo**と**Redo**（矢印）、**最近使ったコマンド**（最初はMove、Rotate、Scale、Delete。使ったものが先頭に来ます）、**Proportional Editing**があります。移動、回転、拡大縮小の間はツールバーが変わります。緑のチェックマークで確定、赤い×印でキャンセルし、座標系を選べます。**X**、**Y**、**Z**で、その軸に沿った動きに限定します。[移動・回転・拡大縮小](transformar.md#ツールバー)を参照してください。

![Android](../tutoriales/img/icono-android.svg) タッチスクリーンでは、**Shift**と**Ctrl**が加わります。オンのまま（緑で）残り、それぞれのキーの代わりになります。さらに**View**ボタン（モニター）があり、Edit Modeやペイントモードで、指で選択やペイントをする代わりにナビゲーションできます。

![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95では、画面をシーンに使えるように、ツールバーは最初は隠れていて、必要なときに表示されます。ペイントモードではブラシの行（**Radius**と**Value**）が表示され、Proportional Editingをオンにして移動、回転、拡大縮小している間は、その半径のバーが表示されます。ボタンは**仮想マウス**で使います。右ソフトキーでオンにし、方向キーで動かし、<kbd>OK</kbd>でクリックします。

## ギズモと3Dカーソル

![選択した立方体の上にギズモ（色付きの矢印）、左に3Dカーソルがあり、それぞれオレンジの円と名前で示されています](../tutoriales/img/vista-3d-gizmo-cursor.webp)

- **ギズモ**は、選択したものの上にある色付きの矢印です。つかんで移動、回転、拡大縮小します。ギズモをクリックすると、次のモードに切り替わります。[ギズモ](transformar.md#ギズモ)を参照してください。
- **3Dカーソル**は、十字の付いた円です。Addで追加したものはそこに現れ、ピボットポイントにもできます。[3Dカーソル](../tutoriales/modo-objeto.md#3dカーソル)を参照してください。

![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95では、画面が小さく、キーで移動するので、ギズモは最初はオフになっています。**Overlays → Gizmo**でオンにし、仮想マウスでつかみます。

# 2Dインターフェースと画面

| 関数 | 説明 |
|---|---|
| `screen() → width, height` | キャンバスのサイズ。 |
| `screenMargin() → mx, my` | 固定サイズのキャンバスのまわりに、実際の画面の端まで片側あたりどれだけ画面が余っているか（キャンバスの単位。フレーミングの帯も画面に含まれます）。縦向きで4:3のキャンバスなら、メーターを`my`だけ上にずらすと上端にぴったり付きます。 |
| `screenFrame() → x, y, width, height` | 3D画像の枠（帯を除いたカメラのフレーミング）。キャンバスの単位で、その中心から測ります。 |
| `captureScreen(name [, withHud]) → path` | このフレームの3D画像を（シーンの描画後に。`withHud`を指定するとHUDの後に）テクスチャへコピーします。パス（`"pantalla:<name>"`）は、`setTexture`、マテリアル、またはルーチンの「Bind texture」ステップに使えます。 |
| `captureUV(name) → u0, v0, u1, v1` | キャプチャーしたテクスチャのうち、画像が正しい向きになる部分。`setUVRect`用です（コピーがまだできていなければ`nil`。コピーはフレームの描画時に行われます）。 |
| `safeArea() → x, y, w, h` | ノッチを除いた画面の部分（`start`では使わないでください）。 |
| `getScale() → f` | 引数なしの場合：サイズを画面に合わせるための係数（短い辺 / 480）。 |
| `getPosPx(obj) → x, y`、`setPosPx(obj, x, y)` | 2D要素の位置（`nil`を渡した軸は変わりません）。 |
| `getSizePx(obj) → w, h`、`setSizePx(obj, w, h)` | 2D要素のサイズ。 |
| `uiBox(obj) → x, y, w, h` | 実際に占めている矩形（x, y = 中心）。 |
| `setText(obj, value)`、`setSize(text, px)` | テキストまたはボタンの文字列と、文字のサイズを変えます。 |
| `setTexture(img, "ruta.png")`、`setUVRect`、`setFlipbook` | 画像を変える、一部を切り抜く（u1 < u0 または v1 < v0 なら反転）、またはコマのシートでアニメーションします。 |
| `setOpacity(obj, 0..1)`、`fade(obj, objetivo, paso)` | 透明度とフェード。 |
| `setRot2D(obj, degrees)` | 2D要素を回転します。 |
| `collide(a, b) → bool` | 2つの2D要素が画面上で重なっているかどうか。 |
| `limit(obj, area [, axis])`、`inside(area, x [, y])` | 要素を別の要素の内側に収める、または点が内側にあるかを調べます。 |
| `quit()` | ゲームを終了します（エディターでは何もしません）。 |

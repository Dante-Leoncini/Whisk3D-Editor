# 3D Viewport

3D Viewportは、シーンを表示して編集する場所です。いくつかの**モード**で動作し、クリック、キー、メニューの働きは現在のモードによって変わります。モードは上部バーのセレクター（アクティブなオブジェクトがメッシュまたはアーマチュアのときだけ表示されます）か、<kbd>Tab</kbd>で選びます。

| モード | 対象のオブジェクト | 用途 |
|---|---|---|
| [Object Mode](modo-objeto.md) | すべて | オブジェクト全体の作成、移動、回転、拡大縮小、複製、ペアレント、削除。 |
| [Edit Mode](modo-edicion.md) | メッシュとカーブ | 頂点、辺、面の編集（モデリング）。 |
| [Edit Mode（アーマチュア）](esqueletos.md#edit-modeアーマチュア) | アーマチュア | ボーンの作成と配置。 |
| [Pose Mode](esqueletos.md#pose-mode) | アーマチュア | ボーンのポーズ付けとアニメーション。 |
| [Weight Paint](pintura.md#weight-paint) | 3Dメッシュのみ | 各ボーンが各頂点をどれだけ動かすかをペイントします。 |
| [Vertex Paint](pintura.md#vertex-paint) | 3Dメッシュのみ | 頂点に直接色を塗ります。 |
| [Texture Paint](pintura.md#texture-paint) | 3Dメッシュのみ | オブジェクトの上から直接、テクスチャにペイントします。 |

## 3D Viewportの上部バー

| ボタン | 説明 |
|---|---|
| Mode selector | モードを切り替えます（Object、Edit、Pose、Paint…）。 |
| Vertex / Edge / Face | Edit Modeのみ：何を選択するかを決めます。 |
| Transform Pivot Point | 回転と拡大縮小の中心：3D Cursor、Individual Origins、Median Point、Active Element。Lock Normalsのチェックボックス付きです。 |
| Orientation | トランスフォームの軸：Global、Local、Normal（面の向き）、View。 |
| Snap（スナップ） | スナップのオプションです。有効なときは緑になります（<kbd>Shift+Tab</kbd>）。 |
| Proportional Editing | 何かを動かすと、近くにあるものが滑らかについてきます。有効なときは緑になります（<kbd>O</kbd>）。 |
| View | カメラ、視点、フレーミング、Local View、Perspective、Lock Orbit。 |
| Select | All、None、Invert、Box Select、Select Linked、ループと経路の選択。 |
| Add (+) | Object Modeのみ：新しいオブジェクトを作成します。 |
| Mesh | Edit Modeのみ：モデリングの操作です。 |
| Save Texture（フロッピーディスク） | Texture Paintのみ。保存していないペイントがあると緑になります。 |
| Animation（ひし形） | キーフレームの挿入、削除、クリア。Motion Trail。 |
| Context | モードのメニュー：Object、Vertex/Edge/Face、Armature、Pose。 |
| Overlays（目） | 描画する補助表示：Floor、軸、Gizmo、3D Cursor、Normals、Statistics、X-Ray。 |
| Render（カメラ） | シェーディングモードです。 |
| Scene | 編集するプロジェクトのシーン、ゲーム、プレハブを選び、新しく作成します。 |
| UV | Edit Modeのみ：UVの展開と投影。 |

## 複数のモードに共通のメニュー

### Select

| 機能 | 説明 | ショートカット |
|---|---|---|
| All | すべてを選択します。 | <kbd>A</kbd> |
| None | すべての選択を解除します。 | <kbd>Alt+A</kbd> |
| Invert | 選択中のものを解除し、それ以外を選択します。 | <kbd>Ctrl+I</kbd> |
| Box Select | 左から右へドラッグする（緑）と、完全に囲まれたものだけを選択します。右から左へ（青）では、触れたものも選択します。<kbd>Shift</kbd>で追加します。 | <kbd>B</kbd> |
| Show selection | 選択したものの輪郭を表示・非表示にします。 | メニュー |

### Render（シェーディングモード）

| 機能 | 説明 |
|---|---|
| Render Preview | シーンのライトを使って表示します。レイトレーシングが有効な場合は、トレースした画像が少しずつ表示されます。 |
| Material Preview | 初期設定の表示です。ビューの方向からのニュートラルなライトで照らし、シーンのライトには依存しません。 |
| Solid Preview / Wireframe Preview / ZBuffer Preview | 単色のグレー、辺のみ、または深度を表示します。 |
| Normal View / Alpha Preview | 法線をカラーで、または透明度を白黒で表示します。 |
| Motion vectors | 前のフレームから各ピクセルが画面上でどれだけ動いたか（赤 = X、緑 = Y、ラベンダー色 = 静止、オリーブ色 = 背景）。レンダーの `_vectores` パスと同じ画像です。 |
| Wireframe: Only Visible | ワイヤーフレーム表示で、実際に描画されるものだけを表示します。 |

### Overlays

| 機能 | 説明 | ショートカット |
|---|---|---|
| Show Overlays | すべての補助表示を一度にオン・オフします。 | <kbd>K</kbd> |
| Floor、X Axis、Y Axis、Origins、3D Cursor、Relationship Lines | 補助表示を個別に切り替えます。 | メニュー |
| Gizmo | 移動・回転・拡大縮小のギズモです。 | メニュー |
| Objects | アーマチュア、ライト、カメラ、エンプティ、パーティクル、カーブ、ヒットボックスの表示・非表示。 | メニュー |
| Normals | 頂点法線、カスタム法線、面の法線を、指定したサイズで描画します。 | メニュー |
| Statistics | FPS、頂点数、面数、ドローコール数、処理時間。 | メニュー |
| X-Ray | 編集中のメッシュが半透明になり、裏側の要素も選択できるようになります。 | メニュー |

### Snap（スナップ）

| 機能 | 説明 | ショートカット |
|---|---|---|
| Toggle Snap | 動かしたものが、頂点、辺、面、またはそれらの中心に吸着します。 | <kbd>Shift+Tab</kbd> |
| Snap Base / Snap Target | 動かすもののどの点（Closest、Center、Median、Active）を、何に吸着させるかを決めます。 | メニュー |
| Face Project / Face Nearest | リトポロジー用：頂点を別のサーフェスの上に乗せます。 | メニュー |
| Affect Move / Rotate / Scale | スナップがどの操作で働くかを決めます。 | メニュー |
| Snap menu（カーソル） | Selection to Grid、Selection to Cursor、Selection to Active。Cursor to Selected、Cursor to World Origin、Cursor to Grid、Cursor to Active。 | <kbd>Shift+S</kbd> |

### Proportional Editing

| 機能 | 説明 | ショートカット |
|---|---|---|
| Enable | 移動、回転、拡大縮小するときに、近くのものが距離に応じてついてきます。 | <kbd>O</kbd> |
| Connected Only | 距離を辺に沿って測ります。つながっていないパーツは引きずられません。 | メニュー |
| Falloff | 影響の形：Smooth、Sphere、Root、Inverse Square、Sharp、Linear、Constant、Random。 | メニュー |
| Proportional Size | 影響の半径です。トランスフォーム中は、ホイールまたは<kbd>PageUp</kbd> / <kbd>PageDown</kbd>で変更できます。 | メニュー / ホイール |

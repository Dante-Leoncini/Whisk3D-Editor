# 3D Viewportのメニューバー

![Object Modeの3D Viewportのメニューバー。立方体を選んでいます：Object Mode、ピボット、Global、スナップ、プロポーショナル編集、View、Select、Add、Animation、Object、Overlays、Render、シーン](../tutoriales/img/vista-3d-barra-menus.webp)

メニューバーが収まりきらないときは、スクロールできます。[メニューバーのオプションが全部見えないとき](interfaz.md#メニューバーのオプションが全部見えないとき)を参照してください。

## モード

![モードのボタン](../tutoriales/img/boton-vista3d-modo.webp) 作業中のモードを表示し、タップすると別のモードを選べます。表示されるモードは、アクティブなオブジェクトによって変わります。メッシュにはEdit Modeと3つのペイントモードがあり、アーマチュアにはEdit ModeとPose Modeがあります。ほかの種類のオブジェクト（ライト、カメラ）では、モードはObject Modeしかないので、このボタンは表示されません。各モードの説明は[モード](vista-3d.md#モード)にあります。

![立方体を選んでいるときのモードのメニュー：Object Mode、Edit Mode、Vertex Paint、Weight Paint、Texture Paint](../tutoriales/img/vista-3d-menu-modo.webp)

![PC](../tutoriales/img/icono-pc.svg) <kbd>Tab</kbd>でObject ModeからEdit Modeに切り替わり、もう一度押すと戻ります。![Nokia](../tutoriales/img/icono-nokia.svg) N95では<kbd>OK</kbd>です。

メッシュの**Edit Mode**では、その横に**Vertex / Edge / Face**のボタンが表示されます。クリックで何を選択するかを決めます（[Edit Mode](modo-edicion.md)を参照）。

## ピボットポイント

![ピボットのボタン](../tutoriales/img/boton-vista3d-pivote.webp) **ピボットポイント**は、トランスフォームの基準になる場所です。選択したものを何のまわりに回転させ、どこから拡大縮小するかを決めます。ボタンのアイコンは選んだピボットによって変わるので、どれが設定されているかがひと目でわかります。

![Transform Pivot Pointメニュー：3D Cursor、Individual Origins、Median Point（選択中、緑）、Active Element。それぞれにアイコンがあります](../tutoriales/img/vista-3d-menu-pivote.webp)

| ピボット | 回転と拡大縮小の基準 |
|---|---|
| ![3D Cursor](../tutoriales/img/boton-pivote-cursor.webp) **3D Cursor** | 3Dカーソルです。任意の点のまわりに回転させるのに使います。カーソルをそこに置いて（[Snapメニュー](#スナップ)で）、回転させます。 |
| ![Individual Origins](../tutoriales/img/boton-pivote-individual.webp) **Individual Origins** | 各オブジェクトの原点です。すべてが同時に、それぞれの位置で回転します。 |
| ![Median Point](../tutoriales/img/boton-pivote-medio.webp) **Median Point** | 選択したもの全体の中心です。初期設定はこれです。 |
| ![Active Element](../tutoriales/img/boton-pivote-activo.webp) **Active Element** | アクティブなオブジェクト（最後に選んだもの）の原点です。ほかのものはそのまわりを回ります。 |

それぞれの例は、チュートリアル[Object Mode](../tutoriales/modo-objeto.md#ピボットポイント)の動画で見られます。

## 座標系

![座標系のボタン](../tutoriales/img/boton-vista3d-orientacion.webp) 1つの軸に沿って移動、回転、拡大縮小するとき（<kbd>X</kbd>、<kbd>Y</kbd>、<kbd>Z</kbd>、ギズモ、N95のキーで）に使う軸です。ボタンには選んだ座標系が表示されます。

![Transform Orientationsメニュー：Global、Local、Normal、View](../tutoriales/img/vista-3d-menu-orientacion.webp)

| 座標系 | 軸 |
|---|---|
| **Global** | ワールドの軸です。常に同じで、Zが上です。初期設定はこれです。 |
| **Local** | オブジェクト自身の軸です。オブジェクトと一緒に回転します。回転させた車は、自分のLocal軸の方向に進みます。 |
| **Normal** | Edit Mode用：選んだ面が向いている方向です（Extrudeが使う方向です）。 |
| **View** | 画面の軸です。Xが横、Yが上です。 |

![PC](../tutoriales/img/icono-pc.svg) トランスフォーム中に軸の文字をもう一度押すと、GlobalからLocalに切り替わります：<kbd>G</kbd> <kbd>Y</kbd> <kbd>Y</kbd>。

## スナップ

![スナップのボタン](../tutoriales/img/boton-vista3d-iman.webp) **スナップ**がオンのときは、動かしたものが何かに吸着します。ほかのオブジェクトの頂点、辺、面、またはそれらの中心です。オンのときは、ボタンが緑になります。

![Snapメニュー：Enable、Snap Base、Snap Target、Affect Move、Rotate、Scaleと、Include Active、Edited、Non-Editedのチェックボックス](../tutoriales/img/vista-3d-menu-iman.webp)

| オプション | 働き | ショートカット |
|---|---|---|
| Enable | スナップをオン・オフします。 | <kbd>Shift+Tab</kbd> |
| Snap Base | 動かすもののどの点を吸着させるか：いちばん近い点、中心、中央値、アクティブ。 | メニュー |
| Snap Target | 何に吸着させるか：頂点、辺、面、それらの中心。Face Project / Face Nearestでは、頂点を別のサーフェスの上に乗せます（リトポロジー用）。 | メニュー |
| Affect Move / Rotate / Scale | どの操作で働くかです。 | メニュー |
| Include Active / Edited / Non-Edited | どのオブジェクトに吸着できるかです。 | メニュー |

**3Dカーソル**は、別のメニュー、カーソルのスナップメニューで動かします：<kbd>Shift</kbd> + <kbd>S</kbd>、またはObjectメニュー → Snap（Cursor to Selected、Selection to Cursor...）。説明はチュートリアル[Object Mode](../tutoriales/modo-objeto.md#3dカーソル)にあります。

## プロポーショナル編集

![プロポーショナル編集のボタン](../tutoriales/img/boton-vista3d-proporcional.webp) **プロポーショナル編集**では、何かを移動、回転、拡大縮小すると、近くにあるものが滑らかについてきます。近いほど大きく動きます。頂点を1つずつ動かさずに、有機的な形（地形、顔）をモデリングするのに使います。オンのときは緑になります。

![Proportional Editingメニュー：Enable、Connected Only、減衰の形（Smooth、Sphere、Root、Inverse Square、Sharp、Linear、Constant、Random）、Proportional Size](../tutoriales/img/vista-3d-menu-proporcional.webp)

| オプション | 働き | ショートカット |
|---|---|---|
| Enable | プロポーショナル編集をオン・オフします。 | <kbd>O</kbd> |
| Connected Only | 距離を辺に沿って測ります。近くにあっても、つながっていないパーツは引きずられません。 | メニュー |
| 減衰 | 距離に応じて影響をどう配分するか：Smooth、Sphere、Root、Inverse Square、Sharp、Linear、Constant、Random。 | メニュー |
| Proportional Size | 影響の半径です。トランスフォーム中は、ホイールまたは<kbd>PageUp</kbd> / <kbd>PageDown</kbd>で変更できます。 | メニュー / ホイール |

## View

![Viewボタン](../tutoriales/img/boton-vista3d-vista.webp) **ナビゲーション**に関するすべてです。シーンをどこから見るかを決めます。各オプションの動画付きの説明は、チュートリアル[3D Viewportのナビゲーション](../tutoriales/navegacion.md)にあります。

![Viewメニュー：Cameras、Viewpoint、Frame Selected、Local View、Perspective/Ortho、Lock Orbit、Lock the camera to the view](../tutoriales/img/vista-3d-menu-vista.webp)

| オプション | 働き | ![PC](../tutoriales/img/icono-pc.svg) ショートカット |
|---|---|---|
| Cameras | アクティブなカメラから見る、または選んだオブジェクトをアクティブなカメラにします。 | <kbd>Num 0</kbd> / <kbd>Ctrl+Num 0</kbd> |
| Viewpoint | Front、Back、Right、Left、Top、Bottom。 | <kbd>Num 1</kbd>、<kbd>Num 3</kbd>、<kbd>Num 7</kbd>（<kbd>Ctrl</kbd>を押すと反対側） |
| Frame Selected | 選んだものをビューの中心に置き、ちょうど収まるまで近づけます。 | <kbd>Num .</kbd> |
| Local View | 選んだものだけを表示します。もう一度で元に戻ります。 | <kbd>/</kbd> |
| Perspective/Ortho | 透視投影と平行投影を切り替えます。 | <kbd>Num 5</kbd> |
| Lock Orbit | オービットしていた操作が、ビューの平行移動になります。 | — |
| Lock the camera to the view | カメラから見ているとき：ナビゲーションでカメラが動きます。 | — |

## Select

![Selectボタン](../tutoriales/img/boton-vista3d-seleccionar.webp) すべて、なし、今の選択の反対、またはボックスの中に入るものを、一度に選択します。

![Selectメニュー：All、None、Invert、Box Select](../tutoriales/img/vista-3d-menu-seleccionar.webp)

| オプション | 働き | ショートカット |
|---|---|---|
| All | すべてを選択します。 | <kbd>A</kbd> |
| None | 何も選択していない状態にします。 | <kbd>Alt+A</kbd> |
| Invert | 選択していたものを解除し、それ以外を選択します。 | <kbd>Ctrl+I</kbd> |
| Box Select | ドラッグで描くボックスです。左から右へ（緑）では、完全に中に入ったものだけを選択し、右から左へ（青）では、触れたものも選択します。<kbd>Shift</kbd>で追加します。 | <kbd>B</kbd>、または背景でドラッグ |

Edit Modeでは、Select Linked、ループ、経路の選択が加わります（[Edit Mode](modo-edicion.md)を参照）。

## Add

![Addボタン](../tutoriales/img/boton-vista3d-anadir.webp) 新しいオブジェクトを作成します。オブジェクトは3Dカーソルの位置に現れ、選択された状態になります。Object Modeにだけあります。

![Addメニューを開いたところ。すべての種類のオブジェクトが並んでいます](../tutoriales/img/vista-3d-menu-anadir.webp)

各種類の説明は[Object Mode → Addメニュー](modo-objeto.md#addメニュー)にあります。![PC](../tutoriales/img/icono-pc.svg) <kbd>Shift</kbd> + <kbd>A</kbd>で、マウスの位置に開きます。

Edit Modeでは、代わりに**Mesh**（モデリングの操作。[Edit Mode](modo-edicion.md)を参照）と**UV**（UVの展開と投影。[UV Editor](editor-uv.md)を参照）が表示されます。Texture Paintでは**フロッピーディスク**が表示され、ペイントしたテクスチャを保存します。保存していないペイントがあると緑になります。

## Animation

![Animationボタン](../tutoriales/img/boton-vista3d-animacion.webp) 選択したものの**キーフレーム**です。現在のフレームで位置、回転、スケールの値を保存し、アニメーションがキーフレームからキーフレームへ進むようにします。何かを選択しているとき、Object ModeとPose Modeで表示され、オプションは選択したものによって変わります。

![立方体を選んでいるときのAnimationメニュー：Insert Keyframe、Insert Mesh Keyframe、Delete Keyframe、Clear Keyframe、Delete Mesh Layer、Motion Trail](../tutoriales/img/vista-3d-menu-animacion.webp)

| オプション | 働き | ショートカット |
|---|---|---|
| Insert Keyframe | 何を保存するかを選ぶメニューです：位置、回転、スケール、またはすべて。 | <kbd>I</kbd>（<kbd>Shift+I</kbd>：確認なしですべて） |
| Insert Mesh Keyframe | 3Dメッシュのみ：メッシュのアニメーションのうち、頂点、法線、UVのどれを保存するかを選びます。 | メニュー |
| Delete Keyframe | 現在のフレームのキーフレームを削除します。 | メニュー |
| Clear Keyframe | 選択したもののアニメーションをすべて削除します。 | メニュー |
| Delete Mesh Layer | 3Dメッシュのみ：メッシュのアニメーションのレイヤーを1つ丸ごと削除します。**頂点**、**法線**（たとえば、ライティングを使わないモデルで）、**UV**のレイヤーです。キーフレームは必ず1つ以上残ります。 | メニュー |
| Motion Trail | アニメーションするオブジェクトが通る経路を、ビューに描画します。 | メニュー |

### Pose Modeでは

アーマチュアにポーズを付けているときは、メニューは選択したボーン（とアクティブなボーン）のキーフレームを扱います。

![Pose ModeのAnimationメニュー。Leonのアーマチュアです：Insert Keyframe、Delete Keyframe、Clear Keyframe、Reset Pose、Motion Trail](../tutoriales/img/vista-3d-menu-animacion-pose.webp)

| オプション | 働き | ショートカット |
|---|---|---|
| Insert Keyframe | 選択したボーンのポーズを、現在のフレームに保存します：位置、回転、スケール、またはすべて。 | <kbd>I</kbd> |
| Delete Keyframe | それらのボーンの、現在のフレームのキーフレームを削除します。<kbd>Ctrl+Z</kbd>で元に戻せます。 | メニュー |
| Clear Keyframe | アクティブなアニメーションにある、それらのボーンのキーフレームをすべて削除します。<kbd>Ctrl+Z</kbd>で元に戻せます。 | メニュー |
| Reset Pose | 選択したボーン（何も選択していなければすべてのボーン）が、Edit Modeでの位置、回転、スケールに戻ります。<kbd>Ctrl+Z</kbd>で元に戻せます。 | メニュー |
| Motion Trail | アニメーションするオブジェクトが通る経路を、ビューに描画します。 | メニュー |

アニメーションには専用のチュートリアルと章があります：[Timeline](linea-de-tiempo.md)。

## Object

![Objectボタン](../tutoriales/img/boton-vista3d-objeto.webp) 選択したものに対してできる操作です。ビューで右クリックしたとき（タッチスクリーンでは指を1本置いたままにしたとき）に開くメニューと同じです。何かを選択しているときに表示されます。

![Objectメニュー：Transform、Set Origin、Apply、Snap、Cut、Copy、Paste、Duplicate Objects、Duplicate Linked、Join、Set Parent、Clear Parent、Delete、プレハブ](../tutoriales/img/vista-3d-menu-objeto.webp)

各オプションの説明は[Object Mode](modo-objeto.md)にあります。Edit Modeでは、このボタンは選択対象（Vertex、Edge、Face）のメニューになります。Pose ModeではPoseメニューに、アーマチュアの編集ではArmatureメニューになります。

## Overlays

![Overlaysボタン](../tutoriales/img/boton-vista3d-superposiciones.webp) **オーバーレイ**は、作業のためにシーンの上に描画される補助表示で、レンダリングには出ません。床、軸、ギズモ、3Dカーソルなどです。ここで、まとめて、または1つずつオン・オフします。

![Overlaysメニュー：Show Overlays、Floor、X Axis、Y Axis、Origins、Gizmo、Objects、3D Cursor、Relationship Lines、Normals、Statistics、X-Ray、Black Letterbox](../tutoriales/img/vista-3d-menu-superposiciones.webp)

| オプション | 働き | ショートカット |
|---|---|---|
| Show Overlays | すべての補助表示を一度にオン・オフします。 | <kbd>K</kbd> |
| Floor、X Axis、Y Axis、Origins、3D Cursor、Relationship Lines | 補助表示を個別に切り替えます。Relationship Linesは、各子とその親を結びます。 | メニュー |
| Gizmo | 移動・回転・拡大縮小のギズモです。Nokia N95では初期状態でオフです。 | メニュー |
| Objects | どの種類のオブジェクトを描画するか：アーマチュア、ライト、カメラ、エンプティ、パーティクル、カーブ、ヒットボックス。 | メニュー |
| Normals | 頂点または面の法線を、指定したサイズで描画します。 | メニュー |
| Statistics | 1秒あたりのフレーム数、頂点数、面数、ドローコール数、処理時間。 | メニュー |
| X-Ray | Edit Modeで、メッシュが半透明になり、裏側の要素も選択できるようになります。 | メニュー |
| Black Letterbox | 画面と縦横比の違うカメラから見ているときに、ショットの外側を隠します。 | メニュー |

## Render

![Renderボタン](../tutoriales/img/boton-vista3d-render.webp) **シェーディングモード**です。ビューでシーンをどう描画するかを決めます。プロジェクトは変わらず、作業中の見え方が変わるだけです。

![Renderメニュー：Render Preview、Material Preview、Solid Preview、Wireframe Preview、ZBuffer Preview、Normal View、Alpha Preview、Motion vectors、Wireframe: Only Visible](../tutoriales/img/vista-3d-menu-render.webp)

![Leonが、シェーディングモードを1つずつ順に切り替えていきます](../tutoriales/img/vista-3d-sombreado.webp)

| モード | 見え方 |
|---|---|
| Render Preview | シーンのライトを使って表示します。レイトレーシングがオンのときは、トレースした画像が少しずつ表示されます。 |
| Material Preview | 初期設定です。マテリアルとテクスチャを、ビューの方向からのニュートラルなライトで表示し、シーンのライトには依存しません。 |
| Solid Preview | 形を見るために、すべてをグレーで表示します。 |
| Wireframe Preview | 辺だけを表示します。**Wireframe: Only Visible**では、正面から見えるものの辺だけを表示します。 |
| ZBuffer Preview | 深度です。近いほど明るく、遠いほど暗くなります。 |
| Normal View | 法線をカラーで表示します。 |
| Alpha Preview | 透明度を白黒で表示します。 |
| Motion vectors | 前のフレームから各ピクセルがどれだけ動いたか（赤 = X、緑 = Y）。レンダーの`_vectores`パスと同じ画像です。 |

![PC](../tutoriales/img/icono-pc.svg) <kbd>Z</kbd>で次のモードに切り替わります。![Nokia](../tutoriales/img/icono-nokia.svg) N95では<kbd>6</kbd>です。

## シーン

![シーンのボタン](../tutoriales/img/boton-vista3d-escena.webp) 編集しているものの名前です。プロジェクトの**シーン**、**ゲーム**、または**プレハブ**です。タップすると、別のものを選んだり、新しく作成したり、シーンをゲームに変換したりできます。

![シーンのメニュー：現在のシーン、New Scene、New Game、New Prefab、Convert to Game](../tutoriales/img/vista-3d-menu-escena.webp)

シーン、ゲーム、プレハブについては、[プロジェクト](proyecto.md)を参照してください。

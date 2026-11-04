# Properties

アクティブなオブジェクトのすべてのデータを、タブ（アイコン）に分けて表示するパネルです。表示されるタブは、オブジェクトの種類によって変わります。カードのタイトルをクリックすると折りたたまれます。数値フィールドでは、クリックで入力（計算式も使えます）、ドラッグで値を増減できます。右側のひし形で、キーフレームを追加・削除します。

| タブ | 内容 |
|---|---|
| File / Render（フロッピーディスク） | **File**：Open project、Save、Save As、Save Version、Extract assets。**Render**：出力先、サイズ、追加のレンダーパス、背景、**Ray Tracing**（レイ数、サンプル数、パス数）、Render ImageとRender Animation。**Game**：プラットフォーム（Windows、Symbian、Linux .deb、AppImage、WebGL、Android）、ウィンドウ、画面の向き、アイコン、物理、サウンド、Build game。**Export**：OBJ、glTF、GLB。**Settings**：言語、アンチエイリアス、グラフィックス、スキン、ログ、スケール、**Keyboard Shortcuts…**、Save Changes。 |
| Object | 位置、回転（オイラー角、クォータニオン、軸と角度）、スケール、名前、表示、レンダリング。オブジェクトのアニメーションとパレット。 |
| Contextual | 種類によって変わります：メッシュのパーツと**マテリアル**（テクスチャ、ライティング、頂点カラー、透明、反射、ノーマルマップ、ブレンド、深度、テクスチャレイヤー、**Advanced: custom shader**（[自作シェーダー](shaders.md)を参照）。レイトレーシングではRoughness、Metallic、Transmission、IOR）、ライト、カメラ、フォグ、パーティクル、物理、ヒットボックス、アーマチュアとボーン、2D要素、プレハブ、ルーチン。 |
| 3D Mesh | 使用しているメッシュ（と、それを共有している数）、UVマップ、カラーレイヤー、頂点グループ、UVグループ。Libraryでリソースを選んでいる場合は、そのデータと場所（.w3dの中か外か）。 |
| Modifiers | [モディファイアー](modificadores.md)を参照してください。 |
| Transform | Edit Modeのみ：選択範囲とそのUVの中心を表示し、編集できます。 |
| Armature 2D | メッシュの2Dアーマチュアと2Dボーン。 |
| Constraints | Copy Location、Copy Rotation、Billboard（カメラの方を向く）、Child Of。 |
| Scripts | オブジェクトのLuaスクリプトとそのプロパティ（[Luaのドキュメント](../lua/README.md)を参照）。 |
| Animation | アクティブなアニメーション、ブレンドレイヤー（Mix）、選択したキーフレームの正確な値、カメラのカット。 |

## ルーチン

ルーチンは、Propertiesで組み立てる描画ステップのリストです（移動、回転、テクスチャの設定、メッシュの描画、別のルーチンの呼び出し、Call Lua…）。N95でいちばん高速に描画できる方法です。

| 機能 | 説明 | ショートカット |
|---|---|---|
| Add / Remove（ステップ） | テーマ別にまとめられたステップを追加する、または選択したステップを削除します。 | ボタン。<kbd>X</kbd>または<kbd>Delete</kbd>で削除 |
| Grab step | 方向キーでステップを並べ替えます。<kbd>Enter</kbd>で確定します。 | <kbd>G</kbd> |
| Insert Keyframe | ステップの値を、現在のフレームでアニメーションに記録します。 | <kbd>I</kbd> |
| Repeat | リストを続けて何回実行するか（Routine loop）。1 = 通常のルーチン。 | Repeatフィールド |
| Render mode / Use | シェーディングモードごとに別のリストを使う、またはすべてで同じリストを使います。 | ドロップダウン |

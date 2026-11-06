# 3D Viewportでのナビゲーション

## マウス（PC）

| 機能 | 説明 | 使い方 |
|---|---|---|
| オービット | ピボットポイントを中心にビューを回転します。 | 中ボタン + ドラッグ、または<kbd>Alt</kbd> + 左ボタン + ドラッグ。 |
| 平行移動（パン） | ビューを横方向に動かします。 | <kbd>Shift</kbd> + 中ボタン、または<kbd>Alt+Shift</kbd> + 左ボタン + ドラッグ。 |
| ズーム | 近づけたり遠ざけたりします。 | マウスホイール。 |
| Select | ポインターの下にあるものを選択します（ボタンを離したときに確定します）。 | 左クリック。<kbd>Shift</kbd>+クリックで追加または解除。 |
| Box Select | 背景の上をドラッグすると、四角形で範囲を指定できます。 | 左ボタンでドラッグ、または<kbd>B</kbd>。 |
| Context menu | ポインターの位置にモードのメニューを開きます（トランスフォーム中は、トランスフォームをキャンセルします）。 | 右クリック。 |
| Lock Orbit | オービットだった操作がパンになり、ビューが2Dのボードのようになります。 | Viewメニュー → Lock Orbit。 |
| Lock the camera to the view | アクティブなカメラから見ているとき、ナビゲーションでカメラが動きます。こうしてショットを調整します。 | 鍵をクリック（カメラのビューの下）、またはViewメニュー → Lock the camera to the view。 |

## キーボード

| 機能 | 説明 | ショートカット |
|---|---|---|
| Front View / Back View | 正面または背面から見ます。 | <kbd>Num1</kbd> / <kbd>Ctrl+Num1</kbd> |
| Right View / Left View | 側面から見ます。 | <kbd>Num3</kbd> / <kbd>Ctrl+Num3</kbd> |
| Top View / Bottom View | 上または下から見ます。 | <kbd>Num7</kbd> / <kbd>Ctrl+Num7</kbd> |
| Perspective / Orthographic | 2つの投影方法を切り替えます。 | <kbd>Num5</kbd> |
| ビューの回転 | ビューをその軸まわりに15°回転します。 | <kbd>Num4</kbd> / <kbd>Num6</kbd> |
| Camera View | アクティブなカメラから見ます（もう一度押すと戻ります）。 | <kbd>Num0</kbd> |
| Set Active Camera | アクティブなオブジェクトをアクティブなカメラにします。 | <kbd>Ctrl+Num0</kbd> |
| Frame Selected | 選択したものを中心に表示し、ズームを合わせます。 | <kbd>Num.</kbd>（テンキーのピリオド） |
| Local View | 選択したものだけを表示して画面に収めます。もう一度押すとすべて元に戻ります。 | <kbd>/</kbd> |
| モデルのインポート | ファイルブラウザーを開いて、OBJ、FBX、glTF、GLBをインポートします（PCのみ）。 | <kbd>Num9</kbd> |

## 指（Android、タッチ操作のWeb）

| 機能 | 説明 | ジェスチャー |
|---|---|---|
| オービット | ビューを回転します。 | 1本指でドラッグ。 |
| ズームとパン | ピンチで拡大・縮小し、2本の指を一緒に動かすとパンします。 | 2本指。 |
| Select | 指の下にあるものを選択します。 | 動かさずにタップして離します。 |
| Frame Selected | <kbd>Num.</kbd>と同じです。 | 2本指でダブルタップ。 |
| タッチ用の修飾キー | 下部バーのShiftボタンとCtrlボタンです。オン（緑）のままになり、選択への追加や経路の選択に使えます。 | Shift / Ctrlをタップ。 |
| View（ナビゲーション） | Edit Modeとペイントモードで、指が選択やペイントの代わりにビューを操作するようになります。 | 下部バーにあるモニターの形のボタン。 |

## Nokia N95のキーパッド

| 機能 | キー |
|---|---|
| オービット | 方向キー（長押し） |
| ズーム / Frame Selected | <kbd>0</kbd> + 上/下 / <kbd>0</kbd>のみ |
| パン | <kbd>\*</kbd> + 方向キー |
| ビュー（正面、側面、上）/ Perspective-Orthographic | <kbd>#</kbd> + 方向キー / <kbd>#</kbd>のみ |
| Move / Rotate / Scale（トランスフォーム中：X / Y / Z軸） | <kbd>1</kbd> / <kbd>2</kbd> / <kbd>3</kbd> |
| Confirm / Cancel | OK（中央）/ <kbd>C</kbd> |
| Delete（キャンセルするものがないとき）/ Undo と Redo | <kbd>C</kbd> / <kbd>C</kbd> + 左または右 |
| シェーディングの切り替え | <kbd>6</kbd> |
| Extrude（Edit Mode） | <kbd>7</kbd> |
| Loop Cut（Edit Mode）/ Camera View（Edit Mode以外） | <kbd>8</kbd> |
| Lock Orbit | <kbd>9</kbd> |
| Lock the camera to the view（カメラから見ているとき） | 左ソフトキー → Viewメニュー |
| 選択の切り替え / 追加、解除、すべて、なし | Pencilキー / Pencilキー + 方向キー |
| Menu bar / 仮想カーソル | 左 / 右ソフトキー |
| ビューポートの切り替え、サイズ変更、最大化 | 緑キー（単独、+ 方向キー、+ OK） |
| フルスクリーンのゲームモード | 緑キー + <kbd>0</kbd>（もう一度：画像が画面全体を覆う、もう一度：元に戻る）。UI もカメラの枠も周りの暗い部分もなく（カメラが画面と違うアスペクト比を指定している場合だけ、2 回目まで黒帯が残ります）、矢印キーでパンもしません。ゲームがそのまま見えます |

N95はBluetoothのマウスとキーボードにも対応しています。テキストの入力にはT9を使います。

# Timeline

アニメーションを表示・編集します：キーフレームのシート（ドープシート）またはカーブエディター。**選択した**もののアニメーションが表示されます。

## 上部バー

| 機能 | 説明 |
|---|---|
| トランスポート | Go to start、Previous keyframe、Previous frame、逆再生、Play / Pause、Next frame、Next keyframe、Go to end。 |
| Start / End / FPS | アニメーションの範囲と速度です（クリックして入力します）。 |
| Animation | 編集するアニメーション（シーン、クリップ、アーマチュア、オブジェクト、フリップブック、Game、Mix）を選び、新しく作成します。 |
| Auto Key | 赤のとき：トランスフォームするたびに、自動でキーフレームを記録します。 |
| Curves / Dope Sheet | キーフレームのシートとカーブエディターを切り替えます。 |
| Select、Key、View、Pivot | 選択のメニュー、キーフレームの操作、フレーミングとサイドパネル、そして<kbd>S</kbd>で拡大縮小するときの基準（中心または現在のフレーム）。 |

## 時間の移動

| 機能 | 説明 | ショートカット |
|---|---|---|
| フレームの変更 | フレーム番号の上をクリックまたはドラッグします（スクラブ）。現在のフレーム（緑）は入力することもできます。 | クリック |
| Previous frame / Next frame | 1フレーム戻る、または進みます。 | <kbd>Left</kbd> / <kbd>Right</kbd> |
| Previous keyframe / Next keyframe | いちばん近いキーフレームにジャンプします。 | <kbd>Down</kbd> / <kbd>Up</kbd> |
| Go to start / Go to end | 最初または最後のフレームに移動します。 | <kbd>Home</kbd> / <kbd>End</kbd> |
| Play / Pause | どのパネルからでも使えます。 | <kbd>Space</kbd> |
| ズームとパン | ホイール = ズーム（カーブでは、<kbd>Ctrl</kbd> = 時間のみ、<kbd>Shift</kbd> = 値のみ）。中ボタン = パン。 | ホイール / 中ボタン |
| Frame Selected | 選択したものにビューを合わせます。 | <kbd>Num.</kbd> |

## キーフレームの編集（ドープシートとカーブ）

| 機能 | 説明 | ショートカット |
|---|---|---|
| Select | ひし形をクリック、または行をクリックしてその行のキーフレームをすべて選択します。<kbd>Shift</kbd>で追加します。 | クリック。<kbd>A</kbd> / <kbd>Alt+A</kbd> / <kbd>Ctrl+I</kbd> |
| Move Keyframes | キーフレームを時間方向に動かします（ひし形をドラッグしても動かせます）。 | <kbd>G</kbd> |
| Scale Keyframes | ピボットを基準に伸ばす、または縮めます。 | <kbd>S</kbd> |
| Rotate handles | カーブのみです。 | <kbd>R</kbd> |
| Duplicate | キーフレームをコピーして、つかんだ状態にします。 | <kbd>Shift+D</kbd> |
| Delete | 選択したキーフレーム（または選択した行のカーブ）を削除します。 | <kbd>X</kbd> |
| Interpolation | Constant、Linear、Bezier。 | <kbd>T</kbd> |
| Handle Type | Free、Aligned、Vector、Automatic、Auto Clamped（カーブのみ）。 | <kbd>V</kbd> |
| Smart Euler | 選択した回転から余分な回転を取り除きます。 | Keyメニュー |
| カーブを隠す | 各行の目のアイコンで隠せます。 | 目のアイコンをクリック |
| 正確な値 | 選択したキーフレームのフレーム、値、ハンドル。 | Properties → Animation → Keyframe |

## サウンド

| 機能 | 説明 | ショートカット |
|---|---|---|
| サウンドのストリップ | 選択した Sound オブジェクトごとにストリップがあります。開始位置と長さは**秒単位**です（FPS を変えても動きません）。ループならタイムラインの最後まで繰り返します。 | — |
| 移動 | ストリップをドラッグ、またはキーフレームを選ばずに <kbd>G</kbd>（選択中のサウンドをすべて移動）。<kbd>Ctrl</kbd> でフレームにスナップ。 | ドラッグ / <kbd>G</kbd> |
| トリム | ストリップの端をつかみます。 | 端をドラッグ |
| Waveform / Volume line | View メニュー：ストリップ内に波形と音量カーブ（ストリップは3行分）。オフならただのストリップ。 | View メニュー |

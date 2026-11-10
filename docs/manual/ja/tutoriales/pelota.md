# ボールを弾ませる

いつまでも弾み続けるボール。どんなアニメーターも、この練習から始めます。簡単そうに見えますが、うまく見せるには、アニメーションでいちばん大切な原則のうち2つが必要です。

- **伸ばしとつぶし**（スクワッシュ＆ストレッチ）：ボールは床に当たるとつぶれ、離れるときに伸びます。これで重さと弾力が出ます。
- **加速と減速**：重力があるときのように、上がるときはだんだん遅くなり、落ちるときはだんだん速くなります。

![完成したボール。何度も弾み、床に当たるとつぶれ、離れるときに伸び、上で減速します](img/pelota-final.webp)

その途中で、次のことを学びます。

- 床に**Empty**を置いて、ボールを中心に向かってではなく、床に押しつけるようにつぶす
- キーフレームを**複製**して、すでにあるポーズに戻る
- **ループ**を作る。つなぎ目がわからないように繰り返すアニメーションです
- **カーブ**を使って、ボールに重さを与える

まだ何もアニメーションさせたことがなければ、[ドアをアニメーションさせる](animar-puerta.md)から始めてください。キーフレーム、Timeline、カーブについて説明しています。

## 1. ボールとEmpty

1. 立方体を削除して、球を追加します：<kbd>Shift</kbd> + <kbd>A</kbd> → **Mesh → UV Sphere**。高さは2メートルで、中心が床の高さにある状態で作られます。床の上に載るように上げます：<kbd>G</kbd> <kbd>Z</kbd> `1` <kbd>Enter</kbd>。
2. **Edit Mode**に入り（<kbd>Tab</kbd>）、**Vertex / Edge / Face**のボタンで**Vertex**を選びます。いちばん下の頂点は球に隠れています。**Overlays → X-Ray**をオンにして球を透かして見えるようにし、その頂点をクリックします。
3. <kbd>Shift</kbd> + <kbd>S</kbd> → **Cursor to Selected**。3Dカーソルが、ボールのいちばん下の点、つまり床の上に移ります。
4. **Object Mode**に戻り（<kbd>Tab</kbd>）、**X-Ray**をオフにします。
5. <kbd>Shift</kbd> + <kbd>A</kbd> → **Empty**。3Dカーソルの位置、つまり床の上の、ボールのちょうど真下に作られます。
6. ボールをEmptyに**ペアレント**します。**Outliner**で**UVSphere**をクリックし、**Empty**を<kbd>Ctrl</kbd> + クリックします（最後に選択したものが親になります）。マウスを3D Viewportの上に置いて、<kbd>Ctrl</kbd> + <kbd>P</kbd> → **Object (Keep Transform)**。Outlinerで、球がEmptyの中に入ります。

![Tab、X-Ray、いちばん下の頂点。Shift + S、Cursor to Selected。Tab。Shift + A、Empty。OutlinerでUVSphereとEmptyをCtrl + クリック。Ctrl + P、Object (Keep Transform)](img/pelota-vacio-raton.webp)

### なぜEmptyを使うのか

拡大縮小は**原点**を基準に縮めたり大きくしたりします。球の原点は中心にあるので、球をつぶすと中心に向かって縮み、宙に浮いてしまいます。Emptyは床の上にあるので、Emptyをつぶすと、ボールは弾むときのように**床に押しつけられて**つぶれます。

![球だけにS Z 0.5：中心に向かって縮み、床から離れます。EmptyにS Z 0.5：ボールが床に押しつけられてつぶれます](img/pelota-por-que-raton.webp)

ここからは、球ではなく、必ず**Empty**をアニメーションさせます。

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) タッチスクリーンとN95では：頂点はタップ（または仮想マウス）で選び、カーソルは**Mesh → Snap → Cursor to Selected**、Emptyは**Add → Empty**で追加します。ペアレントするには、球とEmptyを選択して（Emptyをアクティブにして）、**Object → Set Parent → Object (Keep Transform)**を選びます。

## 2. つぶす

ボールは床に当たると**つぶれます**。低く、幅が広くなります。小さくなったように見えないように、高さで失った分を幅で増やします。

1. **Empty**を選択します。フレーム**1**では、ボールは丸く、床に触れています：<kbd>I</kbd> → **Scale Only**。
2. <kbd>→</kbd>（右矢印キー）でフレーム**2**に移ります。ボールをつぶします。<kbd>S</kbd> <kbd>Z</kbd> `0.75` <kbd>Enter</kbd>で低くし、<kbd>S</kbd> <kbd>Shift</kbd> + <kbd>Z</kbd> `1.6` <kbd>Enter</kbd>で幅を広くします。<kbd>Shift</kbd> + <kbd>Z</kbd>は*Z以外のすべての軸*、つまりXとYを一緒に、という意味です。
3. <kbd>I</kbd> → **Scale Only**。

![フレーム1でIとScale Only。右矢印キー。S Z 0.75とS Shift + Z 1.6で、ボールが床に押しつけられてつぶれます。IとScale Only](img/pelota-aplastar-raton.webp)

![Android](img/icono-android.svg) タッチスクリーンでは：キーフレームは、3D Viewportのメニューバーのひし形のボタンで入れます（**Animation → Insert Keyframe → Scale Only**）。フレームはTimelineの数字をタップして変えます。つぶすには、**Scale** **Z** `0.75`、**Scale** **X** `1.6`、**Scale** **Y** `1.6`。数値は上のバーのテンキーで入力します。

![Nokia](img/icono-nokia.svg) Nokia N95では：<kbd>3</kbd>、<kbd>3</kbd>（Z軸）、鉛筆キー + `0.75`、<kbd>OK</kbd>。次に<kbd>3</kbd>、<kbd>1</kbd>（X）と`1.6`、<kbd>3</kbd>、<kbd>2</kbd>（Y）と`1.6`。キーフレームは**Animation → Insert Keyframe → Scale Only**で入れます。

## 3. 伸ばす

床から離れるとき、ボールは進む方向に**伸びます**。高く、細くなります。そして上に着くと、また丸くなります。

1. 丸いボールは、フレーム1のキーフレームにもうあります。これを複製します。Timelineでフレーム**1**のひし形をクリックし、<kbd>Shift</kbd> + <kbd>D</kbd>を押して、コピーをフレーム**4**まで動かしてクリックします。ポインターではなく、コピーのひし形を見てください。マウスの1フレーム右に出ます。
2. フレーム**17**と**29**にも同じようにします。そこでボールはまた丸くなります。
3. フレーム**4**に移動します（Timelineの数字をクリック）。ボールを伸ばします：<kbd>S</kbd> <kbd>Z</kbd> `1.1` <kbd>Enter</kbd>と<kbd>S</kbd> <kbd>Shift</kbd> + <kbd>Z</kbd> `0.9` <kbd>Enter</kbd>。
4. <kbd>I</kbd> → **Scale Only**。

![フレーム1のキーフレームを4、17、29に複製します。フレーム4で、S Z 1.1とS Shift + Z 0.9でボールを伸ばし、IとScale Only](img/pelota-estirar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) タッチスクリーンとN95（仮想マウスで）では、複製する代わりにコピーして貼り付けます。フレーム1のキーフレームをタップし、Timelineの**Key**メニュー（ひし形のボタン）で**Copy**を選びます。数字をタップしてフレーム4に移動し、**Key → Paste**を選びます。17と29も同じようにします。伸ばすには、**Scale** **Z** `1.1`、**Scale** **X** `0.9`、**Scale** **Y** `0.9`。

![フレーム1のキーフレームをタップし、Key、Copy。4をタップしてKey、Paste。フレーム4でもボールが丸くなります](img/pelota-copiar-dedo.webp)

## 4. ジャンプ

1. フレーム**3**に移動します。ボールはまだ床の上にあり、離れる瞬間です。<kbd>I</kbd> → **Location Only**。
2. フレーム**16**に移動して、ボールを上げます：<kbd>G</kbd> <kbd>Z</kbd> `5` <kbd>Enter</kbd>（5メートル）。<kbd>I</kbd> → **Location Only**。
3. 床に戻るように、フレーム**3**のキーフレームを**29**に複製します。

**Play**（<kbd>Space</kbd>）で再生します。ボールが弾みます。でも、どこかおかしいところがあります。床に着く前に減速して、浮いているように見えます。これはパート6で、カーブを使って直します。

![フレーム3でIとLocation Only。16でG Z 5とI、Location Only。3のキーフレームを29に複製します。Playでボールが上下しますが、床に着く前に減速します](img/pelota-saltar-raton.webp)

![Android](img/icono-android.svg) タッチスクリーンでは：**Move** **Z** `5`と、**Animation → Insert Keyframe → Location Only**。3のキーフレームは、パート3と同じように、**Key → Copy**と**Key → Paste**で29に持っていきます。

![Nokia](img/icono-nokia.svg) Nokia N95では：<kbd>1</kbd>、<kbd>3</kbd>（Z軸）、鉛筆キー + `5`、<kbd>OK</kbd>。そして**Animation → Insert Keyframe → Location Only**。

## 5. ループ

フレーム**29**はフレーム**1**と同じです。丸いボールが床に触れています。アニメーションが29まであると、同じフレームが2つ続いて、ボールが一瞬止まってしまい、つなぎ目がわかってしまいます。そのため、アニメーションは**28**で終わらせます。28の次は、29と同じ1が来ます。

Timelineの**時計**をクリックし、**End**に`28`と入力して<kbd>Enter</kbd>を押します。

同じ時計に、このアニメーションの**FPS**があります。ボールは30で動きます。1回の弾みは28フレームで、ほぼ1秒です。映画のような雰囲気にしたければ24にしますが、アニメーションを作る**前に**設定してください。同じフレーム数のままだと、24では遅くなります。どんなときにどのFPSがよいかは、[ドアをアニメーションさせる](animar-puerta.md)のタイミングのパートで説明しています。

![Timelineの時計。Endに28と入力してEnter](img/pelota-fin-raton.webp)

## 6. カーブ：重力

**Location Only**は3つの軸を保存しましたが、ボールはZの方向にしか動きません。**X Location**と**Y Location**の行は平らな線です。これを削除します。それぞれをクリックして<kbd>X</kbd>を押します。

次に、カーブエディターに切り替えます（**View → Curves**）。Zのカーブがジャンプで、上がって下がります。本物のボールを思い浮かべてください。

- **上がるとき**は、全速力で床を離れ、だんだん減速して、上で止まります。**速く飛び出して減速**です。フレーム**3**のキーフレームを選択し、Timelineのツールバーで**Cubic**と**Ease Out**を選びます。
- **下がるとき**は、ゆっくり動き出し、だんだん速く落ちて、床にぶつかります。**加速して急停止**です。フレーム**16**のキーフレームを選択し、**Cubic**と**Ease In**を選びます。

（キーボードでは、<kbd>E</kbd>で**Interpolation Mode**、<kbd>Ctrl</kbd> + <kbd>E</kbd>で**Easing Type**です。）

カーブは本物のボールの軌跡のような弧になります。上は丸く、床にぶつかる下はとがっています。**Play**で再生してみましょう。今度は重さが感じられます。

![X LocationとY LocationをXで削除。View、Curves。3のキーフレームをCubicとEase Out、16のキーフレームをCubicとEase Inにします。カーブの下がとがり、ボールが床にぶつかります](img/pelota-curvas-raton.webp)

もっと高く、または低く弾ませたいですか？ カーブエディターで、上のキーフレーム（フレーム16のもの）をつかんで動かします。

## 7. 見てみよう

グリッドやEmptyを表示せずに見るには、**Overlays → Show Overlays**をオフにして、**Play**で再生します。

下の3つのボールは、同じ時間で弾んでいます。1つ目はリニアのカーブで、機械のように、いつも同じ速さで上下します。2つ目は重力のカーブで、上で減速し、床にぶつかります。3つ目はさらに伸びたりつぶれたりします。これがこのチュートリアルのボールです。

![同時に弾む3つのボール。機械のようなリニアのボール、上で減速して床にぶつかるカーブのボール、そしてカーブに伸ばしとつぶしを加えたボール](img/pelota-comparar.webp)

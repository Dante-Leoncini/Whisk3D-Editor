# ドアノブをモデリングする

前のチュートリアル[原点を変える：ドア](puerta.md)のドアから始めます。大きさは0.90 × 0.10 × 2メートルで、原点は蝶番にあります。このドアに、2つの部品でできたドアノブを作ります。ドアに貼り付いた**プレート**と、回る**レバー**です。

その途中で、次のことを学びます。

- 面を複製して、そこから新しい部品を作る
- メッシュの一部を別のオブジェクトに**分離**し（<kbd>P</kbd>）、また**結合**する（<kbd>Ctrl</kbd> + <kbd>J</kbd>）
- メッシュの部分ごとに別の**マテリアル**を付ける
- **Mirror**モディファイアーを使って、ドアの両側にドアノブを付ける
- **X-Ray**で、決して見えない面を表示して、削除する
- 3Dカーソルを使って、レバーが回る位置に**原点**を置く
- レバーとドアが一緒に動くように、レバーをドアに**ペアレント**する

## 1. プレート

1. ドアを選択した状態で**Edit Mode**に入り（<kbd>Tab</kbd>）、**Vertex / Edge / Face**のボタンで**Face**を選びます。
2. ドアの前側の大きな面をクリックします。
3. **複製します：** <kbd>Shift</kbd> + <kbd>D</kbd>。コピーはマウスについてきます。右クリック（または<kbd>Esc</kbd>）で元の位置、つまり元の面にぴったり重なった位置に置かれます。
4. 小さな正方形になるまで**縮めます**。目分量なら、<kbd>S</kbd>を押してマウスを動かし、次に<kbd>S</kbd> <kbd>Z</kbd>で高さだけを調整します。数値なら、<kbd>S</kbd> `0.15` <kbd>Enter</kbd>と<kbd>S</kbd> <kbd>Z</kbd> `0.45` <kbd>Enter</kbd>で、13.5 cmの正方形になります。
5. ドアノブを付ける、蝶番と反対の側に**移します**：<kbd>G</kbd> <kbd>X</kbd> `-0.31` <kbd>Enter</kbd>。
6. 厚みが出るように、少しだけ**押し出します**：<kbd>E</kbd> `0.03` <kbd>Enter</kbd>（3 cm）。

![ドアの前側の面をクリックし、Shift + DとEscでコピーを重ねて置きます。S 0.15とS Z 0.45で正方形に縮め、G X -0.31で蝶番と反対の側に移し、E 0.03で厚みを付けます](img/picaporte-placa-raton.webp)

![Android](img/icono-android.svg) タッチスクリーンでは：面をタップして、**Mesh → Duplicate**を選びます（コピーが重なって置かれます）。次にツールバーの**Scale**、**Move**、**Extrude**を使い、数値は上のバーのテンキーで入力して、チェックマークで確定します。

![指で前側の面をタップし、Mesh、Duplicate。上のバーのテンキーで、Scale 0.15、Scale Z 0.45、Move X -0.31、Extrude 0.03](img/picaporte-placa-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95では：左ソフトキーで**Mesh → Duplicate**。次に<kbd>3</kbd>（拡大縮小）、<kbd>1</kbd>（移動）、<kbd>7</kbd>（押し出し）を使い、数値は鉛筆キーを押したまま入力して、<kbd>OK</kbd>で確定します。

![Nokia N95。3と鉛筆キー + 0.15、3、3（Z）と0.45、1、1（X）と-0.31、7と0.03。それぞれOKで確定](img/picaporte-placa-nokia.webp)

## 2. プレートを分離する

プレートはまだドアのメッシュの一部です。これを独立したオブジェクトにします。

1. プレートを選択した状態で<kbd>L</kbd>（**Select Linked**）を押すと、部品全体が選択されます。ドアは含まず、プレートの6つの面だけです。
2. <kbd>P</kbd>（**Separate**）を押すと、プレートが新しいオブジェクト**Door.001**になります。Outlinerで確認できます。

![マウスをプレートの上に置いてLを押すと部品全体が選択され、Pで分離されます。OutlinerにDoor.001が現れます](img/picaporte-separar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) タッチスクリーンとN95では：**Select → Select Linked**を選んでプレートをタップし、次に**Mesh → Separate**を選びます。

![Select、Select Linkedを選んでプレートをタップし、Mesh、Separate](img/picaporte-separar-dedo.webp)

分離すると、ドアにはプレートの面も線も、何も残りません。

## 3. マテリアル：木と金属

これで、それぞれの部品に別のマテリアルを付けられます。**Properties**の**Material**タブで：

1. ドアを選択します。**Rename Material**に`Wood`と入力し、**Color**で茶色を選びます。
2. プレート（Door.001）を選択します。ドアから作ったので、これにもWoodが付いています。**New Material**をタップし、名前を`Metal`にして、灰色にします。

![木のドアと金属のプレート。プレートのMaterialカードには、Metalのマテリアルが表示されています](img/picaporte-materiales.webp)

## 4. ドアの反対側：Mirror

ドアノブはドアの両側に付きます。もう1つプレートを作る代わりに、**Mirror**モディファイアーを使います。

1. プレートを選択した状態で、**Properties → Modifiers**で**Mirror**を追加します。
2. **Mirror X**をオフにして、**Mirror Y**をオンにします。ドアの反対側に、もう1つのプレートが現れます。

![横から見たところ。ModifiersでMirrorを追加し、Mirror Xをオフ、Mirror Yをオンにすると、ドアの反対側にプレートが現れます](img/picaporte-espejo-raton.webp)

ミラーは、オブジェクトの**原点**を基準に行われます。プレートはドアから作ったので、原点も同じで、蝶番の位置、ちょうどドアの厚みの真ん中にあります。そのため、コピーはちょうど反対側に来ます。[Mirror](../editor/modificador-espejo.md)を参照してください。

## 5. レバー

レバーは、同じ方法でプレートから作ります。

1. プレートを選択した状態で、テンキーの<kbd>/</kbd>を押します（**Local View**）。プレートだけが表示され、作業しやすくなります。**Edit Mode**に入ります。
2. プレートの前側の面を選択し、**複製して**（<kbd>Shift</kbd> + <kbd>D</kbd>と右クリック）、縮めます：<kbd>S</kbd> `0.6` <kbd>Enter</kbd>。
3. 外側に**押し出します**：<kbd>E</kbd> `0.08` <kbd>Enter</kbd>。これがレバーの軸になります。

![プレートのローカルビュー。前側の面をクリックし、Shift + DとEsc、S 0.6で縮め、E 0.08で外側に押し出すと、レバーの軸ができます](img/picaporte-manija-raton.webp)
4. **Edge**に切り替えて、ドアの端の側にある外側の辺を選択します。それを外側に動かします：<kbd>G</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>。前側の面が斜めになります。
5. **Face**に切り替えて、その斜めの面を選択します。法線の方向ではなく、**Xの方向に**押し出します：<kbd>E</kbd>、<kbd>X</kbd>、`0.34`、<kbd>Enter</kbd>。レバーの腕ができます。
6. 先端が斜めになっています。まっすぐにするには、Xの方向にスケールを0にします：<kbd>S</kbd> <kbd>X</kbd> `0` <kbd>Enter</kbd>。先端の頂点がすべて同じXの位置になり、先端が平らになります。

![Edgeで、外側の辺がG Y 0.05で上に動きます。Faceで、斜めの面をE X 0.34でXの方向に押し出し、S X 0で先端をまっすぐにします](img/picaporte-brazo-raton.webp)
7. <kbd>L</kbd>でレバー全体を選択し、<kbd>P</kbd>で分離します。**Door.002**になります。名前を付けましょう。Outlinerでダブルクリックして、`Lever`と入力します。

![LとPでレバーを分離し、Outlinerでダブルクリックして、名前をLeverに変えます](img/picaporte-manija-separar-raton.webp)

![Android](img/icono-android.svg) タッチスクリーンでは：面をタップして、**Mesh → Duplicate**、**Scale** `0.6`、**Extrude** `0.08`。**Edge**にして外側の辺をタップし、**Move** **Y** `0.05`。**Face**にして斜めの面をタップし、**Extrude** **X** `0.34`、**Scale** **X** `0`。分離するには、**Select → Select Linked**と**Mesh → Separate**です。

![指で：プレートの面、Duplicate、Scale 0.6、Extrude 0.08。外側の辺がMove Y 0.05で上に動き、斜めの面がExtrude X 0.34で押し出され、Scale X 0でまっすぐになります](img/picaporte-manija-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95では：<kbd>3</kbd>と`0.6`、<kbd>7</kbd>と`0.08`。辺を（仮想マウスで）選び、<kbd>1</kbd>、<kbd>2</kbd>（Y軸）と`0.05`。斜めの面を選び、<kbd>7</kbd>、<kbd>1</kbd>（X軸）と`0.34`。<kbd>3</kbd>、<kbd>1</kbd>と`0`。

![Nokia N95。3と0.6、7と0.08。1、2と0.05で辺が上に動き、7、1と0.34でXの方向に腕が出て、3、1と0で先端がまっすぐになります](img/picaporte-manija-nokia.webp)

レバーはプレートから作ったので、マテリアル（Metal）とMirrorモディファイアーも付いています。

## 6. プレートをドアに結合する

プレートは単独では動きません。ドアの一部です。もう一度ドアと結合します。

1. プレートを選択し、**Modifiers**で**Apply Modifier**をタップします。ミラーが本物のジオメトリになります。
2. プレートを選択し、次にドアを<kbd>Shift</kbd> + クリックで選択して（ドアがアクティブになります）、<kbd>Ctrl</kbd> + <kbd>J</kbd>（**Join**）を押します。これで、2つのマテリアルを持つ1つのメッシュになります。

![Apply Modifierで、プレートのミラーがジオメトリになります。プレートをクリックし、ドアをShift + クリックして、Ctrl + J。OutlinerからDoor.001が消え、ドアの一部になります](img/picaporte-unir-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) タッチスクリーンとN95では：**Object → Join**。

![プレートとドアを選択した状態で、Object、Join。OutlinerからDoor.001が消えます](img/picaporte-unir-dedo.webp)

このように、メッシュの部分は<kbd>P</kbd>で分離し、<kbd>Ctrl</kbd> + <kbd>J</kbd>で結合します。

## 7. 決して見えない面

プレートはドアに接しています。各プレートの裏の面は隠れていて、決して見えることはありませんが、プログラムはそれでも描画します。その面を見るには：

1. ドアを選択して、**Edit Mode**に入ります。
2. **Overlays**で**X-Ray**をオンにします。メッシュが透けて、裏の面が見えるようになります。
3. 各プレートの裏の面（ドアに接している面）を選択して削除します：<kbd>X</kbd> → **Faces**。

![Edit Modeのドア。Overlays、X-Ray。2つのプレートの裏の面を選択し、X、Facesで削除します](img/picaporte-rayosx-raton.webp)

見えないジオメトリは削除したほうがよいでしょう。むだに描画される面だからです。小さなモデルでは気になりませんが、大きなシーンや電話では、そのぶん性能が上がります。

## 8. レバーが回る位置：原点

レバーは、ドアの蝶番ではなく、自分の軸を中心に回る必要があります。

1. レバーを選択して、そのMirrorを適用します（**Modifiers → Apply Modifier**）。
2. **X-Ray**をオンにしたまま**Edit Mode**に入ります。レバーの裏の2つの面、つまりプレートに接している面（両側に1つずつ）を選択します。
3. <kbd>Shift</kbd> + <kbd>S</kbd> → **Cursor to Selected**。3Dカーソルが2つの面の真ん中、ちょうどレバーの軸の位置に来ます。
4. この2つの面も見えないので、削除します（<kbd>X</kbd> → **Faces**）。
5. **Object Mode**に戻り、原点をカーソルに移します：<kbd>W</kbd>（または右クリック）→ **Set Origin → Origin to 3D Cursor**。

![レバー。Apply Modifier。Edit Modeで裏の2つの面を選択し、Shift + S、Cursor to Selected、X、Faces。TabとSet Origin、Origin to 3D Cursor](img/picaporte-origen-raton.webp)

[原点を変える：ドア](puerta.md#原点を蝶番に移す)で蝶番に対してしたことと同じです。

## 9. レバーがドアと一緒に動くようにする

ドアを回してみましょう。レバーが宙に浮いたまま残ります。2つのオブジェクトがばらばらだからです。回転を元に戻します（<kbd>Ctrl</kbd> + <kbd>Z</kbd>）。

![R Z -70でドアが回り、レバーは宙に浮いたまま残ります。Ctrl + Zで元に戻ります](img/picaporte-flota-raton.webp)

レバーがドアと一緒に動くようにするには、**ペアレント**します。レバーをドアの**子**にするのです。

1. レバーを選択し、次にドアを<kbd>Shift</kbd> + クリックで選択します（親であるドアがアクティブになります）。
2. <kbd>Ctrl</kbd> + <kbd>P</kbd> → **Object (Keep Transform)**。Outlinerで、レバーがドアの中に入ります。

ドアだけを選択して（ドアをクリック）、もう一度回します。今度はレバーも一緒に動きます。そしてレバーは、本物のドアノブのように、自分の軸を中心に回ることもできます。両方が選択されているときは、ドアだけが回り、レバーは子なのでそれについていきます。

![レバーをクリックし、ドアをShift + クリックして、Ctrl + PとObject (Keep Transform)。ドアだけをクリックしてR Z -70を押すと、今度はレバーがドアと一緒に動きます](img/picaporte-emparentar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) タッチスクリーンとN95では：**Object → Set Parent → Object (Keep Transform)**。

![レバーとドアを選択した状態で、Object、Set Parent、Object (Keep Transform)。Outlinerで、レバーがドアの中に入ります](img/picaporte-emparentar-dedo.webp)

次のチュートリアルでは、ドアをアニメーションさせます：[ドアをアニメーションさせる](animar-puerta.md)。

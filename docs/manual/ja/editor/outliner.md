# Outliner

> [!NOTE]
> ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95はマウスとキーボード（Bluetooth）に対応しています。このページのNokiaの説明は、マウスやキーボードを使わずに、電話のキーだけで操作する方法です。

**Outliner**は、シーンにあるものすべての一覧です：オブジェクト、カメラ、ライト、コレクション。「[ビューポート](interfaz.md)」で見たビューポートの1つで、PCでは右上にあります。ここでオブジェクトを選択したり、隠したり、並べ替えたり、別のオブジェクトの中に入れたり、名前を変えたりします。

Outlinerで選択したものは3D Viewportでも選択され、その逆も同じです。最後に選択したものが**アクティブなオブジェクト**で、Propertiesに表示されるのはこのオブジェクトです。

![Android](../tutoriales/img/icono-android.svg) **Androidでは**、Outlinerは画面にありません。その場所は3D ViewportとPropertiesに使われています。使うには、「[種類を変える](interfaz.md#種類を変える)」と同じように、隅のボタンでビューポートをOutlinerに変えます（またはビューポートを1つ分割します）。Outlinerがなくても作業はできます：オブジェクトは3D Viewportでタップして選択でき、Propertiesには名前と、**Visible**と**Renderable**のチェックボックスがあります。

![Nokia](../tutoriales/img/icono-nokia.svg) **Nokia N95では**、Outlinerは画面にありません。ビューポートが2つしか入らないからです。シーンを整理する間は、PropertiesをOutlinerに変えます：**左ソフトキー**でタブに入り、左の方向キーで隅のボタンのメニューまで行き、そこでOutlinerを選びます。元に戻すときは、Outlinerで左ソフトキーを押すと、そのメニューが直接開きます。緑の**通話キー** ![](../tutoriales/img/tecla-verde.svg) でOutlinerをアクティブなビューポートにします：このあとのN95の操作は、すべてOutlinerに緑の枠がある状態で行います。

## 各行に表示されるもの

各行が1つのオブジェクトです。**矢印**で、中にあるものを折りたたんだり展開したりします。**アイコン**は種類（メッシュ、カメラ、ライト、コレクション…）を示します。その次が**名前**で、アクティブなものは緑、選択されているものは少し暗い緑になります。右側には**目**と**カメラ**があります。別のオブジェクトの中にあるオブジェクトは、そのオブジェクトにぶら下がるように少し右に表示されます。薄く表示された行は、隠されたオブジェクトです。

新しいプロジェクトでは、いちばん上に画面を準備するルーチンの**Constructor**があり、その下にカメラ、ライト、立方体が入った**Collection**があります。コレクションは、整理するためのフォルダーです。

## 選択

| | 選択の方法 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 行をクリックすると、そのオブジェクトだけを選択します。<kbd>Shift</kbd> + クリックで、アクティブな行からその行までのすべての行を選択します。<kbd>Ctrl</kbd> + クリックで1つ追加または解除します。マウスがOutlinerの上にあるとき、上下の矢印キーで前の行または次の行に移ります。 |
| ![Android](../tutoriales/img/icono-android.svg) タッチスクリーン | 行をタップします。3D Viewportのツールバーで**Shift**または**Ctrl**をオンにしておくと、タップがキーボードと同じ働きをします。指でドラッグすると一覧がスクロールします。 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 上下の方向キーで前の行または次の行に移り、一覧はそれに合わせて自動でスクロールします。 |
| メニュー | Select → Select All、Deselect All、Invert Selection |

![CameraをクリックするとCameraが選択されます。CubeをShift + クリックすると、CameraからCubeまでが選択されます。Lightをクリックするとライトだけが選択されます。CubeをCtrl + クリックすると追加され、もう一度Ctrl + クリックすると外れます](../tutoriales/img/outliner-seleccionar-raton.webp)

## 折りたたみと展開

オブジェクトの中に別のオブジェクトがあるときは、その矢印で表示したり隠したりします。使っていないものを折りたたむと、一覧が短くなります。

| | 折りたたみと展開の方法 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 矢印をクリックします。マウスがOutlinerの上にあるとき、キーボードの右矢印キーでアクティブなものを展開し、左矢印キーで折りたたみます。すでに折りたたまれている場合は、左矢印キーで外側のものに移って、それを折りたたみます。 |
| ![Android](../tutoriales/img/icono-android.svg) タッチスクリーン | 矢印をタップします。 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 右でアクティブなものを展開し、左で折りたたみます。すでに折りたたまれている場合は、左で外側のものに移って、それを折りたたみます。 |

![Collectionの矢印をクリックするとCamera、Light、Cubeが隠れ、もう一度クリックすると再び表示されます](../tutoriales/img/outliner-plegar-raton.webp)

## 隠す：目とカメラ

- **目**は、3D Viewportでオブジェクトを表示したり隠したりします。隠したオブジェクトはレンダーにも出ません。中に別のオブジェクトがある場合は、それらも一緒に隠れ、その行は薄く表示されます。
- **カメラ**は、**レンダー**に出るかどうかを決めます。オフにすると、作業中はオブジェクトが見えたままですが、レンダリングする画像や動画には表示されません。

これらに触れても、選択は変わりません。

| | 目 | カメラ |
|---|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 目をクリックします。アクティブなものは<kbd>H</kbd>（マウスがOutlinerの上にあるとき）でも切り替えられます | カメラをクリックします |
| ![Android](../tutoriales/img/icono-android.svg) タッチスクリーン | 目をタップします | カメラをタップします |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | <kbd>2</kbd>（アクティブなものの目） | <kbd>3</kbd>（アクティブなもののカメラ） |
| メニュー | Element → Hide/Show（アクティブなもの） | — |

![Cubeの目をクリックすると目が閉じ、立方体が3D Viewportから消えます。もう一度クリックすると再び表示されます。Cubeのカメラをクリックするとオフになりますが、立方体は見えたままです](../tutoriales/img/outliner-ocultar-raton.webp)

この2つは、PropertiesのTransformカードにも、**Visible**と**Renderable**としてあります。ひし形があるのでアニメーションできます：たとえば、動画のある瞬間に何かを登場させたいときに使います。

## 並べ替えと、オブジェクトを別のオブジェクトの中に入れる

一覧の順番は変えられます。また、オブジェクトを別のオブジェクトの**中**に入れることもできます：入れたオブジェクトはその**子**になり、親が移動、回転、拡大縮小すると、子もついていきます。これは「[Object Mode](../tutoriales/modo-objeto.md#outlinerシーンのオブジェクト)」で**ペアレント**と呼んでいるものです。オブジェクトはシーンの中の位置からは動きません。変わるのは一覧の中だけです。

![PC](../tutoriales/img/icono-pc.svg) **マウスで：** 行をつかんでドラッグします。別の行の上端または下端の上では**緑の線**が表示され、離すとオブジェクトはその行の前または後ろに、同じ階層で置かれます。別の行の真ん中の上では、その行が強調表示され、離すとオブジェクトはその行の**中**に入ります。最後の行より下で離すと、それまで入っていたものすべてから外に出ます。一覧の上端や下端の近くでは自動でスクロールするので、長い一覧のどの行にも届きます。

![Lightの行をCameraの上へドラッグすると、緑の線でいちばん上に置かれます。次にCameraをCubeの行の真ん中で離すと、Cubeの中に入り、右にずれて表示されます](../tutoriales/img/outliner-mover-raton.webp)

![Android](../tutoriales/img/icono-android.svg) **指で：** 電話が振動するまで、行の上で指を1秒ほど動かさずに置いておきます。これで行がつかまれた状態になります。あとはマウスと同じようにドラッグして離します。動かさずに離すと、オブジェクトのメニューが開きます。この待ち時間なしに指をドラッグすると、一覧がスクロールします。

![Nokia](../tutoriales/img/icono-nokia.svg) **Nokia N95では：** 方向キーで行を選び、<kbd>1</kbd>を押します。行が緑になり、方向キーで行を動かせるようになります：

- ![Nokia](../tutoriales/img/icono-nokia.svg) **上**と**下**で、同じ階層の行の間で上下に移動します。
- ![Nokia](../tutoriales/img/icono-nokia.svg) **右**で、すぐ上にある同じ階層の行の中に入れます。
- ![Nokia](../tutoriales/img/icono-nokia.svg) **左**で、入っているものから外に出します。外に出た行は、そのものの後ろに同じ階層で置かれます。
- ![Nokia](../tutoriales/img/icono-nokia.svg) もう一度<kbd>1</kbd>（または<kbd>OK</kbd>）を押すと、その位置で確定します。<kbd>C</kbd>でキャンセルし、元の位置に戻します。

![Nokia](../tutoriales/img/icono-nokia.svg) 移動全体は、<kbd>C</kbd> + 左で一度に取り消せます。たとえば、照らすオブジェクトより前にライトを置きたいときに便利です。

![Outlinerがアクティブになっている Nokia N95：1でLightの行が緑になり、上の方向キーでCameraの上に移動し、1でその位置に確定します。2で立方体が3D Viewportから消えて、また現れます。3でその行のカメラがオフになります](../tutoriales/img/outliner-nokia.webp)

## アクティブなものを見つけて画面に収める

大きなシーンでは一覧が長くなり、3D Viewportで何かを選択しても一覧は動きません。そのため、その行が見えない位置にあることがあります。一覧でその行を見つけるには、または3D Viewportでオブジェクトを見る（「[選択したものを画面に収める](../tutoriales/navegacion.md#選択したものを画面に収める)」のように**画面に収める**）には：

| | Outlinerで | 3D Viewportで |
|---|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | マウスをOutlinerの上に置いて<kbd>Num .</kbd>（**Show active**）：選択したものが真ん中に来るまで一覧がスクロールします。折りたたまれたものの中にある場合は行がないので、先に展開します。 | マウスを3D Viewportの上に置いて<kbd>Num .</kbd> |
| ![Android](../tutoriales/img/icono-android.svg) タッチスクリーン | 指で一覧をドラッグします | 3D Viewportの上を2本指でダブルタップ |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 方向キーで選んだ行に、一覧が自動でついていきます | <kbd>0</kbd>を1回押します（アクティブなビューポートがOutlinerでも使えます） |
| メニュー | — | 3D Viewportのバーで、View → Frame Selected |

![何十ものオブジェクトがあるシーン：3D Viewportで1つを選択しても、その行はOutlinerに見えません。マウスをOutlinerの上に置いてNum .を押すと一覧がスクロールし、緑の行が真ん中に来ます](../tutoriales/img/outliner-activo-raton.webp)

## 名前の変更

| | 名前の変え方 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 名前をダブルクリックするか、アクティブなものなら<kbd>F2</kbd>を押します：行がテキスト欄になります。新しい名前を入力し、<kbd>Enter</kbd>か欄の外をクリックして確定します。<kbd>Esc</kbd>で元のままにします。 |
| ![Android](../tutoriales/img/icono-android.svg) タッチスクリーン | 名前をダブルタップすると、画面キーボードが開きます。 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 左ソフトキーでバーに入り、右の方向キーでElement → Renameまで行きます。T9で入力し、<kbd>OK</kbd>で確定します。 |
| メニュー | Element → Rename |

![Cubeの名前をダブルクリックすると欄になり、Boxと入力してEnterで確定します。次にLightを選択した状態でF2を押し、Lampと入力してEnterを押します](../tutoriales/img/outliner-renombrar-raton.webp)

同じシーンの2つのオブジェクトに同じ名前は付けられません：その名前がすでに使われている場合は、Whisk3Dが番号を付け加えて知らせます。スクリプトがそのオブジェクトの名前を使っていた場合は、自動で更新されます。

## 複製と削除

| | 複製 | 削除 |
|---|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | <kbd>Shift</kbd> + <kbd>D</kbd>。Duplicate Linkedは<kbd>Alt</kbd> + <kbd>D</kbd> | <kbd>X</kbd>または<kbd>Delete</kbd> |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | Element → Duplicate | <kbd>C</kbd> |
| メニュー | Element → DuplicateまたはDuplicate Linked | Element → Delete |

Outlinerから複製すると、コピーは元のオブジェクトのすぐ上に置かれ、3D Viewportのようにマウスについてくることはありません：あとで動かしてください。削除するときは確認があり、どちらも取り消せます。

## Outlinerのメニューバー

| ボタン | 内容 |
|---|---|
| **View options**（目） | 何を表示するか：Constructor、そのほかのルーチン、目とカメラの列（**Visibility icons**と**Render icons**）。 |
| **Element**（立方体） | Delete、Rename、Move、Hide/Show、Duplicate、Duplicate Linked、Create Prefab、Unpack Prefab。 |
| **Select** | Select All、Deselect All、Invert Selection。 |
| シーン | プロジェクトのどのシーン、ゲーム、プレハブを編集しているか（[シーン、ゲーム、プレハブ](outliner.md#シーンゲームプレハブ)）。 |

![PC](../tutoriales/img/icono-pc.svg) 行を右クリックすると、その行が選択され、同じメニューがその場所（ポインターの位置）に開きます。

![Android](../tutoriales/img/icono-android.svg) 指で、振動するまで行を押したままにし、動かさずに離すと、同じメニューが開きます。

## Library

プロジェクトの**リソース**（メッシュ、マテリアル、テクスチャ、アニメーション、サウンド、スクリプト、プレハブ…）はOutlinerにはありません：専用のビューポート**Library**があり、隅のボタンで選びます。フォルダーに分けてサムネイルを表示し、どのリソースが使われていてどれが使われていないかを示します。リソースは3D Viewportにドラッグできます：メッシュならオブジェクトが作成され、マテリアルならその下にあるものに割り当てられます。詳しくは「[Libraryビュー](outliner.md#libraryビュー)」で、Outlinerのそのほかの機能は[マニュアル](outliner.md)と[ショートカット](atajos.md#outliner)で説明しています。

## Libraryビュー

| 機能 | 説明 |
|---|---|
| リソース | メッシュ、マテリアル、テクスチャ、アニメーション、プレハブ、シーン、サウンド、スクリプト、[シェーダー](shaders.md)、フォント、動画が、フォルダーに分かれて表示されます。3D Viewportのメッシュの上にドラッグしたマテリアルやシェーダーは、ドロップする前からポインターの下のパーツに表示されます。緑のチェックマーク = 使用中、赤い×印 = 孤立、`*` = 未保存。 |
| Filter / List or Grid | 1種類だけを表示します。サムネイルを表示します。 |
| リソースのドラッグ | フォルダーへ（移動します）、3D Viewportへ（メッシュ = 新しいオブジェクト、マテリアルまたはテクスチャ = ドロップした先に割り当て、プレハブ = インスタンス）、またはPropertiesのドロップダウンへ。 |
| New Folder / Move to Folder | 整理します（フォルダーは並べるためだけのものです）。 |
| Duplicate Resource / Select Users / Purge Orphans | コピーする、何が使っているかを調べる、何にも使われていないものを削除する。 |
| Inside the .w3d / External | Properties（Resourceカード）から行います：リソースをプロジェクトの中または外に移します。 |

## シーン、ゲーム、プレハブ

シーン名が書かれたボタン（Outlinerと3D Viewportにあります）には、プロジェクトのシーン、ゲーム、プレハブが一覧表示されます。そこから新しく作成したり、開始シーンを選んだり、シーンをゲームに変換したり（またはその逆）できます。

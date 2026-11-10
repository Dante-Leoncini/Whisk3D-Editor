# 原点を変える：ドア

このチュートリアルでは、ドアが蝶番を軸に開くようにします。そのためには、ドアの**原点**を変える必要があります。原点は、オブジェクトが回転するときの中心になる点です（Object Modeのチュートリアルの[原点](modo-objeto.md#原点)を参照）。

前のチュートリアル[ドアをモデリングする](modelar-puerta.md)で作った、原点が中心にあるドアから始めます。側面をずらして作った場合は、原点もずれています。**Object → Set Origin → Origin to Geometry**で中心に戻せます。

## ドアの回り方がおかしい

回転させてみましょう。ドアは蝶番ではなく自分の中心を軸に、回転ドアのように回ります。

![PC](img/icono-pc.svg) ギズモで：中心を1回クリックすると**Rotate**に切り替わり、青いリングで垂直な軸のまわりに回せます。

![ギズモの中心を1回クリックするとRotateに切り替わります。青いリングをドラッグすると、ドアが回転ドアのように中心を軸に回り、元に戻ります](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) 指で：ギズモの中心を1回タップすると**Rotate**に切り替わります。あとは青いリングをドラッグします。

![指でギズモの中心をタップしてRotateに切り替え、青いリングをドラッグすると、ドアが中心を軸に回り、元に戻ります](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) Nokia N95では、ギズモは最初はオフになっています。<kbd>2</kbd>（回転）、<kbd>3</kbd>（垂直なZ軸）、方向キーを使い、<kbd>OK</kbd>で確定します。

![Nokia N95。2と3で垂直な軸のまわりの回転が始まり、右の方向キーでドアが中心を軸に回り、左で戻り、OKで確定します](img/puerta-gira-mal-nokia.webp)

原点が中心にあるので、回り方がおかしくなります。原点を蝶番に移しましょう。

## 原点を蝶番に移す

1. **Edit Modeに入ります。** モードのボタンをクリックし、**Edit Mode**をクリックします（PCでは<kbd>Tab</kbd>でも入れます）。

   ![モードのボタンが開き、Edit Modeが強調されています](img/puerta-paso-1.webp)

2. **面を選べるようにします。** Edit Modeでは**Vertex / Edge / Face**のボタンが表示されます。クリックして**Face**を選びます。

   ![Vertex、Edge、Faceのボタンが開き、Faceが強調されています](img/puerta-paso-2.webp)

3. **蝶番の側の面を選択します。** ドアの側面の細い面のうち、蝶番が付く側をクリックします。面が緑になります。

   ![ドアの側面の細い面が選択されて緑になり、円で囲まれています](img/puerta-paso-3.webp)

4. **3Dカーソルをそこに移します。** **Mesh**メニューで**Snap** → **Cursor to Selected**を選びます。3Dカーソルが蝶番の位置に来ます。

   ![Meshメニューが開き、Snapのサブメニューと、Cursor to Selectedが強調されています](img/puerta-paso-4.webp)

5. **Object Modeに戻ります。** モードのボタンをクリックし、**Object Mode**をクリックします（または<kbd>Tab</kbd>）。

   ![モードのボタンが開き、Object Modeが強調されています。3Dカーソルはもう蝶番の位置にあります](img/puerta-paso-5.webp)

6. **原点を3Dカーソルに移します。** **Object**メニューで**Set Origin** → **Origin to 3D Cursor**を選びます。ドアは動きません。動くのは原点で、それと一緒にギズモも動き、蝶番の位置に来ます。

   ![Objectメニューが開き、Set Originのサブメニューと、Origin to 3D Cursorが強調されています](img/puerta-paso-6.webp)

## これで正しく回る

もう一度ギズモで回してみましょう。今度は蝶番を軸に開いたり閉じたりします。

![原点が蝶番にあるので、ギズモの青いリングでドアが横向きになるまで開き、また閉じます](img/puerta-gira-bien-raton.webp)

![指で青いリングを動かすと、ドアが蝶番を軸に開き、また閉じます](img/puerta-gira-bien-dedo.webp)

![Nokia N95。2、3と右の方向キーでドアが蝶番を軸に開き、左で閉じ、OKで確定します](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) タッチスクリーンでも、上の手順は同じで、タップで操作します。メニューはタップで開き、タップで選びます。

![Nokia](img/icono-nokia.svg) Nokia N95では、<kbd>OK</kbd>でObject ModeからEdit Modeに切り替わり、メニューは左ソフトキーで開きます。

## Set Originのオプション

| オプション | 内容 |
|---|---|
| **Origin to Geometry** | 原点をメッシュの中心に移します。 |
| **Origin to 3D Cursor** | 原点を3Dカーソルの位置に移します。 |
| **Geometry to Origin** | 逆の操作です。原点はそのままで、メッシュの中心が原点に来るまでメッシュを動かします。 |

次のチュートリアルでは、ドアノブを作ります：[ドアノブをモデリングする](modelar-picaporte.md)。

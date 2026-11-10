# カードとプロパティ

## カード

`GroupPropertie`（`Propieties/GroupPropertie.h`）は、タイトルと行を持つカードです。各行はプロパティで、`PropertieBase`を継承したクラスです。自分の変数を指していて、カードはその値を表示し、その場で変更します。

```cpp
GroupPropertie* jugador = new GroupPropertie("Jugador");
jugador->icono = IconType::gamepad;        // オプション：矢印とタイトルの間

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
jugador->properties.push_back(vel);

jugador->Resize(ancho, 0);    // 高さはカードが計算する
jugador->Render();            // 現在の原点に
```

カードは自分の行を所有しています：カードを削除すると、行も削除されます。

- タイトルには、カードを折りたたむための矢印（`open`）が付きます。`flecha = false`にすると矢印はなくなります：フローティングウィンドウ用です。
- その下は2列です：左にラベル、右に値。`anchoValores`は幅のうち値が使う割合で、初期値は0.5です。
- `reservaKeyBtn = true`にすると、いちばん右にアニメーションのひし形用の列を確保します。
- ほとんどの行の高さは、1行分に`gap`を1つ足したものです。変数のない（`value`が`NULL`の）数値、チェックボックス、色は、場所を取らず、描画もされません。
- `visible = false`にすると、カード全体が隠れます。

選択中の行は、ラベルが`accent`、値が白、枠が白になります。編集中は、値と枠が`accent`になります。マウスの下にある行は白になります：これはカードを含む側が、`PropHoverGroup`と`PropHoverFila`で指定します。

## 選択と編集

`selectIndex`は選択中の行です：`-2`はなし、`-1`はタイトル、`0`以上は行です。

| 関数 | 働き |
|---|---|
| `NextSelect()`、`PrevSelect()` | 下または上の行に移ります。スペースや変数のない行は飛ばします。カードの外に出ると`true`を返すので、次のカードに移れます |
| `EnterPropertieSelect()` | タイトルでは、カードを折りたたむか展開します。行では、その行を編集します |
| `button_up()`、`button_down()`、`button_left()`、`button_right()` | 編集中は、行に送られます |
| `Cancel()` | 行を元のままにします |

`editando`は、編集中の行があるかどうかを示します。これを使って、矢印キーで値を変えるか、選択を動かすかを決めます：

```cpp
if (jugador->editando) jugador->button_up();   // 値を変える
else jugador->PrevSelect();                    // 上の行へ
```

マウスは、カードを含む側が処理します：カードは、画面上のどこに描かれたかを知りません。上には枠（`borderGS`）とタイトル（`RenglonHeightGS + gapGS`）があります。各行の高さは、その`Resize(ancho)`が返す値です。

## 行

| クラス | 内容 | 編集のしかた |
|---|---|---|
| `PropFloat` | 数値 | 矢印キー、または入力 |
| `PropBool` | 右端に寄せたチェックボックス | OKでオン・オフ |
| `PropColor` | 右端に寄せた、その色の正方形 | OKで編集に入り、キャンセルに備えて色を保存します |
| `PropButton` | ボタン | OKでその動作を呼びます |
| `PropButtonRow` | 1行に並んだ複数のボタン | 左右で1つを選び、OKで押します |
| `PropText` | テキストボックス | OKでフォーカスして入力します |
| `PropListMeshParts` | スクロールできるリスト | 上下で選びます |
| `PropLabel` | 読むためのテキスト（1行または複数行） | 選べません |
| `PropSeparator` | 線 | 選べません |
| `PropGap` | `gap`1つ分のスペース | 選べません |
| `PropSelectText` | ラベル付きのボックス。値はありません | まだ何もしません |

### 数値

```cpp
PropFloat* vida = new PropFloat("Vida");
vida->value = &vidaJugador;
vida->SetRango(0, 100);           // この範囲を超えない
vida->entero = true;              // 整数で表示し、整数ずつ進む
vida->stepFino = 1;               // 上下キー
vida->stepGrueso = 10;            // 左右キー
vida->onChange = AlCambiarVida;   // void AlCambiarVida()
```

| フィールド | 働き |
|---|---|
| `unit` | 数値の後ろに付けるテキスト（`°`など）。コンストラクターの2番目の引数です |
| `stepFino`、`stepGrueso` | 矢印キーで足す量：初期値は0.01と0.1 |
| `entero` | 数値を丸めて表示し、矢印キーは丸めた値から進みます。これがなければ小数点以下4桁です |
| `centrado` | 数値をボックスの中央に置きます |
| `flechas` | 両側に`<`と`>`を描きます。そこでのクリックは、含む側が処理します |
| `opciones`、`nOpciones` | `entero`と組み合わせて：数値はインデックスになり、その選択肢の名前を`W3dMenuTraducir`で翻訳して表示します |
| `acelera` | 左右キーは1ずつ始まり、繰り返すと10まで上がります |
| `dragStep` | ドラッグしたとき1ピクセルあたりに変わる量。ドラッグ自体は、含む側が処理します |

`Set(v)`は範囲内で値を設定し、`onChange`を呼びます。矢印キーもここを通ります。

入力するには：

```cpp
vida->IniciarEdicionTexto();   // 値を表示し、全体を選択する
// キー入力はg_textFieldActivoへ：「入力とテキストボックス」を参照
NumEditCommit();               // Enter：適用する
NumEditCancel();               // Esc：元のままにする
```

`NumEditActivo()`は、編集中の数値があるかどうかを示します。数値でないテキストは0になります。

### チェックボックス

`value`は`bool`を指します。`onChange`は変わるたびに呼ばれます。ラベルはチェックボックスの左側の場所をすべて使い、収まらなければ`..`で切られます。

### 色

`value`は4つの`float`（RGBA）を指します。正方形はUIが描画しますが、カラーピッカーはUIのものではありません。編集中、矢印キーで動くのは最初の成分だけです。

### ボタン

`action`は、呼び出す関数です。`gris = true`にすると薄く表示され、何もしなくなります。`oculto = true`にすると行を取りません。

`conLabel = true`にすると、行は名前とボタンになります：名前は左側で、テキストの幅（最大で行の55%）を取り、ボタンは残りすべてを使います。`labelChars`は名前の幅を固定し、複数の行をそろえるためのものです。`button->desplegable`にすると、メニューの小さな矢印が付きます。さらに：

- `AgregarPrefijo(accion, ayuda)`は、ボタンの前に正方形を置きます。色を表示するためのものです。
- `AgregarExtra(icono, accion, ayuda)`は、右側にアイコンだけの小さなボタンを2つまで置きます。`slotsExtras`はその場所を確保し、行の間で位置がそろうようにします。`ExtraEnX(x)`は、そのxにあるのがどれかを返します。クリック用です。

### ボタンの行

```cpp
PropButtonRow* fila = new PropButtonRow();
fila->Agregar("Aceptar", Aceptar);
fila->Agregar("Cancelar", Cancelar);
jugador->properties.push_back(fila);
```

ボタンは幅を均等に分け合います。非表示のボタンは飛ばされ、残りのボタンがその場所を分け合います。`Disparar(i)`はボタン`i`を押します：クリックがその部分に当たったときに、含む側が呼びます。

### テキストボックス

```cpp
PropText* nombre = new PropText("Nombre", "Jugador 1");
// nombre->field.textに、入力した内容が入る
```

`onClick`：編集する代わりにその関数を呼びます。たとえばファイルブラウザーを開くためのものです。`error = true`にすると、何か入力されるまで、選択中の行の枠を赤くします。`oculto = true`にすると行を取りません。OKを押すと`TextFieldEnfocar`でフォーカスしますが、この関数は現在エディターで定義されています。入力については、[入力とテキストボックス](entrada.md)を参照してください。

### テキスト

`PropLabel("texto")`は1行です。`PropLabel("texto", true)`は、幅に収まるようにスペースで改行します。1語だけで収まらない単語は、そのまま残ります。`rojo = true`はエラー用です。`oculto = true`にすると行を取りません。

### リスト

`PropListMeshParts`は`filasMax`行（初期値は3）を表示し、それより多ければ小さなバーを付けます。選択中の行は常に見える位置に保たれます。`Cancel`は、`selectIndex`を編集に入ったときの値に戻します。

まだ任意のリストには使えません：`modo`に応じて、シーンのリスト（メッシュのパーツ、そのUVマップ、カラーレイヤー、モディファイアー、頂点グループ、ボーン、アニメーション、コンストレイント、スクリプト）を読むか、エディターの関数のフック（`MixFila*`、`RutinaFila*`、`PostFila*`）に問い合わせます。

## アニメーションのひし形

アニメーションできる行（`PropFloat`、`PropBool`、`PropColor`、`PropButton`で`animProp`が0以上のもの）は、カードに`reservaKeyBtn`があれば、右側にひし形を表示します。その色が状態を示します：

| 状態 | 色 | 意味 |
|---|---|---|
| 0 | グレー | アニメーションなし |
| 1 | `accent`（暗め） | アニメーションあり。このフレームにキーフレームはない |
| 2 | `accent` | このフレームにキーフレームがある |
| 3 | 緑 | 値が変わったが、保存されていない |

ひし形はUIのものですが、その状態はUIのものではありません。状態は、プログラムが設定するフックに問い合わせ、クリックではもう一方のフックを呼びます：

```cpp
W3dKeyframeEstado = MiEstado;   // int MiEstado(int prop, int comp)：0〜3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp)：キーフレームを追加または削除する
```

フックがなければ、ひし形はグレーのままです。カードは`W3dKeyframeToggle`を呼びません：呼ぶのはクリックを処理する側です。`g_kfFocoProp`は、ひし形にキーボードのフォーカスがある行を指し、そのひし形を白い枠で示します。`W3dRenderRomboKey(estado)`は、単独のひし形を描画します。

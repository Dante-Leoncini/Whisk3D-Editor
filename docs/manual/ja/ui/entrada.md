# 入力とテキストボックス

## UIはイベントを読まない

Whisk3D UIはウィンドウを開かず、マウスもキーボードもタッチスクリーンも監視しません。イベントを受け取るのはプログラムで（ほとんどのシステムではSDL2経由。Coreの[入力](../core/entrada.md)を参照）、プログラムがそれぞれのウィジェットに必要なものを渡します：

| イベント | 渡す先 |
|---|---|
| マウスや指が動く | `PopupMenu::MouseMove(x, y)`、ボタンとタブの`hover`、カードの`PropHoverGroup`と`PropHoverFila` |
| クリックまたはタップ | `PopupMenu::Click(x, y)`、`Button::Contains`と`Tab::Contains`、カードではプログラムが計算した行 |
| ホイール | `PopupMenu::Wheel(dir)` |
| 矢印キー | メニュー、カード、行の`button_up`、`button_down`、`button_left`、`button_right` |
| OKまたはEnter | `PopupMenu::Enter()`、`GroupPropertie::EnterPropertieSelect()` |
| キャンセルまたはEsc | `PopupMenu::Cerrar()`、`GroupPropertie::Cancel()` |
| 文字 | フォーカスのあるテキストボックス |

座標は画面のピクセルで、Yは下向きです。

## マウスなしで

すべてを4つの矢印キーとOK、キャンセルで操作できます：N95ではそうやって使います。そのためにUIが用意しているもの：

- カードは1行ずつたどれて、選べないものは飛ばします。
- メニューは矢印キーでたどれて、ハイライトされた選択肢が表示範囲から外れることはありません。
- 数値は、入力しなくても矢印キーで変えられます。
- キーボードのフォーカスが見えます：ボタンでは`focoMenu`、タブでは`foco`が`accent`で示され、ひし形には`g_kfFocoProp`の白い枠が付きます。

電話のどのキー（ソフトキーやOKキー）がどれに当たるかは、UIの担当ではありません：エディターと各システムが変換します。

## タッチスクリーン

- タップはクリックです。
- サブメニューを開いたタップでは、その中のものは何も選ばれません（[ドロップダウンメニュー](menus.md)）。
- `UITactil`は、指で押しやすいようにボタンとバーを大きくします（[色とスケール](tema.md)）。現在はどこからもオンにされていません。

ドラッグしてのスクロール、長押し、ダブルタップは、UIを使う側が処理します。

## テキストボックス

`TextField`（`widgets/TextField.h`）は、テキストボックスの状態です：テキスト（`text`）、カーソルの位置（`caret`）、全体が選択されているかどうか（`selectAll`）。描画もキーの読み取りもしません：データだけです。

| 関数 | 働き |
|---|---|
| `SetText(t)` | テキストを設定し、カーソルを末尾に置きます |
| `SelectAll()` | 全体を選択します：最初のキー入力で置き換わります |
| `InsertChar(c)` | カーソルの位置に1バイトを書き込みます |
| `Backspace()`、`DelForward()` | 後ろ向き、または前向きに削除します |
| `CaretIzq()`、`CaretDer()` | カーソルを動かします。全体が選択されているときは、選択を解除します |

削除とカーソル移動では、`ñ`やキリル文字のような複数バイトの文字を1文字として扱います。そうした文字を入力するには、各バイトを`InsertChar`に渡します。

フォーカスのあるボックスは、一度に1つだけです：`g_textFieldActivo`で、ない場合は`NULL`です。キー入力はそのボックスに送ります。

```cpp
TextField nombre;
nombre.SetText("Jugador 1");
nombre.SelectAll();
g_textFieldActivo = &nombre;

// 文字が入力されるたびに
if (g_textFieldActivo) g_textFieldActivo->InsertChar(c);

// 描画時、テキストの色を設定したうえで
RenderTextField(nombre, g_textFieldActivo == &nombre, textAlign::left, 200);
```

`RenderTextField(caja, foco, alineación, ancho)`は、ボックスを現在の原点に描画します：

- フォーカスがなければ、テキストだけです。
- フォーカスがあって全体が選択されていれば、テキストは`accentDark`の上に白で表示されます。
- フォーカスがあって選択がなければ、カーソルが表示されます：530 msごとに点滅する短い線です。

`TextFieldParpadeoTick()`は、その線が切り替わったときに`true`を返します：再描画のタイミングです。

`TextField.h`では、`TextFieldEnfocar`、`TextFieldCancelar`（Esc：フォーカスする前のテキストに戻します）、`TextFieldInputChar`、`TextFieldFinDeCuadro`も宣言されています。現在、これらはエディターの`main/ui/ViewPorts/NumInput.cpp`で定義されていて、UIだけでは使えません。

# ボタン、タブ、チェックボックス

## ボタン

`Button`（`widgets/Button.h`）は、テキストと、左側に置くオプションのアイコンを持つカードです。暗い色の枠が、ボタンらしく見せています。マウスの下にあるとき（`hover`）は、枠が白になり、背景が少し明るくなります。

```cpp
Button* guardar = new Button("Guardar", IconType::guardar);   // 幅はテキストに合わせる
guardar->Resize(200);                                         // これより広くはならない

gfx::PushMatrix();
gfx::Translatef(x, y, 0);
guardar->Render();
gfx::PopMatrix();

guardar->sx = x; guardar->sy = y;      // どこに描かれたか（マウス用）
guardar->hover = guardar->Contains(mx, my);
```

`Render`は現在の原点に描画するだけで、画面上のどこに描かれたかは知りません：`sx`と`sy`は描画する側が設定し、`Contains`はそれと比較します。コンストラクターの3番目の引数は`adapt`です：`false`にすると、ボタンは`Resize`で与えられた幅をすべて使います。

| フィールド | 働き |
|---|---|
| `centered` | アイコンとテキストを中央に。AcceptやCancelのように |
| `square` | 幅を高さと同じにします：アイコンだけのボタン用 |
| `rowHeight` | バーの高さではなく、プロパティの行の高さにします |
| `caretMenu` | 右側に小さな矢印を付けます：メニューが開くことを示します |
| `tint` | `gray`の代わりに使う背景色 |
| `textColor`、`borderColor` | テキストとアイコンの色、枠の色 |
| `focusMenu` | キーボードでフォーカスされている状態：枠とテキストが`accent`になります |
| `alpha` | 背景と枠の不透明度。後ろにあるものを透けて見せるためのもの |
| `iconFlip` | アイコンを反転します：1で左右、2で上下 |
| `bar` | 0〜1の数値：ボタンがそこまで`accentDark`で満たされ、テキストは中央に表示されます |
| `fill` | 内側にその色の正方形を描きます：色を選ぶためのもの |
| `editField` | `TextField`：ボタンをテキストボックスとして描画します。その場で名前を変えるためのもの |
| `visible` | `false`にすると描画されません |

ボタンが保持はするものの描画には使わないフィールドもあります：これらはボタンを含む側が使います。`dropdown`は、メニューが開くことを示します。`separator`は、バーの中で手前に線を引くよう求めます。`role`は、順番に頼らずにバーの中でボタンを見つけるためのものです。`help`は、マウスを乗せたままにすると表示されるツールチップのテキストです。ツールチップ自体はUIのものではありません。

高さは`UIButtonHeight()`（[色とスケール](tema.md)）で、`rowHeight`のときは`RowHeightGS + 2 * GlobalScale`です。1行分より細くなることはありません。

## タブ

`Tab`（`widgets/Tab.h`）は、下側の枠がないボタンです：下にあるものと一体に見えます。アクティブなタブ（`active`）は少し高く、中身と同じ色になり、それ以外のタブは暗くなります。テキストは、キーボードのフォーカスがあるとき（`focus`）は`accent`、アクティブなときやマウスの下にあるときは白、それ以外はグレーです。

使い方はボタンと同じで、`Resize(maxW)`、`Render()`、`Contains(mx, my)`を使い、`sx`と`sy`は描画する側が設定します。

## チェックボックスと色見本

チェックボックスの見た目は1か所、`widgets/card.h`だけで決まります。そのため、メニューのチェックボックスとカードのチェックボックスが違う見た目になることはありません。

| 関数 | 働き |
|---|---|
| `DrawCheckbox(side, on)` | ボックスを描画し、オンならチェックマークも描きます |
| `DrawSampleColor(side, rgba)` | チェックボックスと同じ角を持つ、その色の正方形を描画します |
| `CheckboxSide()` | 1辺の長さ：1行分の高さ |
| `CheckboxBoxColor(on)`、`CheckboxCheckColor()` | ボックスとチェックマークの色 |
| `CheckboxCheck()` | チェックマークの文字、`✔` |

オンのとき、ボックスは`grayUI`で、チェックマークは背景色です。オフのとき、ボックスは背景色で、チェックマークはありません。

```cpp
DrawCheckbox(CheckboxSide(), onSound);
```

# ドロップダウンメニュー

`PopupMenu`（`widgets/PopupMenu.h`）は、選択肢を並べたフローティングのリストです：バーのメニュー、コンテキストメニュー、セレクターなど。各選択肢（`MenuItem`）はテキストを持ち、さらに次のものを持てます：

- 左側の**アイコン**
- 右側に矢印が付く**サブメニュー**
- `bool`を表示して切り替える**チェックボックス**
- 2つの値の間で`float`を編集する**スライダー**
- 右側に薄いグレーで表示される**ショートカット**（"Shift D"）
- `green`：アクティブな選択肢で、アイコンとテキストが`accent`になります
- `gray`：`bool`を指します。それが`false`の間、選択肢は半透明で表示され、反応しません
- `lineUp`：上のグループと区切る線

メニューには**タイトル**（`title`）を付けられます：白で表示され、下に線が引かれます。

## テーブルで組み立てる

おすすめの方法は、`MenuDef`のテーブルを使うことです：テキストと、各選択肢がすることを同じ行に書きます。別の場所の`switch`と結び付ける番号はありません。

```cpp
static bool grid = true;
static PopupMenu* recent = NULL;
static void New() { /* ... */ }
static void Open() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // テキスト  動作    キー      アイコン                 サブメニュー チェック
    { "Nuevo",   New,  "Ctrl N", ICON(IconType::more) },
    { "Abrir",   Open,  "Ctrl O", ICON(IconType::folder) },
    { "Recientes", NULL, NULL,     ICON(IconType::list),  &recent },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grid },
};

recent = new PopupMenu();          // サブメニューは、組み立てる前に存在している必要がある
PopupMenu* archivo = new PopupMenu();
archivo->title = "Archivo";
archivo->Build(ARCHIVO, 4);
```

`ICON(x)`はアイコンに1を足します：テーブルでは、0は「アイコンなし」を意味するからです。各選択肢のidは、その行の番号です。

プログラムが設定する、オプションのフックが2つあります：

```cpp
W3dMenuTranslate = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuShortcutText = MiShortcut;      // std::string MiAtajo(const std::string& atajo)
```

`W3dMenuTranslate`を設定すると、メニューを組み立てるときにテーブルのテキストが翻訳されます。`W3dMenuShortcutText`を設定すると、ショートカットに`@`で始まる名前を書けるようになり、フックが対応するキーを返します。このフックがないと、`@`付きのショートカットは表示されません。

## 手で組み立てる

```cpp
PopupMenu* view = new PopupMenu();
view->Add("Frente", 0, IconType::camera);
view->AddCheck("Mostrar piso", 1, &showFloor);
view->AddFloat("Tamaño", 2, &size, 0.0f, 4.0f);
MenuItem* it = view->Add("Alambre", 3);
it->green = isWire;
it->lineUp = true;
view->action = OnPick;              // void AlElegir(int id)
```

`Add`は`MenuItem*`を返すので、ほかの設定を変えられます。選択肢を選ぶと、`Run(id)`は、その選択肢に`action`があればそれを呼び、なければメニューの`action(id)`を呼びます。`Clear()`はすべての選択肢を削除します：内容が変わるメニュー用です。

## 開く

```cpp
archivo->Open(mx, my, MenuScreenW, MenuScreenH);
OpenMenu = archivo;
```

渡した位置に開きます：カーソルの位置や、ボタンの端などです。収まらなければ、画面の内側へずれます。`Open`は`OpenMenu`に触れません：設定するのは開く側です。`Close()`はそれを`NULL`に戻します。

開くときに、サイズを測ります：

- 幅は、テキストと右側の列（ショートカット、矢印、チェックボックス、スライダー）を含めた、いちばん長い行の幅です。アイコンを持つ行が1つでもあれば、すべての行がその分の場所を空けるので、テキストの位置がそろいます。
- 画面より広くなることはありません。収まらなければ、各行のテキストは右側の列の手前で切られます：チェックボックスは見えたままです。
- 画面より高くなることもありません。行がもっと多ければ、収まる分だけを表示し、右側にスクロールバーを付けます。ホイール（`Wheel`）で動かします。

サブメニューは、その行の隣に開きます：収まれば右に、収まらなければ左に開きます。どちらにも収まらなければ幅を狭め、後ろのメニューの1列分を見えるように残すので、マウスを別の選択肢へ動かせます。

## マウスと指

```cpp
// 動かしたとき
if (OpenMenu) OpenMenu->MouseMove(mx, my);

// クリックまたはタップしたとき
if (OpenMenu) {
    PopupMenu* m = OpenMenu;
    int id = m->Click(mx, my);
    if (id >= 0) m->Run(id);
}

// 描画時、画面全体の投影で
if (OpenMenu) OpenMenu->Render();
```

`Click`は、選んだidを返します。何も選ばなければ`-1`、サブメニューを開いたなら`-2`を返します。

- マウスを乗せると、クリックしなくても行がハイライトされ、そのサブメニューが開きます。
- マウスが`60 * GlobalScale`ピクセルより遠くへ離れると、メニューは自動で閉じます。`closeFar = false`にすると閉じません：マウスの位置で開いたのではないメニュー用です。
- 外側をクリックすると閉じます。
- 通常の選択肢を選ぶとメニューは閉じます。チェックボックスは切り替わってもメニューを開いたままにするので、複数をオンにできます。スライダーをクリックすると、クリックした位置に応じた値になります。
- スライダーの上で押したままドラッグすると、値がリアルタイムで変わります。マウスを処理する側は、`MenuSliderDragActive()`が`true`の間、動きのたびに`MenuSliderDragMove(mx)`に渡し、離したときに`MenuSliderDragRelease()`を呼びます。
- タッチスクリーンでは、サブメニューを開いたタップでは、たとえサブメニューが指の下に来ても、その中のものは何も選ばれません。もう一度タップする必要があります。
- サブメニューで選んだものは、そのサブメニューで実行されます：メニューはそれぞれ独自のidを持っています。

## キーボード

フォーカスを持つのは、いちばん内側で開いているサブメニューです：

```cpp
PopupMenu* m = OpenMenu;
while (m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
MenuItem* it = m->CurrentItem();      // ハイライトされた選択肢、またはNULL
```

| 関数 | 働き |
|---|---|
| `button_up()`、`button_down()` | ハイライトを動かします。端まで行くと反対側に回ります。サブメニューは開きません |
| `OpenSubmenuCurrent()` | ハイライトされた選択肢のサブメニューを開き、その最初の選択肢をハイライトします |
| `FitSlider(+1)`、`FitSlider(-1)` | スライダーを範囲の20分の1だけ動かします |
| `Enter()` | ハイライトされた選択肢をクリックしたのと同じで、同じ値を返します。`OpenMenu`で呼ぶと、いちばん内側のサブメニューにだけ届きます |
| `Close()` | メニューとそのサブメニューを閉じます |

キーボードでは、ハイライトされた選択肢は常に見える位置に保たれます。どのキーで何を呼ぶかは、メニューを使う側が決めます。

`MenuScreenW`と`MenuScreenH`には、画面のサイズを入れておく必要があります：メニューの位置はこれを使って調整されます。

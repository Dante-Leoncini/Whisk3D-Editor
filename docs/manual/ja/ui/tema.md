# 色とスケール

## パレット

UIの色は、固定の色ではなく役割です：enumの`ColorID`（`theme/colores.h`）。

| 役割 | 用途 |
|---|---|
| `background` | 背景。オフのチェックボックスのボックスと、通常状態のボタンの枠 |
| `blanco` | テキストと、マウスの下にあるものや選ばれているものの枠 |
| `accent` | フォーカス：メニューの枠、編集中のもの、アクティブな選択肢、スライダーの塗り |
| `accentDark` | 選択したテキストの背景と、数値ボタンのバー |
| `negro` | `PropSeparator`の線 |
| `gris` | カード、ボタン、メニューの本体 |
| `headerColor` | メニューのハイライトされた行と、スライダーのトラック |
| `negroTransparente` | 25%の黒。UIでは使っていません |
| `grisUI` | 通常のテキストと、オンのチェックボックスのボックス |
| `grisLinea` | メニューでグループを区切る線 |

`ListaColores`は色を`float`で、`ListaColoresUbyte`はバイトで持っています。`SetColorID`は、オプションのアルファ付きで、その色を描画色に設定します：

```cpp
SetColorID(ColorID::accent);         // 不透明
SetColorID(ColorID::grisUI, 0.5f);   // 半透明
```

`loadColorsW3d(ruta)`は、色をテキストファイルから読み込みます。1行に1色で、名前と0〜1の4つの成分を書きます（例：`accent 0.0 1.0 0.0 1.0`）。知らない名前は無視するので、同じ`skin.ini`にほかのプログラムの色が入っていてもかまいません。ファイルを開けなければ`false`を返します。ファイルがなければ、コードに書かれた色（`theme/colores.cpp`）のままです。Whisk3Dのスキンはそれを変更していて、たとえば`accent`は緑になります。

パレットからではなく、コードに直接書かれている色もあります：保存していない変更を示すひし形の緑、エラーの`PropLabel`の赤、未入力の`PropText`の赤い枠、無効な`PropButton`のグレーです。

## スケール

基本の寸法は、ピクセル単位で`core/UI.h`にあります：

| 寸法 | 値 |
|---|---|
| `margin`、`padding` | 10 |
| `border` | 4 |
| `gap` | 5 |
| `RenglonHeight` | 12 |
| `LetterWidth`、`CharacterWidth` | 6 |
| `LetterHeight` | 11 |

`GlobalScale`はこれらに掛ける値で、必ず整数です：そうすることで、ピクセルの文字がくっきりしたままになります。たとえば、N95の240ピクセルの画面では1、PCでは3です。

各寸法には、すでに掛け算をした、末尾に`GS`が付いた版があります：`RenglonHeightGS`、`gapGS`、`borderGS`、`bordersGS`（枠の2倍）、`CharacterWidthGS`、`LetterHeightGS`…。ウィジェットが使うのはこちらです。

UIには、これらを再計算する関数は入っていません：エディターのものは`main/ui/ViewPorts/ViewPorts.cpp`の`SetGlobalScale`です。単体で使う場合は、次のようにします：

```cpp
void Escala(int s) {
    GlobalScale = s;
    marginGS = margin * s;  paddingGS = padding * s;  gapGS = gap * s;
    borderGS = border * s;  bordersGS = borderGS * 2;
    RenglonHeightGS = RenglonHeight * s;
    LetterWidthGS = LetterWidth * s;  LetterHeightGS = LetterHeight * s;
    CharacterWidthGS = CharacterWidth * s;
    SetIconScale(s);
    if (WhiskFont) WhiskFont->SetScale(s);
}
```

各ウィジェットのサイズは、その`Resize`で計算されます：スケールを変えたあとは、すべてのカード、ボタン、メニューでもう一度呼ぶ必要があります。

## コンパクトかタッチか

`UIBotonAltura()`はボタンの高さを、`UIBarPadding()`はバーの中のボタンの上下の余白を返します。どちらも`UITactil`によって変わります：

- `false`はN95向けのコンパクトモードです：ボタンの高さは1行分に枠を足して`2 * GlobalScale`を引いたもので、バーは上下に`GlobalScale`ずつ余白を取ります。
- `true`：指で押しやすいように、ボタンとバーに`gap`を1つ足します。枠は太くなりません。

`UITactil`の初期値は`false`で、現在はどこからもオンにされていません。

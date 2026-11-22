# アイコン

## ファイル

各アイコンは、エディターのリポジトリの`res/Skins/Whisk3D/atlas/icons/`にある**10x10**のPNGです。現在は106個あります。ピクセルは透明の上に白で描かれていて、アイコンは描画するときの色になります。そのため、同じアイコンをグレーでも、白でも、アクセントカラーでも使えます。

## enumと名前

`draw/icons.h`には、enumの`IconType`と、その数`ICON_TOTAL`（106）があります。`draw/icons.cpp`では、`KNames`が同じ順番でそれぞれの名前を持っています：これは`.png`を除いたファイル名です。

```cpp
IconName(IconType::folder);   // "carpeta"
IconIndex("carpeta");           // IconType::carpeta、存在しなければ-1
```

ほとんどはenumと同じ名前です。例外は`lockOpen`で、ファイルは`candado_abierto.png`です。

多くはエディター用のものです：モデリングのツール、オブジェクトの種類、アニメーションのカーブ。汎用のものもあります：`arrow`と`arrowRight`（折りたたみの矢印）、`visible`と`hidden`、`folder`、`save`、`trash`、`plus`と`minus`、`list`と`grid`、`lock`、`notifOk`と`notifError`、`scissors`、`copy`と`paste`。

## 座標

`IconsUV`は`std::vector<IconUV*>`で、アトラス上での各アイコンの座標（`uvs`、8つの数値）を持っています。埋め方は2通りです：

- `CreateIcons(width, height)`：`font.png`の中で各アイコンがどこにあるかを示すテーブル（`icons.cpp`の`TableLegacy`）を使います。`atlas/icons/`フォルダーを使わない方法です。
- `CreateIconsFrom(rects, width, height)`：組み立てたアトラスの、独自の矩形のリストを使います。`x = -1`の矩形は`font.png`のものを使うので、PNGのないアイコンも表示され続けます。

`SetIconScale(scale)`は、`IconSizeGS`と2つのメッシュを調整します：アイコン1つ分の正方形である`IconMesh`と、ツリーの線に使う、1行分の高さの正方形である`IconLineMesh`です。

## アイコンを描画する

アトラスを設定した状態で、現在の原点に描画します：

```cpp
SetColorID(ColorID::grayUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::folder]->uvs);
```

`Button`、`Tab`、`PopupMenu`、`GroupPropertie`は、アイコンをenumの`int`として受け取ります。アイコンなしは`-1`です。

## アイコンを追加する

1. 10x10のPNGを`atlas/icons/`に置きます。
2. enumの`IconType`の最後に1行追加し、`ICON_TOTAL`に1を足します。
3. その名前を`KNames`の最後に追加します。
4. `TableLegacy`の最後に1行追加し、PNGがないときに代わりに使う`font.png`のアイコンを指定します。

手順4の行がないと、アトラスが組み立てられていないときに、そのアイコンには座標がなくなります。

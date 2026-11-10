# アイコン

## ファイル

各アイコンは、エディターのリポジトリの`res/Skins/Whisk3D/atlas/iconos/`にある**10x10**のPNGです。現在は106個あります。ピクセルは透明の上に白で描かれていて、アイコンは描画するときの色になります。そのため、同じアイコンをグレーでも、白でも、アクセントカラーでも使えます。

## enumと名前

`draw/icons.h`には、enumの`IconType`と、その数`ICON_TOTAL`（106）があります。`draw/icons.cpp`では、`KNombres`が同じ順番でそれぞれの名前を持っています：これは`.png`を除いたファイル名です。

```cpp
IconoNombre(IconType::carpeta);   // "carpeta"
IconoIndice("carpeta");           // IconType::carpeta、存在しなければ-1
```

ほとんどはenumと同じ名前です。例外は`candadoAbierto`で、ファイルは`candado_abierto.png`です。

多くはエディター用のものです：モデリングのツール、オブジェクトの種類、アニメーションのカーブ。汎用のものもあります：`arrow`と`arrowRight`（折りたたみの矢印）、`visible`と`hidden`、`carpeta`、`guardar`、`borrar`、`mas`と`menos`、`lista`と`cuadricula`、`candado`、`notifOk`と`notifError`、`tijera`、`copiar`と`pegar`。

## 座標

`IconsUV`は`std::vector<IconUV*>`で、アトラス上での各アイコンの座標（`uvs`、8つの数値）を持っています。埋め方は2通りです：

- `CrearIconos(ancho, alto)`：`font.png`の中で各アイコンがどこにあるかを示すテーブル（`icons.cpp`の`TablaLegacy`）を使います。`atlas/iconos/`フォルダーを使わない方法です。
- `CrearIconosDesde(rects, ancho, alto)`：組み立てたアトラスの、独自の矩形のリストを使います。`x = -1`の矩形は`font.png`のものを使うので、PNGのないアイコンも表示され続けます。

`SetIconScale(escala)`は、`IconSizeGS`と2つのメッシュを調整します：アイコン1つ分の正方形である`IconMesh`と、ツリーの線に使う、1行分の高さの正方形である`IconLineMesh`です。

## アイコンを描画する

アトラスを設定した状態で、現在の原点に描画します：

```cpp
SetColorID(ColorID::grisUI);
W3dDrawStrip4(IconMesh, IconsUV[IconType::carpeta]->uvs);
```

`Button`、`Tab`、`PopupMenu`、`GroupPropertie`は、アイコンをenumの`int`として受け取ります。アイコンなしは`-1`です。

## アイコンを追加する

1. 10x10のPNGを`atlas/iconos/`に置きます。
2. enumの`IconType`の最後に1行追加し、`ICON_TOTAL`に1を足します。
3. その名前を`KNombres`の最後に追加します。
4. `TablaLegacy`の最後に1行追加し、PNGがないときに代わりに使う`font.png`のアイコンを指定します。

手順4の行がないと、アトラスが組み立てられていないときに、そのアイコンには座標がなくなります。

# UIのものとエディターのもの

エディターはWhisk3D UIで作られていますが、エディターで目にするものの多くはエディター自身のものです。UIのものは`libs/WhiskUI/`に、エディターのものはWhisk3Dリポジトリの`main/`にあります。

## UIのもの

- 2D描画、枠付きのカード、矩形。
- アイコンのテーブルと、アトラスのパッカー。
- ビットマップフォント、拡張アトラス、ベイク済みのTTFフォントによるテキスト。
- カラーパレットと寸法、そしてそのスケール。
- ボタン、タブ、チェックボックス、ドロップダウンメニュー。
- プロパティのカードとその行、アニメーションのひし形。
- テキストボックスの状態とその描画。

## エディターのもの

| 内容 | 場所 |
|---|---|
| ビューポート：画面の分割、その枠、メニューバーとツールバー | `main/ui/ViewPorts/`（`ViewPorts.cpp`、`WithBorder.cpp`、`ToolbarBase.cpp`） |
| プロパティパネル：どのカードがあるか、そのタブ、行の上でのクリックとドラッグ | `main/ui/ViewPorts/Properties.cpp` |
| モーダルウィンドウ：カラーピッカー、ファイルブラウザー、確認、プログレスバー、画面上のキーボード | `main/ui/ViewPorts/PopUp/` |
| 通知 | `main/ui/ViewPorts/Notificaciones.cpp` |
| パネルのスクロールバー | `main/ui/ViewPorts/ScrollBar.cpp` |
| ヘルプのツールチップ。テキストは`Button::ayuda`が保持します | `main/ui/ViewPorts/LayoutInput.cpp`の`W3dAyudaRender` |
| どのキー、クリック、タップをどのウィジェットに送るか、N95のキー | `main/ui/ViewPorts/LayoutInput.cpp`、`main/app/controles.cpp`、`platform/symbian/src/` |
| テキストボックスのフォーカス用の関数 | `main/ui/ViewPorts/NumInput.cpp` |
| 翻訳、`T()`。UIにあるのは`W3dMenuTraducir`というフックだけです | `main/config/W3dLang.cpp` |
| UIの起動 | `main/app/W3dInitUI.cpp` |
| 個別のアイコンからスキンのアトラスを組み立てる処理 | `main/io/SkinAtlas.cpp` |
| スケールの再計算 | `main/ui/ViewPorts/ViewPorts.cpp`の`SetGlobalScale` |
| PCとAndroidでの`skin.ini`の読み込み | `main/app/main.cpp`の`loadColors` |
| エディター独自の色：床、軸、法線 | `main/ui/W3dColors.cpp` |
| スキンのファイル：`font.png`、`font-ext.png`、`atlas/iconos/`、`skin.ini` | `res/Skins/Whisk3D/` |
| 拡張アトラスの生成ツール | `tools/fuente_ext.py` |
| [2D Editor](../editor/editor-2d.md)と、ゲームの`.w3dui`インターフェース：テキスト、画像、矩形、コンテナー、Slice 9、ボタン、動画 | `main/objects/`（`UI.h`、`Texto2D.h`、`Boton2D.h`...）、`main/render/UIOverlay.cpp`、`main/io/UI2DFormato.cpp`、`main/io/Fuente2D.cpp`、`main/ui/ViewPorts/Editor2D.cpp` |

2D Editorはテキストを`W3dTextAtlas`で描画していて、これはUIのものです。実行中にstb_truetypeでTTFをベイクする処理は、エディターのもの（`main/io/Fuente2D.cpp`）です。

## UIの中にあるが、エディターのもの

`libs/WhiskUI/`にあるものの、まだエディターのものであるもの：

- `PropListMeshParts`はメッシュ、アーマチュア、アニメーション、コンストレイント、スクリプトを読み、プロパティパネルの`RebindMaterialMeshPart()`を呼びます。
- `PropColor`は、カラーピッカーのために、パレットと色の取得元のオブジェクト（`palRef`、`palObj`）を保持します。
- `GroupProperties`は、プロパティパネルのカードのグローバルなリストです。
- `core/UI.h`はエディターの状態を持っています：Edit Modeのタッチ用ビューモード（`g_viewEditMode`）、マウスの移動量（`dx`、`dy`）、Alt、Shift、Ctrlキー（`LAltPressed`、`LShiftPressed`、`LCtrlPressed`）、ビューポートの寸法（`paddingViewport`、`MinViewportHeight`、`MinViewportWidth`）。
- アイコンのほとんどは、エディターのツール用です。

分離が残っている部分と、ライブラリの現状：[UIを直接使う](empezar.md)の最後を参照してください。

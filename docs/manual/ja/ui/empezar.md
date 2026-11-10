# UIを直接使う

CまたはC++でプログラミングをしていて、エディターなしでインターフェースを使いたい人向けです。必要なもの：

- **Coreと、そのシーンモデル。** UIは`w3dEngine`で描画し、一部の行はシーンのオブジェクト（`objects/`、`animation/`、`script/`）を読みます。[Coreを直接使う](../core/empezar.md)を参照してください。
- **UIのソース。** `libs/WhiskUI/`のすべての`.cpp`。インクルードディレクトリは、`"WhiskUI/draw/icons.h"`のための`libs/`と、サブフォルダーの`widgets/`、`text/`、`draw/`、`theme/`、`core/`です。内部ではフォルダーを付けずに互いをインクルードしている（`"icons.h"`、`"card.h"`）からです。
- **スキンのファイル。** `font.png`（文字とカードの縁）、`font-ext.png`（拡張アトラス）、`skin.ini`（色）、そして新しいアイコン用の`atlas/iconos/`フォルダー。これらはライブラリには含まれていません：エディターのリポジトリの`res/Skins/Whisk3D/`にあります。

## 起動

エディターが起動時に行うのと同じ処理から、エディター固有の部分を除いたもの：

```cpp
#include "w3dTexture.h"
#include "WhiskUI/core/UI.h"
#include "WhiskUI/theme/colores.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/widgets/card.h"
#include "WhiskUI/text/font.h"
#include "WhiskUI/text/W3dFont.h"

loadColorsW3d("Skins/Whisk3D/skin.ini");    // ファイルがなければ、コードに書かれた色のまま

unsigned int atlas = 0; int w = 0, h = 0;
w3dEngine::LoadTexture("Skins/Whisk3D/font.png", atlas, &w, &h);

CalcCardUV(w, h);          // 角丸の縁
CrearIconos(w, h);         // font.pngの中にあるアイコン
W3dFontSetTexSize(w, h);
SetIconScale(GlobalScale);

WhiskFont = new Font(w, h, atlas);
WhiskFont->SetScale(GlobalScale);
WhiskFont->SetAtlasExtendido("Skins/Whisk3D/font-ext.png");   // 必要になったときに初めて読み込まれる
```

`GlobalScale`の初期値は1です。別のスケールにするには、[色とスケール](tema.md)を参照してください。`atlas/iconos/`の個別のアイコンについては、[描画とアトラス](dibujo.md)を参照してください。

ウィンドウのサイズが変わるたびに：

```cpp
W3dPantallaAlto = alto;    // インターフェースのYを、ScissorとViewportのYに変換するため
MenuPantallaW = ancho;     // メニューが画面からはみ出さないように
MenuPantallaH = alto;
```

## 2Dの状態

UIはピクセル単位で描画します。Yは下向きで、原点は左上です。各ウィジェットは現在の行列の原点に描画されるので、`Translatef`で位置を決めます。描画の前に：

```cpp
namespace gfx = w3dEngine;

gfx::Viewport(0, 0, ancho, alto);
gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
gfx::Ortho(0, ancho, alto, 0, -1, 1);       // Yは下向き
gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();
gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
gfx::Disable(gfx::Lighting);  gfx::Disable(gfx::Fog);
gfx::EnableArray(gfx::VertexArray); gfx::EnableArray(gfx::TexCoordArray);
gfx::DisableArray(gfx::NormalArray);
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(atlas);
gfx::TexFilter(false);                       // くっきりしたピクセル
gfx::Enable(gfx::Blend); gfx::BlendAlpha();
```

## カード

数値、チェックボックス、ボタンを1つずつ持つカード。各行は自分の変数を指します：

```cpp
#include "WhiskUI/Propieties/GroupPropertie.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"

static float volumen = 0.8f;
static bool  pantallaCompleta = false;
static void Salir() { /* ... */ }

GroupPropertie* opciones = new GroupPropertie("Opciones");

PropFloat* vol = new PropFloat("Volumen");
vol->value = &volumen;
vol->SetRango(0.0f, 1.0f);
opciones->properties.push_back(vol);

PropBool* pc = new PropBool("Pantalla completa");
pc->value = &pantallaCompleta;
opciones->properties.push_back(pc);

PropButton* salir = new PropButton("Salir", IconType::borrar);
salir->action = Salir;
opciones->properties.push_back(salir);

opciones->Resize(200 * GlobalScale, 0);    // 高さはカードが計算する

// 毎フレーム、2Dの状態を設定したうえで
gfx::PushMatrix();
gfx::Translatef(10, 10, 0);
opciones->Render();
gfx::PopMatrix();
```

UIはキーボードもマウスも読みません：自分で渡します。キーボードでは：

```cpp
// 下矢印
if (opciones->editando) opciones->button_down();   // 値を変える
else opciones->NextSelect();                        // 下の行へ
// OKまたはEnter：タイトルではカードを折りたたみ、行ではその行を編集するか押す
opciones->EnterPropertieSelect();
// キャンセル：行を元のままにする
opciones->Cancel();
```

マウスの場合、ポインターの下にある行は自分で計算します：[カードとプロパティ](propiedades.md)を参照してください。メニューは座標を受け取ります：[ドロップダウンメニュー](menus.md)を参照してください。

## 現状を率直に言うと

- **本番で使える状態ではありません。** リポジトリの`README.md`にもそう書かれていて、実際そのとおりです：中にエディターのコードが入っていて、分離している最中です。
- **UIを単体で使うサンプルはありません。** [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples)の`ui/`にあるサンプルは、ゲームのランタイムが動かすLua付きの`.w3dui`インターフェースで、これはエディターの領分です。このページのコードはエディターが使っているのと同じ呼び出しで組み立てたもので、動作確認済みのサンプルから取ったものではありません。
- **現時点では単体でリンクできません。** `PropList.cpp`が呼ぶ`RebindMaterialMeshPart()`は、エディターのプロパティパネルにあります。`TextField.h`のフォーカス用の関数（`TextFieldEnfocar`、`TextFieldCancelar`、`TextFieldInputChar`、`TextFieldFinDeCuadro`）は、エディターの`main/ui/ViewPorts/NumInput.cpp`で定義されています。
- **Coreのシーンモデルに依存しています。** `PropertieBase.h`は`objects/Objects.h`をインクルードしていて、プロパティのリストはメッシュ、アーマチュア、アニメーション、スクリプトを読みます。
- **スケールはエディターが再計算しています。** `SetGlobalScale`は`main/ui/ViewPorts/ViewPorts.cpp`にあります。単体で使う場合は、寸法を手で再計算する必要があります（[色とスケール](tema.md)）。
- **すべてがグローバルです。** 1つのプログラムに、フォント（`WhiskFont`）、アイコンのテーブル（`IconsUV`）、開いているメニュー（`MenuAbierto`）、フォーカスのあるテキストボックス（`g_textFieldActivo`）が1つずつです。
- **ヘッダーにOpenGLの型が出ています**（`GLfloat`、`GLshort`、`GLuint`）。もうOpenGLを呼んではいないのですが。
- **今後の予定**（`README.md`より）：エディターのものであるコードを整理し、使い方のサンプルを作ること。

UIのものとエディターのものを1つずつ：[UIのものとエディターのもの](ui-y-editor.md)。質問は[Telegram](https://t.me/Whisk3D)のグループへどうぞ。

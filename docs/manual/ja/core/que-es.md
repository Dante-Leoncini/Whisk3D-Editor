# Whisk3D Coreとは

**Whisk3D Core**は、Whisk3Dをそれぞれのマシンから切り離すレイヤーです。いわば秘密の材料で、これのおかげで同じコードが**Windows**、**Linux**、**macOS**、**Android**、**ブラウザー**（WebGL）、**Nokia N95**と**N8**（Symbian）、**Sega Dreamcast**、**Sony PSP**、**PS Vita**、**Nintendo 3DS**で動きます。

CoreはC++のライブラリで、専用のリポジトリ[Whisk3D-Core](https://github.com/Dante-Leoncini/Whisk3D-Core)があり、MITライセンスです。設計は**RenderWare**のような往年のエンジンにならっています：上には小さく安定したAPIがあり、下にはシステムごとに異なる実装があります。

![3つのレイヤーで見るWhisk3Dの構成：上がEditor（緑）で、その中にLuaで作ったゲームやプロジェクトが入ります。中央がWhisk3D UI（黄）、下がWhisk3D Core（赤）です。EditorとUIは必須ではありません。外から、C++のゲームがUIを使うことも、Coreを直接使うこともできます。Coreは各システムとそれぞれのバックエンドを通してやり取りします](img/whisk3d-capas.webp)

## 考え方：自分でやらずに、頼む

最新のグラフィックボードを積んだPC、2007年の携帯電話、Dreamcastは、まったく違う方法で描画します。PCはOpenGLかVulkanを使います。PSPには専用のチップ（GE）とライブラリ（sceGu）があり、3DSはcitro3d、N95はOpenGL ES 1.1を使います。ゲームがそれぞれを直接呼び出すとしたら、11回書かなければなりません。

これを解決するのが**抽象化レイヤー**です。ゲームは自分で描画せず、**頼みます**。「このテクスチャとこのライトで、これらの三角形を描いて」と伝えると、そのシステムを知っている部品、つまり**バックエンド**が描き方を知っています。サウンド、ファイル、ログ、ネットワークも同じで、頼み方は1つ、実装はシステムごとに1つです。

決まりごと：

- **上のコードは、システムを決して呼び出しません。** OpenGLもsceGuもSymbianも呼びません。すべてCoreの関数を通します。
- **ヘッダーにはシステムの型を出しません。** テクスチャは`unsigned int`で、機能はOpenGLのものではなく独自のenum（`w3dEngine::DepthTest`、`w3dEngine::Blend`）です。そのため、Coreを使う側はグラフィックスのヘッダーに一切依存しません。
- **各システムが用意するのは最小限です。** 起動処理（ウィンドウ、GPUのコンテキスト）と、共有コードに入れられないものだけです。エディターのリポジトリでは、それは`platform/<sistema>/`と、`#ifdef W3D_PSP`、`#ifdef W3D_3DS`などのブロックに置かれています。

Whisk3D Coreを使うように書かれた、すべてのシステムで動くゲームの小さな例：

```cpp
#include "graphics/w3dGraphics.h"   // 描画
#include "graphics/w3dTexture.h"    // テクスチャ
#include "io/w3dFilesystem.h"       // ファイル
#include "base/w3dlog.h"            // ログ
#include "audio/W3dAudio.h"         // サウンド

namespace gfx = w3dEngine;

// ファイル：ディスク上、.w3dの中、APKの中、またはゲームに同梱されていてもよい
std::vector<unsigned char> nivel;
if (!w3dFileSystem::ReadFileBytes("niveles/uno.txt", nivel))
    w3dLogfE("no encontre %s", "niveles/uno.txt");

// テクスチャ：stb（またはSymbianのICL）がデコードし、バックエンドがアップロードする
unsigned int pasto = 0;
gfx::LoadTexture("texturas/pasto.png", pasto);

// サウンド：ミキシングはCore、出力は各システムが担当する
w3dEngine::W3dSound* salto = w3dEngine::W3dSoundLoad("sonidos/salto.wav");
w3dEngine::W3dSoundPlay(salto, 1.0f, false);

// 描画：OpenGL、sceGu、citro3dの呼び出しは1つもない
gfx::Enable(gfx::Texture2D);
gfx::BindTexture(pasto);
gfx::EnableArray(gfx::VertexArray);   gfx::VertexPointer3f(0, posiciones);
gfx::EnableArray(gfx::TexCoordArray); gfx::TexCoordPointer2f(0, uvs);
gfx::DrawTriangles(cantidadDeIndices, indices);
```

## 中身

Coreのフォルダーは、それぞれが1つのモジュールです：

| フォルダー | 役割 |
|---|---|
| `graphics/` | 描画API（`w3dEngine`）、テクスチャ、画面、バックエンド：[グラフィックス](graficos.md) |
| `audio/` | 効果音、音楽、音量：[サウンドと音楽](sonido.md) |
| `io/` | ファイル、コンテナー（.w3d、zip、pak）、メッシュ、リソース：[ファイル](archivos.md) |
| `base/` | ログ、保存される設定、すべてに共通する部分：[ログ](logs.md) |
| `input/` | タッチ、ゲームパッド、傾き、振動、クリップボード：[入力](entrada.md) |
| `net/` | ネットワークプレイ：[ネットワーク](red.md) |
| `physics/` | 物理、衝突、ヒットボックス、ラグドール、ナビゲーション：[物理](fisica.md) |
| `script/` | Lua：[Luaスクリプト](lua.md) |
| `math/` | ベクトル、行列、クォータニオン：[数学、シーン、フォーマット](formatos.md) |
| `objects/`、`animation/` | シーン：オブジェクト、メッシュ、マテリアル、ライト、カメラ、アニメーション：[数学、シーン、フォーマット](formatos.md) |
| `tools/` | ディスパッチの生成ツール、独立性テスト、パッカー |

# Coreを直接使う

CまたはC++でプログラミングをしていて、エディターを使わずにゲームを作りたい人向けです。必要なもの：

- **Coreのソース。** インクルードディレクトリは、Coreのルートと、`base/`、`graphics/`、`io/`、`thirdparty/`です。
- **`graphics/`のバックエンド**（そのシステム用のもの。たとえば`graphics/opengl/w3dGraphics.cpp`）と`base/w3dlog.cpp`。あとは、使うモジュール（オーディオ、ファイル、物理…）を加えます。
- **ウィンドウとそのコンテキスト。** サンプルはSDL2を使っています。シェーダーのバックエンドでは、コンテキストを作成したあとに`w3dEngine::GLES2Init(SDL_GL_GetProcAddress)`を1回呼ぶ必要があります。

最小限のコード（`simple_cube`のサンプルから）：

```cpp
gfx::Viewport(0, 0, w, h);
gfx::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
gfx::MatrixMode(gfx::Projection);  gfx::LoadIdentity();
gfx::Perspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
gfx::MatrixMode(gfx::ModelView);   gfx::LoadIdentity();
gfx::Translatef(0.0f, 0.0f, -5.0f);
gfx::Rotatef(angulo, 0.0f, 1.0f, 0.0f);
gfx::EnableArray(gfx::VertexArray); gfx::VertexPointer3f(0, cubeVerts);
gfx::EnableArray(gfx::NormalArray); gfx::NormalPointer3b(cubeNormals);
gfx::DrawTriangles(36, cubeIdx);
```

サンプルは[Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples)に順次追加しています：`simple_cube`（ライト1つと立方体）、`textured_cube`（`LoadTexture`で読み込んだテクスチャ付き）、`normal_map_cube`（DOT3による疑似的な凹凸）。そこには`game/w3drun`もあります。エディターで作ったゲーム（シーン、インターフェース、Lua）を、エディターなしで動かすランタイムです。

現状を率直に言うと：

- **Coreはまだ本番で使える状態ではありません。** 書き直しが続いていて、頻繁に変わります。
- **中にエディターのロジックが入っています**（Edit Mode、モディファイアー、レンダーモード）。これは取り除いている最中です。`tools/prueba_independencia.sh`で、単体で使えるようになるまでにあとどれだけ残っているかを測れます。
- **サンプルは、フォルダー構成が以前のバージョンのときに書かれたものです。** CMakeでは`w3dGraphics.cpp`がCoreのルートにあることになっていますが、今は`graphics/opengl/`にあります。パスの修正が必要になるかもしれません。
- **今後の予定：** アーマチュア、マップ用のBSP風のシステム、オプションの物理、レトロなバックエンドの追加、サンプルとドキュメントの充実。

質問は[Telegram](https://t.me/Whisk3D)のグループへどうぞ。

C++まで降りる必要がなければ、エディターを使うのが近道です。ゲームは[Lua](../lua/README.md)でプログラミングし、[Properties](../editor/propiedades.md)の**Game**カードで各システム向けにビルドします。内部では同じCoreを使っています。

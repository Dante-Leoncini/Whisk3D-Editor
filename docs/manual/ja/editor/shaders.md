# 自作シェーダー

**自作シェーダー**は、マテリアルをピクセルごとにどう描くかを決めるテキストファイル（`.glsl`）です。通常のマテリアルにはない効果に使います：後ろのものがぼやけて見えるすりガラス、揺れる水、陽炎、アニメ調の塗り、脈打つ光など。

書くのは自分の部分だけです：`Fragmento()` 関数（各ピクセルの色）と、必要なら `Vertice()`（頂点を動かす）。それ以外はすべてWhisk3Dが用意します。

## 動作する環境

同じファイルが、すべてのグラフィックバックエンドで同じように動きます：

| バックエンド | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0：Android、ブラウザー（WebGL）、Nokia N8、**Graphics → OpenGL ES 2.0** にしたPC | GPUで実行 | GPUで実行 |
| Vulkan（PC） | GPUで実行 | GPUで実行 |
| PCのOpenGL（初期設定のもの）、OpenGL ES 1.1（Nokia N95）、Nintendo 3DS、Dreamcast | **CPUで実行** | 非対応：マテリアルはいつもどおり描かれます |

シェーダーのないバックエンドでも何も壊れません：**`@vertice` はCPUで実行され**（波打つ水はN95でも同じように動きます）、色は通常のマテリアルから来ます。Materialカードにその旨が表示されます：*"This graphics backend has no shaders: the @vertice section runs on the CPU and the rest of the material is drawn as usual"*（このグラフィックバックエンドにはシェーダーがありません：@verticeセクションはCPUで実行され、マテリアルの残りはいつもどおり描かれます）。

### CPUでの `@vertice`

シェーダーがない環境のために、Whisk3Dにはインタープリターが入っています。`@vertice` の **同じテキスト** を読み、描画の前に頂点ごとに実行します。違うものを書く必要はありません。頂点を動かすのに使うものは理解します：

- `float`、`int`、`bool`、`vec2`、`vec3`、`vec4`。定数（`const float VELOCIDAD = 2.0;`）と自作の関数（`in`、`out`、`inout` 付き）
- `if` / `else`、`for`（`break` と `continue` 付き）、`return`、三項演算子 `? :`
- 演算子、成分（`pos.y`、`n.xz`。書き込みにも使えます）、コンストラクター（`vec3(...)`）
- GLSLの数値関数：`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

行列、配列、`while`、`#define` は理解しません：使うとエラーが行番号付きで知らせます（GPUなら動きます）。1フレームに1回、メッシュのすべての頂点に対して実行されます：N95では小さなメッシュ（サンプルの水のように数百頂点程度）で使うのがよいでしょう。メッシュの **すべて** のパーツが同じシェーダーを使っている場合に動作します。そうでなければ、メッシュは動かさずに描かれます。

> **同じファイルがどこでも動く理由：** ファイルには特定のバックエンドに固有のものが何もありません。各バックエンドが自分の部分（`w3d_*` の入力、精度、`main`）を先頭に付け足します。OpenGL ES 2.0はそのままコンパイルし、Vulkanは内部でSPIR-Vに変換します。将来Whisk3Dに別のバックエンドが加わっても、あなたのシェーダーはそのまま使えます。

## マテリアルでシェーダーを使う

1. オブジェクトを選び、Propertiesの **Material** タブを開きます。
2. カードの一番下にある **Advanced: custom shader** にチェックを入れます。マテリアルにまだファイルがなければ、テンプレートから新しいファイル（`shader.glsl`）が作られ、すぐに使われ、IDEで開きます。
3. **Shader** でファイルを選びます：
   - プロジェクトにすでにある `.glsl`
   - **New shader**：テンプレート入りの別のファイル
   - **Load from disk...**：パソコン上の `.glsl`。テクスチャやスクリプトと同じく、プロジェクトの中にコピーされます。
4. **Shader** の右にある小さなボタンで、ファイルが **IDE** で開きます（レイアウトにIDEが必要です）。
5. 書いて **Ctrl+S** を押すと、保存してコンパイルします。マテリアルまたはIDEのバーにある **Compile** を押してもかまいません（ファイルが `.glsl` のとき、ファイル名の横に表示されます）。
6. **Compile** の下に結果が出ます：**Compiled**、またはエラーが行番号付きで赤く表示されます。

始めるのにプロジェクトを保存しておく必要はありません。まだ保存していない場合、シェーダーはエディターの作業フォルダーに書き込まれます。プロジェクトを保存すると `.w3d` の中の `shaders/` フォルダーに入り、マテリアルとIDEはそのコピーを使うようになります。それ以降はプロジェクトと一緒に移動します（スマートフォンへ、ビルドしたゲームへ、別のパソコンへ）。チェックを外すとマテリアルはいつもどおりの描画に戻り、ファイルは選んだままになります。

シェーダーがコンパイルできない場合、直すまでオブジェクトは通常のマテリアルで描かれます。エディターは落ちません。

### Libraryから：シェーダーをメッシュにドラッグする

プロジェクトのシェーダーは **Library**（LibraryモードのOutliner）の **Shaders** に、使っているオブジェクトの数とともに表示されます。そこから次のことができます：

- 3D Viewportで **メッシュの上にドラッグ** します：動かしている間、ポインターの下にある **メッシュのパーツ** がそのシェーダーで描かれるので、どのパーツに適用するかがわかります。別のパーツに移ったりメッシュから外れたりすると、元のマテリアルに戻ります。ドロップすると、そのパーツのマテリアルがシェーダーを使うようになります（デフォルトマテリアルの場合は、パーツに新しいマテリアルが割り当てられます）。**Ctrl+Z** で元に戻せます。
- **マテリアル** でも同じです：ドラッグしている間、パーツがそのマテリアルで表示されます。
- スクリプトと同じように、**Open in IDE**、名前の変更、フォルダーの移動、`.w3d` の外への書き出しができます。
- **パソコンのファイルエクスプローラーから** ウィンドウにドラッグした `.glsl` はプロジェクトに取り込まれます。メッシュの上でドロップすれば、そのパーツにも適用されます。

## ファイルの形

```glsl
// コメントは // または /* */ で書きます

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // 任意：頂点を動かす
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** はピクセルのコードの始まりを、**`@vertice`** は頂点のコードの始まりを示します。どちらも任意です：印のないファイルは全体がフラグメントになります。
- **`vec4 Fragmento()`** は必須です：ピクセルの色（赤、緑、青、アルファ。0〜1）を返します。
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** は任意です。頂点の位置と法線を **オブジェクト** 空間（Yが上、単位はメートル）で受け取り、変更できます。
- 自分の関数や定数（`const float VELOCIDAD = 2.0;`）を、使う場所より上に書けます。
- 言語は **GLSL ES 1.00**（WebGL 1と同じ）です。`float`、`vec2/3/4`、`mat3/4`、`sin`、`cos`、`mix`、`clamp`、`pow`、`dot`、`normalize`、`reflect`、`refract`、`fract`、`smoothstep` など、GLSLの機能が使えます。
- `main` は書か**ない**でください。`uniform`、`attribute`、`varying`、`precision` も宣言しません：それらはWhisk3Dが付けます。
- エラーは **あなたのファイル** の行番号を示します（たとえば `0(4)` や `w3d.frag:4` は4行目）。

## 使えるもの

### `Fragmento()` の中

| 名前 | 型 | 内容 |
|---|---|---|
| `w3d_uv` | `vec2` | その点のUV座標（最初のUVレイヤー）。 |
| `w3d_Normal()` | `vec3` | その点の法線。正規化済みで、カメラ空間。裏面が見えているときは反対を向きます。 |
| `w3d_normal` | `vec3` | 正規化も反転もしていない法線。 |
| `w3d_posVista` | `vec3` | カメラ空間でのその点の位置（カメラは0,0,0にあり、−Zの方向を向いています）。 |
| `w3d_HaciaCamara()` | `vec3` | その点からカメラへの方向。 |
| `w3d_HaciaLuz()` | `vec3` | その点から最初のライトへの方向。 |
| `w3d_Luz(n)` | `vec3` | 法線 `n` で届く光：環境光と最初のライト（拡散）の合計。 |
| `w3d_luzColor` | `vec4` | 最初のライトの色（強さ込み）。 |
| `w3d_luzPos` | `vec4` | カメラ空間での最初のライトの位置（`w` = 0：方向、つまり太陽）。 |
| `w3d_ambiente` | `vec4` | 環境光。 |
| `w3d_color` | `vec4` | マテリアルの **Color**（アルファ込み）。 |
| `w3d_emision` | `vec4` | マテリアルの **Emission**。 |
| `w3d_Textura(uv)` | `vec4` | そのUVでのマテリアルのテクスチャ（なければ白）。`v` は画像の **上** から始まります（0 = 一番上の行、1 = 一番下の行）。 |
| `w3d_colorVertice` | `vec4` | 頂点カラー（メッシュにあり、マテリアルが **Vertex color** を使う場合）。 |
| `w3d_rugosidad`、`w3d_metalico` | `float` | マテリアルの **Roughness** と **Metallic**。 |
| `w3d_ior`、`w3d_transmision` | `float` | マテリアルの **IOR** と **Transmission**（レイトレーシング用の値）。 |
| `w3d_tiempo` | `float` | 秒数。アニメーション用です。エディターでは **タイムライン** の時間（現在のフレーム ÷ FPS）です：一時停止中は止まり、**Play** で進み、タイムラインをドラッグすれば任意の瞬間を確認できます。書き出したゲームでは起動からの時間です。 |
| `w3d_PuntoPantalla()` | `vec2` | ビュー内でのピクセルの位置：(0,0) が左下、(1,1) が右上。 |
| `w3d_Pixel()` | `vec2` | 同じ単位でのピクセル1つ分の大きさ。 |
| `w3d_Pantalla(p)` | `vec4` | ビューの点 `p` に **すでに描かれているもの**。下を参照。 |
| `w3d_AMundo(v)` | `vec3` | 方向をカメラ空間から **ワールド** 空間（Yが上）に変換します。カメラと一緒に回らない環境マップを読むのに使います（*環境の反射* を参照）。 |

### `Vertice()` の中

`pos` と `normal`（オブジェクトのもの）、そして `w3d_tiempo`。

## 後ろを見る：`w3d_Pantalla`

`w3d_Pantalla` で画面を読むマテリアルは、オブジェクトの後ろにあるものを見せたり、ずらしたり、ぼかしたり、色を付けたりできます。ガラス、水、熱い空気、フォースフィールドはこうして作ります。

- 画面を読むシェーダーを持つメッシュは、後ろのものが描き終わっているように、シーンの残りのあと、**最後に** 描かれます。Outlinerでの位置は関係ありません。
- それぞれの前に、ビューがテクスチャにコピーされます（メッシュごと、メッシュのパーツごとに1回）。PCや最近のスマートフォンなら軽い処理です。古いスマートフォンでは、こうしたオブジェクトは少なめにしましょう。
- **パーティクル** はあとで描かれるので、ガラス越しには見えません。
- マテリアルは **Transparent** に**しない**ほうがよいです：シェーダーがすでに後ろのものを混ぜ、アルファ1を返すからです。

## 例

これらはすべて、リポジトリの **`examples/shaders/`** にファイルとしてあり、テクスチャは `examples/shaders/texturas/` にあります。また、プロジェクト **`examples/shaders/Ejemplos_Shaders.w3d`** でオブジェクトに適用されています：開いて試してみてください。`.glsl` を自分のプロジェクトにドラッグすることもできます。

| ファイル | 内容 |
|---|---|
| `vidrio_esmerilado.glsl` | どんな形にも使えるガラス：後ろのものがぼやけて曲がって見えます。 |
| `vidrio_plano.glsl` | **平面** 用のすりガラス（窓、ガラスの仕切りなど）。テクスチャでどこをすりガラスにするかを決めます。 |
| `reflejo_entorno.glsl` | 周囲のパノラマ写真（テクスチャ）を映す金属やプラスチック。 |
| `reflejo_matcap.glsl` | 最も軽い反射：球を描いたテクスチャ1枚。 |
| `latido.glsl` | 脈打つ光（`w3d_tiempo`）。 |
| `dibujo_animado.glsl` | 3階調のライティング。 |
| `agua.glsl` | `@vertice` による波と、ずらした下の景色。 |
| `bandera.glsl` | はためく旗（`@vertice` のみ：N95でも動きます）。 |
| `respirar.glsl` | 法線に沿ってふくらんだりしぼんだりします（`@vertice` のみ）。 |
| `torcer.glsl` | 垂直軸のまわりにねじれます（`@vertice` のみ）。 |
| `saltar.glsl` | 床に当たると跳ねてつぶれます（`@vertice` のみ）。 |

頂点の例は、N95向けに作った **`examples/shaders/Ejemplos_Vertex_N95.w3d`** の小さなメッシュに適用されています（N95では `@vertice` はCPUで実行されます）。タイムラインの **Play** に合わせて動きます。

### 脈打つ色

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

`w3d_tiempo` を使うシェーダーは、タイムラインの **Play** に合わせて動きます（アニメーションと同じ）。一時停止中は止まったままで、エディターが余計な再描画をすることはありません。書き出したゲームでは自動で動きます。

### アニメ調（3階調）

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### 揺れる水（`@vertice`）

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // 下にあるものを少しずらす
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

波が見えるには、平面に頂点が必要です：Edit Modeで細分化してください。

### すりガラス

`examples/shaders/vidrio_esmerilado.glsl` です（サンプルプロジェクトのガラス球）。後ろのものがぼやけて、少し曲がって見えます：

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // 屈折：法線に応じてずらした位置の画面を読む
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // ぼかし：周囲13か所の読み取りの平均。ラフネスで円が広がる
    vec2 r = w3d_Pixel() * (1.0 + 18.0 * w3d_rugosidad);
    vec3 c = w3d_Pantalla(p).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 2.5,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  2.5)).rgb;
    c += w3d_Pantalla(p + r * vec2(-2.5, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0, -2.5)).rgb;
    c /= 13.0;

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // ガラスの色で色付け
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // ライトのハイライトと明るい縁
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

マテリアルから調整します：**Transmission**（0 = マテリアルの色の不透明なガラス、1 = 後ろがすべて見える）、**IOR**（後ろのものがどれだけ曲がるか）、**Roughness**（どれだけぼやけるか）。これらは **レイトレーシング** が使うのと同じ値なので、Render Previewでは同じオブジェクトが本物のガラスになります。

### 平面のすりガラス

**平面** 用です：窓、ガラスの仕切り、ガラスのスクリーンなど。`examples/shaders/vidrio_plano.glsl` です。後ろのものを2つのリング上で17回読み取って平均します。**Roughness** でリングが広がります。マテリアルの **テクスチャ** で、*どこ* をすりガラスにするかを決めます：白 = ぼかす、黒 = 透明。`texturas/escarcha.png` を使うと、くもり、小さなきれいな窓、流れ落ちる水滴ができます。テクスチャがなければガラス全体がすりガラスになります。両面から見えるように、平面の **Culling** はオフにするとよいでしょう。

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // この点をどれだけぼかすか
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // 読み取りの半径
```

### 環境の反射（反射にテクスチャを使う方法）

`examples/shaders/reflejo_entorno.glsl` です。**マテリアルのテクスチャ** は、オブジェクトの周囲を写した **パノラマ** 写真です：幅が高さの2倍で、周囲360°、上下180°を覆う *正距円筒図法*（equirectangular）の画像です（`texturas/entorno.png` のように、空、太陽、建物、市松模様の床）。各ピクセルで：

1. `reflect(-ojo, n)` が、カメラ空間で **視線が跳ね返る** 方向を求めます。
2. `w3d_AMundo(...)` がその方向を **ワールド** に変換するので、カメラが回っても反射は動きません。
3. `Panorama(...)` が方向を写真上の点に変換します：周囲の角度（`atan`）が `u`、上下の角度（`acos`）が `v` です。
4. **フレネル**（斜めから見るほど強く反射）と **Metallic**（金属はすべてを、自分の **Color** で色付けして反射）に応じて、表面と混ぜます。

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // 周囲の一周（0..1）
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // 上（0）から下（1）へ
    return vec2(u, v);
}

vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();
    vec3 r = reflect(-ojo, n);
    vec3 entorno = w3d_Textura(Panorama(normalize(w3d_AMundo(r)))).rgb;
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(n, ojo), 0.0), 5.0);
    float cuanto = mix(fresnel, 1.0, w3d_metalico);
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 reflejo = entorno * mix(vec3(1.0), w3d_color.rgb, w3d_metalico);
    return vec4(mix(superficie, reflejo, cuanto), 1.0);
}
```

2:1のパノラマ写真なら何でも使えます（スマートフォンの360°写真や、PNGに変換したHDRIなど）。

### Matcapの反射

`examples/shaders/reflejo_matcap.glsl` です：テクスチャの読み取りは1回だけなので、どんなGPUでも動きます。テクスチャは **描かれた球** です（`texturas/matcap.png`）。各ピクセルは、球の上で自分の法線と同じ方向を向いた点の色を取ります。反射はカメラに追従します（ワールドではありません）。

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // （テクスチャのvは上から始まる：yを反転）
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## ルーチンの中で：**Shader** ステップ

自作シェーダーは **GLのステート** のようなものです：設定すると、そのあとに描くものがそれを使います。**ルーチン**（Add → Draw → **Shader**）では、このステップに `.glsl` の **ファイル**、**On** のチェック、そして4つの数値があります：**IOR**、**Transmission**、**Roughness**、**Metallic**（`w3d_ior`、`w3d_transmision`、`w3d_rugosidad`、`w3d_metalico` として届きます。アニメーションさせたり、メモリーから読んだりできます）。

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← ここからはシェーダーなし
```

**On** のとき、続く "Draw elements" はシェーダーを使います。チェックを外すと解除されます。ルーチンが終わると自動で解除されます。**OpenGL ES 2.0とVulkan** で動作し、OpenGL ES 1.1と固定機能のGLではこのステップは何もしません（ルーチンの描画はメッシュのデータを直接参照するためです。CPUでの `@vertice` はマテリアル用です）。画面を読むシェーダー（`w3d_Pantalla`）は、ルーチンとシーンがそのステップより **前** に描いたものを見ます：順番は自分で決めます。

## レイトレーシングとの関係

レイトレーシングはあなたのシェーダーを実行しません。マテリアルの値を使います：**Color**、**Roughness**、**Metallic**、**Transmission**、**IOR**、そしてテクスチャ。シェーダーでも同じ値を使えば（すりガラスのように）、Material PreviewとRender Previewの見た目がそろいます。

**Transmission**（0〜1。レイトレーシングをオンにしたときのMaterialカードにあります）は、表面のどれだけがガラスかを表します：光は **IOR** で曲がって通り抜け、**Roughness** があればすりガラスのように出てきます。**アルファ** は別物です：アルファ（またはそのマップ）で透明な部分はそもそも存在せず、そこからは後ろがまっすぐ、曲がらずに見えます。アルファ50 %、transmission 1なら、半分はまっすぐ、もう半分は屈折して通ります。transmission 0では、以前のバージョンと同じく、IORがアルファ越しに見えるものを曲げます。

## （まだ）できないこと

- ライトは1つだけ使います：シーンの最初のライト（またはプレビューのライト）。
- **フォグ** と鏡の **クリッピング平面** は反映されません。
- **Solid** やデバッグ表示（法線、アルファ、ベクトル）では通常のマテリアルが表示されます。
- 独自のパラメーターはありません：マテリアルのもの（色、ラフネス、メタリック、transmission、IOR、エミッション）を使います。

## よくあるエラー

| メッセージ | 原因 |
|---|---|
| `undefined variable` / `undeclared identifier` | 名前の書き間違い、または存在しない名前（`w3d_Normal()` に括弧を付けましたか？）。 |
| `cannot convert` / `wrong operand types` | 型が混ざっています：`1` は `int` なので `1.0` と書きます。`vec3` と `vec4` は足せません。 |
| `Fragmento: no matching overloaded function`、または `Fragmento` がないとコンパイルできない | `vec4 Fragmento()` 関数がないか、別の型を返しています。 |
| `redefinition of main` | `main` は書かないでください：Whisk3Dが付けます。 |
| 後ろが見えない | シェーダーは `w3d_Pantalla` を使っているが、マテリアルが低いアルファで **Transparent** になっている、または後ろのものがあとで描かれている（パーティクル）。 |

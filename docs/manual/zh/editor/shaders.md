# 手写着色器

**手写着色器**是一个文本文件（`.glsl`），逐像素决定材质如何绘制。它用于普通材质做不到的效果：能模糊地透出后方物体的磨砂玻璃、流动的水、热浪扭曲、卡通渲染、跳动的光晕……

你只需写自己的部分：一个 `Fragmento()` 函数（每个像素的颜色），如果需要，再写一个 `Vertice()`（移动顶点）。其余的一切都由 Whisk3D 补上。

## 在哪里可用

同一个文件在所有图形后端上表现一致：

| 后端 | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0：Android、浏览器（WebGL）、Nokia N8、设置为 **Graphics → OpenGL ES 2.0** 的 PC | 在 GPU 上 | 在 GPU 上 |
| Vulkan（PC） | 在 GPU 上 | 在 GPU 上 |
| PC 的 OpenGL（默认的那个）、OpenGL ES 1.1（Nokia N95）、Nintendo 3DS、Dreamcast | **在 CPU 上** | 不支持：材质照常绘制 |

在没有着色器的后端上不会出任何问题：**`@vertice` 在 CPU 上运行**（带波浪的水在 N95 上也一样会动），颜色来自普通材质。Material 卡片会提示：*"This graphics backend has no shaders: the @vertice section runs on the CPU and the rest of the material is drawn as usual"*（此图形后端没有着色器：@vertice 部分在 CPU 上运行，材质的其余部分照常绘制）。

### 在 CPU 上运行的 `@vertice`

在没有着色器的地方，Whisk3D 自带一个解释器，它读取 `@vertice` 的**同一段文本**，并在绘制前逐个顶点运行。不需要写任何不同的东西。它能理解移动顶点时用到的内容：

- `float`、`int`、`bool`、`vec2`、`vec3`、`vec4`；常量（`const float VELOCIDAD = 2.0;`）和自定义函数（可用 `in`、`out`、`inout`）；
- `if` / `else`、`for`（可用 `break` 和 `continue`）、`return`、三元运算符 `? :`；
- 运算符、分量（`pos.y`、`n.xz`，也可用于写入）和构造函数（`vec3(...)`）；
- GLSL 的数值函数：`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`；
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

它不支持矩阵、数组、`while` 和 `#define`：如果用了，错误信息会指出并给出行号（在 GPU 上则可以正常运行）。它每帧对网格的所有顶点运行一次：在 N95 上最好只用于小网格（几百个顶点，比如示例中的水）。只有当网格的**所有**部分都使用同一个着色器时才有效；否则网格会不加移动地绘制。

> **为什么同一个文件到处都能用：** 文件里没有任何某个后端特有的内容。每个后端都会在它前面加上自己的部分（`w3d_*` 输入、精度、`main`）。OpenGL ES 2.0 直接编译它；Vulkan 在内部把它转换为 SPIR-V。即使将来 Whisk3D 再增加一个后端，你的着色器也照样能用。

## 在材质中使用着色器

1. 选择对象，打开 Properties 的 **Material** 选项卡。
2. 在卡片最底部勾选 **Advanced: custom shader**。如果材质还没有文件，就会用模板新建一个（`shader.glsl`），立即投入使用，并在 IDE 中打开。
3. 在 **Shader** 中选择文件：
   - 项目中已有的 `.glsl`；
   - **New shader**：用模板新建另一个文件；
   - **Load from disk...**：你电脑上的某个 `.glsl`。它会像纹理和脚本一样被复制到项目内部。
4. **Shader** 右侧的小按钮会在 **IDE** 中打开该文件（布局中需要有一个 IDE）。
5. 编写代码，按 **Ctrl+S** 即可保存并编译。也可以点击材质中或 IDE 工具栏上的 **Compile**（文件是 `.glsl` 时，它会出现在文件名旁边）。
6. **Compile** 下方会显示结果：**Compiled**，或者红色的错误信息及其行号。

开始之前不必先保存项目。如果项目还没保存，着色器会写入编辑器的一个工作文件夹；保存项目时，它会进入 `.w3d` 内部的 `shaders/` 文件夹，材质和 IDE 也会改用这份副本。从此它随项目一起移动（到手机上、到编译好的游戏里、到另一台电脑上）。取消勾选后，材质恢复照常绘制；文件仍保持选中。

如果着色器编译失败，在你修好之前对象会用普通材质绘制。编辑器不会崩溃。

### 从 Library：把着色器拖到网格上

项目中的着色器会显示在 **Library**（Library 模式下的 Outliner）的 **Shaders** 中，并标出有多少对象在使用。在那里可以：

- 在 3D Viewport 中**把它拖到网格上**：移动时，指针下方的**网格部分**会用该着色器绘制，这样你能看到要应用到哪个部分。移到另一个部分或离开网格时，它会恢复原来的材质。松开后，该部分的材质就会使用这个着色器（如果是默认材质，该部分会得到一个新材质）。可用 **Ctrl+Z** 撤销。
- **材质**也一样：拖动时，该部分会以它显示。
- 像脚本一样，可以 **Open in IDE**、重命名、移到其他文件夹，或把它移出 `.w3d`。
- 从**你电脑的文件管理器**拖到窗口中的 `.glsl` 会加入项目；如果把它放到网格上，还会应用到该部分。

## 文件的样子

```glsl
// 注释用 // 或 /* */

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // 可选：移动顶点
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** 标记像素代码的开始，**`@vertice`** 标记顶点代码的开始。两者都是可选的：没有标记的文件全部视为片元代码。
- **`vec4 Fragmento()`** 是必需的：它返回像素的颜色（红、绿、蓝、alpha；范围 0 到 1）。
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** 是可选的。它接收顶点在**对象**空间中的位置和法线（Y 轴向上，单位为米），你可以修改它们。
- 你可以编写自己的函数和常量（`const float VELOCIDAD = 2.0;`），写在使用它们的位置之前。
- 语言是 **GLSL ES 1.00**，即 WebGL 1 所用的版本。你可以使用 `float`、`vec2/3/4`、`mat3/4`、`sin`、`cos`、`mix`、`clamp`、`pow`、`dot`、`normalize`、`reflect`、`refract`、`fract`、`smoothstep` 以及 GLSL 的其余功能。
- **不要**写 `main`，也不要声明 `uniform`、`attribute`、`varying` 或 `precision`：这些由 Whisk3D 补上。
- 错误信息给出的是**你的文件**中的行号（例如 `0(4)` 或 `w3d.frag:4` 指第 4 行）。

## 可以使用的内容

### 在 `Fragmento()` 中

| 名称 | 类型 | 含义 |
|---|---|---|
| `w3d_uv` | `vec2` | 该点的 UV 坐标（来自第一个 UV 层）。 |
| `w3d_Normal()` | `vec3` | 该点的法线，已归一化，位于相机空间。如果看到的是背面，它会指向另一侧。 |
| `w3d_normal` | `vec3` | 未归一化、未翻转的法线。 |
| `w3d_posVista` | `vec3` | 该点在相机空间中的位置（相机位于 0,0,0，朝向 −Z）。 |
| `w3d_HaciaCamara()` | `vec3` | 从该点指向相机的方向。 |
| `w3d_HaciaLuz()` | `vec3` | 从该点指向第一个灯光的方向。 |
| `w3d_Luz(n)` | `vec3` | 法线为 `n` 时照到的光：环境光加上第一个灯光（漫反射）。 |
| `w3d_luzColor` | `vec4` | 第一个灯光的颜色（含强度）。 |
| `w3d_luzPos` | `vec4` | 第一个灯光在相机空间中的位置（`w` = 0：表示方向，即太阳光）。 |
| `w3d_ambiente` | `vec4` | 环境光。 |
| `w3d_color` | `vec4` | 材质的 **Color**（含 alpha）。 |
| `w3d_emision` | `vec4` | 材质的 **Emission**。 |
| `w3d_Textura(uv)` | `vec4` | 材质纹理在这些 UV 处的颜色（没有纹理时为白色）。`v` 从图像**顶部**开始（0 = 最上面一行，1 = 最下面一行）。 |
| `w3d_colorVertice` | `vec4` | 顶点颜色（网格有顶点颜色且材质使用 **Vertex color** 时）。 |
| `w3d_rugosidad`、`w3d_metalico` | `float` | 材质的 **Roughness** 和 **Metallic**。 |
| `w3d_ior`、`w3d_transmision` | `float` | 材质的 **IOR** 和 **Transmission**（光线追踪所用的值）。 |
| `w3d_tiempo` | `float` | 秒数，用于制作动画。在编辑器中它是**时间轴**的时间（当前帧 ÷ FPS）：暂停时保持不变，按 **Play** 时向前推进，拖动时间轴可以查看任意时刻。在导出的游戏中，它是游戏启动以来的时间。 |
| `w3d_PuntoPantalla()` | `vec2` | 像素在视图中的位置：(0,0) 为左下角，(1,1) 为右上角。 |
| `w3d_Pixel()` | `vec2` | 一个像素在同一单位下的大小。 |
| `w3d_Pantalla(p)` | `vec4` | 视图中点 `p` 处**已经绘制的内容**。见下文。 |
| `w3d_AMundo(v)` | `vec3` | 把方向从相机空间转换到**世界**空间（Y 轴向上）。用于读取不随相机旋转的环境贴图（见 *环境反射*）。 |

### 在 `Vertice()` 中

`pos` 和 `normal`（对象的），以及 `w3d_tiempo`。

## 看到后方：`w3d_Pantalla`

用 `w3d_Pantalla` 读取屏幕的材质可以显示对象后方的内容，并对其进行偏移、模糊或着色。玻璃、水、热空气或力场就是这样做出来的。

- 带有读取屏幕的着色器的网格会在场景其余部分之后**最后**绘制，这样后方的内容已经画好了。它们在 Outliner 中的位置无关紧要。
- 绘制每一个之前，视图都会被复制到一张纹理中（每个网格、每个网格部分各复制一次）。在 PC 或较新的手机上开销很小。在旧手机上，这类对象最好少用。
- **粒子**在之后才绘制，所以透过玻璃看不到它们。
- 材质最好**不要**设为 **Transparent**：着色器已经混合了后方内容，并返回 alpha 1。

## 示例

这些示例都以文件形式放在仓库的 **`examples/shaders/`** 中，纹理在 `examples/shaders/texturas/` 中，并在项目 **`examples/shaders/Ejemplos_Shaders.w3d`** 中应用到了对象上：打开它试试吧。你也可以把 `.glsl` 拖到自己的项目中。

| 文件 | 作用 |
|---|---|
| `vidrio_esmerilado.glsl` | 适用于任何形状的玻璃：后方内容模糊且弯折。 |
| `vidrio_plano.glsl` | 用于**平面**的磨砂玻璃（窗户、玻璃隔断）；由纹理决定哪里是磨砂的。 |
| `reflejo_entorno.glsl` | 反射周围环境全景照片（纹理）的金属或塑料。 |
| `reflejo_matcap.glsl` | 最省资源的反射：一张画有球体的纹理。 |
| `latido.glsl` | 跳动的光晕（`w3d_tiempo`）。 |
| `dibujo_animado.glsl` | 三阶色调的光照。 |
| `agua.glsl` | 用 `@vertice` 做的波浪，并偏移下方内容。 |
| `bandera.glsl` | 一面飘扬的旗帜（仅 `@vertice`：在 N95 上也能运行）。 |
| `respirar.glsl` | 沿着法线膨胀和收缩（仅 `@vertice`）。 |
| `torcer.glsl` | 绕竖直轴扭转（仅 `@vertice`）。 |
| `saltar.glsl` | 落地时弹跳并被压扁（仅 `@vertice`）。 |

这些顶点示例应用在 **`examples/shaders/Ejemplos_Vertex_N95.w3d`** 中的小网格上，该项目是为 N95 准备的（在那里 `@vertice` 在 CPU 上运行）。它们随时间轴的 **Play** 而动。

### 跳动的颜色

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

使用 `w3d_tiempo` 的着色器会随时间轴的 **Play** 而动（就像动画一样）；暂停时保持静止，编辑器也不会多余地重绘。在导出的游戏中它会自己动起来。

### 卡通（三阶色调）

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### 流动的水（`@vertice`）

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // 下方的内容，稍微偏移一点
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

要让波浪明显，平面需要足够的顶点：在 Edit Mode 中细分它。

### 磨砂玻璃

这是 `examples/shaders/vidrio_esmerilado.glsl`（示例项目中的玻璃球）。后方的内容看起来模糊且略微弯折：

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // 折射：按法线偏移后读取屏幕
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // 模糊：周围 13 次读取的平均值；粗糙度会扩大采样圆
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

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // 用玻璃的颜色染色
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // 灯光的高光和更亮的边缘
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

它通过材质来控制：**Transmission**（0 = 材质颜色的不透明玻璃，1 = 后方全部可见）、**IOR**（后方内容弯折的程度）和 **Roughness**（模糊的程度）。这些与**光线追踪**使用的值相同，所以在 Render Preview 中同一个对象就是真正的玻璃。

### 平面磨砂玻璃

用于**平面**：窗户、玻璃隔断、玻璃屏幕。这是 `examples/shaders/vidrio_plano.glsl`。它在两个圆环上对后方内容读取 17 次并取平均；**Roughness** 会扩大圆环。材质的**纹理**决定*哪里*是磨砂的：白色 = 模糊，黑色 = 清晰。使用 `texturas/escarcha.png` 会出现污痕、一个干净的小窗口和向下流的水滴；没有纹理时，整块玻璃都是磨砂的。最好关闭平面的 **Culling**，这样两面都能看到。

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // 这个点模糊的程度
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // 读取的半径
```

### 环境反射（如何用纹理做反射）

这是 `examples/shaders/reflejo_entorno.glsl`。**材质的纹理**是一张对象周围环境的**全景**照片：一张*等距柱状投影*（equirectangular）图像，宽是高的两倍，水平覆盖 360°，从上到下覆盖 180°（例如 `texturas/entorno.png`：天空、太阳、建筑和棋盘格地面）。对每个像素：

1. `reflect(-ojo, n)` 给出视线在相机空间中**反弹的方向**。
2. `w3d_AMundo(...)` 把该方向转换到**世界**空间，这样即使相机旋转，反射也保持不动。
3. `Panorama(...)` 把方向转换为照片上的一个点：水平角度（`atan`）是 `u`，从上到下的角度（`acos`）是 `v`。
4. 根据**菲涅耳**（掠射角处反射更强）和 **Metallic**（金属反射一切，并带上自身的 **Color**）与表面混合。

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // 水平一圈（0..1）
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // 从上（0）到下（1）
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

任何 2:1 的全景照片都可以（手机拍的 360° 照片，或转换为 PNG 的 HDRI）。

### Matcap 反射

这是 `examples/shaders/reflejo_matcap.glsl`：只读取一次纹理，在任何 GPU 上都能运行。纹理是一个**画好的球体**（`texturas/matcap.png`）；每个像素取球体上与其法线朝向相同的点的颜色。反射跟随相机（而不是世界）。

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // （纹理的 v 从顶部开始：y 取反）
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## 在例程中：**Shader** 步骤

手写着色器就像一个 **GL 状态**：设置之后，之后绘制的内容都会使用它。在**例程**中（Add → Draw → **Shader**），这个步骤包含 `.glsl` **文件**、**On** 复选框和四个数值：**IOR**、**Transmission**、**Roughness** 和 **Metallic**（它们以 `w3d_ior`、`w3d_transmision`、`w3d_rugosidad`、`w3d_metalico` 传入；可以做动画，也可以从内存中读取）。

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← 此后不再使用着色器
```

勾选 **On** 时，后面的 "Draw elements" 会使用该着色器；取消勾选即移除。例程结束时会自动移除。它在 **OpenGL ES 2.0 和 Vulkan** 上可用；在 OpenGL ES 1.1 和固定管线 GL 上该步骤不起作用（例程的绘制直接指向网格数据：CPU 上的 `@vertice` 是给材质用的）。读取屏幕的着色器（`w3d_Pantalla`）能看到例程和场景在该步骤**之前**绘制的内容：顺序由你决定。

## 与光线追踪的关系

光线追踪不会运行你的着色器。它使用材质的值：**Color**、**Roughness**、**Metallic**、**Transmission**、**IOR** 和纹理。如果着色器中也使用这些值（像磨砂玻璃那样），Material Preview 和 Render Preview 的效果就是一致的。

**Transmission**（0 到 1，在开启光线追踪时的 Material 卡片中）表示表面有多少是玻璃：光线按 **IOR** 弯折后穿过，配合 **Roughness** 会呈现磨砂效果。**alpha** 则是另一回事：因 alpha（或其贴图）而透明的部分相当于不存在，透过它看到的后方内容是笔直的、不弯折的。alpha 为 50 %、transmission 为 1 时，一半直接穿过，另一半发生折射。transmission 为 0 时，IOR 会像以前的版本一样，弯折透过 alpha 看到的内容。

## （目前）还不支持的功能

- 只使用一个灯光：场景中的第一个灯光（或预览中的灯光）。
- 不包含**雾**，也不包含镜子的**裁剪平面**。
- 在 **Solid** 和调试视图（法线、alpha、矢量）中显示的是普通材质。
- 没有自己的参数：使用材质的参数（颜色、粗糙度、金属度、transmission、IOR、自发光）。

## 常见错误

| 消息 | 原因 |
|---|---|
| `undefined variable` / `undeclared identifier` | 名称拼写错误或不存在（`w3d_Normal()` 是否带了括号？）。 |
| `cannot convert` / `wrong operand types` | 类型混用：`1` 是 `int`，要写 `1.0`。`vec3` 不能和 `vec4` 相加。 |
| `Fragmento: no matching overloaded function`，或没有 `Fragmento` 就无法编译 | 缺少 `vec4 Fragmento()` 函数，或它返回了其他类型。 |
| `redefinition of main` | 不要写 `main`：它由 Whisk3D 补上。 |
| 看不到后方内容 | 着色器使用了 `w3d_Pantalla`，但材质设为 **Transparent** 且 alpha 较低，或者后方内容是之后才绘制的（粒子）。 |

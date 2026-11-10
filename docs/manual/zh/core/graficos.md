# 图形

## API

所有绘制都通过 `w3dEngine` 的函数（`graphics/w3dGraphics.h`）：开启和关闭功能（`Enable`、`Disable`）、矩阵（`MatrixMode`、`PushMatrix`、`Translatef`、`Perspective`）、顶点数组（`VertexPointer3f`、`NormalPointer3b`、`TexCoordPointer2f`）、GPU 缓冲区（`GenBuffer`、`ArrayBufferData`）、绘制（`DrawTriangles`、`DrawLines`、`DrawPoints`）、材质（`Material`）、灯光（`LightNfv`、`LightNSpot`）、雾、混合、矩形裁剪、模板和裁剪平面。

它刻意设计得像经典 OpenGL：这是最多系统能理解的绘制方式，从 N95 到 Dreamcast 都是如此。

## 固定管线和着色器

GPU 分为两类：

- **固定管线。** GPU 出厂时就带有组装好的各个步骤：变换顶点、逐顶点计算光照、组合纹理、加雾。程序只需打开开关、传入数值。OpenGL ES 1.1（N95）、PC 上的经典 OpenGL、PSP 的 GE 和 Dreamcast 的 PVR 都是这样工作的。
- **着色器。** GPU 不带任何组装好的东西：每一步都是在 GPU 上运行的小程序。OpenGL ES 2.0、WebGL 和 Vulkan 都是这样工作的。

Core 说的是固定管线的语言。着色器后端用一个“uber-shader”来**模拟**它，做与 OpenGL ES 1.1 相同的计算（最多 8 盏灯的逐顶点光照、衰减、光锥、雾、两张纹理的混合）。所以浏览器和 Android 上的画面与 PC 和 N95 上一致。

## 后端

| 后端 | 是什么 | 用在哪里 |
|---|---|---|
| `opengl/` | 固定管线：桌面 OpenGL、OpenGL ES 1.1、GLdc、vitaGL | Linux、Windows、macOS、N95 和 S60、Dreamcast、PS Vita |
| `gles2/` | 着色器：OpenGL ES 2.0 和 WebGL | Android、浏览器、Nokia N8（Symbian^3）、PC（可选） |
| `vulkan/` | 同一个着色器后端，编译在一个用 Vulkan 实现的 OpenGL ES 2.0 之上 | PC（可选） |
| `gu/` | PSP 的 GE | PSP |
| `citro3d/` | 3DS 的 PICA200 GPU | Nintendo 3DS |
| `pvr/` | 直接向 Dreamcast 的 PVR 绘制 | 供原生游戏使用的可选模块 |
| `video/` | 视频转纹理 | 可选：桌面上用 FFmpeg，浏览器中用 `<video>` |

## 如何选择

几乎总是在**编译时**选择：每个系统只编译一个后端，调用直接进行，没有任何额外开销。

例外是 PC（Windows 和 Linux）。在 PC 上，可执行文件同时带有 OpenGL、OpenGL ES 2.0 和 Vulkan，在**启动时**选择，位置是 **Properties → Settings → Graphics**（重启后生效；参见[其他功能](../editor/otras-funciones.md)）。每个后端都在自己的命名空间中编译（`w3dEngine::gl`、`w3dEngine::es2`、`w3dEngine::vk`），并把自己的函数填入一张**分派表**（`w3dBackendTabla.h`）。`w3dEngine` 的每个函数都会调用当前后端的对应函数：

```cpp
// graphics/w3dDespacho.inc，由 tools/generar_despacho.py 生成
void Enable(Cap c) { W3D_TABLA()->Enable(c); }
void DrawTriangles(int count, const MeshIndex* indices) { W3D_TABLA()->DrawTriangles(count, indices); }
```

如果选中的后端在那台机器上无法运行，就继续使用 OpenGL。Vulkan 需要驱动、GPU 以及放在可执行文件旁边的着色器编译器 `shaderc_shared`，在 32 位版本中默认关闭。在 macOS 上，可执行文件只带有 OpenGL。

## 一次 `DrawTriangles` 的旅程

同一个请求，最终会去到截然不同的地方：

![同一个 gfx::DrawTriangles 经过 Whisk3D Core，下行到各个后端：opengl（PC、macOS、N95、Dreamcast、PS Vita）、gles2（Android、浏览器、N8、PC）、vulkan（PC）、gu（PSP）和 citro3d（3DS）](img/whisk3d-core-despacho.webp)

- **固定管线 OpenGL（PC、N95）：** 几乎直接调用 `glDrawElements`。矩阵直到绘制前才发送，只用一次 `glLoadMatrixf`。
- **OpenGL ES 2.0 和 WebGL：** 矩阵栈保存在 CPU 上，以 uniform 的形式传给 uber-shader。WebGL 不接受内存中的数组，所以后端会把它们放进缓冲区。在 Android 2.3 的 VideoCore IV GPU 上，它会为每种状态组合生成一个小程序，并合并界面的绘制：每帧从 179 次减少到 16 次。
- **PSP（GE）：** CPU 把每个顶点变换到裁剪空间并在那里裁剪，因为 GE 不会针对屏幕两侧进行裁剪。然后把它们交错写入一块环形内存，由 GE 读取。光照（最多四盏）和雾由硬件完成。
- **3DS（PICA200）：** 顶点被复制到线性内存，由一个自己的顶点着色器（`w3d.v.pica`）变换。连续的、状态相同的小绘制会合并提交：每帧从 191 次减少到 18 次。线和点在 CPU 上组装成三角形。
- **Dreamcast：** GLdc 在 PVR 上充当 OpenGL 1.x，与 PC 和 N95 使用同一个固定管线后端，并带有自己的补丁。对于需要榨干每个周期的游戏，可以使用 `pvr/`：GLdc 每个顶点要花费 SH-4 大约 400 个周期，而直接绘制只用一条指令（`FTRV`）就能变换一个顶点。两者不能在同一个程序中共存。
- **PS Vita：** vitaGL 在主机的 GPU 上模拟固定管线，并在运行时编译它的着色器（因此需要 `libshacccg.suprx`）。

## 询问后端会做什么

不是所有 GPU 都能做同样的事，Core 也不隐瞒这一点：每个后端都会回答自己支持什么，如果缺了什么，绘制方就换一条路。

| 询问 | 回答“否”时会怎样 |
|---|---|
| `VBOSoportado()` | 网格以内存中的数组形式提交（Dreamcast、Vita） |
| `EstencilBits()` | 水面反射退回到用平面裁剪 |
| `PlanosRecorteMax()` | 反射改为不用裁剪平面来处理（PVR 没有裁剪平面） |
| `MezclaTexturasSoportada()` | 材质的各层分多遍绘制 |
| `TieneTexGen()` | 球面反射由软件计算，或用纹理矩阵计算（`TexMatrixMatcap`，在 N95 上可用） |
| `SombreadoPorPixelSoportado()` | 光照保持逐顶点，编辑器也不提供该选项 |
| `ShadersEscritos()` | 带自定义着色器的材质按常规方式绘制（参见[手写着色器](../editor/shaders.md)） |

## 状态缓存

每次状态改变都会给驱动带来一些开销。Core 会记住当前设置的状态：开启一个已经开启的功能，或者绑定一张已经绑定的纹理，都不会产生任何调用。`IsEnabled` 和 `GetMatrix` 直接从这份记录中回答，而不去询问 GPU，因为在 N95 上，这样的询问会迫使程序等待 GPU 画完。

## 纹理

`LoadTexture` 分两步。**解码**（从文件到 RGBA 像素）取决于系统：几乎所有系统都用 stb，Symbian 用 ICL。**上传**（从像素到 GPU）是共用的，每个后端会转换成自己芯片的格式：

- **PSP：** 2 的幂，最大 512，“swizzled”，格式为 565 或 8888。
- **3DS：** 按 Morton 顺序排列的 8x8 块，每边 8 到 1024。
- **Dreamcast：** 2 的幂，8 到 1024，16 位（565 或 4444），没有 mipmap。
- **N95：** 只支持 2 的幂；其他尺寸在上传前会重新采样。

项目可以要求使用 **16 位**纹理（内存减半），并且只在 N95 上，从某个尺寸起以**一半尺寸**上传（`texturasMitadDesde`）。`.w3dt` 格式保存的是已经可以直接交给 GPU 的纹理，连同 mipmap：加载它就是读取再上传。

坦白说一个细节：纹理上传和灯光仍然会直接调用两三个 OpenGL 函数。在没有 OpenGL 的主机上，后端带有一个小层来实现它们（`gu/w3dGLgu.cpp`、`citro3d/w3dGLc3d.cpp`）。

## 屏幕和安全区域

`PantallaSetTam` 和 `PantallaEsVertical` 只保存一次屏幕是竖着还是横着（横向 = 宽大于高），这样引擎的任何部分都可以查询。**安全区域**（`SafeAreaGet`）表示每条边有多少像素不能用于界面：刘海、手势条、手机浏览器的地址栏和工具栏。背景绘制在整个窗口中，界面绘制在安全区域内。在 PC、Symbian 和主机上，边距都是 0；在浏览器中会实际测量。

## 最小机器的限制

为 Core 编写的东西必须能放进最受限的机器。以下是会被察觉到的限制：

| 限制 | 在哪里 | Core 如何处理 |
|---|---|---|
| 16 位索引（每个网格最多 65,535 个顶点） | Symbian | `MeshIndex` 在那里是 16 位，在其他系统上是 32 位；导入器会拒绝放不下的内容 |
| 同时最多 8 盏灯 | 所有系统（`LucesMax`） | 着色器后端用相同的计算来模拟；PSP 用硬件处理 4 盏，Android 2.3 最多使用 4 盏 |
| 没有着色器 | 固定管线后端 | 着色器相关的函数回答“否”，材质按常规方式绘制 |
| 16 位 Z | PSP | 近裁剪面根据环绕距离调整，与 N95 上一样 |
| 一共 16 MB | Dreamcast | 用 `-Os` 编译，链接器会丢弃没用到的部分；纹理使用 16 位 |
| C++03 | Symbian | 头文件中不用 `nullptr`、`auto` 和 `override`（用 `W3D_OVERRIDE`）；`tools/prueba_independencia.sh` 以 C++03 单独编译每个头文件 |

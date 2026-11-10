# 卡片和属性

## 卡片

`GroupPropertie`（`Propieties/GroupPropertie.h`）是一张带标题和若干行的卡片。每一行都是一个属性，即一个继承自 `PropertieBase` 的类，指向你的一个变量：卡片显示这个变量，并就地修改它。

```cpp
GroupPropertie* jugador = new GroupPropertie("Jugador");
jugador->icono = IconType::gamepad;        // 可选：位于箭头和标题之间

PropFloat* vel = new PropFloat("Velocidad", " m/s");
vel->value = &velocidad;
jugador->properties.push_back(vel);

jugador->Resize(ancho, 0);    // 高度由卡片自己计算
jugador->Render();            // 在当前原点
```

卡片拥有它的各行：删除卡片时，也会删除这些行。

- 标题带有一个用于折叠的箭头（`open`）。设置 `flecha = false` 就没有箭头：用于浮动窗口。
- 下方分为两栏：左边是标签，右边是值。`anchoValores` 是值所占宽度的比例：默认为 0.5。
- `reservaKeyBtn = true` 会在最右边留出一栏，用于动画菱形。
- 几乎所有行的高度都是一行文字加一个 `gap`。没有变量（`value` 为 `NULL`）的数字、复选框或颜色不占位置，也不绘制。
- `visible = false` 隐藏整张卡片。

被选中的行，标签使用 `accent`，值为白色，边框为白色；编辑时，值和边框变为 `accent`。鼠标下方的行为白色：由包含卡片的一方用 `PropHoverGroup` 和 `PropHoverFila` 标记。

## 选择和编辑

`selectIndex` 是被选中的行：`-2` 表示没有，`-1` 表示标题，`0` 及以上表示某一行。

| 函数 | 作用 |
|---|---|
| `NextSelect()`、`PrevSelect()` | 移到下面或上面一行。会跳过空白和没有变量的行。离开卡片时返回 `true`，以便移到下一张卡片 |
| `EnterPropertieSelect()` | 在标题上时，折叠或展开卡片。在某一行上时，编辑它 |
| `button_up()`、`button_down()`、`button_left()`、`button_right()` | 编辑期间，传给该行 |
| `Cancel()` | 让该行恢复原样 |

`editando` 表示是否有行正在编辑。据此，方向键要么修改数值，要么移动选择：

```cpp
if (jugador->editando) jugador->button_up();   // 修改数值
else jugador->PrevSelect();                    // 上面一行
```

鼠标由包含卡片的一方处理：卡片并不知道自己在屏幕上的位置。顶部是边框（`borderGS`）和标题（`RenglonHeightGS + gapGS`）。每一行的高度是它的 `Resize(ancho)` 返回的值。

## 各种行

| 类 | 是什么 | 如何编辑 |
|---|---|---|
| `PropFloat` | 一个数字 | 用方向键，或直接输入 |
| `PropBool` | 一个复选框，靠右 | OK 勾选或取消勾选 |
| `PropColor` | 一个该颜色的方块，靠右 | OK 进入编辑并保存颜色，以防取消 |
| `PropButton` | 一个按钮 | OK 调用它的动作 |
| `PropButtonRow` | 一行中的多个按钮 | 左右键选择其中一个；OK 按下它 |
| `PropText` | 一个文本框 | OK 让它获得焦点以便输入 |
| `PropListMeshParts` | 一个可滚动的列表 | 上下键选择 |
| `PropLabel` | 一段只读文本，一行或多行 | 不可选择 |
| `PropSeparator` | 一条线 | 不可选择 |
| `PropGap` | 一个 `gap` 大小的空白 | 不可选择 |
| `PropSelectText` | 一个带标签、没有值的框 | 暂时什么也不做 |

### 数字

```cpp
PropFloat* vida = new PropFloat("Vida");
vida->value = &vidaJugador;
vida->SetRango(0, 100);           // 不会超出这个范围
vida->entero = true;              // 以整数显示和步进
vida->stepFino = 1;               // 上和下
vida->stepGrueso = 10;            // 左和右
vida->onChange = AlCambiarVida;   // void AlCambiarVida()
```

| 字段 | 作用 |
|---|---|
| `unit` | 紧跟在数字后面的文本，例如 `°`。它是构造函数的第二个参数 |
| `stepFino`、`stepGrueso` | 方向键每次增加的量：默认为 0.01 和 0.1 |
| `entero` | 显示取整后的数字，方向键也从取整后的值开始。不设置时显示四位小数 |
| `centrado` | 数字在框内居中 |
| `flechas` | 在两侧绘制 `<` 和 `>`。点击它们由容器处理 |
| `opciones`、`nOpciones` | 配合 `entero`：数字是一个索引，显示该选项的名称，并用 `W3dMenuTraducir` 翻译 |
| `acelera` | 左右键起初每次变 1，如果连续按，会逐渐增加到 10 |
| `dragStep` | 拖动时每像素改变多少。拖动由容器处理 |

`Set(v)` 在范围内赋值，并调用 `onChange`。方向键也是通过它来修改的。

要直接输入数值：

```cpp
vida->IniciarEdicionTexto();   // 显示当前值，全部选中
// 按键发给 g_textFieldActivo：参见“输入和文本框”
NumEditCommit();               // Enter：应用
NumEditCancel();               // Esc：保持原样
```

`NumEditActivo()` 表示是否有数字正在编辑。不是数字的文本按 0 处理。

### 复选框

`value` 指向一个 `bool`。每次改变时都会调用 `onChange`。标签占据它左边的全部空间，如果放不下，就用 `..` 截断。

### 颜色

`value` 指向四个 `float`（RGBA）。UI 绘制方块；颜色选择器不属于 UI。编辑期间，方向键只移动第一个分量。

### 按钮

`action` 是它调用的函数。`gris = true` 让它变暗并且不起作用。`oculto = true` 时不占一行。

设置 `conLabel = true` 时，这一行是一个名称加一个按钮：名称在左边，宽度与其文本相同，最多占这一行的 55 %，按钮占据其余全部空间。`labelChars` 固定名称的宽度，用于让多行对齐。设置 `button->desplegable` 时，它带有菜单的小箭头。此外：

- `AgregarPrefijo(accion, ayuda)` 在按钮前面放一个方块，用于显示颜色。
- `AgregarExtra(icono, accion, ayuda)` 在右边放最多两个只有图标的小按钮。`slotsExtras` 为它们预留位置，让各行之间保持对齐。`ExtraEnX(x)` 告诉你在这个 x 处的是哪一个，用于处理点击。

### 按钮行

```cpp
PropButtonRow* fila = new PropButtonRow();
fila->Agregar("Aceptar", Aceptar);
fila->Agregar("Cancelar", Cancelar);
jugador->properties.push_back(fila);
```

各按钮平分宽度。不可见的按钮会被跳过，其余按钮平分它们的位置。`Disparar(i)` 按下第 `i` 个按钮：当点击落在那一部分时，由容器调用。

### 文本框

```cpp
PropText* nombre = new PropText("Nombre", "Jugador 1");
// nombre->field.text 中是输入的内容
```

`onClick`：不进行编辑，而是调用这个函数，例如用来打开文件浏览器。`error = true` 会把被选中行的边框涂成红色，直到输入了内容为止。`oculto = true` 时不占一行。OK 通过 `TextFieldEnfocar` 让它获得焦点，这个函数目前定义在编辑器中。关于输入，参见[输入和文本框](entrada.md)。

### 文本

`PropLabel("texto")` 是一行文字。`PropLabel("texto", true)` 会在空格处换行，以适应宽度；单独一个词就放不下时，保持完整不拆开。`rojo = true` 用于错误。`oculto = true` 时不占一行。

### 列表

`PropListMeshParts` 显示 `filasMax` 行，默认为 3 行，如果还有更多，会显示一个小滚动条。被选中的行始终保持可见。`Cancel` 会把 `selectIndex` 恢复为进入编辑时的值。

它目前还不能用于任意列表：根据 `modo`，它读取场景中的某个列表（网格的各部分、它的 UV 贴图、它的颜色层、修改器、顶点组、骨骼、动画、约束、脚本），或者向编辑器函数的钩子（`MixFila*`、`RutinaFila*`、`PostFila*`）请求列表。

## 动画菱形

可动画的行（`PropFloat`、`PropBool`、`PropColor` 和 `PropButton` 中 `animProp` 为 0 及以上的行），如果卡片设置了 `reservaKeyBtn`，会在右边显示一个菱形。它的颜色表示状态：

| 状态 | 颜色 | 含义 |
|---|---|---|
| 0 | 灰色 | 没有动画 |
| 1 | `accent`，半亮 | 有动画，但这里没有关键帧 |
| 2 | `accent` | 这里有一个关键帧 |
| 3 | 绿色 | 值已改变但尚未保存 |

菱形属于 UI；但它处于什么状态则不是。它向程序安装的一个钩子查询状态，点击时调用另一个钩子：

```cpp
W3dKeyframeEstado = MiEstado;   // int MiEstado(int prop, int comp)：0 到 3
W3dKeyframeToggle = MiToggle;   // void MiToggle(int prop, int comp)：添加或移除关键帧
```

没有钩子时，菱形保持灰色。卡片不会调用 `W3dKeyframeToggle`：由处理点击的一方调用。`g_kfFocoProp` 指向其菱形拥有键盘焦点的那一行，并用白色边框标出。`W3dRenderRomboKey(estado)` 绘制一个单独的菱形。

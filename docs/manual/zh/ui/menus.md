# 下拉菜单

`PopupMenu`（`widgets/PopupMenu.h`）是一个浮动的选项列表：栏中的菜单、右键菜单、选择器。每个选项（`MenuItem`）都有一段文本，还可以有：

- 左侧的**图标**，
- **子菜单**，右侧带一个箭头，
- **复选框**，显示并切换一个 `bool`，
- **滑块**，在两个值之间编辑一个 `float`，
- **快捷键**，显示在右侧，颜色为浅灰（“Shift D”），
- `verde`：它是活动选项，图标和文本使用 `accent`，
- `gris`：指向一个 `bool`；只要它为 `false`，该选项就以半透明显示，并且不响应，
- `lineaArriba`：一条把它与上面一组隔开的线。

菜单可以有一个**标题**（`titulo`）：显示为白色，下方有一条线。

## 用表来构建

推荐的方式是用一张 `MenuDef` 表：每个选项的文本和动作写在同一行里。不需要用一个编号去对应别处的 `switch`。

```cpp
static bool grilla = true;
static PopupMenu* recientes = NULL;
static void Nuevo() { /* ... */ }
static void Abrir() { /* ... */ }

static const MenuDef ARCHIVO[] = {
    // 文本      动作    快捷键    图标                     子菜单      复选框
    { "Nuevo",   Nuevo,  "Ctrl N", ICONO(IconType::mas) },
    { "Abrir",   Abrir,  "Ctrl O", ICONO(IconType::carpeta) },
    { "Recientes", NULL, NULL,     ICONO(IconType::lista),  &recientes },
    { "Grilla",  NULL,   NULL,     0,                       NULL,       &grilla },
};

recientes = new PopupMenu();          // 子菜单必须在构建之前就存在
PopupMenu* archivo = new PopupMenu();
archivo->titulo = "Archivo";
archivo->Construir(ARCHIVO, 4);
```

`ICONO(x)` 会给图标加 1：在表中，0 表示“没有图标”。每个选项的 id 就是它所在的行号。

有两个可选的钩子，由程序安装：

```cpp
W3dMenuTraducir = MiTraduccion;   // const char* MiTraduccion(const char* texto)
W3dMenuAtajoTexto = MiAtajo;      // std::string MiAtajo(const std::string& atajo)
```

有了 `W3dMenuTraducir`，表中的文本在构建菜单时就会被翻译。有了 `W3dMenuAtajoTexto`，快捷键可以是一个以 `@` 开头的名称，由钩子返回对应的按键。没有这个钩子时，带 `@` 的快捷键不会显示。

## 手动构建

```cpp
PopupMenu* vista = new PopupMenu();
vista->Agregar("Frente", 0, IconType::camera);
vista->AgregarCheck("Mostrar piso", 1, &verPiso);
vista->AgregarFloat("Tamaño", 2, &tamanio, 0.0f, 4.0f);
MenuItem* it = vista->Agregar("Alambre", 3);
it->verde = esAlambre;
it->lineaArriba = true;
vista->action = AlElegir;              // void AlElegir(int id)
```

`Agregar` 返回 `MenuItem*`，以便修改其他属性。选中一个选项时，`Ejecutar(id)` 会调用它的 `accion`（如果有的话），否则调用菜单的 `action(id)`。`Limpiar()` 删除所有选项，用于内容会变化的菜单。

## 打开菜单

```cpp
archivo->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
MenuAbierto = archivo;
```

它会在你传入的位置打开：光标所在处，或者某个按钮的边缘。如果放不下，就向屏幕内移动。`Abrir` 不会修改 `MenuAbierto`：由打开菜单的一方设置。`Cerrar()` 会把它重新设为 `NULL`。

打开时会进行测量：

- 宽度是最长那一行的宽度，包括它的文本和右侧一栏：快捷键、箭头、复选框或滑块。如果有一行带图标，所有行都会为图标留出位置，这样文本才能对齐。
- 宽度永远不超过屏幕。如果放不下，每一行的文本会在右侧一栏之前被截断：复选框仍然可见。
- 高度永远不超过屏幕。如果行数更多，就只显示放得下的部分，并在右侧显示一个滚动条。用滚轮（`Wheel`）滚动。

子菜单在它所在的行旁边打开：放得下就在右边，否则在左边。如果两边都放不下，它会变窄，露出后面菜单的一栏，让鼠标可以移到其他选项上。

## 鼠标和手指

```cpp
// 移动时
if (MenuAbierto) MenuAbierto->MouseMove(mx, my);

// 单击或触摸时
if (MenuAbierto) {
    PopupMenu* m = MenuAbierto;
    int id = m->Click(mx, my);
    if (id >= 0) m->Ejecutar(id);
}

// 绘制时，使用全屏投影
if (MenuAbierto) MenuAbierto->Render();
```

`Click` 返回选中的 id，什么都没选中时返回 `-1`，打开了子菜单时返回 `-2`。

- 鼠标经过时会高亮该行并打开它的子菜单，无需单击。
- 如果鼠标离开超过 `60 * GlobalScale` 像素，菜单会自动关闭。设置 `cerrarLejos = false` 则不会：用于不是在鼠标处打开的菜单。
- 在外面单击会关闭菜单。
- 普通选项会关闭菜单。复选框切换后菜单保持打开，方便勾选多个。在滑块上单击会根据点击位置设置数值。
- 在滑块上按住并拖动，会实时改变数值。处理鼠标的一方在 `MenuSliderDragActivo()` 为 `true` 期间，把每次移动传给 `MenuSliderDragMover(mx)`，松开时调用 `MenuSliderDragSoltar()`。
- 在触摸屏上，打开子菜单的那一下触摸不会选中子菜单中的任何内容，即使子菜单正好出现在手指下方。需要再触摸一次。
- 在子菜单中选中的内容在该子菜单中执行：每个菜单都有自己的 id。

## 键盘

焦点在最内层打开的子菜单上：

```cpp
PopupMenu* m = MenuAbierto;
while (m->submenuAbierto && m->submenuAbierto->abierto) m = m->submenuAbierto;
MenuItem* it = m->ItemActual();      // 高亮的选项，或 NULL
```

| 函数 | 作用 |
|---|---|
| `button_up()`、`button_down()` | 移动高亮，到头后循环。不会打开子菜单 |
| `AbrirSubmenuActual()` | 打开高亮选项的子菜单，并高亮其第一个选项 |
| `AjustarSlider(+1)`、`AjustarSlider(-1)` | 把滑块移动其范围的二十分之一 |
| `Enter()` | 相当于在高亮选项上单击，返回值也相同。在 `MenuAbierto` 上调用时，只会作用于最内层的子菜单 |
| `Cerrar()` | 关闭菜单及其子菜单 |

使用键盘时，高亮的选项始终保持可见。哪个按键调用哪个函数，由使用菜单的一方决定。

`MenuPantallaW` 和 `MenuPantallaH` 必须设为屏幕的尺寸：菜单靠它们来调整位置。

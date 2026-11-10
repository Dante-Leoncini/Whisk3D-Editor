# Outliner

> [!NOTE]
> ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 支持鼠标和键盘（通过蓝牙）。本页中关于 Nokia 的部分，讲的是不用鼠标和键盘、只用手机按键时怎么操作。

**Outliner** 是场景中所有内容的列表：对象、相机、灯光和集合。它是[视口](interfaz.md)一节介绍过的视口之一；在 PC 上，它位于右上方。在这里可以选择对象、隐藏对象、调整它们的顺序、把一个对象放进另一个对象里，以及重命名。

在 Outliner 中选中的内容，在 3D Viewport 中也会被选中，反之亦然。最后选中的那个是**活动对象**：Properties 显示的就是它。

![Android](../tutoriales/img/icono-android.svg) **在 Android 上**，屏幕上默认没有 Outliner：位置留给了 3D Viewport 和 Properties。要使用它，请用角落按钮把某个视口改成 Outliner（或拆分出一个），就像[更改类型](interfaz.md#更改类型)中那样。没有 Outliner 也能工作：在 3D Viewport 中点一下对象就能选中它，而 Properties 中有名称以及 **Visible** 和 **Renderable** 复选框。

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上**，屏幕上默认也没有 Outliner：只放得下两个视口。整理场景时，可以把 Properties 改成 Outliner：用**左软键**进入它的选项卡，再用左方向键到达角落按钮的菜单，在那里选择 Outliner。要改回来，在 Outliner 中按左软键会直接打开这个菜单。用**绿色键** ![](../tutoriales/img/tecla-verde.svg) 让它成为活动视口：下面所有关于 N95 的操作，都是在 Outliner 带绿色边框时进行的。

## 每一行显示的内容

每一行是一个对象。**箭头**用来折叠或展开它里面的内容；**图标**表示它的类型（网格、相机、灯光、集合……）；接着是**名称**，活动对象显示为绿色，被选中的对象显示为较深的绿色；最右边是**眼睛**和**相机**。位于另一个对象里面的对象会显示得更靠右，挂在那个对象下面；颜色较淡的行是隐藏的对象。

在新项目中，最上面是 **Constructor**，这是一个准备屏幕的例程；下面是 **Collection**，里面有相机、灯光和立方体。集合就是用来整理内容的文件夹。

## 选择

| | 如何选择 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 单击一行，只选中那个对象。<kbd>Shift</kbd> + 单击会选中从活动对象到该行之间的所有行。<kbd>Ctrl</kbd> + 单击加选或减选一行。鼠标在 Outliner 上时，上下方向键会移到上一行或下一行。 |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 点一下该行。在 3D Viewport 的工具栏上打开 **Shift** 或 **Ctrl** 后，点按的效果和用键盘时一样。拖动手指会滚动列表。 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 上下方向键移到上一行或下一行，列表会自动滚动跟上。 |
| 菜单 | Select → Select All、Deselect All 或 Invert Selection |

![单击 Camera 选中它；Shift + 单击 Cube，选中从 Camera 到 Cube 的所有行；单击 Light，只留下灯光；Ctrl + 单击 Cube 把它加进来，再 Ctrl + 单击一次又把它去掉](../tutoriales/img/outliner-seleccionar-raton.webp)

## 折叠和展开

当一个对象里面有其他对象时，它的箭头用来显示或隐藏它们。把暂时不用的内容折叠起来，列表就会变短。

| | 如何折叠和展开 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 单击箭头。鼠标在 Outliner 上时，键盘的右方向键展开活动对象，左方向键折叠它；如果它已经折叠，左方向键会跳到包含它的上一层对象并将其折叠。 |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 点一下箭头。 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 右方向键展开活动对象，左方向键折叠它；如果它已经折叠，左方向键会跳到包含它的上一层对象并将其折叠。 |

![单击 Collection 的箭头，Camera、Light 和 Cube 被隐藏；再单击一次，它们又显示出来](../tutoriales/img/outliner-plegar-raton.webp)

## 隐藏：眼睛和相机

- **眼睛**用来在 3D Viewport 中显示或隐藏对象。隐藏的对象也不会出现在渲染中。如果它里面有其他对象，它们会一起被隐藏，对应的行会变淡。
- **相机**决定对象是否出现在**渲染**中。关闭后，工作时仍能看到该对象，但它不会出现在你渲染的图像和视频中。

点击它们不会改变选择。

| | 眼睛 | 相机 |
|---|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 单击眼睛，或按 <kbd>H</kbd>（鼠标在 Outliner 上时）切换活动对象的眼睛 | 单击相机 |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 点一下眼睛 | 点一下相机 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | <kbd>2</kbd>，切换活动对象的眼睛 | <kbd>3</kbd>，切换活动对象的相机 |
| 菜单 | Element → Hide/Show（作用于活动对象） | — |

![单击 Cube 的眼睛将其关闭，立方体从 3D Viewport 中消失；再单击一次，它又出现了。单击 Cube 的相机将其关闭，立方体仍然可见](../tutoriales/img/outliner-ocultar-raton.webp)

这两项在 Properties 的 Transform 卡片中也有：**Visible** 和 **Renderable**。它们都有菱形图标，所以可以制作动画：例如，让某个东西在视频的某一时刻出现。

## 调整顺序，以及把一个对象放进另一个对象里

列表的顺序可以改变，一个对象也可以放进另一个对象**里面**：它会成为那个对象的**子级**，父级移动、旋转或缩放时，子级会跟着一起变。这就是 [Object Mode](../tutoriales/modo-objeto.md#outliner场景中的对象) 教程中所说的**设置父级**。对象在场景中的位置不会改变：变的只是它在列表中的位置。

![PC](../tutoriales/img/icono-pc.svg) **用鼠标：**抓住该行并拖动。拖到另一行的上边缘或下边缘时会出现一条**绿线**：松开后，对象会放到那一行的前面或后面，与它处于同一层级。拖到另一行的中间时，那一行会高亮：松开后，对象会放到它**里面**。在最后一行下方松开，对象会移出所有包含它的对象。靠近上边缘或下边缘时，列表会自动滚动，这样在很长的列表中也能到达任意一行。

![把 Light 那一行拖到 Camera 上方，绿线让它排到第一；然后把 Camera 拖到 Cube 那一行的中间松开，它就到了里面，显示得更靠右](../tutoriales/img/outliner-mover-raton.webp)

![Android](../tutoriales/img/icono-android.svg) **用手指：**把手指按在该行上保持不动约一秒，直到手机振动：这一行就被抓住了。然后像用鼠标一样拖动并松开。如果没有移动就松开，会打开该对象的菜单。如果不等待，拖动手指会滚动列表。

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上：**用方向键选中该行，按 <kbd>1</kbd>。这一行会变成绿色，现在方向键用来移动它：

- ![Nokia](../tutoriales/img/icono-nokia.svg) **上**和**下**：在同一层级的行之间上移或下移。
- ![Nokia](../tutoriales/img/icono-nokia.svg) **右**：把它放进上面那一行（同一层级）的里面。
- ![Nokia](../tutoriales/img/icono-nokia.svg) **左**：把它从包含它的对象中移出来，放到那个对象后面，与之同级。
- ![Nokia](../tutoriales/img/icono-nokia.svg) 再按一次 <kbd>1</kbd>（或 <kbd>OK</kbd>）把它留在那里。<kbd>C</kbd> 取消，让它回到原处。

![Nokia](../tutoriales/img/icono-nokia.svg) 整个移动过程可以用 <kbd>C</kbd> + 左方向键一次撤销。例如，可以用这种方式把灯光排到它所照亮的对象前面。

![一部 Nokia N95，Outliner 为活动视口：按 1，Light 那一行变成绿色，上方向键把它移到 Camera 上方，再按 1 把它留在那里；按 2，立方体从 3D Viewport 中消失又回来；按 3，关闭该行的相机](../tutoriales/img/outliner-nokia.webp)

## 找到活动对象并框显它

在大场景中，列表会很长，而在 3D Viewport 中选择某个对象并不会让列表滚动：它的那一行可能在视野之外。要在列表中找到它，或在 3D Viewport 中看到这个对象（**框显**它，就像[框显所选内容](../tutoriales/navegacion.md#框显所选内容)中那样）：

| | 在 Outliner 中 | 在 3D Viewport 中 |
|---|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 鼠标在 Outliner 上时按 <kbd>Num .</kbd>（**Show active**）：列表会滚动，直到选中的内容位于中间。如果它在某个已折叠的对象里面，就没有对应的行：请先展开。 | 鼠标在 3D Viewport 上时按 <kbd>Num .</kbd> |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 用手指拖动列表 | 在 3D Viewport 上用双指双击 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 列表会自动跟随你用方向键选中的行 | 单按一下 <kbd>0</kbd>，即使活动视口是 Outliner 也可以 |
| 菜单 | — | 3D Viewport 菜单栏中的 View → Frame Selected |

![一个有几十个对象的场景：在 3D Viewport 中选中一个对象，它的行在 Outliner 中看不到；鼠标移到 Outliner 上按 Num .，列表滚动，绿色的那一行位于中间](../tutoriales/img/outliner-activo-raton.webp)

## 重命名

| | 如何重命名 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 双击名称，或按 <kbd>F2</kbd> 重命名活动对象：该行会变成文本框。输入新名称，按 <kbd>Enter</kbd> 或在外面单击确认；<kbd>Esc</kbd> 保持原样。 |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 在名称上快速点两下：会打开屏幕键盘。 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 用左软键进入菜单栏，再用右方向键到达 Element → Rename。用 T9 输入，按 <kbd>OK</kbd> 确认。 |
| 菜单 | Element → Rename |

![双击 Cube 的名称：它变成文本框，输入 Box 并按 Enter 确认；然后选中 Light，按 F2，输入 Lamp 并按 Enter](../tutoriales/img/outliner-renombrar-raton.webp)

同一个场景中的两个对象不能同名：如果名称已被使用，Whisk3D 会在后面加一个数字并提示你。如果有脚本用名称引用了这个对象，脚本会自动更新。

## 复制和删除

| | 复制 | 删除 |
|---|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | <kbd>Shift</kbd> + <kbd>D</kbd>；<kbd>Alt</kbd> + <kbd>D</kbd> 为 Duplicate Linked | <kbd>X</kbd> 或 <kbd>Delete</kbd> |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | Element → Duplicate | <kbd>C</kbd> |
| 菜单 | Element → Duplicate 或 Duplicate Linked | Element → Delete |

从 Outliner 复制时，副本会正好叠在原对象上，不会像在 3D Viewport 中那样跟随鼠标：之后再移动它。删除时会要求确认，而且这两种操作都可以撤销。

## Outliner 的菜单栏

| 按钮 | 包含什么 |
|---|---|
| **View options**（眼睛） | 显示哪些内容：Constructor、其他例程，以及眼睛列和相机列（**Visibility icons** 和 **Render icons**）。 |
| **Element**（立方体） | Delete、Rename、Move、Hide/Show、Duplicate、Duplicate Linked、Create Prefab 和 Unpack Prefab。 |
| **Select** | Select All、Deselect All 和 Invert Selection。 |
| 场景 | 当前正在编辑项目中的哪个场景、游戏或预制体（[场景、游戏和预制体](outliner.md#场景游戏和预制体)）。 |

![PC](../tutoriales/img/icono-pc.svg) 在某一行上右键单击会选中它，并在指针处打开同一个菜单。

![Android](../tutoriales/img/icono-android.svg) 用手指时，按住该行直到振动，然后不移动就松开：会打开同一个菜单。

## Library

项目的**资源**（网格、材质、纹理、动画、声音、脚本、预制体……）不在 Outliner 中：它们有自己的视口，即 **Library**，可以通过角落按钮选择。它以文件夹形式显示缩略图，标出哪些资源正在使用、哪些没有；资源还可以拖到 3D Viewport 中：网格会创建一个对象，材质会指定给下方的对象。详细说明见 [Library 视图](outliner.md#library-视图)；Outliner 的其余内容见它的[手册](outliner.md)和[快捷键](atajos.md#outliner)。

## Library 视图

| 功能 | 说明 |
|---|---|
| 资源 | 网格、材质、纹理、动画、预制体、场景、声音、脚本、[着色器](shaders.md)、字体和视频，按文件夹存放。拖到 3D Viewport 中网格上的材质或着色器，在松开之前就会显示在指针下方的部分上。绿色对勾 = 正在使用；红色叉号 = 孤立；`*` = 未保存。 |
| Filter / 列表或 Grid | 只显示一种类型；查看缩略图。 |
| 拖动资源 | 拖到文件夹（移动它）、拖到 3D Viewport（网格 = 新对象，材质或纹理 = 指定给碰到的对象，预制体 = 实例），或拖到 Properties 的下拉框中。 |
| New Folder / Move to Folder | 整理（文件夹只用于整理）。 |
| Duplicate Resource / Select Users / Purge Orphans | 复制、找出谁在使用它、删除没人使用的内容。 |
| Inside the .w3d / External | 在 Properties 中（Resource 卡片）：把资源移入或移出项目。 |

## 场景、游戏和预制体

显示场景名称的按钮（在 Outliner 和 3D Viewport 中）会列出项目中的场景、游戏和预制体。在那里你可以新建它们、选择初始场景，以及把场景转换为游戏（或反过来）。

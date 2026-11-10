# Outliner

> [!NOTE]
> ![Nokia](img/icono-nokia.svg) Nokia N95 支持鼠标和键盘（通过蓝牙）。本页中关于 Nokia 的部分，讲的是不用鼠标和键盘、只用手机按键时怎么操作。

**Outliner** 是场景中所有内容的列表：对象、相机、灯光和集合。它是[视口](viewports.md)中介绍过的视口之一；在 PC 上，它位于右上方。在这里可以选择对象、隐藏对象、调整顺序、把一个对象放进另一个对象里，以及修改名称。

在 Outliner 中选中的内容，在 3D Viewport 中也会被选中，反过来也一样。最后选中的那个就是**活动对象**：Properties 显示的就是它。

![Android](img/icono-android.svg) **在 Android 上**，屏幕上默认没有 Outliner：空间留给了 3D Viewport 和 Properties。要使用它，用角落按钮把某个视口改成 Outliner（或者拆分一个视口），和[更改类型](viewports.md#更改类型)中一样。没有 Outliner 也可以工作：在 3D Viewport 中点一下对象就能选中它，Properties 里也有名称以及 **Visible** 和 **Renderable** 复选框。

![Nokia](img/icono-nokia.svg) **在 Nokia N95 上**，屏幕上默认没有 Outliner：只放得下两个视口。整理场景的时候，可以把 Properties 改成 Outliner：按**左软键**进入它的选项卡，再按左方向键到达角落按钮的菜单；在那里选择 Outliner。要切换回去，在 Outliner 中按左软键会直接打开这个菜单。按**绿色键** ![](img/tecla-verde.svg) 让它成为活动视口：下面所有关于 N95 的操作，都是在 Outliner 带有绿色边框时进行的。

## 每一行显示什么

每一行是一个对象。**箭头**用来折叠或展开它里面的内容；**图标**表示它的类型（网格、相机、灯光、集合……）；接着是**名称**，如果是活动对象就显示为绿色，如果被选中则显示为较深的绿色；最右边是**眼睛**和**相机**。在另一个对象里面的对象会显示得更靠右，挂在那个对象下面；颜色较淡的行是隐藏的对象。

在新项目中，最上面是 **Constructor**，一个用来准备画面的例程；下面是 **Collection**，里面有相机、灯光和立方体。集合就是用来整理东西的文件夹。

## 选择

| | 如何选择 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 单击某一行，只选中这个对象。<kbd>Shift</kbd> + 单击选中从活动对象到这一行之间的所有行。<kbd>Ctrl</kbd> + 单击加选或减选一行。鼠标位于 Outliner 上方时，上下方向键移到上一行或下一行。 |
| ![Android](img/icono-android.svg) 触屏 | 点一下某一行。打开 3D Viewport 工具栏上的 **Shift** 或 **Ctrl** 后，点一下的效果和用键盘时一样。拖动手指可以滚动列表。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 上下方向键移到上一行或下一行，列表会自动滚动跟随。 |
| 菜单 | Select → Select All、Deselect All 或 Invert Selection |

![单击 Camera 选中它；Shift + 单击 Cube，选中从 Camera 到 Cube 的所有行；单击 Light 只留下灯光；Ctrl + 单击 Cube 加选它，再 Ctrl + 单击一次则取消选择](img/outliner-seleccionar-raton.webp)

## 折叠和展开

当一个对象里面还有其他对象时，它的箭头可以显示或隐藏这些对象。把暂时不用的折叠起来，列表就会很短。

| | 如何折叠和展开 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 单击箭头。鼠标位于 Outliner 上方时，键盘上的右方向键展开活动对象，左方向键折叠它；如果它已经折叠，左方向键会跳到外面那一层并把它折叠。 |
| ![Android](img/icono-android.svg) 触屏 | 点一下箭头。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 右方向键展开活动对象，左方向键折叠它；如果它已经折叠，左方向键会跳到外面那一层并把它折叠。 |

![单击 Collection 的箭头，Camera、Light 和 Cube 被隐藏；再单击一次，它们又显示出来](img/outliner-plegar-raton.webp)

## 隐藏：眼睛和相机

- **眼睛**在 3D Viewport 中显示或隐藏对象。隐藏的对象也不会出现在渲染结果中。如果它里面还有其他对象，这些对象会和它一起隐藏，它们的行也会变淡。
- **相机**决定对象是否出现在**渲染**中。关闭后，工作时仍然能看到这个对象，但它不会出现在你渲染出的图像和视频里。

点它们不会改变选择。

| | 眼睛 | 相机 |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | 单击眼睛，或者（鼠标位于 Outliner 上方时）按 <kbd>H</kbd> 切换活动对象的眼睛 | 单击相机 |
| ![Android](img/icono-android.svg) 触屏 | 点一下眼睛 | 点一下相机 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>2</kbd>，切换活动对象的眼睛 | <kbd>3</kbd>，切换活动对象的相机 |
| 菜单 | Element → Hide/Show（作用于活动对象） | — |

![单击 Cube 的眼睛，眼睛闭上，立方体从 3D Viewport 中消失；再单击一次，它又显示出来。单击 Cube 的相机将其关闭，立方体仍然可见](img/outliner-ocultar-raton.webp)

这两项在 Properties 的 Transform 卡片中也有：**Visible** 和 **Renderable**。它们带有菱形，所以可以制作动画：比如让某个东西在视频的某个时刻出现。

## 调整顺序，把一个对象放进另一个对象里

列表的顺序可以调整，一个对象也可以放到另一个对象**里面**：它会成为那个对象的**子级**，父级移动、旋转或缩放时，子级会跟着一起变。这就是 [Object Mode](modo-objeto.md#outliner场景中的对象) 中所说的**设置父级**。对象在场景中的位置不会改变：变化的只是列表。

![PC](img/icono-pc.svg) **用鼠标：**抓住某一行并拖动。拖到另一行的上边缘或下边缘时，会出现一条**绿线**：松开后，对象会放在这一行之前或之后，和它处于同一层级。拖到另一行的中间时，那一行会高亮显示：松开后，对象会放到它**里面**。如果在最后一行下面松开，对象会从所有包含它的对象中移出来。靠近列表的上边缘或下边缘时，列表会自动滚动，这样在很长的列表中也能到达任何一行。

![把 Light 那一行拖到 Camera 上面，绿线让它排到第一位；然后把 Camera 放到 Cube 那一行的中间，它就到了 Cube 里面，显示得更靠右](img/outliner-mover-raton.webp)

![Android](img/icono-android.svg) **用手指：**手指在某一行上按住不动一秒，直到手机振动：这一行就被抓住了。然后像用鼠标一样拖动并松开。如果没有移动就松开，会打开这个对象的菜单。不等待就拖动手指，则会滚动列表。

![Nokia](img/icono-nokia.svg) **在 Nokia N95 上：**用方向键选中某一行，按 <kbd>1</kbd>。这一行会变成绿色，现在方向键用来移动它：

- ![Nokia](img/icono-nokia.svg) **上**和**下**在同一层级的行之间把它上移或下移。
- ![Nokia](img/icono-nokia.svg) **右**把它放进上面那一行（同一层级）的里面。
- ![Nokia](img/icono-nokia.svg) **左**把它从包含它的对象中移出来：它会排在那个对象之后，和它处于同一层级。
- ![Nokia](img/icono-nokia.svg) 再按一次 <kbd>1</kbd>（或 <kbd>OK</kbd>）就把它留在那里。<kbd>C</kbd> 取消，让它回到原来的位置。

![Nokia](img/icono-nokia.svg) 整个移动过程可以用 <kbd>C</kbd> + 左方向键一次撤销。例如，可以用它把一盏灯光放到它照亮的对象前面。

![Outliner 为活动视口的 Nokia N95：按 1 后 Light 那一行变成绿色，上方向键把它移到 Camera 上面，再按 1 把它留在那里；按 2，立方体从 3D Viewport 中消失又出现；按 3，它那一行的相机被关闭](img/outliner-nokia.webp)

## 找到活动对象并框显它

场景很大时，列表会很长，而在 3D Viewport 中选择对象并不会让列表滚动：它那一行可能不在可见范围内。要在列表中找到它，或者在 3D Viewport 中看到这个对象（**框显**它，和[框显所选内容](navegacion.md#框显所选内容)中一样）：

| | 在 Outliner 中 | 在 3D Viewport 中 |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | 鼠标位于 Outliner 上方时按 <kbd>Num .</kbd>（**Show active**）：列表会滚动，直到选中的内容位于中间。如果它在某个折叠起来的对象里面，就没有对应的行：先把它展开。 | 鼠标位于 3D Viewport 上方时按 <kbd>Num .</kbd> |
| ![Android](img/icono-android.svg) 触屏 | 用手指拖动列表 | 在 3D Viewport 上双指双击 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 列表会自动跟随你用方向键选中的那一行 | 单按一下 <kbd>0</kbd>，即使活动视口是 Outliner 也可以 |
| 菜单 | — | 3D Viewport 栏上的 View → Frame Selected |

![一个有几十个对象的场景：在 3D Viewport 中选中一个对象，它那一行在 Outliner 中看不到；鼠标位于 Outliner 上方时按 Num .，列表滚动，绿色的那一行来到中间](img/outliner-activo-raton.webp)

## 重命名

| | 如何重命名 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 双击名称，或者按 <kbd>F2</kbd> 重命名活动对象：这一行会变成文本框。输入新名称，用 <kbd>Enter</kbd> 或在外面单击来确认；<kbd>Esc</kbd> 保持原样。 |
| ![Android](img/icono-android.svg) 触屏 | 双击名称：会打开屏幕键盘。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 按左软键进入栏，用右方向键到达 Element → Rename。用 T9 输入，按 <kbd>OK</kbd> 确认。 |
| 菜单 | Element → Rename |

![双击 Cube 的名称：它变成文本框，输入 Box，按 Enter 确认；然后选中 Light，按 F2，输入 Lamp 并按 Enter](img/outliner-renombrar-raton.webp)

同一个场景中的两个对象不能同名：如果名称已被使用，Whisk3D 会在后面加上一个数字并提示你。如果有脚本引用了这个对象的名称，它会自动更新。

## 复制和删除

| | 复制 | 删除 |
|---|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Shift</kbd> + <kbd>D</kbd>；<kbd>Alt</kbd> + <kbd>D</kbd> 为 Duplicate Linked | <kbd>X</kbd> 或 <kbd>Delete</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | Element → Duplicate | <kbd>C</kbd> |
| 菜单 | Element → Duplicate 或 Duplicate Linked | Element → Delete |

在 Outliner 中复制时，副本会正好放在原对象上，不会像在 3D Viewport 中那样跟随鼠标：之后再移动它。删除会要求确认，两种操作都可以撤销。

## Outliner 的菜单栏

| 按钮 | 内容 |
|---|---|
| **View options**（眼睛图标） | 显示哪些内容：Constructor、其他例程，以及眼睛列和相机列（**Visibility icons** 和 **Render icons**）。 |
| **Element**（立方体图标） | Delete、Rename、Move、Hide/Show、Duplicate、Duplicate Linked、Create Prefab 和 Unpack Prefab。 |
| **Select** | Select All、Deselect All 和 Invert Selection。 |
| 场景 | 正在编辑项目中的哪个场景、游戏或预制体（[场景、游戏和预制体](outliner.md#场景游戏和预制体)）。 |

![PC](img/icono-pc.svg) 在某一行上右键单击，会选中这一行，并在指针处打开同一个菜单。

![Android](img/icono-android.svg) 用手指按住某一行直到振动，然后不移动就松开：会打开同一个菜单。

## Library

项目的**资源**（网格、材质、纹理、动画、声音、脚本、预制体……）不在 Outliner 中：它们有自己的视口，也就是 **Library**，用角落按钮来选择。它以文件夹形式显示缩略图，标出哪些资源正在使用、哪些没有使用；还可以把资源拖到 3D Viewport 中：网格会创建一个对象，材质会指定给下方的对象。具体见 [Library 视图](outliner.md#library-视图)；Outliner 的其他内容，见它的[手册](outliner.md)和[键盘快捷键](../editor/atajos.md#outliner)。

## Library 视图

| 功能 | 说明 |
|---|---|
| 资源 | 网格、材质、纹理、动画、预制体、场景、声音、脚本、[着色器](../editor/shaders.md)、字体和视频，按文件夹存放。拖到 3D Viewport 中网格上的材质或着色器，在松开之前就会显示在指针下方的部分上。绿色对勾 = 正在使用；红色叉号 = 孤立；`*` = 未保存。 |
| Filter / List 或 Grid | 只显示一种类型；查看缩略图。 |
| 拖动资源 | 拖到文件夹（移动它）、拖到 3D Viewport（网格 = 新对象，材质或纹理 = 指定给碰到的对象，预制体 = 实例），或拖到 Properties 的下拉框中。 |
| New Folder / Move to Folder | 整理（文件夹只用于整理）。 |
| Duplicate Resource / Select Users / Purge Orphans | 复制、找出谁在使用它、删除没人使用的内容。 |
| Inside the .w3d / External | 在 Properties 中（Resource 卡片）：把资源移入或移出项目。 |

## 场景、游戏和预制体

显示场景名称的按钮（在 Outliner 和 3D Viewport 中）会列出项目中的场景、游戏和预制体。在那里你可以新建它们、选择初始场景，以及把场景转换为游戏（或反过来）。

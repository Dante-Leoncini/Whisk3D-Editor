# 在 3D Viewport 中导航

> [!NOTE]
> ![Nokia](img/icono-nokia.svg) Nokia N95 支持鼠标和键盘（通过蓝牙）。本教程中关于 Nokia 的部分，讲的是不用鼠标和键盘、只用手机按键时怎么操作。

本教程讲解如何在 3D Viewport 中导航。导航就是移动你的视角：绕着场景转动、往旁边移动，以及拉近或拉远。

学会导航很重要：要舒服又快速地制作 3D 模型和动画，你需要会在场景里移动，并把视角放到便于工作的位置。

正如[上一个教程](viewports.md#活动视口)所讲的，要使用下面这些命令，3D Viewport 必须是活动视口，也就是带绿色边框的那个。

最常用的三种操作是**环绕**、**平移**和**缩放**。每一种都可以用鼠标、在触屏上用手指，或者用 Nokia N95 的键盘来完成。在 PC 的视频中，右下角的鼠标图会用绿色标出正在按下的按键。

## 环绕

环绕就是让视图绕着场景转动，就像绕着一张桌子走一圈，从各个方向看桌上的东西。

![PC](img/icono-pc.svg) **用鼠标：**按住**中键**（把滚轮按下去就是中键）并拖动。在 触控板 上，或者鼠标没有中键时，按住 <kbd>Alt</kbd>，再用**主键**（左键）拖动。

![鼠标指针位于 3D Viewport 上：按住中键拖动，视图就绕着立方体转动](img/navegacion-orbitar-raton.webp)

![Android](img/icono-android.svg) **用手指：**在 3D Viewport 上用一根手指拖动。要从背景上开始：如果手指按在 Gizmo（对象上的彩色箭头）上，就会移动它。

![一根手指在 3D Viewport 的背景上拖动，视图绕着立方体转动](img/navegacion-orbitar-dedo.webp)

![Nokia](img/icono-nokia.svg) **在 Nokia N95 上：**按住方向键中的一个**方向**。按住期间，视图会一直朝那个方向转动。

![一部竖着的 Nokia N95：按住右、左、上、下方向键，视图绕着立方体转动](img/navegacion-orbitar-nokia.webp)

## 平移

平移（*pan*）是把视图往旁边、往上或往下移动，而不转动它。

![PC](img/icono-pc.svg) **用鼠标：**按住 <kbd>Shift</kbd>，再用**中键**拖动。在 触控板 上：按住 <kbd>Alt</kbd> + <kbd>Shift</kbd>，再用主键拖动。

![按住 Shift 和中键，指针画一个圆，视图也跟着平移](img/navegacion-panear-raton.webp)

![Android](img/icono-android.svg) **用手指：**放上**两根手指**，朝同一个方向一起移动。

![两根手指一起画圆移动，视图随之平移](img/navegacion-panear-dedo.webp)

![Nokia](img/icono-nokia.svg) **在 Nokia N95 上：**按住 <kbd>\*</kbd> 键，再用方向键。

![按住星号键的 Nokia N95：每按一个方向键，视图就朝那个方向平移](img/navegacion-panear-nokia.webp)

## 缩放

缩放让你靠近或远离。每一步的幅度会随距离变化：靠近小东西时前进得慢，离得远时前进得快，所以无论是看整个场景还是看一个顶点，都同样好用。

![PC](img/icono-pc.svg) **用鼠标：**滚动**滚轮**。向前滚拉近，向后滚拉远。在 触控板 上，用两根手指上下滑动。

![鼠标指针位于立方体上：滚轮向前滚，视图拉近；向后滚，视图拉远](img/navegacion-zoom-raton.webp)

![Android](img/icono-android.svg) **用手指：**双指捏合。两指分开拉近，两指并拢拉远。

![两根手指分开，视图向立方体拉近；并拢，视图拉远](img/navegacion-zoom-dedo.webp)

![Nokia](img/icono-nokia.svg) **在 Nokia N95 上：**按住 <kbd>0</kbd>，用**上**方向键拉近，用**下**方向键拉远。

![按住 0 的 Nokia N95：按上，视图向立方体拉近；按下，视图拉远](img/navegacion-zoom-nokia.webp)

## 框显所选内容

如果你找不到方向了，或者想专门处理某个东西，就把它**框显**出来：视图会以所选内容为中心，并调整缩放，让它正好填满视图。之后再环绕，视图就会绕着它转。

| | 如何框显 |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num .</kbd>（数字小键盘上的点） |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 单按一下 <kbd>0</kbd>，不按方向键（和方向键一起按住则是缩放） |
| ![Android](img/icono-android.svg) 触屏 | 双指双击 |
| 菜单 | View → Frame Selected |

![视图离得很远而且偏到一边，立方体小小地缩在角落里；按 Num . 后，立方体回到中央，填满视图](img/navegacion-encuadrar.webp)

![一部 Nokia N95：单按一下 0，原本小小地缩在角落里的立方体回到中央，正好填满视图](img/navegacion-encuadrar-nokia.webp)

### 在 Edit Mode 中

在 [Edit Mode](../editor/modo-edicion.md) 中，框显的是网格中被选中的部分：也就是你选择的顶点、边或面，而不是整个对象。这适合处理大模型上的某个细节：选中它，框显它，然后绕着它环绕。

![一个处于 Edit Mode 的球体，选中了六个面（绿色）：按 Num . 后，视图拉近，这些面填满了屏幕](img/navegacion-encuadrar-edicion.webp)

## 局部视图

场景里东西很多时，要处理其中一个会很不方便：前面的东西会挡住视线，环绕时还会插到中间。**局部视图**只显示所选内容（一个或多个对象），并把它们框显出来。其余的东西并没有从场景中删除或隐藏：只是在这个视口里看不到了。左上角会出现“Local View”的标签。

要退出，再执行一次同样的操作：所有东西都会重新出现，视图也会回到之前的样子。

| | 如何进入和退出 |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>/</kbd> |
| ![Nokia](img/icono-nokia.svg) ![Android](img/icono-android.svg) Nokia N95 和触屏 | View 菜单 → Local View |

![一个摆满盒子、球体、圆柱体和圆锥体的场景，其中一个圆锥体被选中：按 / 后只剩下这个圆锥体，并被框显；再按一次 / 全部恢复](img/navegacion-vista-local.webp)

## 前视图、侧视图和顶视图

不用手动调整环绕角度，就能从某一侧正对着看场景：

| 视图 | ![PC](img/icono-pc.svg) PC | 菜单 |
|---|---|---|
| 前 / 后 | <kbd>Num 1</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 1</kbd> | View → Viewpoint → Front / Back |
| 右 / 左 | <kbd>Num 3</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 3</kbd> | View → Viewpoint → Right / Left |
| 顶 / 底 | <kbd>Num 7</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 7</kbd> | View → Viewpoint → Top / Bottom |

![PC](img/icono-pc.svg) 加上 <kbd>Ctrl</kbd>，每个键都会给出相反方向的视图。

![摆满东西的场景，分别从前视图、右视图和顶视图来看，以及加上 Ctrl 后各自相反方向的视图](img/navegacion-vistas.webp)

![Nokia](img/icono-nokia.svg) **在 Nokia N95 上：**按住 <kbd>#</kbd> 键，再按方向键。左和右让视图绕着场景每次转 90°。上切换到顶视图；下回到侧视图，再按一次下则切换到底视图。如果之前在自由环绕，第一次按方向键只会把视图摆正到最接近的那个视图。

![按住井号键的 Nokia N95：按右方向键，视图每次转 90 度；按上切换到顶视图，按下再回来](img/navegacion-vistas-nokia.webp)

## 透视和正交

3D Viewport 有两种显示场景的方式：

- **透视：**和眼睛或相机看到的一样。远处的东西看起来更小，平行线（街道的两边、视频里一排排的盒子）会汇聚到地平线上的一个点：**消失点**。这是真实的样子，带有纵深感。
- **正交：**没有透视。不管远近，所有东西都保持原来的大小，平行线永远不会相交。就像建筑平面图或工程制图。

正交适合精确建模：对齐东西、比较大小和测量，都不会被距离误导。配合前视图、侧视图和顶视图使用，就像在图纸上工作一样。透视则适合看最终效果。

| | 如何切换 |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num 5</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 单按一下 <kbd>#</kbd>，不按方向键 |
| 菜单 | View → Perspective/Ortho |

![两排一模一样的盒子：在透视中，后面的盒子看起来很小；按 Num 5 切换到正交后，所有盒子大小都一样](img/navegacion-perspectiva.webp)

![一部 Nokia N95：单按一下井号键，一排排盒子从透视变成正交，然后再变回来](img/navegacion-perspectiva-nokia.webp)

## 活动相机

场景中可以有好几台相机，但只有一台是**活动相机**：它用于游戏和渲染。玩游戏时看到的，以及渲染图像或视频时输出的，都是这台相机看到的画面。你用来导航的视图只是工作用的：它既不会出现在游戏里，也不会出现在渲染结果中。

要通过活动相机观看：

| | 如何进入和退出 |
|---|---|
| ![PC](img/icono-pc.svg) PC | <kbd>Num 0</kbd> |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>8</kbd>（在 Edit Mode 之外：在 Edit Mode 中，8 是 Loop Cut） |
| 菜单 | View → Cameras → Active Camera |

![按 Num 0 后，3D Viewport 改为通过相机观看，并显示渲染范围的边框；再按一次 Num 0 就回到原来的视图](img/navegacion-camara.webp)

![一部 Nokia N95：按 8，视图切换到活动相机；再按一次 8 就回到原来的视图](img/navegacion-camara-nokia.webp)

这个矩形就是取景框：框内的东西就是输出的内容。底部中间有一把**锁**：

- **打开**（初始状态）：缩放和平移只是让你拉近查看取景框，不会动到相机。环绕会让你离开相机视图，从相机所在的位置继续导航。
- **锁上**：环绕、平移和缩放都会移动**相机**。用这种方式调整镜头。

点一下或单击它，就能打开或锁上。

要让另一台相机成为活动相机：选中它，按 <kbd>Ctrl</kbd> + <kbd>Num 0</kbd>（或者 View → Cameras → Set Active Object as Camera）。

## 更多视图选项

| 选项 | 作用 | ![PC](img/icono-pc.svg) PC | ![Nokia](img/icono-nokia.svg) Nokia N95 | 菜单 |
|---|---|---|---|---|
| 滚转视图 | 让视图绕自身的轴旋转 15°，就像歪一下头。 | <kbd>Num 4</kbd> / <kbd>Num 6</kbd> | — | — |
| Lock Orbit | 视图不再转动：原本的环绕变成平移，就像在一块 2D 画板上操作。 | — | <kbd>9</kbd> | View → Lock Orbit |

编辑器的所有快捷键都在[键盘快捷键](../editor/atajos.md)中，手册里的[导航](../editor/navegacion.md)页面则把它们汇总成了表格。

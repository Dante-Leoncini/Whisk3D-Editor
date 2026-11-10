# 在 3D Viewport 中导航

> [!NOTE]
> ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 支持鼠标和键盘（通过蓝牙）。本页中关于 Nokia 的部分，讲的是不用鼠标和键盘、只用手机按键时怎么操作。

本页讲解如何在 3D Viewport 中导航。导航就是移动你的视角：绕着场景转动、往旁边移动，以及拉近或拉远。

学会导航很重要：要舒服又快速地制作 3D 模型和动画，你需要会在场景里移动，并把视角放到便于工作的位置。

要使用下面这些命令，3D Viewport 必须是[活动视口](interfaz.md#活动视口)，也就是带绿色边框的那个。

最常用的三种操作是**环绕**、**平移**和**缩放**。每一种都可以用鼠标、在触屏上用手指，或者用 Nokia N95 的键盘来完成。在 PC 的视频中，右下角的鼠标图会用绿色标出正在按下的按键。

## 环绕

环绕就是让视图绕着场景转动，就像绕着一张桌子走一圈，从各个方向看桌上的东西。

![PC](../tutoriales/img/icono-pc.svg) **用鼠标：**按住**中键**（把滚轮按下去就是中键）并拖动。在 触控板 上，或者鼠标没有中键时，按住 <kbd>Alt</kbd>，再用**主键**（左键）拖动。

![鼠标指针位于 3D Viewport 上：按住中键拖动，视图就绕着立方体转动](../tutoriales/img/navegacion-orbitar-raton.webp)

![Android](../tutoriales/img/icono-android.svg) **用手指：**在 3D Viewport 上用一根手指拖动。要从背景上开始：如果手指按在 Gizmo（对象上的彩色箭头）上，就会移动它。

![一根手指在 3D Viewport 的背景上拖动，视图绕着立方体转动](../tutoriales/img/navegacion-orbitar-dedo.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上：**按住方向键中的一个**方向**。按住期间，视图会一直朝那个方向转动。

![一部竖着的 Nokia N95：按住右、左、上、下方向键，视图绕着立方体转动](../tutoriales/img/navegacion-orbitar-nokia.webp)

## 平移

平移（*pan*）是把视图往旁边、往上或往下移动，而不转动它。

![PC](../tutoriales/img/icono-pc.svg) **用鼠标：**按住 <kbd>Shift</kbd>，再用**中键**拖动。在 触控板 上：按住 <kbd>Alt</kbd> + <kbd>Shift</kbd>，再用主键拖动。

![按住 Shift 和中键，指针画一个圆，视图也跟着平移](../tutoriales/img/navegacion-panear-raton.webp)

![Android](../tutoriales/img/icono-android.svg) **用手指：**放上**两根手指**，朝同一个方向一起移动。

![两根手指一起画圆移动，视图随之平移](../tutoriales/img/navegacion-panear-dedo.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上：**按住 <kbd>\*</kbd> 键，再用方向键。

![按住星号键的 Nokia N95：每按一个方向键，视图就朝那个方向平移](../tutoriales/img/navegacion-panear-nokia.webp)

## 缩放

缩放让你靠近或远离。每一步的幅度会随距离变化：靠近小东西时前进得慢，离得远时前进得快，所以无论是看整个场景还是看一个顶点，都同样好用。

![PC](../tutoriales/img/icono-pc.svg) **用鼠标：**滚动**滚轮**。向前滚拉近，向后滚拉远。在 触控板 上，用两根手指上下滑动。

![鼠标指针位于立方体上：滚轮向前滚，视图拉近；向后滚，视图拉远](../tutoriales/img/navegacion-zoom-raton.webp)

![Android](../tutoriales/img/icono-android.svg) **用手指：**双指捏合。两指分开拉近，两指并拢拉远。

![两根手指分开，视图向立方体拉近；并拢，视图拉远](../tutoriales/img/navegacion-zoom-dedo.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上：**按住 <kbd>0</kbd>，用**上**方向键拉近，用**下**方向键拉远。

![按住 0 的 Nokia N95：按上，视图向立方体拉近；按下，视图拉远](../tutoriales/img/navegacion-zoom-nokia.webp)

## 框显所选内容

如果你找不到方向了，或者想专门处理某个东西，就把它**框显**出来：视图会以所选内容为中心，并调整缩放，让它正好填满视图。之后再环绕，视图就会绕着它转。

| | 如何框显 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | <kbd>Num .</kbd>（数字小键盘上的点） |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 单按一下 <kbd>0</kbd>，不按方向键（和方向键一起按住则是缩放） |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 双指双击 |
| 菜单 | View → Frame Selected |

![视图离得很远而且偏到一边，立方体小小地缩在角落里；按 Num . 后，立方体回到中央，填满视图](../tutoriales/img/navegacion-encuadrar.webp)

![一部 Nokia N95：单按一下 0，原本小小地缩在角落里的立方体回到中央，正好填满视图](../tutoriales/img/navegacion-encuadrar-nokia.webp)

### 在 Edit Mode 中

在 [Edit Mode](modo-edicion.md) 中，框显的是网格中被选中的部分：也就是你选择的顶点、边或面，而不是整个对象。这适合处理大模型上的某个细节：选中它，框显它，然后绕着它环绕。

![一个处于 Edit Mode 的球体，选中了六个面（绿色）：按 Num . 后，视图拉近，这些面填满了屏幕](../tutoriales/img/navegacion-encuadrar-edicion.webp)

## 局部视图

场景里东西很多时，要处理其中一个会很不方便：前面的东西会挡住视线，环绕时还会插到中间。**局部视图**只显示所选内容（一个或多个对象），并把它们框显出来。其余的东西并没有从场景中删除或隐藏：只是在这个视口里看不到了。左上角会出现“Local View”的标签。

要退出，再执行一次同样的操作：所有东西都会重新出现，视图也会回到之前的样子。

| | 如何进入和退出 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | <kbd>/</kbd> |
| ![Nokia](../tutoriales/img/icono-nokia.svg) ![Android](../tutoriales/img/icono-android.svg) Nokia N95 和触屏 | View 菜单 → Local View |

![一个摆满盒子、球体、圆柱体和圆锥体的场景，其中一个圆锥体被选中：按 / 后只剩下这个圆锥体，并被框显；再按一次 / 全部恢复](../tutoriales/img/navegacion-vista-local.webp)

## 前视图、侧视图和顶视图

不用手动调整环绕角度，就能从某一侧正对着看场景：

| 视图 | ![PC](../tutoriales/img/icono-pc.svg) PC | 菜单 |
|---|---|---|
| 前 / 后 | <kbd>Num 1</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 1</kbd> | View → Viewpoint → Front / Back |
| 右 / 左 | <kbd>Num 3</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 3</kbd> | View → Viewpoint → Right / Left |
| 顶 / 底 | <kbd>Num 7</kbd> / <kbd>Ctrl</kbd> + <kbd>Num 7</kbd> | View → Viewpoint → Top / Bottom |

![PC](../tutoriales/img/icono-pc.svg) 加上 <kbd>Ctrl</kbd>，每个键都会给出相反方向的视图。

![摆满东西的场景，分别从前视图、右视图和顶视图来看，以及加上 Ctrl 后各自相反方向的视图](../tutoriales/img/navegacion-vistas.webp)

![Nokia](../tutoriales/img/icono-nokia.svg) **在 Nokia N95 上：**按住 <kbd>#</kbd> 键，再按方向键。左和右让视图绕着场景每次转 90°。上切换到顶视图；下回到侧视图，再按一次下则切换到底视图。如果之前在自由环绕，第一次按方向键只会把视图摆正到最接近的那个视图。

![按住井号键的 Nokia N95：按右方向键，视图每次转 90 度；按上切换到顶视图，按下再回来](../tutoriales/img/navegacion-vistas-nokia.webp)

## 透视和正交

3D Viewport 有两种显示场景的方式：

- **透视：**和眼睛或相机看到的一样。远处的东西看起来更小，平行线（街道的两边、视频里一排排的盒子）会汇聚到地平线上的一个点：**消失点**。这是真实的样子，带有纵深感。
- **正交：**没有透视。不管远近，所有东西都保持原来的大小，平行线永远不会相交。就像建筑平面图或工程制图。

正交适合精确建模：对齐东西、比较大小和测量，都不会被距离误导。配合前视图、侧视图和顶视图使用，就像在图纸上工作一样。透视则适合看最终效果。

| | 如何切换 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | <kbd>Num 5</kbd> |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 单按一下 <kbd>#</kbd>，不按方向键 |
| 菜单 | View → Perspective/Ortho |

![两排一模一样的盒子：在透视中，后面的盒子看起来很小；按 Num 5 切换到正交后，所有盒子大小都一样](../tutoriales/img/navegacion-perspectiva.webp)

![一部 Nokia N95：单按一下井号键，一排排盒子从透视变成正交，然后再变回来](../tutoriales/img/navegacion-perspectiva-nokia.webp)

## 锁定环绕

**锁定环绕**后，视图不再转动：原本的环绕变成平移，就像在桌面上移动一张纸。这非常适合处理那些像 2D 图纸一样绘制的模型，比如房屋平面图或机械零件，用起来很顺手：从上方用正交观看时，你可以在整张图纸上自由移动，图纸始终不会倾斜。

| | 如何锁定和解锁 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | View 菜单 → Lock Orbit |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | View 菜单 → Lock Orbit |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | <kbd>9</kbd> |

锁定或解锁时会显示一条提示；锁定期间，菜单中的 Lock Orbit 显示为绿色。

![从上方看的房屋平面图：选择 View → Lock Orbit 后，按住中键拖动，平面图只平移不转动](../tutoriales/img/navegacion-bloquear-orbita-raton.webp)

![从上方看的房屋平面图：点一下 View → Lock Orbit，手指拖动时平面图只平移不转动](../tutoriales/img/navegacion-bloquear-orbita-dedo.webp)

![显示房屋平面图的 Nokia N95：按 9 锁定环绕，方向键让平面图只平移不转动](../tutoriales/img/navegacion-bloquear-orbita-nokia.webp)

## 活动相机

场景中可以有好几台相机，但只有一台是**活动相机**：它用于游戏和渲染。玩游戏时看到的，以及渲染图像或视频时输出的，都是这台相机看到的画面。你用来导航的视图只是工作用的：它既不会出现在游戏里，也不会出现在渲染结果中。

要通过活动相机观看：

| | 如何进入和退出 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | <kbd>Num 0</kbd> |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | <kbd>8</kbd>（在 Edit Mode 之外：在 Edit Mode 中，8 是 Loop Cut） |
| 菜单 | View → Cameras → Active Camera |

![按 Num 0 后，3D Viewport 改为通过相机观看，并显示渲染范围的边框；再按一次 Num 0 就回到原来的视图](../tutoriales/img/navegacion-camara.webp)

![一部 Nokia N95：按 8，视图切换到活动相机；再按一次 8 就回到原来的视图](../tutoriales/img/navegacion-camara-nokia.webp)

这个矩形就是取景框：框内的东西就是输出的内容。底部中间有一把**锁**：

- **打开**（初始状态）：缩放和平移只是让你拉近查看取景框，不会动到相机。环绕会让你离开相机视图，从相机所在的位置继续导航。
- **锁上**：环绕、平移和缩放都会移动**相机**。用这种方式调整镜头。

### 将相机锁定到视图

把锁锁上后，活动相机就会锁定到视图上：你像平常导航一样环绕、平移和缩放，相机会跟着你一起移动。这是把相机准确放到想要位置的最方便的方法：通过它观看，把它锁定，然后导航，直到镜头符合你的要求。

| | 如何锁定和解锁 |
|---|---|
| ![PC](../tutoriales/img/icono-pc.svg) PC | 单击锁 |
| ![Android](../tutoriales/img/icono-android.svg) 触屏 | 点一下锁 |
| ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 左软键打开菜单栏（参见[菜单栏的选项显示不全时](interfaz.md#菜单栏的选项显示不全时)）；在那里选择 View → Lock the camera to the view |
| 菜单 | View → Lock the camera to the view |

![按 Num 0 切换到相机视图；单击锁把它锁上，然后环绕并滚动滚轮，相机随之移动，直到立方体进入取景框](../tutoriales/img/navegacion-camara-bloquear-raton.webp)

![通过相机观看时，点一下锁把它锁上，然后拖动手指，相机随之移动，直到立方体进入取景框](../tutoriales/img/navegacion-camara-bloquear-dedo.webp)

![一部 Nokia N95：按 8 切换到相机；用左软键打开 View 菜单，选择 Lock the camera to the view；用方向键和 0 移动相机，直到立方体进入取景框](../tutoriales/img/navegacion-camara-bloquear-nokia.webp)

要让另一台相机成为活动相机：选中它，按 <kbd>Ctrl</kbd> + <kbd>Num 0</kbd>（或者 View → Cameras → Set Active Object as Camera）。

## 更多视图选项

| 选项 | 作用 | ![PC](../tutoriales/img/icono-pc.svg) PC | ![Nokia](../tutoriales/img/icono-nokia.svg) Nokia N95 | 菜单 |
|---|---|---|---|---|
| 滚转视图 | 让视图绕自身的轴旋转 15°，就像歪一下头。 | <kbd>Num 4</kbd> / <kbd>Num 6</kbd> | — | — |

编辑器的所有快捷键都在[键盘快捷键](atajos.md)中。

## 所有控制方式一览

### 使用鼠标（PC）

| 功能 | 说明 | 使用方法 |
|---|---|---|
| 环绕 | 让视图绕轴心点旋转。 | 按住中键拖动，或 <kbd>Alt</kbd> + 左键拖动。 |
| 平移 | 横向移动视图。 | <kbd>Shift</kbd> + 中键，或 <kbd>Alt+Shift</kbd> + 左键拖动。 |
| 缩放 | 拉近或拉远。 | 鼠标滚轮。 |
| Select | 选择指针下方的对象（松开按键时才确定）。 | 左键单击；<kbd>Shift</kbd>+单击可加选或减选。 |
| Box Select | 在背景上拖动会框出一个矩形。 | 按住左键拖动，或按 <kbd>B</kbd>。 |
| Context menu | 在指针处打开当前模式的菜单（变换进行中时则会取消变换）。 | 右键单击。 |
| Lock Orbit | 原本的环绕变成平移：视图就像一块 2D 画板。 | View 菜单 → Lock Orbit。 |
| Lock the camera to the view | 通过活动相机观看时，导航会移动相机：用这种方式调整镜头。 | 单击锁（在相机视图的底部），或 View 菜单 → Lock the camera to the view。 |

### 使用键盘

| 功能 | 说明 | 快捷键 |
|---|---|---|
| Front View / Back View | 从前面或后面看。 | <kbd>Num1</kbd> / <kbd>Ctrl+Num1</kbd> |
| Right View / Left View | 从侧面看。 | <kbd>Num3</kbd> / <kbd>Ctrl+Num3</kbd> |
| Top View / Bottom View | 从上方或下方看。 | <kbd>Num7</kbd> / <kbd>Ctrl+Num7</kbd> |
| Perspective / Orthographic | 在两种投影之间切换。 | <kbd>Num5</kbd> |
| 滚转视图 | 让视图绕自身的轴旋转 15°。 | <kbd>Num4</kbd> / <kbd>Num6</kbd> |
| Camera View | 通过活动相机观看（再按一次退出）。 | <kbd>Num0</kbd> |
| Set Active Camera | 把活动对象设为活动相机。 | <kbd>Ctrl+Num0</kbd> |
| Frame Selected | 让视图以所选内容为中心，并调整缩放。 | <kbd>Num.</kbd>（数字小键盘上的点） |
| Local View | 只显示所选内容并将其填满视图；再按一次恢复显示全部。 | <kbd>/</kbd> |
| 导入模型 | 打开文件浏览器，导入 OBJ、FBX、glTF 或 GLB（仅限 PC）。 | <kbd>Num9</kbd> |

### 使用手指（Android、Web 触屏）

| 功能 | 说明 | 手势 |
|---|---|---|
| 环绕 | 旋转视图。 | 单指拖动。 |
| 缩放和平移 | 双指捏合可拉近或拉远；双指一起移动可平移。 | 双指。 |
| Select | 选择手指下方的对象。 | 点一下并抬起，中间不要移动。 |
| Frame Selected | 与 <kbd>Num.</kbd> 相同。 | 双指双击。 |
| 触屏修饰键 | 工具栏上的 Shift 和 Ctrl 按钮：按下后保持开启（绿色），用于加选或选择路径。 | 点击 Shift / Ctrl。 |
| View（导航） | 在 Edit Mode 和各绘制模式中，手指用来导航，而不是选择或绘制。 | 工具栏上显示器形状的按钮。 |

### 使用 Nokia N95 的键盘

| 功能 | 按键 |
|---|---|
| 环绕 | 方向键（按住） |
| 缩放 / Frame Selected | <kbd>0</kbd> + 上/下 / 单按 <kbd>0</kbd> |
| 平移 | <kbd>\*</kbd> + 方向键 |
| 视图（前、侧、顶）/ Perspective-Orthographic | <kbd>#</kbd> + 方向键 / 单按 <kbd>#</kbd> |
| Move / Rotate / Scale（变换过程中：X / Y / Z 轴） | <kbd>1</kbd> / <kbd>2</kbd> / <kbd>3</kbd> |
| Confirm / Cancel | OK（中键）/ <kbd>C</kbd> |
| Delete（没有可取消的操作时）/ Undo 和 Redo | <kbd>C</kbd> / <kbd>C</kbd> + 左或右 |
| 切换着色模式 | <kbd>6</kbd> |
| Extrude（Edit Mode） | <kbd>7</kbd> |
| Loop Cut（Edit Mode）/ Camera View（非编辑状态下） | <kbd>8</kbd> |
| Lock Orbit | <kbd>9</kbd> |
| Lock the camera to the view（通过相机观看时） | 左软键 → View 菜单 |
| 循环切换选择 / 加选、减选、全选或全不选 | Pencil 键 / Pencil 键 + 方向键 |
| 菜单栏 / 虚拟光标 | 左 / 右软键 |
| 切换视口、调整大小、最大化 | 绿色键（单按、+ 方向键、+ OK） |
| 全屏游戏模式 | 绿色键 + <kbd>0</kbd>（再按一次：画面铺满整个屏幕；再按一次：恢复）。没有 UI、没有相机边框，也没有周围的暗色区域（只有当相机声明的宽高比与屏幕不同时才会留下黑边，直到第二次按下），方向键也不会平移：看到的就是游戏本来的样子 |

N95 也支持蓝牙鼠标和键盘。输入文字时使用 T9。

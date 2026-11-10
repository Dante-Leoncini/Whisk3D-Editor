# Properties

> [!NOTE]
> ![Nokia](img/icono-nokia.svg) Nokia N95 支持鼠标和键盘（通过蓝牙）。本页中关于 Nokia 的部分，讲的是不用鼠标和键盘、只用手机按键时怎么操作。

**Properties** 是显示**活动对象**（也就是你最后选中的那个对象）数据的视口：它的位置、名称、材质，灯光的颜色，网格的修改器。它按**选项卡**组织，每个选项卡里有若干**卡片**，卡片里是一行行的数值。你在这里修改的几乎所有内容，都会立刻在 3D Viewport 中显示出来。

正如[视口](viewports.md#活动视口)中所讲的，按键会发送到活动视口，也就是带绿色边框的那个。

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，Properties 位于 3D Viewport 的右边：按**绿色键** ![](img/tecla-verde.svg) 让它成为活动视口。回到这里时，会停在你上次离开的那一行。

## 选项卡

选项卡就是 Properties 菜单栏上的图标。每个选项卡显示一组卡片。

选项卡**会随活动对象变化**：每种对象只显示对它有用的选项卡。网格有 Material、3D Mesh 和 Modifiers；灯光在材质的位置上是 Light 选项卡；相机则是 Camera 选项卡。

| 选项卡 | 何时出现 |
|---|---|
| File / Render（软盘图标） | 始终出现，即使什么都没有选中。 |
| Object | 任何 3D 对象：位置、旋转、缩放、名称和可见性。 |
| 对象类型的选项卡 | 网格是 Material，灯光是 Light，相机是 Camera，其他类型依此类推。它的图标会随类型变化。 |
| 3D Mesh 和 Modifiers | 仅限网格。 |
| Transform | 仅限处于 Edit Mode 的网格。 |
| Constraints | 任何 3D 对象。 |
| Scripts | 任何对象。 |
| Animation | 始终出现。 |

对象类型的选项卡位置不变：如果你正在看网格的材质，然后选中一盏灯光，就会切换到 Light 选项卡。如果你所在的选项卡在新对象上不存在（比如选中灯光时的 Modifiers），Properties 会回到 Object 选项卡。

![选中立方体时，Properties 显示网格的选项卡，停在材质选项卡上；选中灯光后，选项卡随之变化，材质选项卡变成了灯光选项卡；选中相机后，又变成了相机选项卡](img/propiedades-pestanas.webp)

| | 如何切换选项卡 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 单击选项卡。如果放不下所有选项卡，在栏上滚动滚轮。 |
| ![Android](img/icono-android.svg) 触屏 | 点一下选项卡。如果放不下所有选项卡，横向拖动栏。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 用**上**方向键一直往上，到第一张卡片的标题后再按一次：当前选项卡会变成绿色。**左**和**右**切换选项卡，**下**回到卡片。在第一个选项卡上，按左会打开角落按钮的菜单。 |

## 卡片

**卡片**就是每个选项卡中的方框：一个标题和下面的各行。标题上的箭头表示卡片是展开还是折叠。把不用的卡片折叠起来，常用的就都在手边了。

![在立方体的 Object 选项卡中，单击 Transform 卡片的标题将其折叠，下面的卡片随之上移；再单击一次标题，卡片重新展开](img/propiedades-tarjetas.webp)

| | 如何折叠和展开 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 单击标题。 |
| ![Android](img/icono-android.svg) 触屏 | 点一下标题。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 在标题上按 <kbd>OK</kbd>。在卡片的任意一行上按**左**方向键会折叠卡片，在标题上按**右**方向键则会展开它。 |

如果卡片在视口里放不下，面板可以滚动：在 PC 上用鼠标滚轮，在触屏上用手指上下拖动。在 Nokia 上，面板会自动跟随选中的那一行。

### 用按键移动

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上有一个**选中行**：它的名称显示为绿色，数值带有白色边框。**上**和**下**在各行之间移动，也会从一张卡片移到下一张；<kbd>OK</kbd> 使用这一行：编辑数字、勾选复选框、打开下拉框。编辑期间，边框会变成绿色。

![PC](img/icono-pc.svg) 在 PC 上，鼠标位于 Properties 上方时，方向键和 <kbd>Enter</kbd> 的作用相同，<kbd>Esc</kbd> 取消正在进行的编辑。

## 数字

大多数行都是**数字**：名称在左边，数值在右边，比如 Transform 卡片中的 Location、Rotation 和 Scale。数字可以**输入**，也可以**拖动**。每个数字都有自己的变化幅度，有些还有上下限：灯光的 GL Light 只能从 0 到 7。

### 输入数字

![PC](img/icono-pc.svg) 在数字上单击一下，不要移动鼠标：数值会被选中，你输入的内容会替换它。<kbd>Enter</kbd> 应用，<kbd>Esc</kbd> 恢复原样。在其他任何地方单击也会应用。

![单击立方体的 Location X，数值被选中；输入 2，按 Enter 后立方体沿 X 移到 2 米处](img/propiedades-numero-raton.webp)

![Android](img/icono-android.svg) 点一下数字，底部会打开 Whisk3D 的**数字键盘**，上面显示这一行的名称和数值。键盘上有数字、小数点、用来删除的 **DEL**，以及移动光标的方向键。**OK** 应用数值，**X** 取消；点键盘外面也会应用。

![Android](img/icono-android.svg) 数字键盘还能**计算**：用 <kbd>+</kbd>、<kbd>-</kbd>、<kbd>\*</kbd>、<kbd>/</kbd> 和括号，可以输入 `2*3` 或 `1.5+0.25`，点 OK 后得到计算结果。<kbd>-</kbd> 也可以用来输入负数。

![点一下立方体的 Location X，底部打开数字键盘；输入 2*3，点 OK 后数值变成 6，立方体随之移动](img/propiedades-numero-dedo.webp)

![Nokia](img/icono-nokia.svg) 用方向键移到这一行，按 <kbd>OK</kbd>：数值会被选中，手机按键用来输入数字（Properties 的栏上显示 **123**）。你输入的第一个数字会替换原来的数值。<kbd>\*</kbd> 是小数点，<kbd>C</kbd> 删除，**左**和**右**移动光标，<kbd>OK</kbd> 应用，**左软键**取消。

![一部 Nokia N95：绿色键切换到 Properties，下方向键移到 Location X，按 OK 选中数值，输入 2，再按 OK，立方体随之移动](img/propiedades-nokia.webp)

### 拖动数字

![PC](img/icono-pc.svg) 在数字上按住鼠标键，横向移动鼠标：往右数值变大，往左数值变小。如果指针到达视口的边缘，它会从另一边出现，你可以继续拖动。松开后，数值就确定了。

![在 Location X 上按住鼠标键，鼠标向右再向左移动，立方体跟着数字移动](img/propiedades-arrastrar-raton.webp)

![Android](img/icono-android.svg) 把手指放在数字上（不是放在名称上），横向拖动。如果上下拖动，滚动的会是面板，而不是改变数值。

![一根手指放在 Location X 上，向右再向左拖动，立方体跟着数字移动](img/propiedades-arrastrar-dedo.webp)

## 下拉框

**下拉框**是一个显示当前选项的按钮，比如灯光的 **Type**（Point、Sun 或 Spot）或旋转的 **Mode**。激活它会打开选项列表，当前选项显示为绿色；选择另一项后，列表就会关闭。

![选中一盏灯光，打开 Light 卡片中的 Type 下拉框，Point 显示为绿色，然后选择 Sun](img/propiedades-desplegable.webp)

| | 如何打开和选择 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 单击按钮，再单击选项。 |
| ![Android](img/icono-android.svg) 触屏 | 点一下按钮，再点一下选项。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | <kbd>OK</kbd> 打开，**上**和**下**浏览选项，<kbd>OK</kbd> 选择。<kbd>C</kbd> 关闭下拉框，不做任何更改。 |

## 复选框

**复选框**只有开和关：打勾表示是，空着表示否。它位于这一行的右侧，比如材质中的 **Lighting**：关闭后，对象不再接受灯光照射，显示为自身的颜色，没有明暗。单击或点一下这一行（复选框或它的名称）就能切换，在 Nokia 上则按 <kbd>OK</kbd>。

![在立方体的材质中关闭 Lighting，立方体变成一片平涂的白色，没有光照；再打开，它又恢复了光照效果](img/propiedades-casilla.webp)

## 文本

**文本框**存放名称或路径，比如 Transform 卡片末尾的对象 **Name**。激活它后，光标会出现在文本末尾。新名称也会显示在 Outliner 中。

![编辑立方体的 Name：删掉原来的名称，输入 Box，确认后 Outliner 显示新的名称](img/propiedades-texto.webp)

- ![PC](img/icono-pc.svg) **在 PC 上：**单击文本框，然后输入。<kbd>Enter</kbd>（或在别处单击）确认，<kbd>Esc</kbd> 恢复之前的文本。
- ![Android](img/icono-android.svg) **在触屏上：**点一下会打开 Whisk3D 的键盘，其中包括 ñ。**Aa** 切换大写，**123** 切换到数字和符号，**DEL** 删除，**OK** 确认。在网页版中，会打开手机自带的键盘。
- ![Nokia](img/icono-nokia.svg) **在 Nokia N95 上：**按 <kbd>OK</kbd>，然后像写短信一样用手机键盘输入：每个键按多次可以得到不同的字母。<kbd>#</kbd> 在 Abc、abc 和 ABC 之间切换，长按则切换到 123（Properties 的栏上会显示当前模式）。<kbd>0</kbd> 是空格，<kbd>1</kbd> 是标点符号，<kbd>\*</kbd> 打开更多符号。<kbd>C</kbd> 删除，**左**和**右**移动光标，<kbd>OK</kbd> 确认，**左软键**取消。

## 颜色

**颜色**行在右侧显示一个带有该颜色的小方块，比如灯光卡片中的 **Diffuse**、**Ambient** 和 **Specular**。激活它会打开**颜色选择器**：

- **色环**选择色相和饱和度，旁边的条选择明度（颜色有多亮）；
- **RGB**、**HSV** 和 **Hex** 选项卡用数字显示颜色：Red、Green 和 Blue；Hue、Saturation 和 Value；或者十六进制代码（`#FF8800`），并带有 Copy 和 Paste；
- **Alpha** 是透明度；
- **OK** 保留颜色，**Cancel** 恢复之前的颜色。

颜色会随着你的调整实时变化，所以确认之前就能在场景中看到效果。在选择器外面单击或点一下，也会关闭它并保留颜色。

![选中一盏灯光，单击 Diffuse 打开颜色选择器：色环、明度条，以及 RGB、HSV 和 Hex 模式](img/propiedades-color.webp)

![PC](img/icono-pc.svg) ![Android](img/icono-android.svg) 用鼠标或手指，在色环和条里面拖动；数字则和 Properties 中一样横向拖动。

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，**上**和**下**依次经过色环、条、选项卡、各行数字以及 OK / Cancel。在数字行上，**左**和**右**改变数值（按住时会越来越快）。在色环上按 <kbd>OK</kbd> 进入编辑：**左**和**右**转动色相，**上**和**下**改变饱和度，<kbd>OK</kbd> 退出。条的编辑方式相同，用上和下。<kbd>C</kbd> 关闭选择器并恢复之前的颜色。

## 列表

有些卡片里有**列表**：网格的修改器、网格部分、UV 贴图。选中的项会被标出，下面的内容显示它的数据或对它进行操作。列表下面有几行**按钮**。

**Move Up** 和 **Move Down** 改变选中项的位置。顺序很重要：修改器按从上到下的顺序应用，网格的各部分也按这个顺序绘制。列表中有两项或更多时，才会出现这两个按钮。

![一个带有两个修改器的立方体的 Modifiers 选项卡：选中的修改器在列表下方，按 Move Up 后移到了上面](img/propiedades-lista.webp)

| | 如何使用列表 |
|---|---|
| ![PC](img/icono-pc.svg) PC | 单击选择。如果放不下所有项，在列表上滚动滚轮。 |
| ![Android](img/icono-android.svg) 触屏 | 点一下选择。如果放不下所有项，在列表上用手指上下拖动。 |
| ![Nokia](img/icono-nokia.svg) Nokia N95 | 在列表上按 <kbd>OK</kbd>，**上**和**下**选择，<kbd>OK</kbd> 退出。在一行按钮上，**左**和**右**在按钮之间移动（边框标出当前按钮），<kbd>OK</kbd> 按下它。 |

## 菱形：为属性制作动画

可以制作动画的属性右侧有一个**菱形**；没有菱形的属性不能制作动画。单击或点一下菱形会插入一个**关键帧**：在 Timeline 的当前帧保存这个属性的数值。在不同的帧上插入关键帧后，播放动画时属性就会自动变化。

菱形的颜色表示该属性**在当前帧**的状态：

| 菱形 | 含义 | 单击的效果 |
|---|---|---|
| 灰色 | 该属性没有动画。 | 用当前数值插入第一个关键帧。 |
| 淡绿色 | 有动画，但这一帧没有关键帧。 | 用当前数值插入一个关键帧。 |
| 绿色 | 这一帧有关键帧。 | 删除它。 |

修改数值不会改变菱形的颜色：在插入关键帧之前，新数值不会进入动画；如果在那之前换到别的帧，属性会恢复为动画中的值。在已经有关键帧的帧上（菱形为绿色），要保存新数值需要单击两次：第一次删除关键帧，第二次用当前数值重新插入。

![立方体的 Location X：菱形是灰色的；单击后变成绿色；切换到 Timeline 的另一帧时变成淡绿色；修改数值后菱形仍是淡绿色，再单击一下就变成绿色](img/propiedades-rombo.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，停在这一行时，**右**方向键移到菱形上（用白色边框标出），<kbd>OK</kbd> 插入或删除关键帧，**左**方向键回到数值。

在 Animation 选项卡中，**Active camera** 卡片决定动画的每个时刻由哪台相机负责：也就是场景中的镜头切换。它的菱形多一种颜色，**更亮的绿色**：表示选中的相机不是动画在这一帧指定的那台，也就是一个还没保存的更改。单击一下，就会在当前帧保存这次切换。

动画会在后面有专门的教程。

所有选项卡及其卡片都在手册中：[Properties](propiedades.md)。返回[视口](viewports.md)。

## 每个选项卡的内容

| 选项卡 | 内容 |
|---|---|
| File / Render（软盘图标） | **File**：打开、保存、另存为、保存版本、提取资源。**Render**：路径、尺寸、额外通道、背景、**Ray Tracing**（光线、采样、通道）、Render Image 和 Render Animation。**Game**：平台（Windows、Symbian、Linux .deb、AppImage、WebGL、Android、Android 2.3）、窗口、方向、图标、物理、声音、音量、**Resolution**（游戏绘制所用的屏幕百分比：在 Android 2.3 上由系统拉伸，在这些手机上绘制更少的像素能得到更高的帧率）和 Build game（如果有手机通过 adb 连接，会把游戏安装到手机上并打开）。**Export**：OBJ、glTF、GLB、STL（只有三角形，用于 3D 打印或导入 CAD 程序）。**Settings**：语言、抗锯齿、图形、皮肤、日志、缩放、**Keyboard Shortcuts…** 和 Save Changes。 |
| Object | 位置、旋转（Euler、四元数或轴角）、缩放、名称、可见、可渲染；对象的动画和调色板。 |
| Contextual | 取决于对象类型：网格部分和**材质**（纹理、光照、顶点颜色、透明、反射、法线贴图、混合、深度、纹理层、**Advanced: custom shader**（见[手写着色器](../editor/shaders.md)）；启用光线追踪时还有 Roughness、Metallic、Transmission 和 IOR）、灯光、相机、雾、粒子、物理、碰撞盒、骨架和骨骼、2D 元素、预制体、例程。 |
| 3D Mesh | 使用的网格（以及有多少对象共享它）、UV 贴图、颜色层、顶点组、UV 组。在 Library 中选中某个资源时：显示它的数据和存放位置（.w3d 内部或外部）。 |
| Modifiers | 参见[修改器](../editor/objeto-malla.md#modifiers-选项卡)。 |
| Transform | 仅在 Edit Mode 中：所选内容及其 UV 的中心，可以编辑。 |
| Armature 2D | 网格的 2D 骨架和骨骼。 |
| Constraints | Copy Location、Copy Rotation、Billboard（朝向相机）、Child Of。 |
| Scripts | 对象的 Lua 脚本及其属性（参见 [Lua 文档](../lua/README.md)）。 |
| Animation | 活动动画、混合层（Mix）、所选关键帧的精确数值，以及镜头切换。 |

## 例程

例程是在 Properties 中搭建的一系列绘制步骤（平移、旋转、设置纹理、绘制网格、调用另一个例程、Call Lua…）。这是在 N95 上绘制最快的方式。

| 功能 | 说明 | 快捷键 |
|---|---|---|
| 添加 / 移除步骤 | 按主题分组添加步骤，或移除选中的步骤。 | 按钮；<kbd>X</kbd> 或 <kbd>Delete</kbd> 移除 |
| Grab step | 用方向键重新排列该步骤；<kbd>Enter</kbd> 确认。 | <kbd>G</kbd> |
| Insert Keyframe | 在当前帧为该步骤的数值记录动画。 | <kbd>I</kbd> |
| Repeat | 整个列表连续运行多少次（例程循环）。1 = 普通例程。 | Repeat 字段 |
| Render mode / Use | 每种着色模式使用不同的列表，或所有模式共用同一个列表。 | 下拉框 |

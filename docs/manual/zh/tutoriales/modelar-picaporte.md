# 制作门把手的模型

我们从上一个教程[更改原点：一扇门](puerta.md)中的门开始：它的尺寸是 0.90 × 0.10 × 2 米，原点在铰链上。我们要给它做一个由两部分组成的门把手：一块贴在门上的**底板**，和一个可以转动的**把手**。

在这个过程中，你会学到：

- 复制一个面，并以它为基础做出一个新的部件；
- 把网格的一部分**分离**成另一个对象（<kbd>P</kbd>），然后再把它**合并**回来（<kbd>Ctrl</kbd> + <kbd>J</kbd>）；
- 给网格的每个部分指定不同的**材质**；
- 使用 **Mirror** 修改器，让门的两面都有门把手；
- 用 **X-Ray** 查看那些永远看不到的面，并把它们删掉；
- 用 3D 光标把**原点**放到把手转动的位置；
- 把把手**设置父级**到门上，让它们一起移动。

## 1. 底板

1. 选中门，进入 **Edit Mode**（<kbd>Tab</kbd>），然后在 **Vertex / Edge / Face** 按钮中选择 **Face**。
2. 单击门正面那个大面。
3. **复制它：**<kbd>Shift</kbd> + <kbd>D</kbd>。副本会跟着鼠标走；右键单击（或按 <kbd>Esc</kbd>）把它留在原处，正好叠在原来的面上。
4. **缩小它**，直到变成一个小正方形。凭眼睛：按 <kbd>S</kbd> 并移动鼠标；然后按 <kbd>S</kbd> <kbd>Z</kbd> 单独调整高度。用数字：<kbd>S</kbd> `0.15` <kbd>Enter</kbd> 和 <kbd>S</kbd> <kbd>Z</kbd> `0.45` <kbd>Enter</kbd> 会得到一个 13.5 厘米的正方形。
5. **把它移到**铰链的另一侧，也就是装门把手的地方：<kbd>G</kbd> <kbd>X</kbd> `-0.31` <kbd>Enter</kbd>。
6. 稍微**挤出**一点，让它有厚度：<kbd>E</kbd> `0.03` <kbd>Enter</kbd>（3 厘米）。

![单击门的正面；Shift + D 和 Esc 把副本留在上面；S 0.15 和 S Z 0.45 把它缩成一个正方形；G X -0.31 把它移到铰链的另一侧，E 0.03 给它厚度](img/picaporte-placa-raton.webp)

![Android](img/icono-android.svg) 在触屏上：点一下这个面，然后选择 **Mesh → Duplicate**（副本会叠在上面）。之后用工具栏上的 **Scale**、**Move** 和 **Extrude**，在顶部栏的键盘上输入数字，再点对勾。

![手指点一下正面；Mesh、Duplicate；用顶部栏的键盘执行 Scale 0.15、Scale Z 0.45、Move X -0.31 和 Extrude 0.03](img/picaporte-placa-dedo.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上：用左软键选择 **Mesh → Duplicate**；然后用 <kbd>3</kbd>（缩放）、<kbd>1</kbd>（移动）和 <kbd>7</kbd>（挤出），按住铅笔键输入数字，再按 <kbd>OK</kbd>。

![一部 Nokia N95：3 和铅笔键 + 0.15；3、3（Z）和 0.45；1、1（X）和 -0.31；7 和 0.03；每一步都按 OK](img/picaporte-placa-nokia.webp)

## 2. 分离底板

底板现在还是门的网格的一部分。我们把它变成一个独立的对象：

1. 选中底板，按 <kbd>L</kbd>（**Select Linked**）选中整个部件：底板的 6 个面，不包括门。
2. 按 <kbd>P</kbd>（**Separate**）：底板变成一个新对象 **Door.001**。你可以在 Outliner 中看到它。

![鼠标位于底板上，按 L 选中整个部件，按 P 把它分离：Outliner 中出现 Door.001](img/picaporte-separar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) 在触屏和 N95 上：选择 **Select → Select Linked**，再点一下底板；然后选择 **Mesh → Separate**。

![Select、Select Linked，点一下底板；Mesh、Separate](img/picaporte-separar-dedo.webp)

分离之后，门上不会留下底板的任何东西：既没有它的面，也没有它的线。

## 3. 材质：木头和金属

现在每个部分都可以有自己的材质了。在 **Properties** 的 **Material** 选项卡中：

1. 选中门。在 **Rename Material** 中输入 `Wood`，在 **Color** 中选一种棕色。
2. 选中底板（Door.001）。因为它是从门上分出来的，所以也带着 Wood 材质：点 **New Material**，命名为 `Metal`，给它一种灰色。

![木门上装着金属底板，以及底板的 Material 卡片，显示的是 Metal 材质](img/picaporte-materiales.webp)

## 4. 门的另一面：Mirror

门把手两面都有。我们不再做一块底板，而是使用 **Mirror** 修改器：

1. 选中底板，在 **Properties → Modifiers** 中添加 **Mirror**。
2. 关闭 **Mirror X**，打开 **Mirror Y**：另一块底板出现在门的另一面。

![从侧面看：在 Modifiers 中添加 Mirror，关闭 Mirror X，打开 Mirror Y，门的另一面出现了底板](img/picaporte-espejo-raton.webp)

镜像以对象的**原点**为中心。底板是从门上分出来的，所以原点和门相同：在铰链上，正好位于门厚度的中间。因此副本正好位于另一面。参见 [Mirror](../editor/modificador-espejo.md)。

## 5. 把手

把手用同样的技巧，从底板上做出来：

1. 选中底板，按数字小键盘上的 <kbd>/</kbd>（**Local View**）：只显示底板，处理起来更方便。进入 **Edit Mode**。
2. 选中底板的正面，**复制它**（<kbd>Shift</kbd> + <kbd>D</kbd> 再右键单击），然后缩小：<kbd>S</kbd> `0.6` <kbd>Enter</kbd>。
3. 向外**挤出**：<kbd>E</kbd> `0.08` <kbd>Enter</kbd>。这就是把手的转轴。

![在底板的局部视图中：单击它的正面，Shift + D 和 Esc，S 0.6 把它缩小，E 0.08 把它向外挤出：这就是把手的转轴](img/picaporte-manija-raton.webp)
4. 切换到 **Edge**，选中靠门边缘那一侧的外边。把它向外移动：<kbd>G</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>。正面就变成了斜的。
5. 切换到 **Face**，选中那个斜面。**沿 X** 挤出，而不是沿它的法线：<kbd>E</kbd>、<kbd>X</kbd>、`0.34` 和 <kbd>Enter</kbd>。把手的手柄就出来了。
6. 末端是斜的。要把它弄平，就沿 X 把它缩放为零：<kbd>S</kbd> <kbd>X</kbd> `0` <kbd>Enter</kbd>。它的所有顶点都位于同一个 X 上：末端就变平了。

![在 Edge 中，用 G Y 0.05 把外边移出去；在 Face 中，用 E X 0.34 沿 X 挤出斜面，S X 0 把末端弄平](img/picaporte-brazo-raton.webp)
7. 按 <kbd>L</kbd> 选中整个把手，按 <kbd>P</kbd> 把它分离：它成为 **Door.002**。给它起个名字：在 Outliner 中双击，输入 `Lever`。

![L 和 P 把把手分离出来；在 Outliner 中双击，把它命名为 Lever](img/picaporte-manija-separar-raton.webp)

![Android](img/icono-android.svg) 在触屏上：点一下这个面，**Mesh → Duplicate**，**Scale** `0.6` 和 **Extrude** `0.08`；**Edge**，点一下外边，**Move** **Y** `0.05`；**Face**，点一下斜面，**Extrude** **X** `0.34` 和 **Scale** **X** `0`。要分离，选择 **Select → Select Linked** 和 **Mesh → Separate**。

![用手指：底板的面，Duplicate、Scale 0.6、Extrude 0.08；用 Move Y 0.05 把外边移出去；用 Extrude X 0.34 挤出斜面，Scale X 0 把它弄平](img/picaporte-manija-dedo.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上：<kbd>3</kbd> 和 `0.6`，<kbd>7</kbd> 和 `0.08`；选中边（用虚拟鼠标），<kbd>1</kbd>、<kbd>2</kbd>（Y 轴）和 `0.05`；选中斜面，<kbd>7</kbd>、<kbd>1</kbd>（X 轴）和 `0.34`；<kbd>3</kbd>、<kbd>1</kbd> 和 `0`。

![一部 Nokia N95：3 和 0.6；7 和 0.08；1、2 和 0.05 把边移出去；7、1 和 0.34 沿 X 挤出手柄；3、1 和 0 把末端弄平](img/picaporte-manija-nokia.webp)

把手是从底板上做出来的，所以带着底板的材质（Metal）和 Mirror 修改器。

## 6. 把底板合并到门上

底板不会单独移动：它是门的一部分。我们把它合并回去：

1. 选中底板，在 **Modifiers** 中点 **Apply Modifier**：镜像变成了真正的几何体。
2. 选中底板，再用 <kbd>Shift</kbd> + 单击选中门（门成为活动对象），然后按 <kbd>Ctrl</kbd> + <kbd>J</kbd>（**Join**）。现在它们是同一个网格，带有两种材质。

![Apply Modifier 把底板的镜像变成几何体；单击底板，Shift + 单击门，再按 Ctrl + J：Outliner 中的 Door.001 消失了，它现在是门的一部分](img/picaporte-unir-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) 在触屏和 N95 上：**Object → Join**。

![选中底板和门后，选择 Object、Join：Door.001 从 Outliner 中消失](img/picaporte-unir-dedo.webp)

网格的各部分就是这样分离和合并的：<kbd>P</kbd> 分离，<kbd>Ctrl</kbd> + <kbd>J</kbd> 合并。

## 7. 永远看不到的面

底板贴在门上。每块底板的背面都被挡住了：永远不会被看到，但程序照样会绘制它。要看到它：

1. 选中门，进入 **Edit Mode**。
2. 在 **Overlays** 中打开 **X-Ray**：网格变成半透明，可以看到背面。
3. 选中每块底板的背面（贴着门的那一面），把它们删掉：<kbd>X</kbd> → **Faces**。

![处于 Edit Mode 的门；Overlays、X-Ray；选中两块底板的背面，X、Faces 把它们删掉](img/picaporte-rayosx-raton.webp)

最好删掉看不到的几何体：这些面画了也是白画。在小模型里感觉不出来，但在大场景中或者在手机上，就能换来性能。

## 8. 把手绕哪里转：原点

把手要绕自己的轴转，而不是绕门的铰链转。

1. 选中把手，应用它的 Mirror（**Modifiers → Apply Modifier**）。
2. 进入 **Edit Mode**，打开 **X-Ray**。选中把手背面的两个面，也就是贴着底板的那两个（每面一个）。
3. <kbd>Shift</kbd> + <kbd>S</kbd> → **Cursor to Selected**：3D 光标会位于这两个面的中间，正好在把手的轴上。
4. 这两个面也看不到：把它们删掉（<kbd>X</kbd> → **Faces**）。
5. 回到 **Object Mode**，把原点移到光标处：<kbd>W</kbd>（或右键单击）→ **Set Origin → Origin to 3D Cursor**。

![把手：Apply Modifier；在 Edit Mode 中选中它背面的两个面，Shift + S、Cursor to Selected、X、Faces；Tab，然后 Set Origin、Origin to 3D Cursor](img/picaporte-origen-raton.webp)

这和我们在[更改原点：一扇门](puerta.md#把原点移到铰链处)中对铰链所做的一样。

## 9. 让把手跟着门一起动

试着转动门：把手会悬在半空中。它们是两个互不相关的对象。撤销这次旋转（<kbd>Ctrl</kbd> + <kbd>Z</kbd>）。

![R Z -70 转动门，把手悬在半空中；Ctrl + Z 把它恢复](img/picaporte-flota-raton.webp)

要让把手跟着门一起动，就要**设置父级**：让把手成为门的**子级**。

1. 选中把手，再用 <kbd>Shift</kbd> + 单击选中门（门，也就是父级，成为活动对象）。
2. <kbd>Ctrl</kbd> + <kbd>P</kbd> → **Object (Keep Transform)**。在 Outliner 中，把手到了门的里面。

只选中门（单击它），再转动它：现在把手跟着门一起动了。而且把手仍然可以绕自己的轴转动，就像真正的门把手一样。如果两个都选中了，只有门会转动，把手作为它的子级跟着它走。

![单击把手，Shift + 单击门，Ctrl + P 和 Object (Keep Transform)；只单击门，再按 R Z -70：现在把手跟着门一起动了](img/picaporte-emparentar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) 在触屏和 N95 上：**Object → Set Parent → Object (Keep Transform)**。

![选中把手和门后，选择 Object、Set Parent、Object (Keep Transform)：在 Outliner 中，把手到了门的里面](img/picaporte-emparentar-dedo.webp)

下一个教程中，我们要为门制作动画：[为门制作动画](animar-puerta.md)。

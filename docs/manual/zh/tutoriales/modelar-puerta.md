# 制作门的模型

我们来做第一个简单的模型：一扇门！一开始，用场景里自带的立方体就够了，它的尺寸是 2 × 2 × 2 米。

我（Dante）经常用门来营造空间：这是我学建筑时学到的。门几乎总是 2 米高，而一个人（通常）比门矮。场景里有一扇门，其他所有东西的比例马上就清楚了。

## 让它立在地面上

立方体的中心在地面上，所以有一半在地面以下。我们把它沿 Z 轴升高 1 米，让它立在地面上。选中立方体后：

![PC](img/icono-pc.svg) 按 <kbd>G</kbd>：立方体会跟随鼠标。再按 <kbd>Z</kbd>：它被约束在 Z 轴上（出现一条蓝线）。输入 `1`，按 <kbd>Enter</kbd>。

![按 G 后立方体跟随鼠标；按 Z 后它停在竖直轴的蓝线上；输入 1，它正好升高 1 米，按 Enter 把它留在那里，立在地面上](img/modelar-subir-raton.webp)

![Android](img/icono-android.svg) 点工具栏上的 **Move**，然后点 **Z**，点一下顶部的栏打开数字键盘，输入 `1`，点 **OK**，再点绿色对勾。

![在工具栏上点 Move 和 Z；点一下顶部的栏打开数字键盘，输入 1 并点 OK，立方体升高，点对勾完成操作；底部出现 Move 面板，点一下展开，点 Accept 关闭](img/modelar-subir-dedo.webp)

![Nokia](img/icono-nokia.svg) <kbd>1</kbd>（移动），<kbd>3</kbd>（Z 轴），按住**铅笔**键再按 <kbd>1</kbd>，然后按 <kbd>OK</kbd>。

![一部 Nokia N95：按 1 开始移动，按 3 约束到 Z 轴，铅笔键加 1 输入数值，按 OK 确认](img/modelar-subir-nokia.webp)

在各种设备上如何输入数字，请参阅[使用精确数值](modo-objeto.md#使用精确数值)。

### 修正上一步操作

移动、旋转或缩放之后，左下角会出现一个小面板 **调整上一步操作**，取代工具栏。折叠时它只显示你做了什么（例如 **Move**）；点一下或单击它的标题就会展开：

![展开后的 Move 面板：X 0、Y 0 和 Z 1 米，方向 Global，比例编辑复选框，以及 Cancel 和 Accept 按钮](img/modelar-ficha.webp)

每个数值都可以修正：如果你想要的不是 1 米而是 1.2 米，点 Z，输入 `1.2`，操作就会用新的数值重新执行。**Accept** 关闭面板，工具栏重新出现；**Cancel** 撤销这次操作。

## 塑造形状

给门建模有两种方法。

### 最简单但我不推荐的方法：缩放对象

在 Object Mode 中，沿 X 轴和 Y 轴缩放立方体，直到它变成门的形状。例如，<kbd>S</kbd> <kbd>X</kbd> `0.45` <kbd>Enter</kbd>（宽 90 厘米），以及 <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>（厚 10 厘米）。

我不推荐这种方法，因为这样对象在每个轴上的缩放都不一样（0.45、0.05 和 1），而理想情况是三个轴的缩放都为 **1**（参见[缩放最好保持为 1](modo-objeto.md#缩放最好保持为-1)）。如果你真的这样做了，**一定要应用变换**：<kbd>Ctrl</kbd> + <kbd>A</kbd> → **Scale**（或 Object → Apply → Scale）。形状不会改变，缩放会回到 1。

![在 Object Mode 中，S X 0.45 和 S Y 0.05 把立方体变成门的形状，Properties 显示每个轴的缩放都不一样；按 Ctrl+A 选择 Scale 应用缩放，缩放回到 1](img/modelar-escalar-raton.webp)

![Android](img/icono-android.svg) 在触屏上：**Scale**、**X**、顶部的栏、`0.45` 和 OK，然后用 **Y** 和 `0.05` 再做一次。之后选择 **Object → Apply → Scale**。

![用工具栏和数字键盘沿 X 缩放 0.45、沿 Y 缩放 0.05；然后选择 Object、Apply、Scale，缩放回到 1](img/modelar-escalar-dedo.webp)

### 我推荐的方法：Edit Mode

选中立方体，进入 Edit Mode：按 <kbd>Tab</kbd>，或者在 3D Viewport 的模式按钮中选择 **Edit Mode**。进入后，所有顶点都处于选中状态。

在 Edit Mode 中，你选择的是**顶点**、**边**和**面**：这是最基本的。和 Object Mode 一样，它们也可以移动、旋转和缩放（参见 [Edit Mode](../editor/modo-edicion.md)）。

全部选中后，沿 X 和 Y 缩小，直到门变成你想要的样子。Z 不需要改，因为它已经是 2 米高了！由于改变的是网格而不是对象，对象的缩放仍然是 1。

**凭眼睛，用 Gizmo：**单击（或点一下）Gizmo 的中心，它会切换到 Rotate，再一下切换到 **Scale**。抓住红色的端点（X），朝中心拖动，直到你想要的宽度；然后拖动绿色的端点（Y），直到它像门一样薄。

![在 Edit Mode 中，在 Gizmo 中心单击两次切换到缩放；把红色端点朝中心拖动，立方体变窄；再拖动绿色端点，它就变得像门一样薄](img/modelar-gizmo-raton.webp)

![用手指也一样：在 Gizmo 中心点两下，然后把红色和绿色端点朝中心拖动](img/modelar-gizmo-dedo.webp)

**用数字：**<kbd>S</kbd>、<kbd>X</kbd>、`0.45`、<kbd>Enter</kbd>，以及 <kbd>S</kbd>、<kbd>Y</kbd>、`0.05`、<kbd>Enter</kbd>。

![按 Tab 后立方体进入 Edit Mode，全部显示为绿色；S、X、0.45 和 Enter 让它变窄，S、Y、0.05 和 Enter 让它变薄；对象的缩放仍然是 1](img/modelar-edicion-raton.webp)

![Android](img/icono-android.svg) 在触屏上：模式按钮 → **Edit Mode**；然后 **Scale**、**X**，点一下顶部的栏，`0.45`、**OK** 和对勾，再用 **Y** 和 `0.05` 做一次。

![模式按钮切换到 Edit Mode；Scale、X、顶部的栏、0.45 和 OK 让它变窄；再用 Y 和 0.05 做一次](img/modelar-edicion-dedo.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上：<kbd>OK</kbd> 切换到 Edit Mode。<kbd>3</kbd>（缩放），<kbd>1</kbd>（X 轴），按住铅笔键输入 `0` <kbd>*</kbd> `45`（<kbd>*</kbd> 是小数点），然后按 <kbd>OK</kbd>。再用 <kbd>2</kbd>（Y 轴）和 `0.05` 做一次。

![一部 Nokia N95：OK 切换到 Edit Mode；3 和 1 沿 X 缩放，铅笔键加 0.45 输入数值，OK 确认；再用 3、2 和 0.05 做一次](img/modelar-edicion-nokia.webp)

## 使用精确尺寸

如果你不想凭眼睛，而是想要一扇正好**宽 90 厘米**的门：在 **Vertex / Edge / Face** 按钮中选择 **Face**，然后单击其中一个侧面。我们知道立方体宽 2 米，所以要减去多出来的部分，让它变成 0.9 米：这个侧面需要移动 **1.1 米**。<kbd>G</kbd> <kbd>X</kbd> `-1.1` <kbd>Enter</kbd>。

![在 Edit Mode 中选择 Face，单击右侧面只选中它，然后 G、X、-1.1 和 Enter 移动它：立方体变成 0.9 米宽](img/modelar-exacto-raton.webp)

![用手指：在选择按钮中选择 Face，点一下侧面，然后 Move、X、顶部的栏、-1.1 和 OK；立方体变成 0.9 米宽](img/modelar-exacto-dedo.webp)

移动时，**X** 轴把移动约束在单个轴上，这样立方体就不会变形：

- ![PC](img/icono-pc.svg) 用 <kbd>X</kbd> 键。
- ![Android](img/icono-android.svg) 点一下工具栏上的 **X**。`-1.1` 用数字键盘输入，点顶部的栏即可打开。
- ![Nokia](img/icono-nokia.svg) 移动时按 <kbd>1</kbd>。数字要按住铅笔键来输入：<kbd>#</kbd> 改变正负号，<kbd>*</kbd> 是小数点。面要用虚拟鼠标来选择（右软键、方向键和 <kbd>OK</kbd>）。

厚度方面，全部选中后，和前面一样按 <kbd>S</kbd> <kbd>Y</kbd> `0.05` <kbd>Enter</kbd>。

我们的第一个 3D 模型完成了！恭喜。

下一个教程中，我们会让门绕着铰链打开：[更改原点：一扇门](puerta.md)。

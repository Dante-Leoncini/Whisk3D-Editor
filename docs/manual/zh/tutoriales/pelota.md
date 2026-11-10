# 让球弹跳

一个不停弹跳、永远弹下去的球：这是每个动画师入门的第一个练习。看起来很简单，但要让它看着舒服，需要用到动画中最重要的两个原则：

- **挤压与拉伸**：球撞到地面时被压扁，离开地面时被拉长。这让它有了重量感和弹性；
- **加速与减速**：上升时越来越慢，下落时越来越快，就像受到重力作用一样。

![完成后的球：一次又一次地弹跳；碰到地面时被压扁，离开时被拉长，到顶部时减速](img/pelota-final.webp)

在这个过程中，你会学到：

- 在地面上放一个 **Empty**，让球朝地面压扁，而不是朝它的中心压扁；
- **复制**关键帧，回到已有的姿态；
- 制作**循环**：一段重复播放、看不出衔接处的动画；
- 用**曲线**让球有重量感。

如果你还没做过任何动画，请先看[为门制作动画](animar-puerta.md)：那里讲了关键帧、Timeline 和曲线。

## 1. 球和它的 Empty

1. 删除立方体，添加一个球体：<kbd>Shift</kbd> + <kbd>A</kbd> → **Mesh → UV Sphere**。它高 2 米，创建时中心在地面上：把它升高，让它立在地面上，<kbd>G</kbd> <kbd>Z</kbd> `1` <kbd>Enter</kbd>。
2. 进入 **Edit Mode**（<kbd>Tab</kbd>），在 **Vertex / Edge / Face** 按钮中选择 **Vertex**。最底部的顶点被球体挡住了：打开 **Overlays → X-Ray**，透过球体看到它，然后单击它。
3. <kbd>Shift</kbd> + <kbd>S</kbd> → **Cursor to Selected**：3D 光标移到球的最低点，也就是地面上。
4. 回到 **Object Mode**（<kbd>Tab</kbd>），关闭 **X-Ray**。
5. <kbd>Shift</kbd> + <kbd>A</kbd> → **Empty**。它会在 3D 光标所在的位置创建：在地面上，正好位于球的下方。
6. 把球**设置父级**到 Empty 上。在 **Outliner** 中，单击 **UVSphere**，再 <kbd>Ctrl</kbd> + 单击 **Empty**（最后选中的是父级）。鼠标位于 3D Viewport 上方时，按 <kbd>Ctrl</kbd> + <kbd>P</kbd> → **Object (Keep Transform)**。在 Outliner 中，球体到了 Empty 的里面。

![Tab、X-Ray、最底部的顶点；Shift + S、Cursor to Selected；Tab；Shift + A、Empty；在 Outliner 中选中 UVSphere，再 Ctrl + 单击 Empty；Ctrl + P、Object (Keep Transform)](img/pelota-vacio-raton.webp)

### 为什么要用 Empty？

缩放以**原点**为中心放大或缩小。球体的原点在它的中心：如果压扁球体，它会朝中心缩小，悬在半空中。Empty 在地面上：如果压扁的是 Empty，球就会**朝地面**压扁，就像弹跳时那样。

![只对球体执行 S Z 0.5：它朝自己的中心缩小，离开了地面。对 Empty 执行 S Z 0.5：球朝地面压扁](img/pelota-por-que-raton.webp)

从现在起，制作动画的是 **Empty**，而不是球体。

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) 在触屏和 N95 上：顶点点一下就能选中（或用虚拟鼠标）；光标用 **Mesh → Snap → Cursor to Selected**；Empty 用 **Add → Empty**；设置父级时，选中球体和 Empty（Empty 为活动对象），然后选择 **Object → Set Parent → Object (Keep Transform)**。

## 2. 挤压

球碰到地面时会被**压扁**：变得更矮、更宽。为了不让它看起来像是变小了，高度上减少的部分要在宽度上补回来。

1. 选中 **Empty**。在第 **1** 帧，球是圆的，正好碰到地面：<kbd>I</kbd> → **Scale Only**。
2. <kbd>→</kbd>（右方向键）切换到第 **2** 帧。把它压扁：<kbd>S</kbd> <kbd>Z</kbd> `0.75` <kbd>Enter</kbd> 让它变矮，<kbd>S</kbd> <kbd>Shift</kbd> + <kbd>Z</kbd> `1.6` <kbd>Enter</kbd> 让它变宽。<kbd>Shift</kbd> + <kbd>Z</kbd> 表示*除 Z 以外的所有轴*：也就是 X 和 Y 一起。
3. <kbd>I</kbd> → **Scale Only**。

![在第 1 帧按 I，选择 Scale Only；按右方向键；S Z 0.75 和 S Shift + Z 1.6 把球朝地面压扁；按 I，选择 Scale Only](img/pelota-aplastar-raton.webp)

![Android](img/icono-android.svg) 在触屏上：用 3D Viewport 菜单栏上的菱形按钮插入关键帧，**Animation → Insert Keyframe → Scale Only**；点一下 Timeline 上的数字切换帧。要压扁它，执行 **Scale** **Z** `0.75`、**Scale** **X** `1.6` 和 **Scale** **Y** `1.6`，数字在顶部栏的键盘上输入。

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上：<kbd>3</kbd>、<kbd>3</kbd>（Z 轴），铅笔键 + `0.75`，然后按 <kbd>OK</kbd>；接着 <kbd>3</kbd>、<kbd>1</kbd>（X）和 `1.6`，以及 <kbd>3</kbd>、<kbd>2</kbd>（Y）和 `1.6`。关键帧用 **Animation → Insert Keyframe → Scale Only** 插入。

## 3. 拉伸

离开地面时，球会朝运动的方向**拉长**：变得更高、更细。到达顶部时，它又恢复成圆形。

1. 圆形的球你已经有了，就在第 1 帧的关键帧里：复制它。在 Timeline 中，单击第 **1** 帧的菱形，按 <kbd>Shift</kbd> + <kbd>D</kbd>，把副本移到第 **4** 帧并单击。看副本的菱形，而不是指针：它出现在鼠标右边一帧的位置。
2. 对第 **17** 帧和第 **29** 帧也这样做：在这两帧，球又恢复成圆形。
3. 转到第 **4** 帧（单击 Timeline 上的数字）。把它拉长：<kbd>S</kbd> <kbd>Z</kbd> `1.1` <kbd>Enter</kbd> 和 <kbd>S</kbd> <kbd>Shift</kbd> + <kbd>Z</kbd> `0.9` <kbd>Enter</kbd>。
4. <kbd>I</kbd> → **Scale Only**。

![第 1 帧的关键帧被复制到第 4、17 和 29 帧；在第 4 帧，S Z 1.1 和 S Shift + Z 0.9 把球拉长；按 I，选择 Scale Only](img/pelota-estirar-raton.webp)

![Android](img/icono-android.svg) ![Nokia](img/icono-nokia.svg) 在触屏和 N95 上（用虚拟鼠标），不用复制，而是拷贝和粘贴：点一下第 1 帧的关键帧，在 Timeline 的 **Key** 菜单（菱形按钮）中选择 **Copy**；点一下数字转到第 4 帧，选择 **Key → Paste**。第 17 帧和第 29 帧也一样。要拉长它，执行 **Scale** **Z** `1.1`、**Scale** **X** `0.9` 和 **Scale** **Y** `0.9`。

![点一下第 1 帧的关键帧，Key、Copy；点一下第 4 帧，Key、Paste：第 4 帧的球也是圆的](img/pelota-copiar-dedo.webp)

## 4. 跳起

1. 转到第 **3** 帧：球还在地面上，这是它离开地面的时刻。<kbd>I</kbd> → **Location Only**。
2. 转到第 **16** 帧，把它升高：<kbd>G</kbd> <kbd>Z</kbd> `5` <kbd>Enter</kbd>（5 米）。<kbd>I</kbd> → **Location Only**。
3. 为了让它回到地面，把第 **3** 帧的关键帧复制到第 **29** 帧。

按 **Play**（<kbd>Space</kbd>）：球弹起来了。但有些地方不对：它在碰到地面之前减速了，好像飘着一样。这要靠曲线来解决，在第 6 部分。

![在第 3 帧按 I，选择 Location Only；在第 16 帧按 G Z 5，再按 I，选择 Location Only；第 3 帧的关键帧被复制到第 29 帧；按 Play，球上升又下落，但在碰到地面之前减速了](img/pelota-saltar-raton.webp)

![Android](img/icono-android.svg) 在触屏上：**Move** **Z** `5`，然后 **Animation → Insert Keyframe → Location Only**。第 3 帧的关键帧用 **Key → Copy** 和 **Key → Paste** 放到第 29 帧，和第 3 部分一样。

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上：<kbd>1</kbd>、<kbd>3</kbd>（Z 轴），铅笔键 + `5`，然后按 <kbd>OK</kbd>；再选择 **Animation → Insert Keyframe → Location Only**。

## 5. 循环

第 **29** 帧和第 **1** 帧一样：圆形的球，碰着地面。如果动画一直播放到第 29 帧，就会连续出现两帧相同的画面，球会停顿一瞬间：衔接处就会被看出来。所以动画在第 **28** 帧结束：第 28 帧之后是第 1 帧，而第 1 帧和第 29 帧一样。

单击 Timeline 的**时钟**，在 **End** 中输入 `28` <kbd>Enter</kbd>。

同一个时钟里还有这个动画的 **FPS**。这个球是 30 FPS：每次弹跳持续 28 帧，将近一秒。如果你想要电影风格，就设为 24，但要在制作动画**之前**设置：帧数不变的话，24 FPS 下会变慢。每种情况适合用什么 FPS，请参阅[为门制作动画](animar-puerta.md)中讲时间的部分。

![Timeline 的时钟：在 End 中输入 28 并按 Enter](img/pelota-fin-raton.webp)

## 6. 曲线：重力

**Location Only** 保存了三个轴，但球只在 Z 上移动：**X Location** 和 **Y Location** 这两行是水平线。把它们删掉：单击每一行，按 <kbd>X</kbd>。

现在切换到曲线编辑器（**View → Curves**）。Z 的曲线就是跳跃：先上升再下降。想想真正的球：

- **上升时**，它以全速离开地面，逐渐减速，直到在顶部停住：**快速出发后逐渐减速**。选中第 **3** 帧的关键帧，在 Timeline 的工具栏上选择 **Cubic** 和 **Ease Out**。
- **下落时**，它慢慢出发，越落越快，直到撞上地面：**加速后猛停**。选中第 **16** 帧的关键帧：**Cubic** 和 **Ease In**。

（用键盘：<kbd>E</kbd> 打开 **Interpolation Mode**，<kbd>Ctrl</kbd> + <kbd>E</kbd> 打开 **Easing Type**。）

曲线变得像真正的球的弧线一样：顶部是圆的，底部撞到地面的地方是尖的。按 **Play**：现在它有重量感了。

![用 X 删掉 X Location 和 Y Location；View、Curves；第 3 帧的关键帧选择 Cubic 和 Ease Out，第 16 帧的选择 Cubic 和 Ease In：曲线底部变尖，球撞到地面](img/pelota-curvas-raton.webp)

想让它弹得更高或更低？在曲线编辑器中，抓住顶部的关键帧（第 16 帧的那个）并移动它。

## 7. 看看效果

要在没有栅格和 Empty 的情况下观看，关闭 **Overlays → Show Overlays**，然后按 **Play**。

下面三个球在相同的时间内弹跳。第一个用的是线性曲线：上升和下落始终是同样的速度，像机器一样。第二个用的是重力曲线：在顶部减速，撞到地面。第三个还加上了拉伸和挤压：这就是本教程中的球。

![三个同时弹跳的球：线性的，像机器一样；用了曲线的，在顶部减速，撞到地面；以及用了曲线并加上挤压与拉伸的](img/pelota-comparar.webp)

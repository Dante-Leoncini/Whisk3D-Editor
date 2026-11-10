# 更改原点：一扇门

在本教程中，你要让一扇门绕着铰链打开。为此需要更改它的**原点**：对象旋转时围绕的那个点（参见 Object Mode 教程中的[原点](modo-objeto.md#原点)）。

我们从上一个教程[制作门的模型](modelar-puerta.md)中的门开始，它的原点在中心。如果你是通过移动一个侧面做出这扇门的，原点就偏离了中心：**Object → Set Origin → Origin to Geometry** 可以把它移回中心。

## 门转得不对

试着转动它。门绕着自己的中心转，就像旋转门一样，而不是绕着铰链转：

![PC](img/icono-pc.svg) 用 Gizmo：在它的中心单击一下，切换到 **Rotate**，蓝色圆环让门绕竖直轴转动。

![在 Gizmo 中心单击一下，切换到 Rotate；拖动蓝色圆环，门像旋转门一样绕着中心转动，然后转回来](img/puerta-gira-mal-raton.webp)

![Android](img/icono-android.svg) 用手指：在 Gizmo 的中心点一下，切换到 **Rotate**，然后拖动蓝色圆环。

![手指点一下 Gizmo 的中心，切换到 Rotate；然后拖动蓝色圆环，门绕着中心转动，然后转回来](img/puerta-gira-mal-dedo.webp)

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，Gizmo 默认是关闭的：按 <kbd>2</kbd>（旋转）、<kbd>3</kbd>（Z 轴，也就是竖直轴），再用方向键；<kbd>OK</kbd> 确认。

![一部 Nokia N95：按 2 和 3 开始绕竖直轴旋转；右方向键让门绕着中心转动，左方向键把它转回来，OK 确认](img/puerta-gira-mal-nokia.webp)

它转得不对，是因为原点在中心。我们把原点移到铰链处。

## 把原点移到铰链处

1. **进入 Edit Mode。**单击模式按钮，再单击 **Edit Mode**（在 PC 上也可以按 <kbd>Tab</kbd>）。

   ![打开的模式按钮，标出了 Edit Mode](img/puerta-paso-1.webp)

2. **选择面。**在 Edit Mode 中会出现 **Vertex / Edge / Face** 按钮：单击它，选择 **Face**。

   ![打开的 Vertex、Edge、Face 按钮，标出了 Face](img/puerta-paso-2.webp)

3. **选中铰链所在的面。**单击门侧面那个窄面，也就是装铰链的那一侧：它会变成绿色。

   ![门侧面的窄面，被选中显示为绿色，并用圆圈标出](img/puerta-paso-3.webp)

4. **把 3D 光标移到那里。**在 **Mesh** 菜单中选择 **Snap** → **Cursor to Selected**：3D 光标会移到铰链上。

   ![打开的 Mesh 菜单，带有 Snap 子菜单，标出了 Cursor to Selected](img/puerta-paso-4.webp)

5. **回到 Object Mode。**单击模式按钮，再单击 **Object Mode**（或按 <kbd>Tab</kbd>）。

   ![打开的模式按钮，标出了 Object Mode；3D 光标已经在铰链上了](img/puerta-paso-5.webp)

6. **把原点移到 3D 光标处。**在 **Object** 菜单中选择 **Set Origin** → **Origin to 3D Cursor**。门不会移动：移动的是它的原点，Gizmo 也随之移到了铰链上。

   ![打开的 Object 菜单，带有 Set Origin 子菜单，标出了 Origin to 3D Cursor](img/puerta-paso-6.webp)

## 现在转对了

再用 Gizmo 转动它：现在门会绕着铰链打开和关上。

![原点在铰链上之后，Gizmo 的蓝色圆环把门打开到侧向，然后再关上](img/puerta-gira-bien-raton.webp)

![用手指拖动蓝色圆环，门绕着铰链打开，然后再关上](img/puerta-gira-bien-dedo.webp)

![一部 Nokia N95：按 2、3 和右方向键，门绕着铰链打开；左方向键把它关上，OK 确认](img/puerta-gira-bien-nokia.webp)

![Android](img/icono-android.svg) 在触屏上，上面的步骤都一样，只是用点按：点一下打开菜单，点一下选择选项。

![Nokia](img/icono-nokia.svg) 在 Nokia N95 上，<kbd>OK</kbd> 从 Object Mode 切换到 Edit Mode，菜单用左软键打开。

## Set Origin 的选项

| 选项 | 作用 |
|---|---|
| **Origin to Geometry** | 把原点移到网格的中心。 |
| **Origin to 3D Cursor** | 把原点移到 3D 光标所在的位置。 |
| **Geometry to Origin** | 反过来：原点不动，移动网格，直到网格的中心位于原点上。 |

下一个教程中，我们给门做门把手：[制作门把手的模型](modelar-picaporte.md)。

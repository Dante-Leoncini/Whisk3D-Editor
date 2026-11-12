# 你的第一个脚本：旋转的立方体

在这个教程中，你将编写你的第一个 **Lua** 脚本：一个让立方体旋转的脚本。

![Whisk3D 的下方是 IDE，也就是原来 Timeline 所在的位置，里面写好了 girar.lua 脚本；右侧是立方体的 Properties，其中有脚本的卡片：girar 复选框，以及值为 90 的 velocidad](img/script-ide.webp)

## 1. IDE

这个教程用不到 Timeline：把它换成 IDE，也就是脚本编辑器。点击 Timeline 的角落按钮（左上角），然后选择 **IDE**。

IDE 打开时是空的。为了写起来更舒服，把它放大：把同一个角落按钮向上拖动。

![鼠标点击 Timeline 的角落按钮，在菜单中选择 IDE，Timeline 就变成了 IDE；然后把同一个按钮向上拖动，IDE 随之变大](img/script-cambiar-ide.webp)

> [!NOTE]
> 也可以使用其他外部编辑器或 IDE，再把脚本加载进来。脚本可以放在 W3D 项目的外部，也可以放在内部。

## 2. 编写脚本

脚本分为两部分：上面是**属性**，下面是让立方体旋转的**函数**。

### 属性

```lua
propiedades = {
  girar     = true,   -- whether the cube spins or not
  velocidad = 90,     -- how many degrees it spins per second
}
```

- **`propiedades`** 是脚本中可以在 Properties 面板里修改的值，每行一个。`girar = true` 是一个**布尔值**（真或假：`true` 或 `false`），在面板中显示为一个复选框。`velocidad = 90` 是一个数字，显示为一个数字字段。你在这里写的是**默认值**：每个使用这个脚本的对象都可以有自己的值。
- `--` 后面的内容是**注释**：Lua 不会读它，它是写给你看的。这段代码的注释用的是英文，因为编辑器的 IDE 只能显示拉丁字母和西里尔字母。

### 函数

```lua
function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
```

**函数**是一个有名字的代码块：以 `function nombre()` 开头，以 `end` 结尾。里面的代码不会在你写下它的时候执行，而是在每次有人**调用**这个函数时执行。

Whisk3D 有一些**保留函数**：如果你的脚本中有一个函数用了其中某个名字，引擎就会在恰当的时机自动调用它。例如：

- **`inicio()`**：只调用一次，在按下 Play 时。用来做准备工作。
- **`actualizar(dt)`**：游戏运行期间的**每一帧**。

还有其他的保留函数，比如在一个对象撞到另一个对象时通知你的函数。它们都在 Lua 手册里：[引擎何时调用你的脚本](../lua/ciclo-del-script.md)。

`actualizar` 非常适合用来做电子游戏。游戏每秒要绘制很多次画面，其中的每一张画面就是一**帧**：所有会动的东西，每一帧都移动一点点。这里，立方体每一帧都多转一点。

- 它在对象**可见**时运行：如果你隐藏了对象，它的脚本就会停止运行，直到对象重新显示出来。
- `dt` 是距上一帧经过的秒数。把速度乘以 `dt`，无论游戏以每秒 30 帧还是 60 帧运行，立方体都会**每秒**旋转 90 度。

它在里面做了什么：

**`if propiedad("girar") then` … `end`** 是一个**条件**。`if`（“如果”）只有在 `if` 和 `then` 之间的内容为真时，才执行它里面的代码。它可以有一个 `elseif`（“否则，如果……就做这个”），用来检查另一个条件；还可以有一个 `else`（“否则”），在所有条件都不满足时执行；这两个都是可选的。它总是以 `end` 结尾。例如：

```lua
if puntos >= 100 then
  premio = "gold"
elseif puntos >= 50 then
  premio = "silver"
else
  premio = "none"
end
```

有 120 分时，奖品是金牌（gold）；有 70 分时，是银牌（silver）；有 10 分时，什么都没有（none）。只会执行第一个满足条件的部分。（在 Lua 中，`elseif` 是连在一起写的。）

在这个脚本中，`if` 既没有 `elseif` 也没有 `else`：如果 **girar** 为真（复选框已勾选），立方体就旋转；如果为假，什么也不会发生，函数就此结束。`propiedad("girar")` 读取该对象在面板中的值。因为每一帧都会读取，所以如果你在游戏运行时修改它，脚本会立刻知道。

- **`yo()`** 是拥有这个脚本的对象：立方体。**`rotacionZ(yo())`** 给出它绕 Z 轴的旋转角度，单位是度（Z 是竖直方向，和你在 Properties 面板中看到的 Z 是同一个），**`setRotacionZ`** 给它设置一个新的旋转：在原来的基础上再多一点。这样，立方体就像陀螺一样旋转。

`propiedad`、`yo`、`rotacionZ` 和 `setRotacionZ` 也是函数，不过方向反过来：它们由 Whisk3D 提供，由你来调用。它们都在 [Lua 手册](../lua/README.md)里。

### 完整的脚本

点击 IDE 内部，把全部内容一起写进去：

```lua
propiedades = {
  girar     = true,   -- whether the cube spins or not
  velocidad = 90,     -- how many degrees it spins per second
}

function actualizar(dt)
  if propiedad("girar") then
    setRotacionZ(yo(), rotacionZ(yo()) + propiedad("velocidad") * dt)
  end
end
```

## 3. 保存脚本

如果还没有保存项目，先保存项目：只有项目已经保存过，脚本才能保存在项目内部。在 Properties 中，打开**软盘**选项卡（**File** 卡片），输入一个 **Name**，例如 `girar-cubo`，然后点击 **Save**。项目会保存到卡片上显示的 **Folder** 中。

然后，在 IDE 的菜单栏中选择 **File → Save As...**：

1. 在 **Name** 中输入 `girar.lua`。
2. 勾选 **Inside the .w3d**：脚本会保存在项目内部，也就是同一个 `.w3d` 文件中。**Folder** 会变成项目内部的文件夹：**(root)** 是主文件夹。
3. 点击 **Save**。

![IDE 的 Save As... 表单，名称为 girar.lua，并勾选了 Inside the .w3d](img/script-guardar.webp)

如果不勾选 **Inside the .w3d**，脚本会作为一个单独的文件，保存在项目外部、你所选的 **Folder** 中。比如，想用其他程序编辑它时，就可以这样做。

![同一个表单，没有勾选 Inside the .w3d：脚本作为单独的文件保存在 Folder 中](img/script-guardar-afuera.webp)

## 4. 把场景转换为游戏

**场景**用于建模和制作动画；脚本在**游戏**中运行。游戏没有自己的动画：动画是在场景或带骨架的 3D 模型中制作的，再导入到游戏中使用。在 3D Viewport（或 Outliner）的菜单栏中，点击场景按钮，也就是显示场景名称的那个按钮，然后选择 **Convert to Game**。

![在 3D Viewport 的菜单栏中打开的场景按钮菜单，指针停在 Convert to Game 上](img/script-convertir-juego.webp)

**Convert to Game** 不会改变你的场景：它会创建一个副本，这个副本是一个游戏，其中的一切都和 Timeline 当前帧中看到的一样。场景的动画不会保留在游戏中：那一帧会被记录为游戏的初始位置和初始状态。原来的场景保持不变，动画也都还在，你可以在场景按钮的菜单中重新找到它。

现在你已经在游戏中了：场景按钮上显示一个摇杆图标，3D Viewport 的菜单栏中出现了 **Play**。

## 5. 把脚本加到立方体上

1. 选中游戏中的 **Cube**（在 Outliner 或 3D Viewport 中）。
2. 在 Properties 中，切换到**摇杆**选项卡。
3. 在 **Control** 卡片中，点击 **Add**：会展开一个列出项目中脚本的菜单。选择 `girar.lua`。

![在 Control 卡片中展开的 Add 菜单：项目中的脚本 girar.lua，以及 Load .lua...](img/script-anadir.webp)

脚本出现在 Control 卡片的列表中，下面还会添加它自己的卡片 **girar.lua**，里面是你写的属性：**girar** 复选框（已勾选）和 **velocidad**（90）。

在列表中，脚本的右边有一个**眼睛**图标：点击它，脚本就会关闭（引擎不再使用它），但不会把它从对象上移除，也不会隐藏对象。再点一下，就会重新打开。

![Control 卡片，列表中有 girar.lua，还有 Add 和 Remove 按钮；下面是 girar.lua 卡片，girar 复选框已勾选，velocidad 为 90](img/script-propiedades.webp)

### 外部的脚本

如果你用其他编辑器写了脚本，或者把它保存在了 `.w3d` 外部，点击 **Add → Load .lua...**。这会打开文件浏览器 **Load script**：选择文件，然后点击 **Open**。

![在 girar.lua 所在文件夹中打开的 Load script 文件浏览器，已选中 girar.lua](img/script-cargar-lua.webp)

## 6. 按下 Play

点击 **Play**：立方体转起来了。游戏运行时，3D Viewport 的菜单栏会显示 **Stop** 和 **Pause**。

在游戏运行时，试着修改 **girar.lua** 卡片中的属性：

- 把 **velocidad** 设为 360：它会转得快四倍；
- 取消勾选 **girar**：它会停下来。再次勾选，它就继续转。

要结束时，点击 **Stop**：立方体会回到按 Play 之前的样子。

![Play：立方体旋转；velocidad 变为 360，转得更快；取消勾选 girar，它停了下来；Stop：立方体回到原位，velocidad 为 90，girar 已勾选](img/script-jugar.webp)

> [!NOTE]
> 游戏运行时发生的一切，在点击 **Stop** 后都会丢失：立方体回到原位，**girar** 和 **velocidad** 也会恢复成按 Play 之前的值。想要真正修改它们，请在游戏停止时修改。

## 继续学习

- [Whisk3D 中的 Lua](../lua/README.md)：Lua 手册，包含所有函数。
- [脚本是对象的一个组件](../lua/scripts-y-objetos.md)：属性（还包括列表、文本和场景中的对象）。
- [引擎何时调用你的脚本](../lua/ciclo-del-script.md)：`actualizar`、`inicio` 以及其他函数。

## 下载示例

这是完成后的项目，你可以试一试，或者和你自己做的比较一下：**[girar-cubo.w3d](ejemplos/girar-cubo.w3d)**。在 Whisk3D 中打开它，然后点击 **Play**。脚本就在 `.w3d` 内部，可以在 IDE 中看到。

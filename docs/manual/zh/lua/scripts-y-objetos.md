# 脚本是对象的一个组件

在 Whisk3D 中，脚本总是属于场景中的某个**对象**（一个网格、一个空物体、一个 2D 按钮……）。一个对象可以有多个脚本；每个脚本独立运行，拥有自己的变量，并按出现的顺序执行。

在脚本中，`self()` 返回拥有该脚本的对象。要使用*其他*对象，脚本需要声明一个名为 `properties` 的全局表。编辑器会读取它，并在 Properties 面板中该脚本的卡片上，为每个条目显示一个字段：

```lua
properties = {
  ball       = "object",                          -- 一个字段，用来选择场景中的对象
  difficulty = { "facil", "normal", "dificil" },  -- 一个下拉列表（第一项是默认值）
  speed      = 200,                               -- 使用该脚本的每个对象各自的数字
  active     = true,                              -- 一个是/否开关
  label      = "caja",                            -- 一段文本
}

function start()
  local p   = object("ball")         -- 在 "pelota" 字段中选择的对象
  local dif = option("difficulty")   -- "facil"、"normal" 或 "dificil"
  local v   = property("speed")      -- 200，或在该对象上设置的值
end
```

> [!NOTE]
> 为了读取 `properties`，即使游戏没有运行，编辑器也会**执行该文件**。所以在函数之外只放声明（表、常量、`local`），绝不要放会实际做事情的代码。

在 Properties 中，脚本的卡片是这样显示的：每个属性一个字段，顺序和代码中的一样。

![Properties 中脚本的卡片：pelota（一个可选择的对象）、dificultad（一个列表）、velocidad（200）、activo（一个已勾选的复选框）和 etiqueta（caja）](../tutoriales/img/lua-propiedades.webp)

这样，同一个脚本就可以用于许多取值不同的对象：一个敌人的 `speed` 字段可以是 200，另一个可以是 350。

## 实时修改属性

`property("speed")` 读取的是此刻的值。如果在游戏运行时在 Properties 中修改它，脚本会在下一帧看到新值，但前提是它用 `property()` 重新读取。如果你把它存进了变量（在 `start()` 中写 `local v = property("speed")`），变量会一直保持开始时的值。

```lua
function update(dt)
  -- 每一帧都读取：游戏运行时在 Properties 中修改的值也能看到
  move(self(), property("speed") * dt, 0, 0)
end
```

每一帧都读取 `property()` 比使用变量更慢。不建议整个游戏都这样写：在调整数值时这样用，数值定下来以后，再在 `start()` 中把它们存进变量。

## 按钮

如果某个属性是一个**函数**，卡片上就会出现一个**按钮**：点击它就会调用这个函数。要让按钮显示名称以外的文字，就把文字和函数放进一个表里：

```lua
properties = {
  speed    = 90,
  zero = { "归零", function()
    setRotation(self(), 0, 0, 0)
  end },
}
```

![带有 velocidad 和“归零”按钮的脚本卡片](../tutoriales/img/lua-boton.webp)

游戏运行时，函数在正在运行的脚本中执行。没有运行游戏时，脚本只会为这次调用而加载：这样，按钮就可以当作**编辑器插件**，对场景做一些事情。例如，用 `selected()` 获取编辑器中所选的对象：

```lua
properties = {
  upload = { "抬高所选对象", function()
    for _, o in ipairs(selected()) do
      move(o, 0, 0, 1)   -- 向上一米
    end
  end },
}
```

> [!NOTE]
> 游戏停止时按钮所做的修改会留在场景中，就像你亲手做的一样，也可以同样撤销：<kbd>Ctrl</kbd> + <kbd>Z</kbd> 会一次撤销按钮所做的全部修改。游戏运行时不会记录到历史中：按下 **Stop** 后，一切都会恢复原样。

脚本之间不共享变量。要互相传递数据（分数、玩家是否获胜……），它们使用一个共享空间：`setShared("puntos", 10)` 写入，`getShared("puntos")` 读取。它可以保存数字、是/否和文本，游戏停止时会被清空。

# Lua 基础

## 注释

```lua
-- 这是单行注释
--[[ 这是一个
     多行注释 ]]
```

## 变量和类型

给变量赋值就会创建它。请始终使用 `local`：这样变量只存在于声明它的代码块中，不会和其他变量冲突。

```lua
local lives = 3         -- 数字（整数）
local speed = 2.5       -- 数字（带小数）
local name = "Whisky"   -- 文本（string）
local live = true       -- 布尔值：true 或 false
local none = nil        -- nil = “没有值”
```

Lua 的类型有：`nil`、`boolean`、`number`、`string`、`table` 和 `function`。Whisk3D 提供给你的场景对象是一种特殊类型，只能把它传给引擎的函数来使用。

## 运算符

| 运算符 | 作用 | 示例 |
|---|---|---|
| `+ - * /` | 加、减、乘、除（除法结果总是带小数） | `7 / 2` → 3.5 |
| `//` | 整除 | `7 // 2` → 3 |
| `%` | 取余 | `7 % 2` → 1 |
| `^` | 乘方 | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | 比较（`~=` 表示“不等于”） | `lives ~= 0` |
| `and or not` | 逻辑运算 | `live and lives > 0` |
| `..` | 连接文本 | `"Vidas: " .. lives` |
| `#` | 文本或列表的长度 | `#"hola"` → 4 |

> [!NOTE]
> 在 Lua 中，只有 `false` 和 `nil` 是“假”。`0` 和空文本 `""` 都算作**真**。

## 条件判断：if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## 循环：for 和 while

```lua
for i = 1, 10 do          -- 从 1 到 10
  print(i)
end
for i = 10, 0, -2 do      -- 从 10 到 0，每次减 2
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Lua 没有 `n++`，也没有 `n += 1`：要写成 `n = n + 1`。要跳出循环，使用 `break`。

## 函数

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- 一个函数可以返回多个值
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Whisk3D 的许多函数会同时返回多个值，例如 `local x, y, z = getPosition(self())`。

## 表：列表和字典

表是 Lua 唯一的数据结构，什么都可以用它来做。

```lua
-- 作为列表（索引从 1 开始，而不是 0）
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- 添加到末尾
table.insert(fruits, "melón")  -- 另一种写法

for i, f in ipairs(fruits) do  -- 按顺序遍历列表
  print(i, f)
end

-- 作为字典（键 = 值）
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- 遍历所有键（顺序不固定）
  print(clave, value)
end
```

## 实用的文本和数字函数

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- 文本转数字
local s = tostring(3.5)                 -- 数字转文本
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

要生成随机数，Whisk3D 提供了 `random()`（参见参考部分）。`math.random` 也能用，它在每局游戏中都已经用不同的种子初始化好了。

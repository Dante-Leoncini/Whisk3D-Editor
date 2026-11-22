# Lua basics

## Comments

```lua
-- this is a single-line comment
--[[ this is a comment
     spanning several lines ]]
```

## Variables and types

A variable is created when you assign it a value. Always use `local`: the variable exists only inside the block where it was declared and doesn't clash with others.

```lua
local lives = 3         -- number (integer)
local speed = 2.5       -- number (with decimals)
local name = "Whisky"   -- text (string)
local live = true       -- boolean: true or false
local none = nil        -- nil = "no value"
```

Lua's types are: `nil`, `boolean`, `number`, `string`, `table` and `function`. The scene objects Whisk3D gives you are a special type that you only use by passing it to the engine's functions.

## Operators

| Operator | What it does | Example |
|---|---|---|
| `+ - * /` | Addition, subtraction, multiplication, division (always with decimals) | `7 / 2` → 3.5 |
| `//` | Integer division | `7 // 2` → 3 |
| `%` | Remainder | `7 % 2` → 1 |
| `^` | Power | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | Comparisons (`~=` means "not equal") | `lives ~= 0` |
| `and or not` | Logical | `live and lives > 0` |
| `..` | Join strings | `"Vidas: " .. lives` |
| `#` | Length of a string or a list | `#"hola"` → 4 |

> [!NOTE]
> In Lua, only `false` and `nil` are "false". `0` and the empty string `""` count as **true**.

## Decisions: if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## Loops: for and while

```lua
for i = 1, 10 do          -- from 1 to 10
  print(i)
end
for i = 10, 0, -2 do      -- from 10 down to 0, in steps of 2
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Lua has no `n++` or `n += 1`: you write `n = n + 1`. To break out of a loop, use `break`.

## Functions

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- a function can return several values
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Many Whisk3D functions return several values at once, for example `local x, y, z = getPosition(self())`.

## Tables: lists and dictionaries

The table is Lua's only data structure, and it's used for everything.

```lua
-- as a list (indices start at 1, not 0)
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- add at the end
table.insert(fruits, "melón")  -- another way

for i, f in ipairs(fruits) do  -- go through a list in order
  print(i, f)
end

-- as a dictionary (key = value)
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- go through all the keys (in no fixed order)
  print(clave, value)
end
```

## Useful string and number functions

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- string to number
local s = tostring(3.5)                 -- number to string
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

For random numbers, Whisk3D has `random()` (see the reference). `math.random` also works, and it already comes seeded differently in each game session.

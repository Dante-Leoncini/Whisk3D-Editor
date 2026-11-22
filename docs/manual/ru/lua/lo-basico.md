# Основы Lua

## Комментарии

```lua
-- это однострочный комментарий
--[[ это комментарий
     на несколько строк ]]
```

## Переменные и типы

Переменная создаётся, когда ей присваивают значение. Всегда используй `local`: такая переменная существует только внутри блока, где она объявлена, и не конфликтует с другими.

```lua
local lives = 3         -- число (целое)
local speed = 2.5       -- число (дробное)
local name = "Whisky"   -- текст (строка)
local live = true       -- логическое значение: true или false
local none = nil        -- nil = "нет значения"
```

Типы Lua: `nil`, `boolean`, `number`, `string`, `table` и `function`. Объекты сцены, которые выдаёт Whisk3D, — это особый тип, который используется только для передачи в функции движка.

## Операторы

| Оператор | Что делает | Пример |
|---|---|---|
| `+ - * /` | Сложение, вычитание, умножение, деление (всегда с дробной частью) | `7 / 2` → 3.5 |
| `//` | Целочисленное деление | `7 // 2` → 3 |
| `%` | Остаток от деления | `7 % 2` → 1 |
| `^` | Возведение в степень | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | Сравнения (`~=` означает "не равно") | `lives ~= 0` |
| `and or not` | Логические | `live and lives > 0` |
| `..` | Склеивание строк | `"Vidas: " .. lives` |
| `#` | Длина строки или списка | `#"hola"` → 4 |

> [!NOTE]
> В Lua "ложью" считаются только `false` и `nil`. `0` и пустая строка `""` считаются **истиной**.

## Условия: if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## Повторения: for и while

```lua
for i = 1, 10 do          -- от 1 до 10
  print(i)
end
for i = 10, 0, -2 do      -- от 10 до 0, с шагом 2 вниз
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

В Lua нет `n++` и `n += 1`: пишут `n = n + 1`. Чтобы прервать цикл, используется `break`.

## Функции

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- функция может возвращать несколько значений
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Многие функции Whisk3D возвращают сразу несколько значений, например `local x, y, z = getPosition(self())`.

## Таблицы: списки и словари

Таблица — единственная структура данных в Lua, и она годится для всего.

```lua
-- как список (индексы начинаются с 1, а не с 0)
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- добавить в конец
table.insert(fruits, "melón")  -- другой способ

for i, f in ipairs(fruits) do  -- обойти список по порядку
  print(i, f)
end

-- как словарь (ключ = значение)
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- обойти все ключи (порядок не гарантирован)
  print(clave, value)
end
```

## Полезное для строк и чисел

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- строку в число
local s = tostring(3.5)                 -- число в строку
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

Для случайных чисел в Whisk3D есть `random()` (см. справочник). Работает и `math.random`: его генератор уже инициализируется по-разному в каждой игре.

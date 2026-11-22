# Luaの基本

## コメント

```lua
-- これは1行のコメントです
--[[ これは
     複数行のコメントです ]]
```

## 変数と型

変数は、値を代入したときに作られます。必ず`local`を使ってください。そうすると、変数は宣言したブロックの中だけに存在し、ほかの変数とぶつかりません。

```lua
local lives = 3         -- 数値（整数）
local speed = 2.5       -- 数値（小数）
local name = "Whisky"   -- テキスト（文字列）
local live = true       -- 真偽値：trueまたはfalse
local none = nil        -- nil =「値なし」
```

Luaの型は、`nil`、`boolean`、`number`、`string`、`table`、`function`です。Whisk3Dが渡すシーンのオブジェクトは特別な型で、エンジンの関数に渡すことでだけ使えます。

## 演算子

| 演算子 | 意味 | 例 |
|---|---|---|
| `+ - * /` | 足し算、引き算、掛け算、割り算（割り算は常に小数） | `7 / 2` → 3.5 |
| `//` | 整数の割り算 | `7 // 2` → 3 |
| `%` | 余り | `7 % 2` → 1 |
| `^` | べき乗 | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | 比較（`~=`は「等しくない」） | `lives ~= 0` |
| `and or not` | 論理演算 | `live and lives > 0` |
| `..` | テキストの連結 | `"Vidas: " .. lives` |
| `#` | テキストまたはリストの長さ | `#"hola"` → 4 |

> [!NOTE]
> Luaでは、`false`と`nil`だけが「偽」です。`0`や空のテキスト`""`は**真**として扱われます。

## 条件分岐：if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## 繰り返し：forとwhile

```lua
for i = 1, 10 do          -- 1から10まで
  print(i)
end
for i = 10, 0, -2 do      -- 10から0まで、2ずつ減らす
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Luaには`n++`も`n += 1`もありません。`n = n + 1`と書きます。ループを途中で抜けるには`break`を使います。

## 関数

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- 関数は複数の値を返せる
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Whisk3Dの多くの関数は、一度に複数の値を返します。たとえば`local x, y, z = getPosition(self())`のように使います。

## テーブル：リストと辞書

テーブルはLuaで唯一のデータ構造で、あらゆる用途に使えます。

```lua
-- リストとして（インデックスは0ではなく1から始まる）
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- 末尾に追加
table.insert(fruits, "melón")  -- 別の方法

for i, f in ipairs(fruits) do  -- リストを順番にたどる
  print(i, f)
end

-- 辞書として（キー = 値）
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- すべてのキーをたどる（順序は決まっていない）
  print(clave, value)
end
```

## テキストと数値の便利な関数

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- テキストを数値に
local s = tostring(3.5)                 -- 数値をテキストに
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

乱数には、Whisk3Dの`random()`を使えます（リファレンスを参照）。`math.random`も使えます。こちらは、プレイごとに異なるシードがあらかじめ設定されています。

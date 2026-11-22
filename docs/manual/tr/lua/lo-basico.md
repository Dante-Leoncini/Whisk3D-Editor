# Lua'nın temelleri

## Yorumlar

```lua
-- bu tek satırlık bir yorumdur
--[[ bu birden fazla
     satırlık bir yorumdur ]]
```

## Değişkenler ve türler

Bir değişken, ona bir değer atandığında oluşturulur. Her zaman `local` kullan: değişken yalnızca tanımlandığı blok içinde var olur ve diğerleriyle çakışmaz.

```lua
local lives = 3         -- sayı (tam sayı)
local speed = 2.5       -- sayı (ondalıklı)
local name = "Whisky"   -- metin (string)
local live = true       -- boolean: true ya da false
local none = nil        -- nil = "değer yok"
```

Lua'nın türleri şunlardır: `nil`, `boolean`, `number`, `string`, `table` ve `function`. Whisk3D'nin sana verdiği sahne nesneleri, yalnızca motorun fonksiyonlarına aktarılarak kullanılan özel bir türdür.

## Operatörler

| Operatör | Ne yapar | Örnek |
|---|---|---|
| `+ - * /` | Toplama, çıkarma, çarpma, bölme (her zaman ondalıklı) | `7 / 2` → 3.5 |
| `//` | Tam sayı bölmesi | `7 // 2` → 3 |
| `%` | Kalan | `7 % 2` → 1 |
| `^` | Üs alma | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | Karşılaştırmalar (`~=` "eşit değil" demektir) | `lives ~= 0` |
| `and or not` | Mantıksal | `live and lives > 0` |
| `..` | Metinleri birleştirme | `"Vidas: " .. lives` |
| `#` | Bir metnin ya da listenin uzunluğu | `#"hola"` → 4 |

> [!NOTE]
> Lua'da yalnızca `false` ve `nil` "yanlış" sayılır. `0` ve boş metin `""` **doğru** sayılır.

## Kararlar: if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## Tekrarlar: for ve while

```lua
for i = 1, 10 do          -- 1'den 10'a
  print(i)
end
for i = 10, 0, -2 do      -- 10'dan 0'a, 2'şer azalarak
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Lua'da `n++` ya da `n += 1` yoktur: `n = n + 1` yazılır. Bir döngüyü kesmek için `break` kullanılır.

## Fonksiyonlar

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- bir fonksiyon birden fazla değer döndürebilir
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Whisk3D'nin birçok fonksiyonu aynı anda birden fazla değer döndürür, örneğin `local x, y, z = getPosition(self())`.

## Tablolar: listeler ve sözlükler

Tablo, Lua'nın tek veri yapısıdır ve her işe yarar.

```lua
-- liste olarak (dizinler 0'dan değil, 1'den başlar)
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- sona ekleme
table.insert(fruits, "melón")  -- başka bir yol

for i, f in ipairs(fruits) do  -- bir listeyi sırayla dolaşma
  print(i, f)
end

-- sözlük olarak (anahtar = değer)
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- tüm anahtarları dolaşma (sabit bir sıra olmadan)
  print(clave, value)
end
```

## Kullanışlı metin ve sayı işlemleri

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- metinden sayıya
local s = tostring(3.5)                 -- sayıdan metne
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

Rastgele sayılar için Whisk3D'de `random()` vardır (başvuruya bak). `math.random` da çalışır; her oyunda farklı bir tohumla başlatılmış olarak gelir.

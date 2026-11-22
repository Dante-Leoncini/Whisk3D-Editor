# Lua की बुनियादी बातें

## टिप्पणियाँ

```lua
-- यह एक लाइन की टिप्पणी है
--[[ यह कई लाइनों वाली
     एक टिप्पणी है ]]
```

## वेरिएबल और टाइप

कोई वेरिएबल तब बनता है जब आप उसे कोई वैल्यू देते हैं। हमेशा `local` का इस्तेमाल करें: वेरिएबल सिर्फ़ उसी ब्लॉक के अंदर रहता है जहाँ उसे घोषित किया गया, और दूसरों से नहीं टकराता।

```lua
local lives = 3         -- संख्या (पूर्णांक)
local speed = 2.5       -- संख्या (दशमलव के साथ)
local name = "Whisky"   -- टेक्स्ट (string)
local live = true       -- बूलियन: true या false
local none = nil        -- nil = "कोई वैल्यू नहीं"
```

Lua के टाइप ये हैं: `nil`, `boolean`, `number`, `string`, `table` और `function`। Whisk3D आपको सीन के जो ऑब्जेक्ट देता है, वे एक ख़ास टाइप के होते हैं, जिनका इस्तेमाल सिर्फ़ उन्हें इंजन के फ़ंक्शन में पास करके होता है।

## ऑपरेटर

| ऑपरेटर | क्या करता है | उदाहरण |
|---|---|---|
| `+ - * /` | जोड़, घटाव, गुणा, भाग (हमेशा दशमलव के साथ) | `7 / 2` → 3.5 |
| `//` | पूर्णांक भाग | `7 // 2` → 3 |
| `%` | शेषफल | `7 % 2` → 1 |
| `^` | घात | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | तुलना (`~=` का मतलब है "बराबर नहीं") | `lives ~= 0` |
| `and or not` | लॉजिकल | `live and lives > 0` |
| `..` | टेक्स्ट जोड़ना | `"Vidas: " .. lives` |
| `#` | किसी टेक्स्ट या सूची की लंबाई | `#"hola"` → 4 |

> [!NOTE]
> Lua में सिर्फ़ `false` और `nil` "असत्य" होते हैं। `0` और ख़ाली टेक्स्ट `""` भी **सत्य** माने जाते हैं।

## फ़ैसले: if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## दोहराव: for और while

```lua
for i = 1, 10 do          -- 1 से 10 तक
  print(i)
end
for i = 10, 0, -2 do      -- 10 से 0 तक, 2-2 करके नीचे की ओर
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Lua में `n++` या `n += 1` नहीं है: `n = n + 1` लिखा जाता है। किसी लूप को बीच में रोकने के लिए `break` का इस्तेमाल होता है।

## फ़ंक्शन

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- एक फ़ंक्शन कई वैल्यू लौटा सकता है
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Whisk3D के कई फ़ंक्शन एक साथ कई वैल्यू लौटाते हैं, जैसे `local x, y, z = getPosition(self())`।

## टेबल: सूचियाँ और डिक्शनरी

टेबल Lua का इकलौता डेटा स्ट्रक्चर है और हर काम आता है।

```lua
-- सूची के रूप में (इंडेक्स 1 से शुरू होते हैं, 0 से नहीं)
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- अंत में जोड़ना
table.insert(fruits, "melón")  -- दूसरा तरीका

for i, f in ipairs(fruits) do  -- सूची में क्रम से घूमना
  print(i, f)
end

-- डिक्शनरी के रूप में (कुंजी = वैल्यू)
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- सारी कुंजियों में घूमना (कोई तय क्रम नहीं)
  print(clave, value)
end
```

## टेक्स्ट और संख्याओं के काम के फ़ंक्शन

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- टेक्स्ट से संख्या
local s = tostring(3.5)                 -- संख्या से टेक्स्ट
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

रैंडम संख्याओं के लिए Whisk3D में `random()` है (रेफ़रेंस देखें)। `math.random` भी काम करता है, जिसका सीड हर गेम में पहले से ही अलग सेट होता है।

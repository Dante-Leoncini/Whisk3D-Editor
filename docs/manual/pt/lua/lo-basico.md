# O básico de Lua

## Comentários

```lua
-- isto é um comentário de uma linha
--[[ isto é um comentário
     de várias linhas ]]
```

## Variáveis e tipos

Uma variável é criada ao receber um valor. Use sempre `local`: a variável existe só dentro do bloco onde foi declarada e não conflita com outras.

```lua
local lives = 3         -- número (inteiro)
local speed = 2.5       -- número (com decimais)
local name = "Whisky"   -- texto (string)
local live = true       -- booleano: true ou false
local none = nil        -- nil = "sem valor"
```

Os tipos de Lua são: `nil`, `boolean`, `number`, `string`, `table` e `function`. Os objetos da cena que o Whisk3D entrega a você são um tipo especial, que só é usado passando-o para as funções do motor.

## Operadores

| Operador | O que faz | Exemplo |
|---|---|---|
| `+ - * /` | Soma, subtração, multiplicação, divisão (sempre com decimais) | `7 / 2` → 3.5 |
| `//` | Divisão inteira | `7 // 2` → 3 |
| `%` | Resto | `7 % 2` → 1 |
| `^` | Potência | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | Comparações (`~=` é "diferente") | `lives ~= 0` |
| `and or not` | Lógicos | `live and lives > 0` |
| `..` | Juntar textos | `"Vidas: " .. lives` |
| `#` | Tamanho de um texto ou de uma lista | `#"hola"` → 4 |

> [!NOTE]
> Em Lua, só `false` e `nil` são "falso". O `0` e o texto vazio `""` contam como **verdadeiro**.

## Decisões: if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## Repetições: for e while

```lua
for i = 1, 10 do          -- de 1 a 10
  print(i)
end
for i = 10, 0, -2 do      -- de 10 a 0, de 2 em 2 para baixo
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Lua não tem `n++` nem `n += 1`: escreve-se `n = n + 1`. Para interromper um laço, usa-se `break`.

## Funções

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- uma função pode devolver vários valores
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Muitas funções do Whisk3D devolvem vários valores de uma vez, por exemplo `local x, y, z = getPosition(self())`.

## Tabelas: listas e dicionários

A tabela é a única estrutura de dados de Lua e serve para tudo.

```lua
-- como lista (os índices começam em 1, não em 0)
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- adicionar no final
table.insert(fruits, "melón")  -- outra forma

for i, f in ipairs(fruits) do  -- percorrer uma lista em ordem
  print(i, f)
end

-- como dicionário (chave = valor)
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- percorrer todas as chaves (sem ordem fixa)
  print(clave, value)
end
```

## Textos e números úteis

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- texto para número
local s = tostring(3.5)                 -- número para texto
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

Para números aleatórios, o Whisk3D tem `random()` (veja a referência). Também funciona `math.random`, que já vem com uma semente diferente em cada partida.

# Les bases de Lua

## Commentaires

```lua
-- ceci est un commentaire d'une ligne
--[[ ceci est un commentaire
     sur plusieurs lignes ]]
```

## Variables et types

Une variable est créée quand on lui affecte une valeur. Utilisez toujours `local` : la variable n'existe que dans le bloc où elle a été déclarée et n'entre pas en conflit avec les autres.

```lua
local lives = 3         -- nombre (entier)
local speed = 2.5       -- nombre (à virgule)
local name = "Whisky"   -- texte (string)
local live = true       -- booléen : true ou false
local none = nil        -- nil = "pas de valeur"
```

Les types de Lua sont : `nil`, `boolean`, `number`, `string`, `table` et `function`. Les objets de la scène que vous fournit Whisk3D sont d'un type spécial qui ne s'utilise qu'en le passant aux fonctions du moteur.

## Opérateurs

| Opérateur | Ce qu'il fait | Exemple |
|---|---|---|
| `+ - * /` | Addition, soustraction, multiplication, division (toujours à virgule) | `7 / 2` → 3.5 |
| `//` | Division entière | `7 // 2` → 3 |
| `%` | Reste | `7 % 2` → 1 |
| `^` | Puissance | `2 ^ 3` → 8.0 |
| `== ~= < > <= >=` | Comparaisons (`~=` signifie « différent ») | `lives ~= 0` |
| `and or not` | Logiques | `live and lives > 0` |
| `..` | Concaténer des textes | `"Vidas: " .. lives` |
| `#` | Longueur d'un texte ou d'une liste | `#"hola"` → 4 |

> [!NOTE]
> En Lua, seuls `false` et `nil` sont « faux ». Le `0` et le texte vide `""` comptent comme **vrai**.

## Décisions : if

```lua
if vidas <= 0 then
  print("perdiste")
elseif vidas == 1 then
  print("última vida")
else
  print("seguís jugando")
end
```

## Répétitions : for et while

```lua
for i = 1, 10 do          -- de 1 à 10
  print(i)
end
for i = 10, 0, -2 do      -- de 10 à 0, en descendant de 2 en 2
  print(i)
end
local n = 0
while n < 5 do
  n = n + 1
end
```

Lua n'a ni `n++` ni `n += 1` : on écrit `n = n + 1`. Pour interrompre une boucle, on utilise `break`.

## Fonctions

```lua
local function add(a, b)
  return a + b
end
print(add(2, 3))        -- 5

-- une fonction peut renvoyer plusieurs valeurs
local function minMax(a, b)
  if a < b then return a, b else return b, a end
end
local less, greater = minMax(8, 3)
```

Beaucoup de fonctions de Whisk3D renvoient plusieurs valeurs à la fois, par exemple `local x, y, z = getPosition(self())`.

## Tables : listes et dictionnaires

La table est la seule structure de données de Lua, et elle sert à tout.

```lua
-- comme liste (les indices commencent à 1, pas à 0)
local fruits = { "manzana", "pera", "uva" }
print(fruits[1])          -- manzana
print(#fruits)            -- 3
fruits[#fruits + 1] = "kiwi"   -- ajouter à la fin
table.insert(fruits, "melón")  -- autre façon

for i, f in ipairs(fruits) do  -- parcourir une liste dans l'ordre
  print(i, f)
end

-- comme dictionnaire (clé = valeur)
local player = { name = "Ana", puntos = 0 }
player.puntos = player.puntos + 10
print(player["nombre"])  -- Ana

for clave, value in pairs(player) do  -- parcourir toutes les clés (sans ordre fixe)
  print(clave, value)
end
```

## Textes et nombres utiles

```lua
local t = string.format("Puntos: %d  Tiempo: %.1f", 120, 3.456)  -- "Puntos: 120  Tiempo: 3.5"
local may = string.upper("hola")        -- "HOLA"
local part = string.sub("Whisk3D", 1, 5)   -- "Whisk"
local n = tonumber("42")                -- texte vers nombre
local s = tostring(3.5)                 -- nombre vers texte
local r = math.floor(2.7)               -- 2
local d = math.sqrt(16)                 -- 4.0
local a = math.abs(-3)                  -- 3
local m = math.max(4, 9, 2)             -- 9
local pi = math.pi
```

Pour les nombres aléatoires, Whisk3D propose `random()` (voir la référence). `math.random` fonctionne aussi, et il est déjà initialisé différemment à chaque partie.

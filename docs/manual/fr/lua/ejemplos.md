# Exemples complets

## Hello world : déplacer un carré 2D avec les flèches

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- un bip pour savoir que le script a démarré
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Un personnage 3D qui marche et saute

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- vitesse verticale (pour le saut)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- saut simple : il monte, la gravité le fait redescendre et il ne passe pas sous le sol (z = 0)
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Ramasser des pièces avec une hitbox

```lua
-- ce script va sur le personnage, qui a une hitbox enfant
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- le script du compteur le lit
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- cet autre script va sur le texte 2D du compteur
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Créer des ennemis toutes les 2 secondes

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", tourné vers nous
  end
end
```

## Un bouton qui change de scène

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> Un Pong complet et commenté se trouve dans le dépôt d'exemples : `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. Il utilise `speed`, `bounceOn`, `limit`, `finger`, `uiBox` et `random`.

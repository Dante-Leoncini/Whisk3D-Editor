# Esempi completi

## Ciao mondo: muovere un riquadro 2D con le frecce

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- un bip per sapere che è partito
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## Un personaggio 3D che cammina e salta

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- velocità verticale (per il salto)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- salto semplice: sale, la gravità lo riporta giù e non scende sotto il pavimento (z = 0)
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Raccogliere monete con un hitbox

```lua
-- questo script va sul personaggio, che ha un hitbox figlio
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- lo script del segnapunti lo legge
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- quest'altro script va sul testo 2D del segnapunti
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Creare nemici ogni 2 secondi

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", rivolto verso di noi
  end
end
```

## Un pulsante che cambia scena

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> C'è un Pong completo e commentato nel repository degli esempi: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. Usa `speed`, `bounceOn`, `limit`, `finger`, `uiBox` e `random`.

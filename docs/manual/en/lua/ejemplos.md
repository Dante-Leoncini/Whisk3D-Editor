# Complete examples

## Hello world: move a 2D square with the arrows

```lua
properties = { frame = "object" }

function start()
  beep(440, 120, 0.5)          -- a beep so you know it started
end

function update(dt)
  local c = object("frame")
  local x, y = getPosPx(c)
  if key("up") then y = y - 200 * dt end
  if key("down")  then y = y + 200 * dt end
  setPosPx(c, x, y)
end
```

## A 3D character that walks and jumps

```lua
properties = { speed = 4, jump = 6 }

local vz = 0                   -- vertical velocity (for the jump)

function update(dt)
  local yo_ = self()
  local v = property("speed")
  local dx, dy = 0, 0
  if key("left") then dx = dx - 1 end
  if key("right")   then dx = dx + 1 end
  if key("up")    then dy = dy - 1 end
  if key("down")     then dy = dy + 1 end
  move(yo_, dx * v * dt, dy * v * dt, 0)

  -- simple jump: it goes up, gravity brings it down and it doesn't go below the floor (z = 0)
  local x, y, z = getPosition(yo_)
  if keyPressed("space") and z <= 0 then vz = property("jump") end
  vz = vz - 9.8 * dt
  z = z + vz * dt
  if z < 0 then z = 0; vz = 0 end
  setPosition(yo_, x, y, z)
end
```

## Collecting coins with a hitbox

```lua
-- this script goes on the character, which has a child hitbox
local monedas = 0

function onEnter(other, myHitbox, otherHitbox)
  if string.sub(getName(other), 1, 6) == "Moneda" then
    destroy(other)
    monedas = monedas + 1
    setShared("monedas", monedas)   -- the scoreboard's script reads it
    sound("sounds/moneda.wav")
  end
end
```

```lua
-- this other script goes on the scoreboard's 2D text
function update(dt)
  setText(self(), "MONEDAS: " .. (getShared("monedas") or 0))
end
```

## Spawning enemies every 2 seconds

```lua
local time = 0

function update(dt)
  time = time + dt
  if time >= 2 then
    time = 0
    local x = random(-5, 5)
    instantiate("Enemigo", x, 0, -10, 180)   -- prefab "Enemigo", facing toward us
  end
end
```

## A button that changes the scene

```lua
properties = { button = "object" }

function update(dt)
  if pressed(object("button")) or keyPressed("enter") then
    changeScene("Nivel1")
  end
end
```

> [!TIP]
> There's a complete, commented Pong in the examples repository: `Whisk3D-Examples/ui/whiskpaddle/content/juego.lua`. It uses `speed`, `bounceOn`, `limit`, `finger`, `uiBox` and `random`.
